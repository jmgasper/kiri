#pragma once
#include "core/Process.h"
#include <optional>
#include <string_view>

namespace kiri {
struct GitFile { char index = ' ', worktree = ' '; std::string path, originalPath; };
struct Commit {
    std::string hash;
    std::vector<std::string> parents;
    std::string author, date, subject, refs;
};
struct GraphEdge { int from = 0, to = 0, color = 0; };
struct GraphRow { int lane = 0, color = 0, width = 1; std::vector<GraphEdge> edges; };
class CommitGraph {
public:
    GraphRow Append(const Commit& commit);
    void Clear();
private:
    struct Lane { std::string hash; int color; };
    std::vector<Lane> fLanes;
    int fNextColor = 0;
};
std::vector<GitFile> ParseGitStatus(std::string_view data);
std::vector<Commit> ParseGitLog(std::string_view data);
std::optional<std::string> GitHubRepository(std::string remote);
std::string EncodeURLPath(std::string_view path);
std::optional<size_t> MapLineToBase(std::string_view diff, size_t currentLine);

class GitRepository {
public:
    explicit GitRepository(std::string directory) : fDirectory(std::move(directory)) {}
    ProcessResult Run(std::vector<std::string> arguments, const std::atomic<bool>* cancel = nullptr,
        std::string input = {}, size_t limit = 32 * 1024 * 1024) const;
    std::string Root() const;
    ProcessResult Status(const std::atomic<bool>* cancel = nullptr) const;
    ProcessResult History(size_t skip, size_t count, const std::string& path = {},
        const std::atomic<bool>* cancel = nullptr) const;
    ProcessResult Diff(const std::string& path, bool staged,
        const std::atomic<bool>* cancel = nullptr) const;
    ProcessResult CommitDiff(const std::string& hash, const std::atomic<bool>* cancel = nullptr) const;
    ProcessResult Stage(const std::vector<std::string>& paths) const;
    ProcessResult Unstage(const std::vector<std::string>& paths) const;
    ProcessResult CommitIndex(const std::string& message) const;
    ProcessResult Permalink(const std::string& relativePath, size_t first, size_t last,
        const std::atomic<bool>* cancel = nullptr) const;
private:
    std::string fDirectory;
};
}
