#include "ui/Application.h"
#include "ui/LauncherWindow.h"
#include "ui/RecentItems.h"
#include "ui/Workspace.h"
#include <Entry.h>
#include <File.h>
#include <Path.h>
#include <filesystem>
#include <algorithm>

namespace kiri {
Application::Application(const std::string& directory,const char* signature)
    :BApplication(signature),fSettings(directory.empty()?SettingsDirectory():directory) {
    RecentItems(fSettings).Load();
}
void Application::ReadyToRun() {
    RecoverWindows();
    if(fWorkspace.IsValid()) return;
    // Recover unsaved work immediately after an abnormal exit.
    if(!ListDrafts(fSettings+"/recovery").empty()) {
        auto* window=new Workspace(fSettings);fWorkspace=BMessenger(window);window->Show();
    } else if(!fWorkspaces.empty()) fWorkspace=fWorkspaces.back();
    else ShowLauncher();
}
void Application::RecoverWindows() {
    std::error_code error;
    for(const auto& entry:std::filesystem::directory_iterator(fSettings+"/windows",error)) {
        if(!entry.is_directory(error)) continue;
        auto directory=entry.path().string();
        if(!std::filesystem::exists(entry.path()/"settings",error) && ListDrafts(directory+"/recovery").empty()) continue;
        auto* window=new Workspace(fSettings,true,directory);window->Show();
        fWorkspaces.push_back(BMessenger(window));
    }
}
bool Application::QuitRequested() {
    fWorkspaces.erase(std::remove_if(fWorkspaces.begin(),fWorkspaces.end(),[](const auto& item){return !item.IsValid();}),fWorkspaces.end());
    if(fWorkspaces.empty()) return BApplication::QuitRequested();
    // A workspace may finish its save asynchronously before it can close.
    // Resume the application quit on its close notification, one window at a time.
    if(!fQuitting) { fQuitting=true;ContinueQuit(); }
    return false;
}
void Application::ContinueQuit() {
    if(fWorkspaces.empty()) { PostMessage(B_QUIT_REQUESTED);return; }
    fClosingWorkspace=fWorkspace.IsValid()?fWorkspace:fWorkspaces.back();
    fClosingWorkspace.SendMessage(B_QUIT_REQUESTED);
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
        case kThemeLibraryChanged: {
            BMessenger source;message->FindMessenger("source",&source);
            for(auto& workspace:fWorkspaces) if(workspace.IsValid() && workspace!=source) workspace.SendMessage(message);
            if(fLauncher.IsValid()) fLauncher.SendMessage(kRecentsChanged);break;
        }
        case kWorkspaceOpened:case kWorkspaceActivated: {
            BMessenger window;if(message->FindMessenger("workspace",&window)!=B_OK || !window.IsValid()) break;
            if(std::find(fWorkspaces.begin(),fWorkspaces.end(),window)==fWorkspaces.end()) fWorkspaces.push_back(window);
            if(message->what==kWorkspaceActivated || !fWorkspace.IsValid()) fWorkspace=window;
            break;
        }
        case kWorkspaceClosed: {
            BMessenger window;message->FindMessenger("workspace",&window);
            fWorkspaces.erase(std::remove_if(fWorkspaces.begin(),fWorkspaces.end(),[&](const auto& item){return item==window || !item.IsValid();}),fWorkspaces.end());
            if(fWorkspace==window || !fWorkspace.IsValid()) fWorkspace=fWorkspaces.empty()?BMessenger():fWorkspaces.back();
            if(fQuitting) {
                if(window==fClosingWorkspace || !fClosingWorkspace.IsValid()) { fClosingWorkspace=BMessenger();ContinueQuit(); }
            } else if(fWorkspaces.empty()) PostMessage(B_QUIT_REQUESTED);
            break;
        }
        case kWorkspaceQuitCancelled: {
            BMessenger window;message->FindMessenger("workspace",&window);
            if(window==fClosingWorkspace) { fQuitting=false;fClosingWorkspace=BMessenger(); }
            break;
        }
        case kLauncherClosed:if(!fWorkspace.IsValid() && fWorkspaces.empty()) PostMessage(B_QUIT_REQUESTED);break;
        case B_SILENT_RELAUNCH:if(fWorkspace.IsValid()) fWorkspace.SendMessage(kActivateWorkspace);else ShowLauncher();break;
        default:BApplication::MessageReceived(message);
    }
}
}
