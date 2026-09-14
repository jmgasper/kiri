// Real Haiku windows and Scintilla buffers, with isolated files and settings.
#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/TabStrip.h"
#include "ui/TerminalPanel.h"
#include <Application.h>
#include <CardLayout.h>
#include <File.h>
#include <OS.h>
#include <SplitView.h>
#include <TextControl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <signal.h>
#include <unistd.h>

namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x); } while(0)
namespace kiri {
static int LanguageFixture() {
    RpcFramer framer;std::map<std::string,std::string> documents;char bytes[4096];
    while(auto count=read(STDIN_FILENO,bytes,sizeof(bytes))) {
        if(count<0) return 1;
        for(auto& message:framer.Feed(std::string_view(bytes,count))) {
            auto method=message.value("method",std::string());auto params=message.value("params",Json::object());Json result=nullptr;
            if(method=="exit") return 0;
            if(method=="initialize") result={{"capabilities",{{"textDocumentSync",1},{"documentSymbolProvider",true},{"completionProvider",Json::object()}}}};
            else if(method=="textDocument/didOpen") documents[params["textDocument"]["uri"]]=params["textDocument"]["text"];
            else if(method=="textDocument/didChange") documents[params["textDocument"]["uri"]]=params["contentChanges"][0]["text"];
            else if(method=="textDocument/didClose") documents.erase(params["textDocument"]["uri"]);
            else if(method=="textDocument/documentSymbol") {
                auto uri=params["textDocument"]["uri"].get<std::string>();auto text=documents.at(uri);
                Json range={{"start",PositionJSON({0,0})},{"end",PositionJSON(PositionAt(text,text.size()))}};
                result=Json::array({{{"name",fs::path(uri).filename().string()},{"kind",13},{"range",range},{"selectionRange",range}}});
            } else if(method=="textDocument/completion") {
                snooze(120000);auto uri=params["textDocument"]["uri"].get<std::string>();
                result=Json::array({{{"label","name"},{"insertText","name"},{"detail",fs::path(uri).filename().string()}}});
            }
            if(message.contains("id")) {
                auto wire=RpcFramer::Frame({{"jsonrpc","2.0"},{"id",message["id"]},{"result",result}});
                size_t at=0;while(at<wire.size()) { auto written=write(STDOUT_FILENO,wire.data()+at,wire.size()-at);if(written<=0) return 1;at+=written; }
            }
        }
    }return 0;
}
struct WorkspaceTestAccess {
    static void Wait(Workspace& w,const std::function<bool()>& ready) {
        auto until=system_time()+15000000;
        do { w.Unlock();snooze(30000);if(!w.Lock()) throw std::runtime_error("Window closed while waiting");if(ready()) return; } while(system_time()<until);
        throw std::runtime_error("Timed out waiting for workspace operation");
    }
    static void Open(Workspace& w,const std::string& path,bool preview=false) {
        w.OpenFile(path,1,1,true,preview);Wait(w,[&]{return w.fPendingOpen.empty();});
        if(!w.Current() || w.Current()->path!=path) throw std::runtime_error("Open expected "+path+", got "+(w.Current()?w.Current()->path:"no document"));
        CHECK(w.Current() && w.Current()->path==path);
    }
    static void Send(Workspace& w,uint32 command) { BMessage message(command);w.MessageReceived(&message); }
    static void Save(Workspace& w) {
        auto* d=w.Current();w.Save(d);Wait(w,[&]{return !d->saving;});CHECK(!d->editor->Dirty());
    }
    static void Run(const std::string& root,const std::string& executable) {
        auto settings=root+"/settings",a=root+"/one.txt",b=root+"/two.txt",c=root+"/three.txt",e=root+"/four.txt";
        std::string lines;for(int i=0;i<400;++i) lines+="line "+std::to_string(i)+" needle\n";
        for(auto& path:{a,b,c,e}) { std::ofstream file(path);file<<path<<'\n'<<lines; }
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());
        Open(*w,a);auto* left=w->fActivePane;auto* first=w->CurrentTab()->editor;
        first->SendMessage(SCI_SETSEL,10,20);first->SendMessage(SCI_SETFIRSTVISIBLELINE,40);
        w->SplitPane(B_HORIZONTAL);auto* right=w->fActivePane;auto* second=w->CurrentTab()->editor;
        CHECK(w->fPanes.size()==2 && w->fDocuments.size()==1);CHECK(first!=second);
        CHECK(first->SendMessage(SCI_GETDOCPOINTER)==second->SendMessage(SCI_GETDOCPOINTER));CHECK(first->State()==second->State());
        second->SendMessage(SCI_SETSEL,70,80);CHECK(first->SendMessage(SCI_GETCURRENTPOS)==20);
        second->SendMessage(SCI_SETFIRSTVISIBLELINE,90);CHECK(first->SendMessage(SCI_GETFIRSTVISIBLELINE)==40);
        auto revision=first->Revision();second->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("shared"));
        CHECK(first->Dirty() && second->Dirty() && first->Revision()>revision);CHECK(first->Matches(second->Text()));
        first->SendMessage(SCI_UNDO);CHECK(!first->Dirty() && !second->Dirty());
        first->MarkRecovered();CHECK(second->Dirty());second->MarkSaved();CHECK(!first->Dirty());
        second->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("saved!"));Save(*w);CHECK(!first->Dirty());CHECK(ReadFile(a).bytes==first->Text());

        Open(*w,b);CHECK(right->tabs.size()==2 && left->tabs.size()==1 && w->fDocuments.size()==2);
        w->SplitPane(B_VERTICAL);auto* bottom=w->fActivePane;CHECK(w->fPanes.size()==3);CHECK(w->fLayout->second->split->Orientation()==B_VERTICAL);
        auto* third=w->CurrentTab()->editor;auto bottomView=w->CurrentTab()->id;
        // Mouse focus and menu commands must choose the view containing input.
        first->MakeFocus();Send(*w,kZoomIn);CHECK(w->fActivePane==left);CHECK(first->SendMessage(SCI_GETZOOM)==1);CHECK(third->SendMessage(SCI_GETZOOM)==0);
        w->fFindText->SetText("needle");Send(*w,kFindNext);CHECK(first->SendMessage(SCI_GETSELECTIONEND)>first->SendMessage(SCI_GETSELECTIONSTART));
        auto otherPosition=first->SendMessage(SCI_GETCURRENTPOS);
        third->MakeFocus();Send(*w,kFindNext);CHECK(w->fActivePane==bottom);CHECK(w->Current()->path==b);
        CHECK(otherPosition==first->SendMessage(SCI_GETCURRENTPOS));CHECK(third->SendMessage(SCI_GETSELECTIONEND)>third->SendMessage(SCI_GETSELECTIONSTART));
        third->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));
        CHECK(w->CloseView(bottomView));CHECK(w->fPanes.size()==2 && w->fDocuments.size()==2); // No dirty prompt while another view exists.
        w->ActivatePane(right,true);w->SelectTab(1);w->CurrentTab()->editor->SendMessage(SCI_UNDO);
        CHECK(!w->CurrentTab()->editor->Dirty());

        Open(*w,c,true);CHECK(w->CurrentTab()->preview);auto previewID=w->CurrentTab()->id;
        Open(*w,e,true);CHECK(!w->FindTab(previewID));CHECK(right->tabs.size()==3);CHECK(w->fClosedTabs.size()==1);
        auto* previewEditor=w->CurrentTab()->editor;previewEditor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));
        // Open before SCN_MODIFIED is delivered: synchronous dirty state protects edits.
        w->OpenFile(c,1,1,true,true);Wait(*w,[&]{return w->fPendingOpen.empty();});
        CHECK(right->tabs.size()==4);CHECK(!right->tabs[2]->preview && right->tabs[2]->editor->Dirty());
        CHECK(w->CurrentTab()->preview);BMessage keep(kKeepTab);keep.AddBool("current_tab",true);keep.AddInt32("index",23);
        auto keepID=w->CurrentTab()->id;w->MessageReceived(&keep);CHECK(w->CurrentTab()->id==keepID && !w->CurrentTab()->preview);
        w->SelectTab(2);Save(*w);
        w->CurrentTab()->editor->SendMessage(SCI_SETSEL,150,160);w->CurrentTab()->editor->SendMessage(SCI_SETFIRSTVISIBLELINE,85);
        CHECK(w->CloseTab(right->selected));w->ReopenTab();Wait(*w,[&]{return w->fPendingOpen.empty();});
        CHECK(w->Current()->path==e);CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETCURRENTPOS)==160);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE)==85);
        CHECK(w->CloseTab(right->selected));Open(*w,e);auto count=right->tabs.size();w->ReopenTab();CHECK(right->tabs.size()==count);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETCURRENTPOS)==160);
        auto closeID=w->CurrentTab()->id,retainID=right->tabs.front()->id;
        BMessage close(kCloseTab);close.AddBool("current_tab",true);close.AddInt32("index",23);w->MessageReceived(&close);
        CHECK(!w->FindTab(closeID) && w->FindTab(retainID));w->ReopenTab();Wait(*w,[&]{return w->fPendingOpen.empty();});CHECK(w->Current()->path==e);
        // An edit followed immediately by Undo still keeps its preview open.
        Open(*w,c);CHECK(w->CloseTab(right->selected));Open(*w,c,true);CHECK(w->CurrentTab()->preview);
        auto* undone=w->CurrentTab()->editor;undone->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));undone->SendMessage(SCI_UNDO);
        w->UpdateTabs();CHECK(!w->CurrentTab()->preview && !undone->Dirty());
        CHECK(w->CloseTab(right->selected));Open(*w,c,true);Save(*w);CHECK(!w->CurrentTab()->preview);
        Open(*w,c);CHECK(w->CloseTab(right->selected));fs::remove(c);w->ReopenTab();CHECK(w->fPendingOpen.empty());CHECK(!fs::exists(c));
        w->NewFile();auto history=w->fClosedTabs.size();CHECK(w->CloseTab(w->fActivePane->selected));CHECK(w->fClosedTabs.size()==history);
        Send(*w,kPreviewTabs);CHECK(!w->fPreviewTabs);

        // Restore a nested layout with the same document in three independent views.
        w->ActivatePane(left,true);w->SplitPane(B_VERTICAL);auto middle=w->fActivePane->id;
        w->CurrentTab()->editor->SendMessage(SCI_SETSEL,250,270);w->CurrentTab()->editor->SendMessage(SCI_SETZOOM,2);
        w->CurrentTab()->editor->SendMessage(SCI_SETWRAPMODE,SC_WRAP_WORD);
        w->CurrentTab()->editor->SendMessage(SCI_SETFIRSTVISIBLELINE,70);
        w->fLayout->split->SetItemWeight(int32(0),.65f,true);w->fLayout->split->SetItemWeight(int32(1),.35f,true);
        Wait(*w,[&]{return true;});w->SaveSettings();auto expected=w->LayoutState(*w->fLayout);auto oldCount=w->fDocuments.size();
        w->Quit();w=new Workspace(settings);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring;});
        CHECK(w->fPanes.size()==3 && w->fDocuments.size()==oldCount);CHECK(w->fActivePane->id==middle);
        CHECK(w->fLayout->split->Orientation()==B_HORIZONTAL && w->fLayout->first->split->Orientation()==B_VERTICAL);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETCURRENTPOS)==270);CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETANCHOR)==250);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETZOOM)==2);CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETWRAPMODE)==SC_WRAP_WORD);
        if(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE)!=70) throw std::runtime_error("Restored first line "+std::to_string(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE))+", expected 70");
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE)==70);CHECK(!w->fPreviewTabs);
        auto restored=w->LayoutState(*w->fLayout);float before=.5,after=.5;expected.FindFloat("ratio",&before);restored.FindFloat("ratio",&after);
        if(std::abs(before-after)>=.06) throw std::runtime_error("Restored split ratio "+std::to_string(after)+", expected "+std::to_string(before));CHECK(std::abs(before-after)<.06);
        auto* restoredEditor=w->CurrentTab()->editor;auto restoredID=w->Current()->id;
        for(auto& pane:w->fPanes) for(auto& tab:pane->tabs) if(tab->document==restoredID) CHECK(tab->editor->SendMessage(SCI_GETDOCPOINTER)==restoredEditor->SendMessage(SCI_GETDOCPOINTER));

        // Recovery writes one snapshot per buffer and recreates all its views.
        restoredEditor->SendMessage(SCI_APPENDTEXT,9,reinterpret_cast<sptr_t>("recovered"));auto text=restoredEditor->Text();
        w->StartRecovery();Wait(*w,[&]{auto* d=w->ByID(restoredID);return !w->fSnapshot && !d->recovering && !d->recoveryFile.empty() && fs::exists(d->recoveryFile);});
        CHECK(ListDrafts(w->fRecoveryDirectory).size()==1);w->SaveSettings();w->Quit();
        w=new Workspace(settings);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring;});
        CHECK(w->fPanes.size()==3);CHECK(w->CurrentTab()->editor->Dirty());CHECK(w->CurrentTab()->editor->Matches(text));
        CHECK(w->ViewCount(w->Current()->id)==3);Save(*w);

        // Concurrent requests for the same path create one buffer in both panes.
        auto concurrent=root+"/concurrent.txt";{std::ofstream out(concurrent);out<<lines;}
        w->OpenFile(concurrent,1,1,true,false,w->fPanes[0]->id);
        w->OpenFile(concurrent,1,1,true,false,w->fPanes[1]->id);
        Wait(*w,[&]{return w->fPendingOpen.empty();});size_t documents=0;int64 concurrentID=0;
        for(auto& d:w->fDocuments) if(d->path==concurrent) { ++documents;concurrentID=d->id; }
        CHECK(documents==1 && w->ViewCount(concurrentID)==2);

        // Exercise actual RPC and formatting processes against the focused view.
        auto tsA=root+"/first.ts",tsB=root+"/second.ts";
        std::string sourceA="const first = obj.na;\n",sourceB="const second = obj.na;\n";
        {std::ofstream out(tsA);out<<sourceA;}{std::ofstream out(tsB);out<<sourceB;}
        w->fLanguageTools.profiles={{"Fixture","typescript","\""+executable+"\" --language-fixture",{".ts"}}};
        w->fLanguageTools.prettier="\""+executable+"\" --format-fixture";w->fLanguageTools.completion=false;w->ResetLanguages();
        Open(*w,tsA);auto* paneA=w->fActivePane;auto* editorA=w->CurrentTab()->editor;auto idA=w->Current()->id;
        w->SplitPane(B_HORIZONTAL);auto* paneB=w->fActivePane;auto* mirror=w->CurrentTab()->editor;
        Open(*w,tsB);auto* editorB=w->CurrentTab()->editor;auto idB=w->Current()->id;
        Wait(*w,[&]{return w->ByID(idA)->symbols.size()==1 && w->ByID(idB)->symbols.size()==1;});
        CHECK(w->ByID(idA)->symbols[0].name=="first.ts" && w->ByID(idB)->symbols[0].name=="second.ts");
        auto chooseSymbol=[&](int64 id) { auto* d=w->ByID(id);BMessage message(kSymbolChosen);message.AddInt64("document",id);message.AddInt64("version",d->symbolVersion);message.AddInt32("symbol",0);w->MessageReceived(&message); };
        editorB->SendMessage(SCI_GOTOPOS,10);chooseSymbol(idA);CHECK(editorB->SendMessage(SCI_GETCURRENTPOS)==10);
        chooseSymbol(idB);CHECK(editorB->SendMessage(SCI_GETCURRENTPOS)==0);
        editorB->SendMessage(SCI_GOTOPOS,sourceB.find("na;")+2);w->Complete(editorB);
        Wait(*w,[&]{return !w->fCompletionLabels.empty();});CHECK(w->fCompletionLabels[0].find("second.ts")!=std::string::npos);
        w->AcceptCompletion(editorB,w->fCompletionLabels[0]);CHECK(editorB->Text().find("obj.name")!=std::string::npos);CHECK(editorA->Matches(sourceA));
        editorB->SendMessage(SCI_UNDO);CHECK(editorB->Matches(sourceB));
        w->ActivatePane(paneA,true);editorA->SendMessage(SCI_GOTOPOS,sourceA.find("na;")+2);w->Complete(editorA);
        CHECK(w->fCompletionDocument==idA);w->ActivatePane(paneB,true);w->SelectTab(0);
        Wait(*w,[&]{return w->fCompletionDocument==0;});CHECK(!mirror->SendMessage(SCI_AUTOCACTIVE));CHECK(editorA->Matches(sourceA));
        // A late formatter applies to its original view even after focus changes.
        w->ActivatePane(paneA,true);w->FormatDocument(editorA);w->ActivatePane(paneB,true);w->SelectTab(1);
        Wait(*w,[&]{return editorA->Text().find("CONST FIRST")!=std::string::npos;});
        CHECK(mirror->Matches(editorA->Text()));CHECK(editorB->Matches(sourceB));
        editorA->SendMessage(SCI_UNDO);CHECK(mirror->Matches(sourceA));
        // Closing the requested view cancels its formatter; the other view remains.
        w->ActivatePane(paneA,true);auto formatView=w->CurrentTab()->id;w->FormatDocument(editorA);CHECK(w->CloseView(formatView));
        Wait(*w,[&]{return true;});w->Unlock();snooze(250000);CHECK(w->Lock());CHECK(mirror->Matches(sourceA));
        w->ActivatePane(paneB,true);w->SelectTab(1);Save(*w);

        // Large-file splits share storage without copying the text or reloading it.
        auto large=root+"/large.txt";{std::ofstream out(large);std::string chunk(1023,'x');chunk+='\n';for(int i=0;i<32768;++i) out<<chunk;}
        Open(*w,large);auto pointer=w->CurrentTab()->editor->SendMessage(SCI_GETDOCPOINTER);auto start=system_time();w->SplitPane(B_HORIZONTAL);
        auto elapsed=system_time()-start;CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETDOCPOINTER)==pointer);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETLENGTH)==32*1024*1024);CHECK(elapsed<2000000);
        std::cout<<"32 MiB shared split: "<<elapsed/1000.0<<" ms\n";
        // Closing an entire group collapses just that leaf and preserves others.
        auto panes=w->fPanes.size();Send(*w,kClosePane);CHECK(w->fPanes.size()==panes-1);
        CHECK(w->ViewCount(concurrentID)==2);
        // The shared Close Tab shortcut must retain terminal selection as well.
        w->fTerminal->CloseTerminals();w->NewTerminal();w->NewTerminal();
        BMessage currentTerminal(kCloseTerminal),firstTerminal(kCloseTerminal);firstTerminal.AddInt32("index",0);
        CHECK(w->fTerminal->IndexForMessage(currentTerminal)==1);CHECK(w->fTerminal->IndexForMessage(firstTerminal)==0);
        w->fTerminal->CloseTerminal(w->fTerminal->IndexForMessage(currentTerminal));
        CHECK(!w->fTerminal->Empty() && w->fTerminal->IndexForMessage(currentTerminal)==0);w->Quit();
    }
};
}
int main(int argc,char** argv) {
    signal(SIGPIPE,SIG_IGN);
    if(argc>1 && std::string(argv[1])=="--language-fixture") return kiri::LanguageFixture();
    if(argc>1 && std::string(argv[1])=="--format-fixture") {
        std::string text((std::istreambuf_iterator<char>(std::cin)),{});snooze(180000);
        std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::toupper(c);});std::cout<<text;return 0;
    }
    BApplication application("application/x-vnd.Kiri-workspace-unit-tests");
    char folder[]="/tmp/kiri-workspace-XXXXXX";auto* root=mkdtemp(folder);if(!root) return 1;
    try { kiri::WorkspaceTestAccess::Run(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));fs::remove_all(root);std::cout<<"Passed "<<checks<<" workspace checks.\n"; }
    catch(const std::exception& error) { std::cerr<<error.what()<<" (test files: "<<root<<")\n";return 1; }
}
