#include "ui/RecentItems.h"
#include "core/FileIO.h"
#include <Autolock.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Locker.h>
#include <Message.h>
#include <Path.h>
#include <algorithm>
#include <cstdio>

namespace kiri {
namespace {
BLocker recentLock("Kiri recent items");
void Append(std::vector<RecentItem>& items,const std::string& path,bool folder) {
    if(path.empty() || path.front()!='/' || items.size()>=RecentItems::Limit) return;
    if(std::none_of(items.begin(),items.end(),[&](const auto& item){return item.path==path;})) items.push_back({path,folder});
}
}
std::string SettingsDirectory() {
    BPath path;find_directory(B_USER_SETTINGS_DIRECTORY,&path);path.Append("Kiri");return path.Path();
}
std::vector<RecentItem> RecentItems::Read() const {
    BFile file((fDirectory+"/recent").c_str(),B_READ_ONLY);BMessage stored;
    std::vector<RecentItem> items;
    if(stored.Unflatten(&file)==B_OK) {
        BMessage item;const char* path;bool folder;
        for(int32 i=0;items.size()<Limit && stored.FindMessage("item",i,&item)==B_OK;++i)
            if(item.FindString("path",&path)==B_OK && item.FindBool("folder",&folder)==B_OK) Append(items,path,folder);
        return items;
    }
    // Import the previous session once, before a new workspace can replace it.
    BFile legacy((fDirectory+"/settings").c_str(),B_READ_ONLY);BMessage settings;
    if(settings.Unflatten(&legacy)==B_OK) {
        const char* path;const char* selected=nullptr;settings.FindString("selected",&selected);
        if(settings.FindString("project",&path)==B_OK && *path) Append(items,CanonicalPath(path),true);
        std::vector<std::string> files;
        for(int32 i=0;settings.FindString("file",i,&path)==B_OK;++i) if(*path) files.push_back(path);
        if(selected && std::find(files.begin(),files.end(),selected)!=files.end()) Append(items,CanonicalPath(selected),false);
        for(auto it=files.rbegin();it!=files.rend();++it) Append(items,CanonicalPath(*it),false);
    }
    Write(items);return items;
}
bool RecentItems::Write(const std::vector<RecentItem>& items) const {
    if(create_directory(fDirectory.c_str(),0755)!=B_OK) return false;
    BMessage stored;
    for(const auto& value:items) {
        BMessage item;item.AddString("path",value.path.c_str());item.AddBool("folder",value.folder);stored.AddMessage("item",&item);
    }
    auto temporary=fDirectory+"/recent.tmp";
    BFile file(temporary.c_str(),B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
    if(file.InitCheck()!=B_OK || file.SetPermissions(0600)!=B_OK || stored.Flatten(&file)!=B_OK || file.Sync()!=B_OK) return false;
    return std::rename(temporary.c_str(),(fDirectory+"/recent").c_str())==0;
}
std::vector<RecentItem> RecentItems::Load() const { BAutolock lock(&recentLock);return Read(); }
bool RecentItems::Remember(const std::string& input,bool folder) const {
    if(input.empty()) return false;
    auto path=CanonicalPath(input);if(path.empty() || path.front()!='/') return false;
    BAutolock lock(&recentLock);auto items=Read();
    items.erase(std::remove_if(items.begin(),items.end(),[&](const auto& item){return item.path==path;}),items.end());
    items.insert(items.begin(),{path,folder});if(items.size()>Limit) items.resize(Limit);
    return Write(items);
}
bool RecentItems::Remove(const std::string& path) const {
    BAutolock lock(&recentLock);auto items=Read();
    items.erase(std::remove_if(items.begin(),items.end(),[&](const auto& item){return item.path==path;}),items.end());
    return Write(items);
}
}
