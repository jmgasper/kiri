#include "core/ProjectMonitor.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace kiri {
namespace {
void Mix(uint64_t& hash,std::string_view text) { for(unsigned char c:text) { hash^=c;hash*=1099511628211ULL; }hash^=255;hash*=1099511628211ULL; }
}
ProjectChanges ProjectMonitor::Poll(const std::vector<std::string>& priority,const std::atomic<bool>* cancel,size_t budget) {
    ProjectChanges result;auto started=std::chrono::steady_clock::now();std::set<std::string> visited;
    for(auto it=priority.rbegin();it!=priority.rend();++it) if(*it==fRoot || fStates.count(*it)) {
        fPending.erase(std::remove(fPending.begin(),fPending.end(),*it),fPending.end());fPending.push_front(*it);fQueued.insert(*it);
    }
    while(!fPending.empty() && visited.size()<budget && !(cancel && *cancel)) {
        auto path=fPending.front();fPending.pop_front();fQueued.erase(path);
        if(!visited.insert(path).second) { fPending.push_back(path);fQueued.insert(path);break; }
        auto listing=ListDirectory(path,cancel);if(cancel && *cancel) break;
        State state;state.names=state.contents=14695981039346656037ULL;
        auto previous=fStates.find(path);
        if(!listing.error.empty()) {
            if(previous!=fStates.end()) { result.changedDirectories.push_back(path);fStates.erase(previous);result.contents=true; }
            if(path!=fRoot) fKnown.erase(path);
            if(path==fRoot) { fPending.push_back(path);fQueued.insert(path); }
            continue;
        }
        for(auto& entry:listing.entries) {
            if(entry.name==".git" || entry.name==".hg" || entry.name==".svn") continue;
            Mix(state.names,entry.name);Mix(state.names,entry.directory?"directory":"file");Mix(state.names,entry.symlink?"link":"node");
            if(entry.directory && !entry.symlink) {
                if(fKnown.count(entry.path) || fKnown.size()<50000) { if(!fQueued.count(entry.path) && !visited.count(entry.path)) { fPending.push_back(entry.path);fQueued.insert(entry.path);fKnown.insert(entry.path); } }
                else result.limited=true;
            } else if(!entry.symlink) {
                auto stamp=StatFile(entry.path);Mix(state.contents,entry.name);
                for(auto value:{stamp.inode,stamp.size,uint64_t(stamp.seconds),uint64_t(stamp.nanoseconds)}) Mix(state.contents,std::to_string(value));
                if(entry.name==".gitignore") Mix(state.names,std::to_string(state.contents));
            }
        }
        if(previous!=fStates.end()) {
            if(previous->second.names!=state.names) result.changedDirectories.push_back(path);
            if(previous->second.contents!=state.contents || previous->second.names!=state.names) result.contents=true;
        } else if(fInitialized) { result.changedDirectories.push_back(path);result.contents=true; }
        fStates[path]=state;fPending.push_back(path);fQueued.insert(path);
        if(std::chrono::steady_clock::now()-started>std::chrono::milliseconds(100)) break;
    }
    for(auto& entry:fStates) result.directories.push_back(entry.first);
    fInitialized=true;
    return result;
}
}
