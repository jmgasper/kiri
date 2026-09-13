#include "ui/Application.h"
#include "ui/LauncherWindow.h"
#include "ui/RecentItems.h"
#include "ui/Workspace.h"
#include <Entry.h>
#include <File.h>
#include <Path.h>
#include <filesystem>

namespace kiri {
Application::Application(const std::string& directory,const char* signature)
    :BApplication(signature),fSettings(directory.empty()?SettingsDirectory():directory) {
    RecentItems(fSettings).Load();
}
void Application::ReadyToRun() {
    if(fWorkspace.IsValid()) return;
    // Recover unsaved work immediately after an abnormal exit.
    if(!ListDrafts(fSettings+"/recovery").empty()) {
        auto* window=new Workspace(fSettings);fWorkspace=BMessenger(window);window->Show();
    } else ShowLauncher();
}
void Application::ShowLauncher() {
    if(fLauncher.IsValid()) fLauncher.SendMessage(kShowLauncher);
    else { auto* window=new LauncherWindow(fSettings);fLauncher=BMessenger(window);window->Show(); }
}
void Application::Open(BMessage request) {
    if(!fWorkspace.IsValid()) {
        bool restore=false;const char* path=nullptr;
        if(request.what==kOpenProject && request.FindString("path",&path)==B_OK) {
            BFile file((fSettings+"/settings").c_str(),B_READ_ONLY);BMessage settings;const char* project;
            restore=settings.Unflatten(&file)==B_OK && settings.FindString("project",&project)==B_OK
                && *project && CanonicalPath(project)==CanonicalPath(path);
        }
        auto* window=new Workspace(fSettings,restore);fWorkspace=BMessenger(window);
        if(restore) RecentItems(fSettings).Remember(path,true);
        else window->PostMessage(&request);
        window->Show();
    } else fWorkspace.SendMessage(&request);
    fWorkspace.SendMessage(kActivateWorkspace);
    if(fLauncher.IsValid()) fLauncher.SendMessage(kHideLauncher);
}
void Application::ArgvReceived(int32 argc,char** argv) {
    const char* cwd=nullptr;if(CurrentMessage()) CurrentMessage()->FindString("cwd",&cwd);
    for(int32 i=1;i<argc;++i) {
        auto path=std::filesystem::path(argv[i]);if(path.is_relative() && cwd) path=std::filesystem::path(cwd)/path;
        std::error_code error;BMessage request(std::filesystem::is_directory(path,error)?kOpenProject:kOpenFile);
        request.AddString("path",path.c_str());Open(request);
    }
}
void Application::RefsReceived(BMessage* message) {
    entry_ref ref;
    for(int32 i=0;message->FindRef("refs",i,&ref)==B_OK;++i) {
        BEntry entry(&ref,true);BPath path;if(entry.GetPath(&path)!=B_OK) continue;
        BMessage request(entry.IsDirectory()?kOpenProject:kOpenFile);request.AddString("path",path.Path());Open(request);
    }
}
void Application::MessageReceived(BMessage* message) {
    switch(message->what) {
        case kOpenProject:case kOpenFile:case kNewFile:Open(*message);break;
        case kShowLauncher:ShowLauncher();break;
        case kRecentsChanged:if(fLauncher.IsValid()) fLauncher.SendMessage(kRecentsChanged);break;
        case kLauncherClosed:if(!fWorkspace.IsValid()) PostMessage(B_QUIT_REQUESTED);break;
        case B_SILENT_RELAUNCH:if(fWorkspace.IsValid()) fWorkspace.SendMessage(kActivateWorkspace);else ShowLauncher();break;
        default:BApplication::MessageReceived(message);
    }
}
}
