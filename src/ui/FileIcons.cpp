#include "ui/FileIcons.h"
#include <Bitmap.h>
#include <Mime.h>
#include <MimeType.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <vector>

namespace kiri {
namespace {
struct Type { const char* mime;const char* fallback; };
const std::map<std::string,Type> kTypes={
    {".c",{"text/x-csrc","text/x-source-code"}},
    {".h",{"text/x-chdr","text/x-source-code"}},
    {".cpp",{"text/x-c++src","text/x-source-code"}},
    {".cc",{"text/x-c++src","text/x-source-code"}},
    {".cxx",{"text/x-c++src","text/x-source-code"}},
    {".hpp",{"text/x-c++hdr","text/x-source-code"}},
    {".py",{"text/x-python","text/x-source-code"}},
    {".js",{"text/javascript","text/x-source-code"}},
    {".ts",{"text/javascript","text/x-source-code"}},
    {".json",{"application/json","text/x-source-code"}},
    {".jsonc",{"application/json","text/x-source-code"}},
    {".html",{"text/html","text/plain"}},
    {".htm",{"text/html","text/plain"}},
    {".xml",{"text/xml","text/x-source-code"}},
    {".css",{"text/css","text/x-source-code"}},
    {".sh",{"text/x-shellscript","text/x-source-code"}},
    {".bash",{"text/x-shellscript","text/x-source-code"}},
    {".zsh",{"text/x-shellscript","text/x-source-code"}},
    {".md",{"text/markdown","text/plain"}},
    {".txt",{"text/plain","text/plain"}},
    {".log",{"text/plain","text/plain"}},
    {".ini",{"text/plain","text/plain"}},
    {".toml",{"text/plain","text/plain"}},
    {".yaml",{"text/plain","text/plain"}},
    {".yml",{"text/plain","text/plain"}},
    {".png",{"image/png","image"}},
    {".jpg",{"image/jpeg","image"}},
    {".jpeg",{"image/jpeg","image"}},
    {".gif",{"image/gif","image"}},
    {".svg",{"image/svg+xml","image"}},
    {".webp",{"image/webp","image"}},
    {".bmp",{"image/bmp","image"}},
    {".pdf",{"application/pdf",B_FILE_MIME_TYPE}},
    {".zip",{"application/zip",B_FILE_MIME_TYPE}},
    {".gz",{"application/x-gzip","application/zip"}},
    {".tar",{"application/x-tar","application/zip"}},
    {".hpkg",{"application/x-vnd.haiku-package",B_FILE_MIME_TYPE}},
    {".mp3",{"audio/mpeg","audio"}},
    {".wav",{"audio/x-wav","audio"}},
    {".ogg",{"audio/ogg","audio"}},
    {".mp4",{"video/mp4","video"}},
    {".webm",{"video/webm","video"}},
    {".bin",{B_FILE_MIME_TYPE,B_FILE_MIME_TYPE}}
};
std::shared_ptr<const BBitmap> LoadIcon(const std::string& mime,const std::string& fallback) {
    auto bitmap=std::make_shared<BBitmap>(BRect(0,0,15,15),B_RGBA32);
    if(bitmap->InitCheck()!=B_OK) return {};
    std::vector<std::string> types={mime,fallback};
    if(mime.rfind("text/",0)==0 || fallback.rfind("text/",0)==0) types.emplace_back("text/plain");
    types.emplace_back(B_FILE_MIME_TYPE);
    for(const auto& type:types) if(BMimeType(type.c_str()).GetIcon(bitmap.get(),B_MINI_ICON)==B_OK) return bitmap;
    return {};
}
}
std::shared_ptr<const BBitmap> FileIcon(const std::string& path,bool directory) {
    std::string name=std::filesystem::path(path).filename().string();
    auto extension=std::filesystem::path(name).extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return std::tolower(c);});
    if(name=="Makefile" || name=="GNUmakefile" || name=="Jamfile" || name=="CMakeLists.txt") extension=".sh";
    if(name=="Dockerfile" || name==".gitignore" || name=="LICENSE" || name=="README" || path.empty() || name.rfind("Untitled ",0)==0) extension=".txt";
    if(directory) extension="<directory>";
    static std::mutex mutex;
    // Views own the bitmaps. A strong static cache would destroy them after
    // BApplication closes its app_server connection during process teardown.
    static std::map<std::string,std::weak_ptr<const BBitmap>> icons;
    std::lock_guard<std::mutex> guard(mutex);
    auto cached=icons.find(extension);
    if(cached!=icons.end()) if(auto icon=cached->second.lock()) return icon;
    std::string mime=B_FILE_MIME_TYPE,fallback=B_FILE_MIME_TYPE;
    if(directory) mime="application/x-vnd.Be-directory";
    else if(auto found=kTypes.find(extension);found!=kTypes.end()) { mime=found->second.mime;fallback=found->second.fallback; }
    else {
        BMimeType guessed;
        if(BMimeType::GuessMimeType(name.c_str(),&guessed)==B_OK) mime=guessed.Type();
    }
    auto icon=LoadIcon(mime,fallback);icons[extension]=icon;return icon;
}
}
