#include "core/LanguageProtocol.h"
#include "core/EditTransaction.h"
#include "core/LanguageServer.h"
#include "core/LanguageTools.h"
#include "core/FileIO.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace kiri;
namespace fs=std::filesystem;
using namespace std::chrono_literals;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> static bool Throws(F action) { try { action();return false; }catch(const std::exception&) { return true; } }
struct Temp {
    std::string path;
    Temp() { char name[]="/tmp/kiri-language-XXXXXX";if(!mkdtemp(name)) throw std::runtime_error("mkdtemp failed");path=CanonicalPath(name); }
    ~Temp() { fs::remove_all(path); }
};
static void Write(const std::string& path,const std::string& text) { std::ofstream stream(path);stream<<text;if(!stream) throw std::runtime_error("Cannot write fixture"); }
static Json Range(size_t line,size_t start,size_t end) { return {{"start",{{"line",line},{"character",start}}},{"end",{{"line",line},{"character",end}}}}; }
static void FramingAndPositions() {
    std::string text="😀x\r\néz\n";
    CHECK(PositionAt(text,5).character==3);CHECK(PositionAt(text,5,PositionEncoding::UTF8).character==5);
    CHECK(PositionAt(text,7).line==1);CHECK(PositionAt(text,9).character==1);
    CHECK(OffsetAt(text,{0,1})==0);CHECK(OffsetAt(text,{0,2})==4);CHECK(OffsetAt(text,{0,99})==5);
    CHECK(OffsetAt(text,{1,1})==9);CHECK(OffsetAt(text,{99,0})==text.size());
    CHECK(PositionAt("a\rb",2).line==1);CHECK(OffsetAt("a\rb",{1,0})==2);
    CHECK(PositionAt(text,4,PositionEncoding::UTF32).character==1);
    CHECK(Throws([]{ReadPosition({{"line",-1},{"character",0}});}));
    CHECK(FileURI("/tmp/日本語 #%.ts")=="file:///tmp/%E6%97%A5%E6%9C%AC%E8%AA%9E%20%23%25.ts");
    Json one={{"jsonrpc","2.0"},{"id",1},{"result","😀"}},two={{"jsonrpc","2.0"},{"id",2},{"result",Json::array({1,2,3})}};
    auto wire=RpcFramer::Frame(one)+RpcFramer::Frame(two);RpcFramer framer;std::vector<Json> messages;
    for(char c:wire) { auto batch=framer.Feed(std::string_view(&c,1));messages.insert(messages.end(),batch.begin(),batch.end()); }
    CHECK(messages.size()==2);CHECK(messages[0]==one);CHECK(messages[1]==two);
    CHECK(RpcFramer().Feed(wire).size()==2);
    CHECK(Throws([]{RpcFramer().Feed("Content-Length: -1\r\n\r\n");}));
    CHECK(Throws([]{RpcFramer().Feed("Content-Length: 99999999999\r\n\r\n");}));
    CHECK(Throws([]{RpcFramer().Feed("Content-Length: 1\r\nContent-Length: 1\r\n\r\n0");}));
    CHECK(Throws([]{RpcFramer().Feed("Wrong: 1\r\n\r\n0");}));
    CHECK(Throws([]{RpcFramer().Feed(std::string(8200,'x'));}));
    CHECK(Throws([]{RpcFramer().Feed("Content-Length: 2\r\n\r\n[]");}));
}
static void EditsAndSymbols() {
    std::string text="const emoji = '😀';\nconst result = obj.na;\n";size_t caret=text.find("na;")+2,start=caret-2;
    Json value={{"label","name"},{"textEdit",{{"range",Range(1,19,21)},{"newText","name"}}},
        {"additionalTextEdits",Json::array({{{"range",Range(0,0,0)},{"newText","import { obj } from './obj';\n"}}})}};
    auto items=ReadCompletions(Json::array({value}));CHECK(items.size()==1);
    auto edits=CompletionEdits(items[0],text,start,caret,PositionEncoding::UTF16);auto changed=ApplyTextEdits(text,edits);
    CHECK(changed=="import { obj } from './obj';\nconst emoji = '😀';\nconst result = obj.name;\n");
    CHECK(MapOffset(caret,edits)==changed.find("name;")+4);
    value["textEdit"]={{"insert",Range(1,19,21)},{"replace",Range(1,19,21)},{"newText","name"}};
    items=ReadCompletions(Json::array({value}));CHECK(ApplyTextEdits(text,CompletionEdits(items[0],text,start,caret,PositionEncoding::UTF16))==changed);
    CHECK(Throws([&]{OrderedEdits({{0,3,"a"},{2,4,"b"}},text.size());}));
    CHECK(Throws([&]{OrderedEdits({{0,text.size()+1,"a"}},text.size());}));
    CHECK(Throws([&]{OrderedEdits({{0,0,"a"},{0,0,"b"}},text.size());}));
    CHECK(ReadCompletions(Json::array({{{"label","snippet"},{"insertTextFormat",2},{"insertText","a${1:b}"}}})).empty());
    auto sorted=ReadCompletions({{"items",Json::array({{{"label","z"},{"sortText","1"}},{{"label","a"},{"sortText","2"}}})}});CHECK(sorted[0].label=="z");
    Json symbol={{"name","Widget"},{"kind",5},{"range",Range(0,0,22)},{"selectionRange",Range(0,6,12)},
        {"children",Json::array({{{"name","count"},{"kind",8},{"range",Range(0,14,21)},{"selectionRange",Range(0,14,19)}}})}};
    auto symbols=ReadSymbols(Json::array({symbol}),"class Widget {count=1;}","file:///test",PositionEncoding::UTF16);
    CHECK(symbols.size()==2);CHECK(symbols[1].container=="Widget");CHECK(symbols[1].depth==1);CHECK(symbols[1].selection==14);
    Json flat={{"name","count"},{"kind",13},{"location",{{"uri","file:///other"},{"range",Range(0,0,1)}}}};
    CHECK(ReadSymbols(Json::array({flat}),"a","file:///test",PositionEncoding::UTF16).empty());
    flat["location"]["uri"]="file:///test";CHECK(ReadSymbols(Json::array({flat}),"a","file:///test",PositionEncoding::UTF16).size()==1);
}
static void Configuration() {
    CHECK(ParseCommand("server --stdio")==std::vector<std::string>({"server","--stdio"}));
    CHECK(ParseCommand("'/tmp/server with spaces' --literal '$HOME' '`id`'")==std::vector<std::string>({"/tmp/server with spaces","--literal","$HOME","`id`"}));
    CHECK(ParseCommand("server \"\" one\\ two")==std::vector<std::string>({"server","","one two"}));
    CHECK(Throws([]{ParseCommand("server 'oops");}));CHECK(Throws([]{ParseCommand("server\\");}));
    Temp temp;LanguageTools settings;CHECK(settings.ForFile("hello.TSX")->language=="typescriptreact");CHECK(settings.ForFile("photo.png")==nullptr);
    settings.prettier="'/tmp/prettier with spaces'";settings.completion=false;settings.profiles[0].command="clangd --background-index";
    CHECK(settings.Save(temp.path).empty());LanguageTools restored;CHECK(restored.Load(temp.path).empty());CHECK(restored.prettier==settings.prettier);CHECK(!restored.completion);CHECK(restored.profiles[0].command==settings.profiles[0].command);
    Write(temp.path+"/language-tools.json","invalid");CHECK(!restored.Load(temp.path).empty());CHECK(restored.prettier==settings.prettier);
    fs::create_directories(temp.path+"/project/node_modules/.bin");fs::create_directories(temp.path+"/project/src");fs::create_directories(temp.path+"/tools/node_modules/.bin");
    for(auto path:{temp.path+"/project/node_modules/.bin/prettier",temp.path+"/tools/node_modules/.bin/prettier"}) { Write(path,"#!/bin/sh\nexit 0\n");chmod(path.c_str(),0700); }
    CHECK(ToolCommand("prettier",temp.path+"/project/src/file.js",temp.path+"/project",temp.path)[0]==temp.path+"/project/node_modules/.bin/prettier");
    CHECK(ToolCommand("prettier",temp.path+"/file.js",temp.path,temp.path)[0]==temp.path+"/tools/node_modules/.bin/prettier");
    CHECK(!FormatWithPrettier("prettier","",temp.path,temp.path,"x").ok());
}
static void SendFixture(const Json& message) {
    auto bytes=RpcFramer::Frame(message);
    for(size_t at=0;at<bytes.size();) { auto count=write(STDOUT_FILENO,bytes.data()+at,std::min<size_t>(13,bytes.size()-at));if(count<=0) _exit(2);at+=count; }
}
static int Fixture(const std::string& mode) {
    if(mode=="malformed") { std::cout<<"Content-Length: -10\r\n\r\n"<<std::flush;return 0; }
    RpcFramer framer;Json document,clientRequest;char buffer[4096];
    while(true) {
        auto count=read(STDIN_FILENO,buffer,sizeof(buffer));if(count<=0) return 0;
        for(const auto& message:framer.Feed(std::string_view(buffer,count))) {
            if(!message.contains("method")) { if(message.value("id",Json())=="server-1") SendFixture({{"jsonrpc","2.0"},{"id",clientRequest},{"result",message.value("result",Json())}});continue; }
            auto method=message["method"].get<std::string>();auto params=message.value("params",Json::object());Json result=nullptr;
            if(method=="exit") return 0;
            if(method=="stall") continue;
            if(method=="crash") return 3;
            if(method=="initialize") result={{"capabilities",{{"positionEncoding","utf-16"},{"textDocumentSync",{{"openClose",true},{"change",2},{"save",{{"includeText",true}}}}},{"documentSymbolProvider",true},{"completionProvider",{{"triggerCharacters",Json::array({"."})}}}}}};
            else if(method=="textDocument/didOpen") document=params["textDocument"];
            else if(method=="textDocument/didChange") {
                auto change=params.at("contentChanges").at(0);auto before=document["text"].get<std::string>();
                auto range=change.value("range",Json{{"start",PositionJSON({0,0})},{"end",PositionJSON(PositionAt(before,before.size()))}});
                auto start=OffsetAt(before,ReadPosition(range["start"])),end=OffsetAt(before,ReadPosition(range["end"]));
                document["text"]=ApplyTextEdits(before,{{start,end,change["text"]}});document["version"]=params["textDocument"]["version"];
            } else if(method=="textDocument/didSave") document["saved"]=params.value("text",std::string());
            else if(method=="textDocument/didClose") document=nullptr;
            else if(method=="document") result=document;
            else if(method=="echo") result=params;
            else if(method=="askClient") {
                clientRequest=message["id"];SendFixture({{"jsonrpc","2.0"},{"id","server-1"},{"method","fixture/client"},{"params",{{"value",7}}}});continue;
            } else if(method=="error") { SendFixture({{"jsonrpc","2.0"},{"id",message["id"]},{"error",{{"code",-32602},{"message","fixture error"}}}});continue; }
            if(method=="initialize" && mode=="full") result["capabilities"]["textDocumentSync"]=1;
            if(message.contains("id")) SendFixture({{"jsonrpc","2.0"},{"id",message["id"]},{"result",result}});
        }
    }
}
template<class Client> static RpcReply Request(Client& client,const std::string& method,Json params=Json::object()) {
    auto promise=std::make_shared<std::promise<RpcReply>>();auto future=promise->get_future();
    client.Request(method,std::move(params),[promise](RpcReply reply){promise->set_value(std::move(reply));});
    if(future.wait_for(35s)!=std::future_status::ready) throw std::runtime_error("No reply to "+method);
    return future.get();
}
static std::unique_ptr<LanguageServer> Start(const std::vector<std::string>& command,const std::string& root,Json initialization=Json::object()) {
    auto promise=std::make_shared<std::promise<std::string>>();auto future=promise->get_future();auto notified=std::make_shared<std::atomic<bool>>(false);
    auto server=std::make_unique<LanguageServer>(command,root,[promise,notified](const std::string& error) { if(!notified->exchange(true)) promise->set_value(error); },initialization);
    if(future.wait_for(40s)!=std::future_status::ready) throw std::runtime_error("Server initialization did not finish");
    auto error=future.get();if(!error.empty()) throw std::runtime_error(error);CHECK(server->Ready());return server;
}
static void Transport(const std::string& executable) {
    Temp temp;
    auto before=std::chrono::steady_clock::now();
    {
        RpcProcess client({executable,"--rpc-fixture","normal"},temp.path,[](const std::string&,const Json& params){return RpcReply{params,{}};},[](const std::string&){});
        auto reply=Request(client,"echo",{{"text","日本語 😀"}});CHECK(reply.ok());CHECK(reply.result["text"]=="日本語 😀");
        CHECK(Request(client,"askClient").result["value"]==7);
        reply=Request(client,"error");CHECK(!reply.ok());CHECK(reply.code==-32602);CHECK(reply.error=="fixture error");
        auto promise=std::make_shared<std::promise<RpcReply>>();auto future=promise->get_future();
        client.Request("stall",{},[promise](RpcReply reply){promise->set_value(std::move(reply));},70ms);
        CHECK(future.wait_for(2s)==std::future_status::ready);CHECK(future.get().code==-32001);
        CHECK(Request(client,"echo",123).result==123);
        std::atomic<bool> cancelledCalled{false};auto id=client.Request("stall",{},[&](RpcReply){cancelledCalled=true;});client.Cancel(id);
        CHECK(Request(client,"echo",456).result==456);CHECK(!cancelledCalled);
    }
    CHECK(std::chrono::steady_clock::now()-before<3s);
    {
        auto server=Start({executable,"--rpc-fixture","normal"},temp.path);
        auto uri=FileURI(temp.path+"/unicode.ts");std::string first="const face = '😀';\r\n",second="const face = '日本語';\n";
        server->Open(uri,"typescript",1,first);CHECK(Request(*server,"document").result["text"]==first);
        server->Change(uri,2,first,second);auto reply=Request(*server,"document");CHECK(reply.result["text"]==second);CHECK(reply.result["version"]==2);
        server->Save(uri,second);CHECK(Request(*server,"document").result["saved"]==second);
        server->Close(uri);CHECK(Request(*server,"document").result.is_null());
    }
    {
        RpcProcess client({executable,"--rpc-fixture","malformed"},temp.path,{},[](const std::string&){});CHECK(!Request(client,"echo").ok());
    }
    {
        auto server=Start({executable,"--rpc-fixture","full"},temp.path);auto uri=FileURI(temp.path+"/full.ts");
        server->Open(uri,"typescript",1,"before");server->Change(uri,2,"before","after 😀");CHECK(Request(*server,"document").result["text"]=="after 😀");
    }
    {
        RpcProcess client({temp.path+"/missing-server"},temp.path,{},[](const std::string&){});CHECK(!Request(client,"echo").ok());
    }
    {
        bool failed=false;RpcProcess client({executable,"--rpc-fixture","normal"},temp.path,{},[&](const std::string&){failed=true;});
        CHECK(!Throws([&]{client.Notify("invalid",std::string("\xff",1));}));CHECK(failed);
    }
}
static void RealTools(const std::string& settings) {
    Temp temp;std::string project=temp.path+"/project with spaces";fs::create_directory(project);
    Write(project+"/.prettierrc","{\"singleQuote\":true,\"semi\":false}\n");Write(project+"/.prettierignore","ignored.js\n");
    std::string file=project+"/format file.js",input="const user={name:\"Kiri\",items:[1,2,3]};\n";Write(file,"// disk stays unchanged\n");
    auto formatted=FormatWithPrettier("prettier",file,project,settings,input);
    if(!formatted.ok()) throw std::runtime_error(formatted.diagnostic());
    CHECK(formatted.output=="const user = { name: 'Kiri', items: [1, 2, 3] }\n");CHECK(ReadFile(file).bytes=="// disk stays unchanged\n");
    auto ignored=FormatWithPrettier("prettier",project+"/ignored.js",project,settings,input);CHECK(ignored.ok());CHECK(ignored.output==input);
    CHECK(!FormatWithPrettier("prettier",file,project,settings,"const = {\n").ok());
    std::atomic<bool> cancel{true};CHECK(FormatWithPrettier("prettier",file,project,settings,input,&cancel).cancelled);
    file=project+"/日本語 demo.ts";
    std::string text="const greeting = '😀';\nclass Greeter {\n  name = 'Kiri';\n  greet() { return greeting + this.name; }\n}\nfunction welcome() { return new Greeter(); }\nconst user = new Greeter();\nconst example = '😀'; user.na\n";
    Write(file,text);Write(project+"/tsconfig.json","{\"compilerOptions\":{\"target\":\"ES2022\"},\"include\":[\"*.ts\"]}\n");
    auto command=ToolCommand("typescript-language-server --stdio",file,project,settings);
    auto server=Start(command,project,Json::parse(LanguageTools().ForFile(file)->initializationOptions));auto uri=FileURI(file);server->Open(uri,"typescript",1,text);
    auto reply=Request(*server,"textDocument/documentSymbol",{{"textDocument",{{"uri",uri}}}});
    if(!reply.ok()) throw std::runtime_error(reply.error);
    auto symbols=ReadSymbols(reply.result,text,uri,server->Encoding());
    for(auto name:{"greeting","Greeter","name","greet","welcome","user"}) CHECK(std::any_of(symbols.begin(),symbols.end(),[&](const auto& symbol){return symbol.name==name;}));
    auto caret=text.find("user.na")+7;
    reply=Request(*server,"textDocument/completion",{{"textDocument",{{"uri",uri}}},{"position",PositionJSON(PositionAt(text,caret,server->Encoding()))},{"context",{{"triggerKind",1}}}});
    if(!reply.ok()) throw std::runtime_error(reply.error);
    auto completions=ReadCompletions(reply.result);auto found=std::find_if(completions.begin(),completions.end(),[](const auto& item){return item.label=="name";});CHECK(found!=completions.end());
    auto item=*found;
    reply=Request(*server,"completionItem/resolve",item.value);CHECK(reply.ok());item.value.update(reply.result);
    auto changed=ApplyTextEdits(text,CompletionEdits(item,text,caret-2,caret,server->Encoding()));CHECK(changed.find("user.name")!=std::string::npos);
    changed+="const afterChange = 42;\n";server->Change(uri,2,text,changed);
    reply=Request(*server,"textDocument/documentSymbol",{{"textDocument",{{"uri",uri}}}});
    CHECK(reply.ok());symbols=ReadSymbols(reply.result,changed,uri,server->Encoding());CHECK(std::any_of(symbols.begin(),symbols.end(),[](const auto& symbol){return symbol.name=="afterChange";}));
    server->Close(uri);std::cout<<"Real Prettier and TypeScript LSP integration passed\n";
    {
        auto renameProject=temp.path+"/rename";fs::create_directory(renameProject);
        auto declaration=renameProject+"/shared.ts",usage=renameProject+"/usage.ts";
        std::string saved="export function greet(name: string) { return name; }\n";
        std::string dirty=saved+"// unsaved 😀\n";
        std::string uses="import { greet } from './shared';\nconst untouched = 'greet';\ngreet('Kiri');\nfunction isolated(greet: string) { return greet; }\n";
        Write(declaration,saved);Write(usage,uses);Write(renameProject+"/tsconfig.json","{\"include\":[\"*.ts\"]}");
        auto renameServer=Start(ToolCommand("typescript-language-server --stdio",declaration,renameProject,settings),renameProject,Json::parse(LanguageTools().ForFile(declaration)->initializationOptions));
        auto declarationURI=FileURI(declaration);renameServer->Open(declarationURI,"typescript",1,dirty);
        auto prepare=Request(*renameServer,"textDocument/prepareRename",{{"textDocument",{{"uri",declarationURI}}},{"position",PositionJSON(PositionAt(dirty,17,renameServer->Encoding()))}});
        CHECK(prepare.ok());CHECK(!prepare.result.is_null());
        auto renamed=Request(*renameServer,"textDocument/rename",{{"textDocument",{{"uri",declarationURI}}},{"position",PositionJSON(PositionAt(dirty,17,renameServer->Encoding()))},{"newName","welcome"}});
        CHECK(renamed.ok());
        auto plan=WorkspaceEditPlan(renamed.result,renameServer->Encoding(),[&](const std::string& path) {
            auto snap=DiskSnapshot(path);if(path==declaration) { snap.open=true;snap.version=1;snap.text=dirty; }return snap;
        });
        if(plan.files.size()!=2) std::cerr<<"Rename reply: "<<renamed.result.dump(2)<<"\n";
        CHECK(plan.files.size()==2);
        for(auto& file:plan.files) {
            if(file.before.path==declaration) CHECK(file.after=="export function welcome(name: string) { return name; }\n// unsaved 😀\n");
            else {
                CHECK(file.before.path==usage);CHECK(file.after=="import { welcome } from './shared';\nconst untouched = 'greet';\nwelcome('Kiri');\nfunction isolated(greet: string) { return greet; }\n");
                CHECK(ApplyDiskEdit(file).empty());CHECK(ReadFile(usage).bytes==file.after);CHECK(ApplyDiskEdit(file,true).empty());CHECK(ReadFile(usage).bytes==uses);
            }
        }
        CHECK(ReadFile(declaration).bytes==saved);renameServer->Close(declarationURI);
        std::cout<<"Real TypeScript prepareRename, cross-file rename, unrelated text and restore passed\n";
    }

    auto clang=ToolCommand("clangd",project+"/test.cpp",project,settings);
    if(!clang.empty() && access(clang[0].c_str(),X_OK)==0) {
        file=project+"/test.cpp";text="struct Widget { int count; void reset() { count = 0; } };\nint total = 0;\nint main() { /* 😀 */ Widget item; item.co; }\n";
        Write(file,text);auto cpp=Start(clang,project);uri=FileURI(file);cpp->Open(uri,"cpp",1,text);
        reply=Request(*cpp,"textDocument/documentSymbol",{{"textDocument",{{"uri",uri}}}});CHECK(reply.ok());
        symbols=ReadSymbols(reply.result,text,uri,cpp->Encoding());
        for(auto name:{"Widget","count","reset","total","main"}) CHECK(std::any_of(symbols.begin(),symbols.end(),[&](const auto& symbol){return symbol.name==name;}));
        caret=text.find("item.co")+7;
        reply=Request(*cpp,"textDocument/completion",{{"textDocument",{{"uri",uri}}},{"position",PositionJSON(PositionAt(text,caret,cpp->Encoding()))},{"context",{{"triggerKind",1}}}});CHECK(reply.ok());
        completions=ReadCompletions(reply.result);found=std::find_if(completions.begin(),completions.end(),[](const auto& item){return item.filter=="count" || item.label=="count";});CHECK(found!=completions.end());
        changed=ApplyTextEdits(text,CompletionEdits(*found,text,caret-2,caret,cpp->Encoding()));CHECK(changed.find("item.count;")!=std::string::npos);
        cpp->Close(uri);std::cout<<"Real clangd C++ completion and symbols passed\n";
    }
}
int main(int argc,char** argv) {
    signal(SIGPIPE,SIG_IGN);
    if(argc>=3 && std::string(argv[1])=="--rpc-fixture") return Fixture(argv[2]);
    try {
        FramingAndPositions();EditsAndSymbols();Configuration();Transport(CanonicalPath(argv[0]));
        if(argc==3 && std::string(argv[1])=="--tools") RealTools(CanonicalPath(argv[2]));
        std::cout<<checks<<" language checks passed\n";return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n";return 1; }
}
