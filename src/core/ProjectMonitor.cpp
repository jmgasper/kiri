#include "core/ProjectMonitor.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace kiri {
namespace {
void Mix(uint64_t& hash,std::string_view text) { for(unsigned char c:text) { hash^=c;hash*=1099511628211ULL; }hash^=255;hash*=1099511628211ULL; }
}
bool ProjectMonitor::Node(const std::string& path,std::pair<uint64_t,uint64_t>& node) const {
    auto found=fDirectoryStamps.find(path);if(found==fDirectoryStamps.end()) return false;
    node={found->second.device,found->second.inode};return true;
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
            fFailed.insert(path);
            if(path!=fRoot) { fKnown.erase(path);fDirectoryStamps.erase(path); }
            if(path==fRoot) { fPending.push_back(path);fQueued.insert(path); }
            continue;
        }
        if(path==fRoot && !fDirectoryStamps.count(fRoot)) { auto stamp=StatFile(fRoot);if(stamp.exists) fDirectoryStamps[fRoot]=stamp; }
        // The newest modification in this directory itself: its own entry
        // (names added or removed) and its files. Subdirectories judge their own.
        int64_t newest=LLONG_MIN;
        if(auto own=fDirectoryStamps.find(path);own!=fDirectoryStamps.end()) newest=own->second.seconds;
        for(auto& entry:listing.entries) {
            if(entry.name==".git" || entry.name==".hg" || entry.name==".svn") continue;
            Mix(state.names,entry.name);Mix(state.names,entry.directory?"directory":"file");Mix(state.names,entry.symlink?"link":"node");
            if(entry.directory && !entry.symlink) {
                if(entry.stamp.exists) fDirectoryStamps[entry.path]=entry.stamp;
                if(fKnown.count(entry.path) || fKnown.size()<50000) { if(!fQueued.count(entry.path) && !visited.count(entry.path)) { fPending.push_back(entry.path);fQueued.insert(entry.path);fKnown.insert(entry.path); } }
                else result.limited=true;
            } else if(!entry.symlink) {
                // The listing's own metadata read; no second stat per file.
                const auto& stamp=entry.stamp;Mix(state.contents,entry.name);
                for(auto value:{stamp.inode,stamp.size,uint64_t(stamp.seconds),uint64_t(stamp.nanoseconds)}) Mix(state.contents,std::to_string(value));
                if(entry.name==".gitignore") Mix(state.names,std::to_string(state.contents));
                if(stamp.exists) newest=std::max(newest,stamp.seconds);
            }
        }
        if(previous!=fStates.end()) {
            if(previous->second.names!=state.names) result.changedDirectories.push_back(path);
            if(previous->second.contents!=state.contents || previous->second.names!=state.names) result.contents=true;
        } else if(fFailed.count(path) || newest>=fIndexedSince.load()) { result.changedDirectories.push_back(path);result.contents=true; }
        fFailed.erase(path);
        fStates[path]=state;fPending.push_back(path);fQueued.insert(path);
        if(std::chrono::steady_clock::now()-started>std::chrono::milliseconds(100)) break;
    }
    for(auto& entry:fStates) {
        result.directories.push_back(entry.first);
        std::pair<uint64_t,uint64_t> node{0,0};Node(entry.first,node);result.nodes.push_back(node);
    }
    return result;
}
}
