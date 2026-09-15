#pragma once
#include "core/Project.h"
#include <deque>
#include <map>
#include <set>

namespace kiri {
struct ProjectChanges {
    std::vector<std::string> directories,changedDirectories;
    bool contents=false,limited=false;
};
// One worker owns this incremental directory scan. Native notifications can
// prioritize directories; periodic passes also work on unwatched filesystems.
class ProjectMonitor {
public:
    explicit ProjectMonitor(std::string root):fRoot(std::move(root)) { fPending.push_back(fRoot);fQueued.insert(fRoot);fKnown.insert(fRoot); }
    ProjectChanges Poll(const std::vector<std::string>& priority={},const std::atomic<bool>* cancel=nullptr,size_t budget=64);
private:
    struct State { uint64_t names=0,contents=0; };
    std::string fRoot;
    std::deque<std::string> fPending;
    std::set<std::string> fQueued;
    std::set<std::string> fKnown;
    bool fInitialized=false;
    std::map<std::string,State> fStates;
};
}
