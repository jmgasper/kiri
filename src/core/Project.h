#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace kiri {
struct DirectoryEntry { std::string name, path; bool directory=false, symlink=false; };
struct DirectoryResult { std::vector<DirectoryEntry> entries; std::string error; };
DirectoryResult ListDirectory(const std::string& path, const std::atomic<bool>* cancel=nullptr);
struct ProjectIndex { std::vector<std::string> paths; bool truncated=false; std::string error; };
ProjectIndex IndexProject(const std::string& root, const std::atomic<bool>* cancel=nullptr, size_t limit=500000);
int FuzzyScore(const std::string& query, const std::string& path);
std::vector<std::string> QuickOpen(const ProjectIndex& index, const std::string& query, size_t limit=100);
struct SearchMatch { std::string path, text; size_t line=0, column=0; };
struct SearchResult { std::vector<SearchMatch> matches; bool truncated=false; size_t skipped=0; std::string error; };
SearchResult SearchProject(const std::string& root, const ProjectIndex& index, const std::string& query,
    bool matchCase, const std::atomic<bool>* cancel=nullptr, size_t limit=2000);
}
