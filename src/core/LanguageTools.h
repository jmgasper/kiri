#pragma once
#include "core/Process.h"
#include <string>
#include <vector>

namespace kiri {
struct LanguageProfile {
    std::string name, language, command;
    std::vector<std::string> extensions;
    std::string initializationOptions="{}",configuration="{}";
};
struct LanguageTools {
    std::string prettier="prettier";
    bool completion=true;
    std::vector<LanguageProfile> profiles;
    LanguageTools();
    const LanguageProfile* ForFile(const std::string& path) const;
    std::string Load(const std::string& directory);
    std::string Save(const std::string& directory) const;
};
// Shell-like quoting for literal argv only: no expansion, shell, or downloads.
std::vector<std::string> ParseCommand(const std::string& command);
std::vector<std::string> ToolCommand(const std::string& command,const std::string& file,
    const std::string& project,const std::string& settings);
ProcessResult FormatWithPrettier(const std::string& command,const std::string& file,
    const std::string& project,const std::string& settings,const std::string& text,
    const std::atomic<bool>* cancel=nullptr);
}
