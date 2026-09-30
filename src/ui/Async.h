#pragma once
#include "ui/Messages.h"
#include <Messenger.h>
#include <Alert.h>
#include <OS.h>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <string>
namespace kiri {
// Owns both pending jobs and delivered results. Closing a window cancels its
// jobs and releases queued results even when their notification messages are
// dropped. Workers share the queue state, so a worker blocked in the kernel
// (for example, on an unresponsive network volume) cannot hold a closing
// window open: shutdown waits a bounded time, then leaves that worker to exit
// on its own. Work functions therefore use only captured values; results that
// touch their owner run on the owner's thread through Drain().
class AsyncQueue {
public:
    using Callback=std::function<void()>;
    using Work=std::function<Callback(const std::atomic<bool>&)>;
    using Failure=std::function<void(const std::string&)>;
    static constexpr bigtime_t kShutdownWait=2000000;
    explicit AsyncQueue(BMessenger target,int count=1):fState(std::make_shared<State>()) {
        fState->target=target;
        for(int i=0;i<count;++i) {
            { std::lock_guard<std::mutex> lock(fState->mutex);++fState->active; }
            std::thread(Worker,fState).detach();
        }
    }
    ~AsyncQueue() { if(!fWaited) StopAndWait(); }
    AsyncQueue(const AsyncQueue&)=delete;
    AsyncQueue& operator=(const AsyncQueue&)=delete;
    // Cancels every job and wakes the workers without waiting for them. Lets
    // several queues shut down together.
    void Stop() {
        { std::lock_guard<std::mutex> lock(fState->mutex);
          fState->stop=true;
          for(auto& job:fState->work) job->cancelled=true;
          for(auto& job:fState->running) job->cancelled=true; }
        fState->wake.notify_all();
    }
    // Stops and waits for the workers to return. Returns false if a worker is
    // still blocked after the timeout; it then exits by itself.
    bool StopAndWait(bigtime_t timeout=kShutdownWait) {
        Stop();fWaited=true;
        std::unique_lock<std::mutex> lock(fState->mutex);
        bool finished=fState->finished.wait_for(lock,std::chrono::microseconds(timeout),[&]{return fState->active==0;});
        // Release results and pending work on the owner's thread when possible.
        std::deque<Callback> done;std::deque<std::shared_ptr<Job>> work;
        done.swap(fState->done);work.swap(fState->work);
        lock.unlock();
        return finished;
    }
    void Submit(Work work,std::string key={},Failure failure={}) {
        auto job=std::make_shared<Job>();job->work=std::move(work);job->key=std::move(key);job->failure=std::move(failure);
        { std::lock_guard<std::mutex> lock(fState->mutex);
          if(fState->stop) return;
          if(!job->key.empty()) CancelLocked(job->key);
          fState->work.push_back(std::move(job)); }
        fState->wake.notify_one();
    }
    void Cancel(const std::string& key) { std::lock_guard<std::mutex> lock(fState->mutex);CancelLocked(key); }
    // Cancels queued and running jobs, for example when the project changes.
    void CancelAll() {
        std::lock_guard<std::mutex> lock(fState->mutex);
        for(auto& job:fState->work) job->cancelled=true;
        for(auto& job:fState->running) job->cancelled=true;
        fState->work.clear();
    }
    // Deliver results from persistent services without tying up a worker.
    // The workspace's language tick also drains results if its port was full.
    void Post(Callback callback) { PostTo(fState,std::move(callback)); }
    // A poster that stays safe to call after this queue is destroyed; results
    // posted after shutdown are discarded without running.
    std::function<void(Callback)> Poster() const {
        std::weak_ptr<State> state=fState;
        return [state](Callback callback) { if(auto shared=state.lock()) PostTo(shared,std::move(callback)); };
    }
    void Drain() {
        std::deque<Callback> done;
        { std::lock_guard<std::mutex> lock(fState->mutex);done.swap(fState->done); }
        for(auto& callback:done) callback();
    }
    size_t Pending() const { std::lock_guard<std::mutex> lock(fState->mutex);return fState->work.size()+fState->running.size(); }
private:
    struct Job { Work work;Failure failure;std::string key;std::atomic<bool> cancelled{false}; };
    struct State {
        BMessenger target;
        std::atomic<bool> stop{false};
        mutable std::mutex mutex;
        std::condition_variable wake,finished;
        std::deque<std::shared_ptr<Job>> work;
        std::vector<std::shared_ptr<Job>> running;
        std::deque<Callback> done;
        int active=0;
    };
    static void PostTo(const std::shared_ptr<State>& state,Callback callback) {
        { std::lock_guard<std::mutex> lock(state->mutex);if(state->stop) return;state->done.push_back(std::move(callback)); }
        BMessage message(kWorkDone);state->target.SendMessage(&message,static_cast<BHandler*>(nullptr),1000);
    }
    static void Worker(std::shared_ptr<State> state) {
        auto& s=*state;
        while(true) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock<std::mutex> lock(s.mutex);
                s.wake.wait(lock,[&]{return s.stop || !s.work.empty();});
                if(s.stop) break;
                job=std::move(s.work.front());s.work.pop_front();s.running.push_back(job);
            }
            Callback callback;
            try { if(!job->cancelled) callback=job->work(job->cancelled); }
            catch(const std::exception& error) {
                std::string detail=error.what();auto failure=job->failure;
                callback=[failure,detail] {
                    if(failure) failure(detail);
                    else (new BAlert("Background Operation",detail.c_str(),"OK"))->Go();
                };
            }
            bool notify=false,stopped=false;
            {
                std::lock_guard<std::mutex> lock(s.mutex);
                s.running.erase(std::remove(s.running.begin(),s.running.end(),job),s.running.end());
                stopped=s.stop;
                if(!stopped && callback && !job->cancelled) {
                    notify=s.done.empty();
                    s.done.push_back([job,callback=std::move(callback)] { if(!job->cancelled) callback(); });
                }
            }
            // Results that capture the owner are never run after shutdown.
            // Release them here without touching the owner.
            if(stopped) { callback=nullptr;job.reset();break; }
            job.reset();
            // Never hold a worker indefinitely on a window's full message port.
            // Retrying a bounded send also lets shutdown cancel the wait.
            if(notify) {
                BMessage message(kWorkDone);
                while(!s.stop) {
                    auto status=s.target.SendMessage(&message,static_cast<BHandler*>(nullptr),100000);
                    if(status!=B_TIMED_OUT && status!=B_WOULD_BLOCK) break;
                }
            }
        }
        { std::lock_guard<std::mutex> lock(s.mutex);--s.active; }
        s.finished.notify_all();
    }
    void CancelLocked(const std::string& key) {
        for(auto& job:fState->work) if(job->key==key) job->cancelled=true;
        for(auto& job:fState->running) if(job->key==key) job->cancelled=true;
        fState->work.erase(std::remove_if(fState->work.begin(),fState->work.end(),[](const auto& job){return job->cancelled.load();}),fState->work.end());
    }
    std::shared_ptr<State> fState;
    bool fWaited=false;
};
}
