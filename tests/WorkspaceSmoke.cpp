// A separate application signature and settings directory let native UI checks
// run alongside a user's open Kiri session without touching its documents.
#include "ui/Workspace.h"
#include "ui/Editor.h"
#include <Application.h>
#include <CardLayout.h>
#include <LayoutItem.h>
#include <cstdio>
#include <signal.h>

// Read-only inspection of the visible editor for native UI verification.
// This message is deliberately confined to the disposable smoke harness.
class ProbeWorkspace:public kiri::Workspace {
public:
    explicit ProbeWorkspace(const std::string& settings):Workspace(settings) {}
    void MessageReceived(BMessage* message) override {
        if(message->what!='lprb') { Workspace::MessageReceived(message);return; }
        BMessage reply(B_REPLY);auto* host=FindView("document host");auto* cards=dynamic_cast<BCardLayout*>(host->GetLayout());
        auto* editor=cards && cards->VisibleItem()?dynamic_cast<kiri::Editor*>(cards->VisibleItem()->View()):nullptr;
        if(editor) {
            auto text=editor->Text();reply.AddString("text",text.c_str());reply.AddInt64("caret",editor->SendMessage(SCI_GETCURRENTPOS));
            reply.AddInt64("length",text.size());reply.AddInt64("nul_count",std::count(text.begin(),text.end(),'\0'));
            reply.AddInt64("anchor",editor->SendMessage(SCI_GETANCHOR));reply.AddBool("dirty",editor->Dirty());
            reply.AddBool("completion",editor->SendMessage(SCI_AUTOCACTIVE));
        }
        message->SendReply(&reply);
    }
};

int main(int argc,char** argv) {
    if(argc<2) { std::fprintf(stderr,"Usage: kiri_workspace_smoke SETTINGS_DIRECTORY [PROJECT] [SIGNATURE]\n");return 1; }
    signal(SIGPIPE,SIG_IGN);
    BApplication application(argc>3?argv[3]:"application/x-vnd.Kiri-workspace-tests");
    auto* window=new ProbeWorkspace(argv[1]);
    if(argc>2) window->OpenProject(argv[2]);
    window->Show();application.Run();
    return 0;
}
