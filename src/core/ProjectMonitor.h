#pragma once
#include "core/Project.h"
#include <atomic>
#include <climits>
#include <deque>
#include <map>
#include <set>

namespace kiri {
struct ProjectChanges {
    std::vector<std::string> directories,changedDirectories;
    // Device and inode of each entry in directories, from the parent listing.
    std::vector<std::pair<uint64_t,uint64_t>> nodes;
    bool contents=false,limited=false;
};
// One worker owns this incremental directory scan. Native notifications can
// prioritize directories; periodic passes also work on unwatched filesystems.
// A directory seen for the first time is only a change when something in it
// is newer than the last index started (SetIndexTime), or when it could not
// be read before. Discovering an unchanged tree never forces a new index.
class ProjectMonitor {
public:
    explicit ProjectMonitor(std::string root):fRoot(std::move(root)) { fPending.push_back(fRoot);fQueued.insert(fRoot);fKnown.insert(fRoot); }
    ProjectChanges Poll(const std::vector<std::string>& priority={},const std::atomic<bool>* cancel=nullptr,size_t budget=64);
    // Seconds since the epoch at which the latest project index began.
    void SetIndexTime(int64_t seconds) { fIndexedSince=seconds; }
    // Device and inode of a directory the scan has listed, if known.
    bool Node(const std::string& path,std::pair<uint64_t,uint64_t>& node) const;
private:
    struct State { uint64_t names=0,contents=0; };
    std::string fRoot;
    std::deque<std::string> fPending;
    std::set<std::string> fQueued;
    std::set<std::string> fKnown;
    std::set<std::string> fFailed;
    std::map<std::string,State> fStates;
    std::map<std::string,FileStamp> fDirectoryStamps;
    std::atomic<int64_t> fIndexedSince{LLONG_MAX};
};
}
