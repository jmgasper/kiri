#include "ui/Workspace.h"
#include <Application.h>
#include <Entry.h>
#include <Path.h>
#include <filesystem>
#include <signal.h>
namespace kiri {
class Application:public BApplication {
public:
    Application():BApplication("application/x-vnd.Kiri-editor") {}
    void ReadyToRun() override { EnsureWindow(); }
    void ArgvReceived(int32 argc,char** argv) override {
        EnsureWindow();
        for(int32 i=1;i<argc;++i) {
            std::error_code e;
            BMessage message(std::filesystem::is_directory(argv[i],e)?kOpenProject:kOpenFile);
            message.AddString("path",argv[i]);fWindow->PostMessage(&message);
        }
    }
    void RefsReceived(BMessage* message) override {
        EnsureWindow();entry_ref ref;
        for(int32 i=0;message->FindRef("refs",i,&ref)==B_OK;++i) {
            BEntry entry(&ref,true);BPath path;if(entry.GetPath(&path)!=B_OK) continue;
            BMessage request(entry.IsDirectory()?kOpenProject:kOpenFile);request.AddString("path",path.Path());fWindow->PostMessage(&request);
        }
    }
private:
    void EnsureWindow() { if(!fWindow) { fWindow=new Workspace();fWindow->Show(); } }
    Workspace* fWindow=nullptr;
};
}
int main() { signal(SIGPIPE,SIG_IGN);kiri::Application app;app.Run();return 0; }
