#pragma once
#include <Application.h>
#include <Messenger.h>
#include <string>

namespace kiri {
class Application:public BApplication {
public:
    explicit Application(const std::string& settingsDirectory={},const char* signature="application/x-vnd.Kiri-editor");
    void ReadyToRun() override;
    void ArgvReceived(int32 argc,char** argv) override;
    void RefsReceived(BMessage* message) override;
    void MessageReceived(BMessage* message) override;
private:
    void ShowLauncher();
    void Open(BMessage request);
    std::string fSettings;
    BMessenger fWorkspace,fLauncher;
};
}
