#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include "core/Search.h"
#include "core/FileIO.h"

namespace kiri {
struct DirectoryEntry { std::string name, path; bool directory=false, symlink=false; };
struct DirectoryResult { std::vector<DirectoryEntry> entries; std::string error; };
DirectoryResult ListDirectory(const std::string& path, const std::atomic<bool>* cancel=nullptr);
struct ProjectIndex { std::vector<std::string> paths; bool truncated=false; std::string error; };
ProjectIndex IndexProject(const std::string& root, const std::atomic<bool>* cancel=nullptr, size_t limit=500000,bool includeIgnored=false);
int FuzzyScore(const std::string& query, const std::string& path);
std::vector<std::string> QuickOpen(const ProjectIndex& index, const std::string& query, size_t limit=100);
struct ProjectSearchOptions : SearchOptions {
    std::string folders,include,exclude; // Semicolon-separated project-relative paths/globs.
    bool includeIgnored=false;
};
bool MatchesProjectPath(const std::string& path,const ProjectSearchOptions& options);
struct SearchMatch { std::string path, text; size_t line=0, column=0,start=0,end=0;std::string replacement; };
struct SearchResult {
    std::vector<SearchMatch> matches;
    bool truncated=false,cancelled=false;
    size_t skipped=0,binary=0,large=0,unreadable=0,symlinks=0,filtered=0;
    std::string error;
    std::map<std::string,FileData> snapshots;
    std::string Summary() const;
};
SearchResult SearchProject(const std::string& root, const ProjectIndex& index, const std::string& query,
    bool matchCase, const std::atomic<bool>* cancel=nullptr, size_t limit=2000);
SearchResult SearchProject(const std::string& root,const ProjectIndex& index,const ProjectSearchOptions& options,
    const std::atomic<bool>* cancel=nullptr,size_t limit=2000,
    const std::map<std::string,FileData>* buffers=nullptr,const std::string* replacement=nullptr);
}
