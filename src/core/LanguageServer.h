#pragma once
#include "core/LanguageProtocol.h"
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace kiri {
struct RpcReply { Json result;std::string error;int code=0;bool ok() const { return error.empty(); } };
class RpcProcess {
public:
    using Callback=std::function<void(RpcReply)>;
    using Incoming=std::function<RpcReply(const std::string&,const Json&)>;
    using Failure=std::function<void(const std::string&)>;
    RpcProcess(std::vector<std::string> command,std::string directory,Incoming incoming,Failure failure);
    ~RpcProcess();
    int64_t Request(const std::string& method,Json params,Callback callback,std::chrono::milliseconds timeout=std::chrono::seconds(15));
    void Notify(const std::string& method,Json params);
    void Cancel(int64_t id);
    void Stop();
    void Wait();
private:
    struct Pending { Callback callback;std::chrono::steady_clock::time_point deadline; };
    void Run(std::vector<std::string> command,std::string directory);
    void Queue(Json message);
    void Receive(const Json& message);
    std::atomic<bool> fStop{false},fAlive{true};
    std::mutex fMutex;
    int64_t fNextID=1;
    std::map<int64_t,Pending> fPending;
    std::deque<std::string> fOutgoing;
    size_t fQueuedBytes=0;
    Incoming fIncoming;
    Failure fFailure;
    std::thread fThread;
};
class LanguageServer {
public:
    using Callback=RpcProcess::Callback;
    using State=std::function<void(const std::string&)>;
    using Notification=std::function<void(const std::string&,const Json&)>;
    LanguageServer(std::vector<std::string> command,const std::string& root,State state,
        Json initializationOptions=Json::object(),Json configuration=Json::object(),Notification notification={});
    ~LanguageServer();
    bool Ready() const { return !fStopping.load() && fReady.load(); }
    Json Capabilities() const;
    PositionEncoding Encoding() const;
    bool SupportsChanges() const;
    void Open(const std::string& uri,const std::string& language,int version,const std::string& text);
    void Change(const std::string& uri,int version,const std::string& before,const std::string& after);
    void Save(const std::string& uri,const std::string& text);
    void Close(const std::string& uri);
    int64_t Request(const std::string& method,Json params,Callback callback);
    void Cancel(int64_t id);
    void Stop();
private:
    std::atomic<bool> fReady{false},fStopping{false};
    mutable std::mutex fMutex;
    Json fCapabilities;
    State fState;
    Notification fNotification;
    std::unique_ptr<RpcProcess> fProcess;
};
}
