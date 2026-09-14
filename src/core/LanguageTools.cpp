#include "core/LanguageTools.h"
#include "core/FileIO.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>

namespace kiri {
namespace fs=std::filesystem;
using Json=nlohmann::json;
LanguageTools::LanguageTools():profiles{
    {"C / C++","cpp","clangd",{".cpp",".cc",".cxx",".hpp",".hxx",".hh",".h"}},
    {"C","c","clangd",{".c"}},
    {"JavaScript","javascript","typescript-language-server --stdio",{".js",".mjs",".cjs"}},
    {"JavaScript JSX","javascriptreact","typescript-language-server --stdio",{".jsx"}},
    {"TypeScript","typescript","typescript-language-server --stdio",{".ts",".mts",".cts"}},
    {"TypeScript JSX","typescriptreact","typescript-language-server --stdio",{".tsx"}},
    {"Python","python","pylsp",{".py",".pyw"}},
    {"Rust","rust","rust-analyzer",{".rs"}},
    {"Go","go","gopls",{".go"}},
    {"HTML","html","vscode-html-language-server --stdio",{".html",".htm"}},
    {"CSS","css","vscode-css-language-server --stdio",{".css"}},
    {"SCSS","scss","vscode-css-language-server --stdio",{".scss"}},
    {"Less","less","vscode-css-language-server --stdio",{".less"}},
    {"JSON","json","vscode-json-language-server --stdio",{".json"}},
    {"JSON with comments","jsonc","vscode-json-language-server --stdio",{".jsonc"}}
} {
    // TypeScript's automatic @types downloader rejects Haiku. Project-installed
    // types still work, and language tools do not implicitly download packages.
    for(auto& profile:profiles) if(profile.command=="typescript-language-server --stdio")
        profile.initializationOptions=R"({"disableAutomaticTypingAcquisition":true})";
}
const LanguageProfile* LanguageTools::ForFile(const std::string& file) const {
    auto extension=fs::path(file).extension().string();
    for(auto& c:extension) c=std::tolower(static_cast<unsigned char>(c));
    for(const auto& profile:profiles)
        if(std::find(profile.extensions.begin(),profile.extensions.end(),extension)!=profile.extensions.end()) return &profile;
    return nullptr;
}
std::string LanguageTools::Load(const std::string& directory) {
    auto path=directory+"/language-tools.json";if(!StatFile(path).exists) return {};
    auto file=ReadFile(path,nullptr,1024*1024);if(!file.ok()) return file.error;
    try {
        auto json=Json::parse(file.bytes);LanguageTools next;
        next.prettier=json.value("prettier",next.prettier);next.completion=json.value("automaticCompletion",true);
        if(json.contains("servers")) {
            if(!json["servers"].is_array()) throw std::runtime_error("servers must be an array");
            auto defaults=next.profiles;next.profiles.clear();
            for(const auto& server:json["servers"]) {
                LanguageProfile profile{server.at("name").get<std::string>(),server.at("language").get<std::string>(),
                    server.at("command").get<std::string>(),server.at("extensions").get<std::vector<std::string>>()};
                for(const auto& original:defaults) if(original.language==profile.language) profile.initializationOptions=original.initializationOptions;
                profile.initializationOptions=server.value("initializationOptions",Json::parse(profile.initializationOptions)).dump();
                profile.configuration=server.value("configuration",Json::object()).dump();
                if(profile.name.empty() || profile.language.empty() || profile.extensions.empty()) throw std::runtime_error("Incomplete language server profile");
                ParseCommand(profile.command);
                for(const auto& extension:profile.extensions) if(extension.empty() || extension[0]!='.') throw std::runtime_error("Extensions must start with a dot");
                next.profiles.push_back(std::move(profile));
            }
        }
        ParseCommand(next.prettier);*this=std::move(next);return {};
    } catch(const std::exception& error) { return "Cannot read language-tools.json: "+std::string(error.what()); }
}
std::string LanguageTools::Save(const std::string& directory) const {
    try {
        ParseCommand(prettier);Json servers=Json::array();
        for(const auto& profile:profiles) {
            ParseCommand(profile.command);
            servers.push_back({{"name",profile.name},{"language",profile.language},{"command",profile.command},{"extensions",profile.extensions},
                {"initializationOptions",Json::parse(profile.initializationOptions)},{"configuration",Json::parse(profile.configuration)}});
        }
        std::error_code error;fs::create_directories(directory,error);if(error) return error.message();
        auto path=directory+"/language-tools.json";
        return SaveFile(path,Json({{"prettier",prettier},{"automaticCompletion",completion},{"servers",servers}}).dump(2)+"\n",StatFile(path));
    } catch(const std::exception& error) { return error.what(); }
}
std::vector<std::string> ParseCommand(const std::string& command) {
    std::vector<std::string> args;std::string word;char quote=0;bool started=false,escape=false;
    for(char c:command) {
        if(c=='\0' || c=='\n' || c=='\r') throw std::runtime_error("A command must be a single line");
        if(escape) { word+=c;escape=false;started=true; }
        else if(c=='\\' && quote!='\'') { escape=true;started=true; }
        else if(quote) { if(c==quote) quote=0;else word+=c; }
        else if(c=='\'' || c=='\"') { quote=c;started=true; }
        else if(std::isspace(static_cast<unsigned char>(c))) { if(started) { args.push_back(word);word.clear();started=false; } }
        else { word+=c;started=true; }
    }
    if(quote || escape) throw std::runtime_error("Unclosed quote or trailing backslash in command");
    if(started) args.push_back(word);
    if(!args.empty() && args[0].empty()) throw std::runtime_error("The executable name cannot be empty");
    return args;
}
std::vector<std::string> ToolCommand(const std::string& command,const std::string& file,const std::string& project,const std::string& settings) {
    auto args=ParseCommand(command);if(args.empty() || args[0].find('/')!=std::string::npos) return args;
    auto find=[&](const fs::path& directory) {
        auto candidate=(directory/args[0]).string();
        if(access(candidate.c_str(),X_OK)!=0) return false;
        args[0]=candidate;return true;
    };
    fs::path directory=fs::path(file).parent_path();if(directory.empty()) directory=project;
    while(!directory.empty()) {
        if(find(directory/"node_modules/.bin")) return args;
        if(directory==directory.root_path() || directory==fs::path(project)) break;
        directory=directory.parent_path();
    }
    if(find(fs::path(settings)/"tools/node_modules/.bin") || find(fs::path(settings)/"tools/bin")) return args;
    const char* env=std::getenv("PATH");std::string paths=env?env:"/bin:/usr/bin:/boot/system/bin";
    for(size_t start=0;start<=paths.size();) {
        auto end=paths.find(':',start);if(end==std::string::npos) end=paths.size();
        if(find(paths.substr(start,end-start))) return args;
        start=end+1;
    }
    return args;
}
ProcessResult FormatWithPrettier(const std::string& command,const std::string& file,const std::string& project,
        const std::string& settings,const std::string& text,const std::atomic<bool>* cancel) {
    ProcessResult result;
    if(file.empty()) { result.error="Save this file with an extension before formatting so Prettier can select its parser.";return result; }
    if(text.size()>8*1024*1024) { result.error="Prettier formatting is limited to files up to 8 MiB.";return result; }
    try {
        auto args=ToolCommand(command,file,project,settings);
        if(args.empty()) { result.error="Set a Prettier command in Edit → Language Tools.";return result; }
        args.insert(args.end(),{"--stdin-filepath",file,"--log-level","error"});
        ProcessOptions options;options.input=text;options.directory=project.empty()?fs::path(file).parent_path().string():project;options.cancel=cancel;
        result=RunProcess(args,options);
        if(result.ok() && (!IsValidUTF8(result.output) || result.output.find('\0')!=std::string::npos)) {
            result.exitCode=-1;result.error="Prettier returned invalid source text.";
        }
    } catch(const std::exception& error) { result.error=error.what(); }
    return result;
}
}
