// A separate application signature and settings directory let native UI checks
// run alongside a user's open Kiri session without touching its documents.
#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/ProblemsView.h"
#include <SplitView.h>
#include "ui/TabStrip.h"
#include "ui/Application.h"
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
        reply.AddRect("frame",workspace.Frame());
        reply.AddString("theme_id",workspace.fEditorSettings.themeID.c_str());reply.AddBool("semantic_enabled",workspace.fEditorSettings.semanticHighlighting);
        reply.AddBool("problems_collapsed",workspace.fTerminalSplit->IsItemCollapsed(2));reply.AddBool("problems_hidden",workspace.fProblems->IsHidden());
        if(auto* d=workspace.Current()) {
            reply.AddString("language_status",d->languageStatus.c_str());reply.AddString("diagnostic_status",d->diagnosticStatus.c_str());reply.AddString("semantic_status",d->semanticStatus.c_str());
            reply.AddInt32("diagnostics",d->diagnostics.size());reply.AddInt32("semantic_tokens",d->semanticTokens.size());reply.AddInt32("server_version",d->serverVersion);
            for(const auto& problem:d->diagnostics) {BMessage row;row.AddInt64("start",problem.start);row.AddInt64("end",problem.end);row.AddString("message",problem.message.c_str());reply.AddMessage("problem",&row);}
            std::map<std::string,int> roles;for(const auto& token:d->semanticTokens) ++roles[SemanticColorRole(token.role)];for(auto& role:roles) reply.AddInt32(role.first.c_str(),role.second);
        }

        for(auto& pane:workspace.fPanes) {
            BMessage geometry;geometry.AddInt64("id",pane->id);geometry.AddRect("strip",pane->strip->ConvertToScreen(pane->strip->Bounds()));
            geometry.AddRect("panel",pane->panel->ConvertToScreen(pane->panel->Bounds()));reply.AddMessage("pane",&geometry);
        }
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

class ProbeApplication:public kiri::Application {
public:
    ProbeApplication(const std::string& settings,const char* signature):Application(settings,signature) {}
    void ReadyToRun() override {}
    void ArgvReceived(int32,char**) override {}
    void MessageReceived(BMessage* message) override {
        if(message->what!='lprb') { Application::MessageReceived(message);return; }
        BMessage reply(B_REPLY);
        for(int32 i=0;i<CountWindows();++i) if(auto* workspace=dynamic_cast<kiri::Workspace*>(WindowAt(i));workspace && workspace->Lock()) {
            auto state=kiri::WorkspaceTestAccess::Inspect(*workspace);reply.AddMessage("workspace",&state);workspace->Unlock();
        }
        message->SendReply(&reply);
    }
};

int main(int argc,char** argv) {
    if(argc<2) { std::fprintf(stderr,"Usage: kiri_workspace_smoke SETTINGS_DIRECTORY [PROJECT] [SIGNATURE]\n");return 1; }
    signal(SIGPIPE,SIG_IGN);
    ProbeApplication application(argv[1],argc>3?argv[3]:"application/x-vnd.Kiri-workspace-tests");
    auto* window=new ProbeWorkspace(argv[1]);
    if(argc>2) window->OpenProject(argv[2]);
    window->Show();application.Run();
    return 0;
}
