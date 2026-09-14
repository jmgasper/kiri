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
    static void CheckCards(Workspace& w) {
        for(auto& pane:w.fPanes) {
            CHECK(pane->cards->CountItems()==int(pane->tabs.size())+1);
            CHECK(pane->cards->VisibleIndex()==pane->selected+1);
            for(size_t i=0;i<pane->tabs.size();++i) {
                CHECK(pane->cards->ItemAt(i+1)->View()==pane->tabs[i]->view);
                CHECK(pane->tabs[i]->view->Window()==&w);
            }
        }
    }
    static void DragTabs(const std::string& root) {
        auto settings=root+"/drag-settings",a=root+"/one.txt",b=root+"/two.txt",c=root+"/four.txt";
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());
        Open(*w,a);auto first=w->CurrentTab()->id;auto* editor=w->CurrentTab()->editor;
        Open(*w,b);auto second=w->CurrentTab()->id;Open(*w,c,true);auto third=w->CurrentTab()->id;
        auto* left=w->fActivePane;auto history=w->fClosedTabs.size();
        CHECK(w->MoveTab(third,*left,0));CHECK(!w->CurrentTab()->preview);
        CHECK(left->tabs[0]->id==third && left->tabs[1]->id==first && left->tabs[2]->id==second);
        CHECK(w->MoveTab(third,*left,3));CHECK(left->tabs[2]->id==third);
        CHECK(w->MoveTab(first,*left,1));CHECK(left->tabs[0]->id==first); // Adjacent slot is a no-op.
        CHECK(!w->MoveTab(first,*left,-1) && !w->MoveTab(99999,*left,0));CheckCards(*w);
        editor->SendMessage(SCI_APPENDTEXT,5,reinterpret_cast<sptr_t>("drag!"));auto contents=editor->Text();
        editor->SendMessage(SCI_SETSELECTION,20,10);editor->SendMessage(SCI_ADDSELECTION,80,70);
        editor->SendMessage(SCI_SETMAINSELECTION,1);editor->SendMessage(SCI_SETZOOM,2);editor->SendMessage(SCI_SETFIRSTVISIBLELINE,35);
        CHECK(w->MoveTab(first,*left,3));CHECK(w->CurrentTab()->editor==editor && editor->Dirty());
        CHECK(editor->SendMessage(SCI_GETSELECTIONS)==2 && editor->SendMessage(SCI_GETSELECTIONNANCHOR,0)==10);
        CHECK(editor->SendMessage(SCI_GETMAINSELECTION)==1 && editor->SendMessage(SCI_GETCURRENTPOS)==80);
        CHECK(editor->SendMessage(SCI_GETFIRSTVISIBLELINE)==35);CHECK(editor->SendMessage(SCI_GETZOOM)==2);CheckCards(*w);
        w->SplitPane(B_HORIZONTAL);auto* right=w->fActivePane;auto duplicate=w->CurrentTab()->id;
        w->SplitPane(B_VERTICAL);auto* bottom=w->fActivePane;auto nested=bottom->id;
        CHECK(w->MoveTab(second,*bottom,0));CHECK(w->Current()->path==b && bottom->tabs.size()==2);CheckCards(*w);
        // A duplicate destination keeps the dragged view and never prompts for shared dirty text.
        CHECK(w->MoveTab(first,*right,1));CHECK(!w->FindTab(duplicate));CHECK(w->CurrentTab()->editor==editor);
        CHECK(w->ViewCount(w->Current()->id)==2 && editor->Matches(contents));
        CHECK(w->fClosedTabs.size()==history);CheckCards(*w);
        CHECK(w->MoveTab(bottom->tabs[1]->id,*right,0));CHECK(bottom->tabs.size()==1);
        CHECK(w->MoveTab(second,*left,1));CHECK(!w->FindPane(nested) && w->fPanes.size()==2);CheckCards(*w);
        // Moving the final tab out of a nested group retains its sibling and order on disk.
        w->SaveSettings();BFile saved((settings+"/settings").c_str(),B_READ_ONLY);BMessage session,layout,leaf,view;
        CHECK(session.Unflatten(&saved)==B_OK && session.FindMessage("editor_layout",&layout)==B_OK);
        CHECK(layout.FindMessage("first",&leaf)==B_OK && leaf.FindMessage("tab",0,&view)==B_OK);
        const char* path=nullptr;CHECK(view.FindString("path",&path)==B_OK && std::string(path)==c);

        // Tear off a unique dirty view without copying or losing its undo history.
        auto movedID=right->tabs[0]->id;auto* movedEditor=right->tabs[0]->editor;
        w->ActivatePane(right,true);w->SelectTab(0);movedEditor->SendMessage(SCI_SETSEL,10,20);
        auto pointer=movedEditor->SendMessage(SCI_GETDOCPOINTER);auto docID=w->Current()->id;
        w->StartRecovery();Wait(*w,[&]{auto* d=w->ByID(docID);return !w->fSnapshot && !d->recovering && !d->recoveryFile.empty() && fs::exists(d->recoveryFile);});
        auto oldDraft=w->Current()->recoveryFile;
        auto* detached=w->DetachTab(movedID,BPoint(180,190));CHECK(detached && !w->FindTab(movedID));
        CHECK(w->fPanes.size()==1 && !w->ByID(docID));CheckCards(*w);
        CHECK(detached->Lock());CHECK(detached->CurrentTab()->editor==movedEditor);
        CHECK(movedEditor->SendMessage(SCI_GETDOCPOINTER)==pointer && movedEditor->Matches(contents) && movedEditor->Dirty());
        CHECK(movedEditor->SendMessage(SCI_GETCURRENTPOS)==20 && movedEditor->SendMessage(SCI_GETANCHOR)==10);
        CHECK(detached->fSettings==settings && detached->fSessionDirectory!=settings);
        CHECK(!fs::exists(oldDraft) && fs::exists(detached->Current()->recoveryFile));CheckCards(*detached);
        CHECK(detached->Current()->serverKey.empty());auto detachedSession=detached->fSessionDirectory;
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        movedEditor->SendMessage(SCI_UNDO);CHECK(!movedEditor->Dirty());movedEditor->SendMessage(SCI_REDO);CHECK(movedEditor->Matches(contents));
        Save(*detached);CHECK(ReadFile(a).bytes==contents);CHECK(detached->QuitRequested());detached->Quit();
        CHECK(!fs::exists(detachedSession+"/settings"));

        // Shared source views become independent buffers in different loopers.
        w->ActivatePane(left,true);w->SelectTab(0);w->SplitPane(B_VERTICAL);auto* shared=w->CurrentTab()->editor;
        shared->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("shared"));auto sharedText=shared->Text();
        auto sharedPointer=shared->SendMessage(SCI_GETDOCPOINTER);auto sharedID=w->CurrentTab()->id;
        detached=w->DetachTab(sharedID,BPoint(200,200));CHECK(detached && !w->FindTab(sharedID));
        CHECK(detached->Lock());CHECK(detached->CurrentTab()->editor->SendMessage(SCI_GETDOCPOINTER)!=sharedPointer);
        CHECK(detached->CurrentTab()->editor->Matches(sharedText) && detached->CurrentTab()->editor->Dirty());
        auto* original=left->tabs[0]->editor;CHECK(original->Matches(sharedText));
        detached->CurrentTab()->editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));CHECK(original->Matches(sharedText));
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        auto recoverySession=detached->fSessionDirectory;detached->Current()->lastRecovery=0;detached->StartRecovery();
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        auto recoveredText=detached->CurrentTab()->editor->Text();detached->SaveSettings();detached->Quit();
        detached=new Workspace(settings,true,recoverySession);detached->Show();CHECK(detached->Lock());Wait(*detached,[&]{return !detached->fRestoring;});
        CHECK(detached->fDocuments.size()==1 && detached->CurrentTab()->editor->Matches(recoveredText));
        CHECK(detached->CurrentTab()->editor->Dirty());Save(*detached);CHECK(detached->QuitRequested());detached->Quit();
        original->SendMessage(SCI_UNDO);CHECK(!original->Dirty());

        // Unsaved, unnamed documents can be detached, saved and undone too.
        w->NewFile();auto untitled=w->CurrentTab()->id;auto* draft=w->CurrentTab()->editor;
        draft->SendMessage(SCI_APPENDTEXT,5,reinterpret_cast<sptr_t>("draft"));
        detached=w->DetachTab(untitled,BPoint(220,210));CHECK(detached && !w->FindTab(untitled));CHECK(detached->Lock());
        CHECK(detached->Current()->path.empty() && detached->CurrentTab()->editor==draft && draft->Dirty());
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        detached->SaveTo(detached->Current()->id,root+"/detached-draft.txt");Wait(*detached,[&]{return !detached->Current()->saving;});
        CHECK(ReadFile(root+"/detached-draft.txt").bytes=="draft");CHECK(detached->QuitRequested());detached->Quit();
        // A mouse release during an asynchronous save is fulfilled when the
        // callback has updated the disk stamp, without requiring another drag.
        Open(*w,b);auto deferred=w->CurrentTab()->id;auto* saving=w->Current();
        saving->editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));w->Save(saving);
        CHECK(saving->saving && !w->DetachTab(deferred,BPoint(200,200)) && w->fDetachTab==deferred);
        Wait(*w,[&]{return !w->FindTab(deferred);});detached=nullptr;
        for(int32 i=0;i<be_app->CountWindows();++i) {
            auto* candidate=dynamic_cast<Workspace*>(be_app->WindowAt(i));if(!candidate || candidate==w) continue;
            if(candidate->Lock()) {
                if(candidate->Current() && candidate->Current()->path==b) { detached=candidate;break; }
                candidate->Unlock();
            }
        }
        CHECK(detached && !detached->Current()->saving && !detached->CurrentTab()->editor->Dirty());
        CHECK(detached->Current()->stamp==StatFile(b));CHECK(detached->QuitRequested());detached->Quit();
        // Image views and bounded binary previews use the same move path.
        auto binary=root+"/binary.bin";{std::ofstream out(binary,std::ios::binary);out.write("a\0b",3);}
        Open(*w,binary);auto binaryID=w->CurrentTab()->id;auto* hex=w->CurrentTab()->editor;
        CHECK(hex && hex->SendMessage(SCI_GETREADONLY));detached=w->DetachTab(binaryID,BPoint(210,210));CHECK(detached && detached->Lock());
        CHECK(detached->CurrentTab()->editor==hex && hex->SendMessage(SCI_GETREADONLY));CHECK(detached->QuitRequested());detached->Quit();
        auto image=CanonicalPath("resources/branding/kiri-icon-256.png");Open(*w,image);auto imageID=w->CurrentTab()->id;auto* imageView=w->CurrentTab()->view;
        CHECK(!w->CurrentTab()->editor);detached=w->DetachTab(imageID,BPoint(210,210));CHECK(detached && detached->Lock());
        CHECK(detached->CurrentTab()->view==imageView && !detached->CurrentTab()->editor);CHECK(detached->QuitRequested());detached->Quit();
        CHECK(w->QuitRequested());w->Quit();
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
    try { kiri::WorkspaceTestAccess::Run(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));kiri::WorkspaceTestAccess::DragTabs(kiri::CanonicalPath(root));fs::remove_all(root);std::cout<<"Passed "<<checks<<" workspace checks.\n"; }
    catch(const std::exception& error) { std::cerr<<error.what()<<" (test files: "<<root<<")\n";return 1; }
}
