#include "core/LanguageServer.h"
#include "core/LanguageTools.h"
#include "core/Process.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
namespace kiri {
namespace {
using Clock=std::chrono::steady_clock;
bool MakePipe(int (&fds)[2]) {
    if(pipe(fds)!=0) return false;
    for(int fd:fds) fcntl(fd,F_SETFD,FD_CLOEXEC);
    return true;
}
void Nonblock(int fd) { fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK); }
}
RpcProcess::RpcProcess(std::vector<std::string> command,std::string directory,Incoming incoming,Failure failure)
    :fIncoming(std::move(incoming)),fFailure(std::move(failure)),fThread([this,command=std::move(command),directory=std::move(directory)] { Run(command,directory); }) {}
RpcProcess::~RpcProcess() { Stop();Wait(); }
void RpcProcess::Stop() { fStop=true; }
void RpcProcess::Wait() { if(fThread.joinable()) fThread.join(); }
void RpcProcess::Queue(Json message) {
    auto frame=RpcFramer::Frame(message);std::lock_guard<std::mutex> lock(fMutex);
    if(fStop || !fAlive) return;
    if(fQueuedBytes+frame.size()>32*1024*1024) throw std::runtime_error("Language server input queue is full");
    fQueuedBytes+=frame.size();fOutgoing.push_back(std::move(frame));
}
int64_t RpcProcess::Request(const std::string& method,Json params,Callback callback,std::chrono::milliseconds timeout) {
    int64_t id;
    {
        std::lock_guard<std::mutex> lock(fMutex);id=fNextID++;
        if(!fStop && fAlive) fPending.emplace(id,Pending{callback,Clock::now()+timeout});
        else id=0;
    }
    if(!id) { if(callback) callback({{},"Language server is not running",-32000});return 0; }
    try { Queue({{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",std::move(params)}}); }
    catch(const std::exception& error) {
        { std::lock_guard<std::mutex> lock(fMutex);fPending.erase(id); }
        if(callback) callback({{},error.what(),-32000});
        return 0;
    }
    return id;
}
void RpcProcess::Notify(const std::string& method,Json params) {
    try { Queue({{"jsonrpc","2.0"},{"method",method},{"params",std::move(params)}}); }
    catch(const std::exception& error) {
        // Backpressure or malformed source must not throw through the UI looper.
        Stop();if(fFailure) fFailure(error.what());
    }
}
void RpcProcess::Cancel(int64_t id) {
    if(id<=0) return;
    { std::lock_guard<std::mutex> lock(fMutex);fPending.erase(id); }
    Notify("$/cancelRequest",{{"id",id}});
}
void RpcProcess::Receive(const Json& message) {
    if(message.contains("method")) {
        auto method=message.at("method").get<std::string>();RpcReply reply;
        try { reply=fIncoming?fIncoming(method,message.value("params",Json::object())):RpcReply{{},"Method not supported",-32601}; }
        catch(const std::exception& error) { reply={{},error.what(),-32603}; }
        if(message.contains("id")) {
            Json response={{"jsonrpc","2.0"},{"id",message["id"]}};
            if(reply.ok()) response["result"]=reply.result;
            else response["error"]={{"code",reply.code?reply.code:-32603},{"message",reply.error}};
            Queue(std::move(response));
        }
        return;
    }
    if(!message.contains("id") || !message["id"].is_number_integer()) return;
    auto id=message["id"].get<int64_t>();Callback callback;
    {
        std::lock_guard<std::mutex> lock(fMutex);auto found=fPending.find(id);if(found==fPending.end()) return;
        callback=std::move(found->second.callback);fPending.erase(found);
    }
    RpcReply reply;
    if(message.contains("error")) { reply.error=message["error"].value("message",std::string("Language server request failed"));reply.code=message["error"].value("code",-32603); }
    else if(message.contains("result")) reply.result=message["result"];
    else { reply.error="Invalid JSON-RPC response";reply.code=-32603; }
    if(callback) callback(std::move(reply));
}
void RpcProcess::Run(std::vector<std::string> command,std::string directory) {
    int out[2]{-1,-1},err[2]{-1,-1},in[2]{-1,-1};pid_t pid=-1;bool reaped=false;int status=0;
    std::string failure,stderrText;
    try {
        if(command.empty()) throw std::runtime_error("No language server command configured");
        // Resolve before fork; only async-signal-safe calls run in the child.
        auto executable=command[0];
        if(executable.find('/')==std::string::npos) {
            auto resolved=ToolCommand(command[0],"",directory,"");
            if(!resolved.empty()) executable=resolved[0];
        }
        std::vector<char*> argv;for(auto& arg:command) argv.push_back(arg.data());argv.push_back(nullptr);
        std::vector<std::string> environment;for(char** entry=environ;*entry;++entry) environment.emplace_back(*entry);
        std::vector<char*> envp;for(auto& value:environment) envp.push_back(value.data());envp.push_back(nullptr);
        if(!MakePipe(out) || !MakePipe(err) || !MakePipe(in)) throw std::runtime_error(strerror(errno));
        pid=fork();
        if(pid==0) {
            setpgid(0,0);dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);dup2(err[1],STDERR_FILENO);
            for(int fd:{in[0],in[1],out[0],out[1],err[0],err[1]}) close(fd);
            if(!directory.empty() && chdir(directory.c_str())!=0) _exit(126);
            execve(executable.c_str(),argv.data(),envp.data());_exit(127);
        }
        if(pid<0) throw std::runtime_error(strerror(errno));
        setpgid(pid,pid);close(out[1]);out[1]=-1;close(err[1]);err[1]=-1;close(in[0]);in[0]=-1;
        Nonblock(out[0]);Nonblock(err[0]);Nonblock(in[1]);
        RpcFramer framer;std::string output;size_t written=0;bool stopping=false,exitSent=false;Clock::time_point stopAt;
        while(!reaped) {
            if(fStop && !stopping) {
                stopping=true;stopAt=Clock::now();
                if(written==0 || written==output.size()) { output.clear();written=0; }
                output+=RpcFramer::Frame({{"jsonrpc","2.0"},{"id",-1},{"method","shutdown"},{"params",nullptr}});
                std::lock_guard<std::mutex> lock(fMutex);fOutgoing.clear();fQueuedBytes=0;
            }
            if(stopping && Clock::now()-stopAt>std::chrono::milliseconds(700)) break;
            if(stopping && !exitSent && Clock::now()-stopAt>std::chrono::milliseconds(350)) {
                output+=RpcFramer::Frame({{"jsonrpc","2.0"},{"method","exit"}});exitSent=true;
            }
            if(written==output.size()) {
                output.clear();written=0;
                if(!stopping) { std::lock_guard<std::mutex> lock(fMutex);if(!fOutgoing.empty()) { output=std::move(fOutgoing.front());fOutgoing.pop_front();fQueuedBytes-=output.size(); } }
            }
            pollfd fds[]={{out[0],POLLIN,0},{err[0],POLLIN,0},{in[1],static_cast<short>(written<output.size()?POLLOUT:0),0}};
            poll(fds,3,15);
            if(in[1]>=0 && written<output.size() && fds[2].revents) {
                ssize_t count=write(in[1],output.data()+written,std::min<size_t>(65536,output.size()-written));
                if(count>0) written+=count;
                else if(count<0 && errno!=EINTR && errno!=EAGAIN) throw std::runtime_error("Language server closed its input");
            }
            for(int which=0;which<2;++which) {
                int& fd=which?err[0]:out[0];if(fd<0) continue;
                char buffer[16384];
                for(int batch=0;batch<8;++batch) {
                    ssize_t count=read(fd,buffer,sizeof(buffer));
                    if(count>0) {
                        if(which) { stderrText.append(buffer,count);if(stderrText.size()>16384) stderrText.erase(0,stderrText.size()-16384); }
                        else for(auto& message:framer.Feed(std::string_view(buffer,count))) {
                            if(stopping) {
                                if(!exitSent && message.contains("id") && message["id"]==-1) { output+=RpcFramer::Frame({{"jsonrpc","2.0"},{"method","exit"}});exitSent=true; }
                            } else Receive(message);
                        }
                    } else if(count==0 || (errno!=EINTR && errno!=EAGAIN)) { close(fd);fd=-1;break; }
                    else break;
                }
            }
            std::vector<std::pair<int64_t,Callback>> expired;
            {
                std::lock_guard<std::mutex> lock(fMutex);
                for(auto it=fPending.begin();it!=fPending.end();) {
                    if(Clock::now()>it->second.deadline) { expired.emplace_back(it->first,std::move(it->second.callback));it=fPending.erase(it); }
                    else ++it;
                }
            }
            if(!stopping) for(auto& entry:expired) { Notify("$/cancelRequest",{{"id",entry.first}});if(entry.second) entry.second({{},"Language server request timed out",-32001}); }
            auto waited=waitpid(pid,&status,WNOHANG);reaped=waited==pid || (waited<0 && errno==ECHILD);
            if(out[0]<0 && !stopping && !reaped) throw std::runtime_error("Language server closed its output");
        }
        if(!fStop) failure=WIFEXITED(status) && WEXITSTATUS(status)==127?"Cannot run "+command[0]+". Install it or choose its command in Edit → Language Tools.":"Language server stopped";
    } catch(const std::exception& error) { failure=error.what(); }
    // Kill the process group as well as the server, including tsserver children.
    if(pid>0) { kill(-pid,SIGKILL);if(!reaped) { kill(pid,SIGKILL);ReapKilledChild(pid,status); } }
    for(int fd:{out[0],out[1],err[0],err[1],in[0],in[1]}) if(fd>=0) close(fd);
    fAlive=false;
    if(!stderrText.empty() && !failure.empty()) failure+="\n"+stderrText;
    std::map<int64_t,Pending> pending;
    { std::lock_guard<std::mutex> lock(fMutex);pending.swap(fPending);fOutgoing.clear();fQueuedBytes=0; }
    if(!fStop) {
        if(failure.empty()) failure="Language server stopped";
        for(auto& entry:pending) if(entry.second.callback) entry.second.callback({{},failure,-32000});
        if(fFailure) fFailure(failure);
    }
}
LanguageServer::LanguageServer(std::vector<std::string> command,const std::string& root,State state,Json initializationOptions,Json configuration,Notification notification):fState(std::move(state)),fNotification(std::move(notification)) {
    fProcess=std::make_unique<RpcProcess>(std::move(command),root,[this,root,configuration](const std::string& method,const Json& params)->RpcReply {
        if(method=="textDocument/publishDiagnostics" || method=="workspace/semanticTokens/refresh") { if(fNotification) fNotification(method,params);return {nullptr,{}}; }
        if(method=="workspace/configuration") {
            Json result=Json::array();
            for(const auto& item:params.value("items",Json::array())) {
                auto section=item.value("section",std::string());Json value=configuration;
                for(size_t start=0;start<section.size();) {
                    auto end=section.find('.',start);if(end==std::string::npos) end=section.size();
                    auto key=section.substr(start,end-start);value=value.is_object()?value.value(key,Json::object()):Json::object();start=end+1;
                }
                result.push_back(value);
            }
            return {result,{}};
        }
        if(method=="workspace/workspaceFolders") return {Json::array({{{"uri",FileURI(root)},{"name",root}}}),{}};
        if(method=="window/workDoneProgress/create" || method=="window/showMessageRequest") return {nullptr,{}};
        if(method=="workspace/applyEdit") return {{{"applied",false},{"failureReason","Workspace edits are not enabled"}},{}};
        return {{},"Method not supported",-32601};
    },[this](const std::string& error) { fReady=false;if(fState) fState(error); });
    Json capabilities={
        {"general",{{"positionEncodings",Json::array({"utf-8","utf-16"})}}},
        {"workspace",{{"configuration",true},{"workspaceFolders",true},{"semanticTokens",{{"refreshSupport",true}}},{"applyEdit",false},{"workspaceEdit",{{"documentChanges",true},{"resourceOperations",Json::array()},{"failureHandling","abort"}}}}},
        {"textDocument",{
            {"publishDiagnostics",{{"versionSupport",true},{"relatedInformation",false}}},
            {"semanticTokens",{{"dynamicRegistration",false},{"requests",{{"full",true}}},{"formats",Json::array({"relative"})},
                {"tokenTypes",Json::array({"namespace","type","class","enum","interface","struct","typeParameter","parameter","variable","property","enumMember","event","function","method","keyword","modifier"})},
                {"tokenModifiers",Json::array({"declaration","definition","readonly","static","deprecated","abstract","async","modification","documentation","defaultLibrary"})},
                {"overlappingTokenSupport",false},{"multilineTokenSupport",false},{"serverCancelSupport",false}}},
            {"synchronization",{{"dynamicRegistration",false},{"didSave",true}}},
            {"rename",{{"dynamicRegistration",false},{"prepareSupport",true},{"prepareSupportDefaultBehavior",1}}},
            {"completion",{{"dynamicRegistration",false},{"contextSupport",true},{"completionItem",{
                {"snippetSupport",false},{"insertReplaceSupport",true},{"documentationFormat",Json::array({"plaintext"})},
                {"resolveSupport",{{"properties",Json::array({"detail","documentation","additionalTextEdits"})}}}}}}},
            {"documentSymbol",{{"dynamicRegistration",false},{"hierarchicalDocumentSymbolSupport",true},{"symbolKind",{{"valueSet",Json::array({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26})}}}}}
        }}
    };
    Json params={{"processId",getpid()},{"clientInfo",{{"name","Kiri"},{"version","0.0.2-alpha"}}},{"rootUri",FileURI(root)},
        {"workspaceFolders",Json::array({{{"uri",FileURI(root)},{"name",root}}})},{"capabilities",capabilities},
        {"initializationOptions",std::move(initializationOptions)}};
    fProcess->Request("initialize",params,[this](RpcReply reply) {
        if(fStopping) return;
        if(!reply.ok()) { if(fState) fState(reply.error);return; }
        try {
            auto capabilities=reply.result.at("capabilities");auto encoding=capabilities.value("positionEncoding",std::string("utf-16"));
            if(encoding!="utf-16" && encoding!="utf-8") throw std::runtime_error("Language server selected an unsupported position encoding");
            { std::lock_guard<std::mutex> lock(fMutex);fCapabilities=std::move(capabilities); }
            fProcess->Notify("initialized",Json::object());fReady=true;if(fState) fState("");
        } catch(const std::exception& error) { if(fState) fState(error.what()); }
    },std::chrono::seconds(30));
}
LanguageServer::~LanguageServer() {
    Stop();
    // Initialization callbacks still use fProcess. Keep its pointer and the
    // callback state alive until the RPC thread has finished, then destroy it.
    if(fProcess) fProcess->Wait();
}
void LanguageServer::Stop() { fStopping=true;fReady=false;if(fProcess) fProcess->Stop(); }
Json LanguageServer::Capabilities() const { std::lock_guard<std::mutex> lock(fMutex);return fCapabilities; }
PositionEncoding LanguageServer::Encoding() const { return Capabilities().value("positionEncoding",std::string("utf-16"))=="utf-8"?PositionEncoding::UTF8:PositionEncoding::UTF16; }
bool LanguageServer::SupportsChanges() const {
    auto sync=Capabilities().value("textDocumentSync",Json(0));auto kind=sync.is_object()?sync.value("change",Json(0)):sync;
    return kind==1 || kind==2;
}
void LanguageServer::Open(const std::string& uri,const std::string& language,int version,const std::string& text) {
    fProcess->Notify("textDocument/didOpen",{{"textDocument",{{"uri",uri},{"languageId",language},{"version",version},{"text",text}}}});
}
void LanguageServer::Change(const std::string& uri,int version,const std::string& before,const std::string& after) {
    auto sync=Capabilities().value("textDocumentSync",Json(0));int kind=sync.is_number_integer()?sync.get<int>():sync.is_object()?sync.value("change",0):0;
    if(!kind) return;
    Json change={{"text",after}};
    if(kind==2) change["range"]={{"start",PositionJSON({0,0})},{"end",PositionJSON(PositionAt(before,before.size(),Encoding()))}};
    fProcess->Notify("textDocument/didChange",{{"textDocument",{{"uri",uri},{"version",version}}},{"contentChanges",Json::array({change})}});
}
void LanguageServer::Save(const std::string& uri,const std::string& text) {
    auto sync=Capabilities().value("textDocumentSync",Json(0));if(!sync.is_object()) return;
    auto save=sync.value("save",Json(false));if(save==false || save.is_null()) return;
    Json params={{"textDocument",{{"uri",uri}}}};if(save.is_object() && save.value("includeText",false)) params["text"]=text;
    fProcess->Notify("textDocument/didSave",params);
}
void LanguageServer::Close(const std::string& uri) { fProcess->Notify("textDocument/didClose",{{"textDocument",{{"uri",uri}}}}); }
int64_t LanguageServer::Request(const std::string& method,Json params,Callback callback) { return fProcess->Request(method,std::move(params),std::move(callback)); }
void LanguageServer::Cancel(int64_t id) { fProcess->Cancel(id); }
}
