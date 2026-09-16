#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/MarkdownView.h"
#include <Application.h>
#include <CardLayout.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <OS.h>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x);} while(0)
namespace kiri {
struct WorkspaceTestAccess {
    static void Wait(Workspace& w,const std::function<bool()>& ready,int where=__builtin_LINE()) {
        int startCheck=checks;
        auto end=system_time()+15000000;
        do {w.Unlock();snooze(30000);CHECK(w.Lock());if(ready()) return;} while(system_time()<end);
        auto* pane=w.CurrentTab()?Pane(w):nullptr;auto* e=w.Current()?w.Current()->editor:nullptr;
        if(e) std::cerr<<"Source width="<<e->Bounds().Width()<<" target="<<e->Target()->Bounds().Width()<<" hidden="<<e->IsHidden()<<" wrap="<<e->SendMessage(SCI_GETWRAPMODE)<<" wraps="<<e->SendMessage(SCI_WRAPCOUNT,2)<<" zoom="<<e->SendMessage(SCI_GETZOOM)<<" first="<<e->SendMessage(SCI_GETFIRSTVISIBLELINE)<<"\n";
        throw std::runtime_error("Timed out at line "+std::to_string(where)+" after check "+std::to_string(startCheck)+"; path="+(w.Current()?w.Current()->path:"")+" preview="+(pane?pane->Preview()->Status():"absent")+" preview-width="+std::to_string(pane?pane->Preview()->Bounds().Width():0)+" window-width="+std::to_string(w.Bounds().Width())+" hidden="+std::to_string(pane?pane->IsHidden():false)+" revisions="+std::to_string(pane?pane->RenderedRevision():-1)+"/"+std::to_string(w.Current() && w.Current()->editor?w.Current()->editor->InputRevision():-1));
    }
    static MarkdownPane* Pane(Workspace& w) {return dynamic_cast<MarkdownPane*>(w.CurrentTab()->view);}
    static void Ready(Workspace& w,MarkdownPane* pane) {Wait(w,[&]{return pane->RenderedRevision()==w.CurrentTab()->editor->InputRevision() && pane->Preview()->Document();});}
    static void Send(Workspace& w,uint32 what) {BMessage m(what);w.MessageReceived(&m);}
    static Workspace* Setup(const std::string& root) {
        fs::create_directories(root+"/settings");fs::create_directories(root+"/project");
        fs::copy_file("resources/branding/kiri-icon-256.png",root+"/project/local image.png",fs::copy_options::overwrite_existing);
        std::ofstream(root+"/project/linked.md")<<"# Linked file\n\nRead from an explicit local link.\n\n## Details\n\nDetail text.\n";
        std::string text="# Markdown preview\n\nA **native** preview with *emphasis*, ~~strikethrough~~, `inline code`, and 日本語.\n\n[Local link](linked.md#details) · [Jump to table](#comparison) · [Website](https://example.com)\n\n- A nested list\n  - Second level\n- [x] Completed task\n- [ ] Pending task\n\n> A quotation wraps within the available width.\n\n![Kiri](local%20image.png)\n\n## Comparison\n\n| Feature | Status |\n| :--- | ---: |\n| Unsaved edits | Live |\n| Scrolling | Synchronized |\n\n```cpp\nint main() {\n    return 42;\n}\n```\n\n<script>alert('inert HTML')</script>\n\n![Remote](https://example.com/image.png)\n";
        for(int i=0;i<120;++i) text+="\n## Section "+std::to_string(i)+"\n\nA paragraph with enough words to wrap across the preview when its divider is resized.\n";
        std::ofstream(root+"/project/README.md")<<text;
        auto* w=new Workspace(root+"/settings",false);w->Show();CHECK(w->Lock());w->OpenProject(root+"/project");w->OpenFile(root+"/project/README.md");
        Wait(*w,[&]{return w->Current() && w->Current()->editor && w->fPendingOpen.empty();});
        w->fSidebarSplit->SetItemCollapsed(0,true);w->fTerminalSplit->SetItemCollapsed(1,true);Send(*w,kMarkdownPreview);Ready(*w,Pane(*w));return w;
    }
    static void Run(const std::string& root,bool visual) {
        auto* w=Setup(root);auto* panel=Pane(*w);auto* preview=panel->Preview();auto* editor=w->Current()->editor;
        if(visual) {std::cout<<"Visual project: "<<root<<"\n"<<std::flush;w->Unlock();return;}
        CHECK(preview->ImageCount()==1);CHECK(preview->LinkCount()>=3);CHECK(preview->ContentHeight()>5000);
        CHECK(w->CurrentTab()->editor==editor && w->CurrentTab()->view==panel);
        auto* menu=dynamic_cast<BMenuBar*>(w->FindView("menu"));CHECK(menu->FindItem(kMarkdownPreview)->IsMarked());
        auto before=editor->Text();auto revision=editor->InputRevision();editor->SendMessage(SCI_SETSEL,3,9);
        auto caret=editor->SendMessage(SCI_GETCURRENTPOS),anchor=editor->SendMessage(SCI_GETANCHOR);
        // Both directions preserve the buffer, undo state, caret and selection.
        editor->SendMessage(SCI_SETFIRSTVISIBLELINE,150);panel->Refresh();CHECK(std::abs(preview->Bounds().top-preview->YForLine(150))<2);
        preview->ScrollToLine(250);CHECK(std::abs(editor->SendMessage(SCI_GETFIRSTVISIBLELINE)-250)<=1);
        CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==caret && editor->SendMessage(SCI_GETANCHOR)==anchor);
        CHECK(editor->Text()==before && editor->InputRevision()==revision && !editor->Dirty());
        Send(*w,kMarkdownFocus);CHECK(preview->IsFocus());char key=B_PAGE_DOWN;auto y=preview->Bounds().top;preview->KeyDown(&key,1);CHECK(preview->Bounds().top>y);
        key=B_HOME;preview->KeyDown(&key,1);CHECK(preview->Bounds().top==0);key=B_ESCAPE;preview->KeyDown(&key,1);CHECK(!preview->IsFocus());
        CHECK(preview->GoToAnchor("comparison"));CHECK(!preview->GoToAnchor("does-not-exist"));
        // Reflow, light/dark themes, wrapped source lines, zoom and folds.
        w->ResizeTo(900,650);Wait(*w,[&]{return preview->Bounds().Width()<450;});CHECK(preview->ContentHeight()>5000);
        w->ApplyTheme(5);CHECK(preview->ViewColor()==w->fEditorSettings.Colors().background);w->ApplyTheme(0);CHECK(preview->ViewColor()==w->fEditorSettings.Colors().background);
        editor->SendMessage(SCI_SETWRAPMODE,SC_WRAP_WORD);editor->SendMessage(SCI_SETZOOM,3);editor->SendMessage(SCI_SETFIRSTVISIBLELINE,0);
        // Scintilla wraps visible lines first; expose this line before asking
        // for its wrap count so the check does not depend on idle scheduling.
        Wait(*w,[&]{return editor->SendMessage(SCI_WRAPCOUNT,2)>1;});
        auto display=editor->SendMessage(SCI_VISIBLEFROMDOCLINE,150);editor->SendMessage(SCI_SETFIRSTVISIBLELINE,display);panel->Refresh();CHECK(std::abs(preview->Bounds().top-preview->YForLine(150))<2);
        editor->SendMessage(SCI_COLOURISE,0,-1);editor->SendMessage(SCI_FOLDLINE,34,SC_FOLDACTION_CONTRACT);preview->ScrollToLine(200);CHECK(editor->SendMessage(SCI_DOCLINEFROMVISIBLE,editor->SendMessage(SCI_GETFIRSTVISIBLELINE))>=190);editor->SendMessage(SCI_FOLDALL,SC_FOLDACTION_EXPAND);
        editor->SendMessage(SCI_SETWRAPMODE,SC_WRAP_NONE);editor->SendMessage(SCI_SETZOOM,0);
        // Pending work clears the old rendering; a newer edit supersedes it.
        std::string inserted="\n# Unsaved heading\n\n**unfinished";editor->SendMessage(SCI_APPENDTEXT,inserted.size(),reinterpret_cast<sptr_t>(inserted.c_str()));panel->Refresh();CHECK(!preview->Document());
        std::string latest=" latest";editor->SendMessage(SCI_APPENDTEXT,latest.size(),reinterpret_cast<sptr_t>(latest.c_str()));Ready(*w,panel);
        CHECK(preview->GoToAnchor("unsaved-heading"));CHECK(editor->Dirty());CHECK(ReadFile(root+"/project/README.md").bytes==before);
        CHECK(preview->Document()->blocks.back().runs.back().text.find("latest")!=std::string::npos);
        // A second source view shares unsaved edits but owns its preview.
        w->SplitPane(B_HORIZONTAL);auto* shared=w->Current()->editor;CHECK(shared!=editor && shared->State()==editor->State());Send(*w,kMarkdownPreview);auto* other=Pane(*w);Ready(*w,other);
        shared->SendMessage(SCI_APPENDTEXT,5,reinterpret_cast<sptr_t>(" more"));Ready(*w,other);Wait(*w,[&]{return panel->RenderedRevision()==editor->InputRevision();});CHECK(other->Preview()->Document()!=preview->Document());
        auto state=w->ViewState(*w->CurrentTab());CHECK(state.HasBool("markdown"));
        auto* originalPane=w->fPanes.front().get();auto view=w->CurrentTab()->id;CHECK(w->MoveTab(view,*originalPane,0));panel=Pane(*w);editor=w->Current()->editor;Ready(*w,panel);CHECK(w->fPanes.size()==1);
        // Closing a preview while work is pending keeps the source and undo.
        editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));panel->Refresh();Send(*w,kMarkdownPreview);CHECK(!Pane(*w) && w->CurrentTab()->view==editor);CHECK(editor->SendMessage(SCI_CANUNDO));
        editor->SendMessage(SCI_UNDO);Send(*w,kMarkdownPreview);panel=Pane(*w);Ready(*w,panel);
        // Save As refreshes relative resource resolution.
        fs::create_directories(root+"/elsewhere");w->SaveTo(w->Current()->id,root+"/elsewhere/saved.md");Wait(*w,[&]{return !w->Current()->saving;});Ready(*w,panel);CHECK(panel->Preview()->ImageCount()==0);
        w->SaveTo(w->Current()->id,root+"/project/README.md");Wait(*w,[&]{return !w->Current()->saving;});Ready(*w,panel);CHECK(panel->Preview()->ImageCount()==1);
        panel->ActivateLink("javascript:alert(1)");CHECK(w->fDocuments.size()==1);
        Send(*w,kMarkdownFocus);preview=panel->Preview();key=B_HOME;preview->KeyDown(&key,1);key=B_TAB;preview->KeyDown(&key,1);key=B_ENTER;preview->KeyDown(&key,1);
        Wait(*w,[&]{return w->Current()->path==root+"/project/linked.md" && w->fPendingOpen.empty();});CHECK(Pane(*w));Ready(*w,Pane(*w));CHECK(w->Current()->editor->SendMessage(SCI_LINEFROMPOSITION,w->Current()->editor->SendMessage(SCI_GETCURRENTPOS))==4);
        // Reopen and detach retain preview mode and usable source views.
        CHECK(w->CloseView(w->CurrentTab()->id));w->ReopenTab();Wait(*w,[&]{return w->fPendingOpen.empty() && w->Current()->path==root+"/project/linked.md";});CHECK(Pane(*w));Ready(*w,Pane(*w));
        auto* detached=w->DetachTab(w->CurrentTab()->id,BPoint(120,140));CHECK(detached && detached->Lock());CHECK(Pane(*detached));Ready(*detached,Pane(*detached));CHECK(!detached->Current()->editor->IsHidden());detached->Quit();
        w->OpenFile(root+"/project/linked.md");Wait(*w,[&]{return w->fPendingOpen.empty() && w->Current()->path==root+"/project/linked.md";});Send(*w,kMarkdownPreview);Ready(*w,Pane(*w));
        // A clean external reload replaces the source buffer and invalidates
        // the preview even when its view object remains the same.
        std::ofstream(root+"/project/linked.md")<<"# External update\n";w->ReloadExternal(*w->Current(),false);
        Wait(*w,[&]{return w->Current()->editor->Matches("# External update\n");});Ready(*w,Pane(*w));CHECK(Pane(*w)->Preview()->Document()->blocks[1].anchor=="external-update");
        // Session restoration keeps preview mode without sharing view objects.
        w->SaveSettings();w->Quit();w=new Workspace(root+"/settings",true);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring && w->fPendingOpen.empty() && w->Current();});CHECK(Pane(*w));Ready(*w,Pane(*w));
        // Large inputs pause explicitly, then resume after shrinking.
        editor=w->Current()->editor;panel=Pane(*w);editor->SetText(std::string(kMarkdownBytes+1,'x'));editor->NoteInput();panel->Refresh();Wait(*w,[&]{return panel->Preview()->Status().find("exceeds 2 MiB")!=std::string::npos;});CHECK(!panel->Preview()->Document());
        editor->SetText("# Resumed\n");editor->NoteInput();Ready(*w,panel);CHECK(panel->Preview()->Document()->blocks[1].anchor=="resumed");
        std::string large;for(int i=0;i<2000;++i) large+="## Heading "+std::to_string(i)+"\n\nA paragraph with **bold**, *italic*, and [links](linked.md).\n\n";
        auto started=system_time();editor->SetText(large);editor->NoteInput();Ready(*w,panel);auto elapsed=system_time()-started;CHECK(panel->Preview()->Status().empty());CHECK(panel->Preview()->ContentHeight()>100000);
        auto resize=system_time();auto previousWidth=panel->Preview()->Bounds().Width();w->ResizeTo(1100,700);Wait(*w,[&]{return panel->Preview()->Bounds().Width()!=previousWidth;});std::cout<<"Markdown: 8,001 lines rendered in "<<elapsed/1000<<" ms; resize settled in "<<(system_time()-resize)/1000<<" ms.\n";
        editor->MarkSaved();w->Quit();
    }
};
}
int main(int argc,char** argv) {
    BApplication app("application/x-vnd.Kiri-markdown-tests");char temp[]="/tmp/kiri-markdown-XXXXXX";char* folder=mkdtemp(temp);if(!folder) return 1;
    bool visual=argc>1 && std::string(argv[1])=="--visual";
    try {kiri::WorkspaceTestAccess::Run(kiri::CanonicalPath(folder),visual);if(visual) app.Run();else {fs::remove_all(folder);std::cout<<"Passed "<<checks<<" native Markdown checks.\n";}}
    catch(const std::exception& error) {std::cerr<<error.what()<<" (files: "<<folder<<")\n";return 1;}
}
