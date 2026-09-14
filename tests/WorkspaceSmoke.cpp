// A separate application signature and settings directory let native UI checks
// run alongside a user's open Kiri session without touching its documents.
#include "ui/Workspace.h"
#include "ui/Editor.h"
#include <Application.h>
#include <CardLayout.h>
#include <LayoutItem.h>
#include <cstdio>
#include <signal.h>

namespace kiri {
// Read-only inspection stays in the disposable harness, outside the application.
struct WorkspaceTestAccess {
    static BMessage Inspect(Workspace& workspace) {
        workspace.SyncFocus();BMessage reply(B_REPLY);auto* tab=workspace.CurrentTab();auto* editor=tab?tab->editor:nullptr;
        reply.AddInt32("panes",workspace.fPanes.size());reply.AddInt32("documents",workspace.fDocuments.size());
        reply.AddInt32("closed_tabs",workspace.fClosedTabs.size());reply.AddBool("preview_tabs",workspace.fPreviewTabs);
        for(auto& pane:workspace.fPanes) for(auto& view:pane->tabs) {
            auto* document=workspace.ByID(view->document);BMessage state=workspace.ViewState(*view);
            state.AddInt64("pane",pane->id);state.AddInt64("tab",view->id);state.AddInt64("document",document->id);
            state.AddString("path",document->path.c_str());state.AddBool("preview",view->preview);
            state.AddBool("active",view.get()==tab);state.AddBool("dirty",view->editor && view->editor->Dirty());reply.AddMessage("view",&state);
        }
        if(editor) {
            auto length=editor->SendMessage(SCI_GETLENGTH);reply.AddInt64("length",length);
            if(length<=8192) { auto text=editor->Text();reply.AddString("text",text.c_str());reply.AddInt64("nul_count",std::count(text.begin(),text.end(),'\0')); }
            reply.AddInt64("caret",editor->SendMessage(SCI_GETCURRENTPOS));reply.AddInt64("anchor",editor->SendMessage(SCI_GETANCHOR));
            reply.AddBool("dirty",editor->Dirty());reply.AddBool("completion",editor->SendMessage(SCI_AUTOCACTIVE));
        }
        return reply;
    }
};
}
class ProbeWorkspace:public kiri::Workspace {
public:
    explicit ProbeWorkspace(const std::string& settings):Workspace(settings) {}
    void MessageReceived(BMessage* message) override {
        if(message->what!='lprb') { Workspace::MessageReceived(message);return; }
        auto reply=kiri::WorkspaceTestAccess::Inspect(*this);
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
