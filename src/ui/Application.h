#pragma once
#include <Application.h>
#include <Messenger.h>
#include <string>
#include <vector>

namespace kiri {
class Application:public BApplication {
public:
    explicit Application(const std::string& settingsDirectory={},const char* signature="application/x-vnd.Kiri-editor");
    void ReadyToRun() override;
    bool QuitRequested() override;
    void ArgvReceived(int32 argc,char** argv) override;
    void RefsReceived(BMessage* message) override;
    void MessageReceived(BMessage* message) override;
private:
    void ShowLauncher();
    void Open(BMessage request);
    void RecoverWindows();
    void ContinueQuit();
    std::string fSettings;
    BMessenger fWorkspace,fLauncher;
    std::vector<BMessenger> fWorkspaces;
    BMessenger fClosingWorkspace;
    bool fQuitting=false;
};
}
