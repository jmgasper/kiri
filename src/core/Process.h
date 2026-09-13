#pragma once
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace kiri {
struct ProcessResult {
    int exitCode = -1;
    std::string output;
    std::string error;
    bool cancelled = false;
    bool timedOut = false;
    bool truncated = false;
    bool ok() const { return exitCode == 0 && !cancelled && !timedOut && !truncated; }
    std::string diagnostic() const;
};
struct ProcessOptions {
    std::string directory;
    std::vector<std::string> environment;
    std::string input;
    std::chrono::milliseconds timeout{30000};
    size_t outputLimit = 32 * 1024 * 1024;
    const std::atomic<bool>* cancel = nullptr;
};
// Arguments are passed directly to execve; no shell expansion is performed.
ProcessResult RunProcess(const std::vector<std::string>& arguments,
    const ProcessOptions& options = {});
}
