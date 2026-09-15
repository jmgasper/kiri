#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/PreviewView.h"
#include "ui/TabStrip.h"
#include <CardLayout.h>
#include <FilePanel.h>
#include <MessageRunner.h>
#include <SplitView.h>
#include <algorithm>
#include <filesystem>

namespace kiri {
std::unique_ptr<Workspace::Tab> Workspace::TakeTab(Pane& pane,int index) {
    auto tab=std::move(pane.tabs[index]);tab->view->RemoveSelf();
    pane.tabs.erase(pane.tabs.begin()+index);
    pane.selected=pane.tabs.empty()?-1:std::clamp(pane.selected-(index<pane.selected?1:0),0,int(pane.tabs.size())-1);
    pane.cards->SetVisibleItem(pane.selected+1);
    return tab;
}

bool Workspace::MoveTab(int64 id,Pane& destination,int slot) {
    Pane* source=nullptr;auto* tab=FindTab(id,&source);
    if(!tab || slot<0 || slot>int(destination.tabs.size())) return false;
    auto* document=ByID(tab->document);auto state=ViewState(*tab);CancelCompletion();
    int index=std::find_if(source->tabs.begin(),source->tabs.end(),[&](const auto& t){return t->id==id;})-source->tabs.begin();
    if(source==&destination && (slot==index || slot==index+1)) {
        tab->preview=false;ActivatePane(source);SelectTab(index);SaveSettings();return true;
    }
    // One file per pane: retain the dragged view (and its position) when the
    // destination already displays this buffer. Neither view is a closed file.
    if(source!=&destination) for(size_t i=0;i<destination.tabs.size();++i) if(destination.tabs[i]->document==tab->document) {
        auto duplicate=TakeTab(destination,i);
        if(document->formatView==duplicate->id) { ++document->formatSerial;fJobs->Cancel("format-"+std::to_string(document->id)); }
        delete duplicate->view;if(int(i)<slot) --slot;break;
    }
    auto moved=TakeTab(*source,index);if(source==&destination && index<slot) --slot;
    moved->preview=false;destination.cards->AddView(slot+1,moved->view);
    destination.tabs.insert(destination.tabs.begin()+slot,std::move(moved));
    destination.selected=slot;destination.cards->SetVisibleItem(slot+1);
    RefreshRepresentative(*document);ActivatePane(&destination,true);
    if(source!=&destination) RemoveEmptyPane(source);
    UpdateIfNeeded();RestoreView(*destination.tabs[slot],state);SaveSettings();return true;
}

Workspace* Workspace::DetachTab(int64 id,BPoint screenPoint) {
    if(fApplyingEdit) { Notice("Wait for the project edit to finish before detaching files.");return nullptr; }
    Pane* source=nullptr;auto* tab=FindTab(id,&source);if(!tab) return nullptr;
    auto* document=ByID(tab->document);
    if(EditPathBusy(document->path)) { Notice("Wait for the project edit to finish before detaching this file.");return nullptr; }
    if(fQuitWhenSaved || document->closeAfterSave || (fSavePanel && fSavePanel->IsShowing() && fSavePanelID==document->id)) {
        Notice("Finish saving this document before moving it to a new window.");return nullptr;
    }
    // Save callbacks and recovery writes belong to this window. Complete the
    // drop as soon as they finish, keeping the live view here in the meantime.
    if(fRestoring || document->saving || document->recovering) {
        fDetachTab=id;fDetachPoint=screenPoint;BMessage ready(kDetachTabReady);
        fDetachTimer=std::make_unique<BMessageRunner>(BMessenger(this),&ready,50000,1);
        Notice("Moving tab after the current save finishes…");return nullptr;
    }
    auto state=ViewState(*tab);bool last=ViewCount(document->id)==1;
    std::unique_ptr<Document> detached;
    std::unique_ptr<Editor> copy;
    // Scintilla documents are confined to a window's looper. A split that
    // remains in the source gets independent text in the detached window.
    // A sole view can transfer its entire buffer, including the undo stack.
    if(!last) {
        detached=std::make_unique<Document>();detached->path=document->path;detached->name=document->name;
        detached->stamp=document->stamp;detached->bom=document->bom;detached->external=document->external;
        detached->externalObserved=document->externalObserved;detached->externalChange=document->externalChange;
        if(document->externalWindow.IsValid()) document->externalWindow.SendMessage(B_QUIT_REQUESTED);
        if(tab->editor) {
            copy=std::make_unique<Editor>();
            auto text=tab->editor->Text();copy->SetLanguage(document->path,text.size()>8*1024*1024);
            copy->SetText(text,tab->editor->SendMessage(SCI_GETREADONLY),tab->editor->SendMessage(SCI_GETEOLMODE));
            if(!copy->Matches(text)) { Notice("Not enough memory to move this tab to a new window.");return nullptr; }
            if(tab->editor->Dirty()) copy->MarkRecovered();
        }
    }
    auto directory=fSettings+"/windows/"+fSessionToken+"-"+std::to_string(system_time());
    auto* window=new Workspace(fSettings,false,directory); // Locked until Show().
    window->fEditorSettings=fEditorSettings;
    if(window->fPreviewTabs!=fPreviewTabs) { BMessage preview(kPreviewTabs);window->MessageReceived(&preview); }
    window->ResizeTo(std::min(Frame().Width(),950.f),std::min(Frame().Height(),700.f));
    window->MoveTo(screenPoint-BPoint(80,50));window->MoveOnScreen();
    if(!fProject.empty()) window->OpenProject(fProject);
    window->fSidebarSplit->SetItemCollapsed(0,true);window->fTerminalSplit->SetItemCollapsed(1,true);
    CancelCompletion();
    if(document->formatView==id) { ++document->formatSerial;fJobs->Cancel("format-"+std::to_string(document->id)); }
    if(last) {
        CloseLanguage(*document);
        if(fSnapshot && fSnapshot->id==document->id) { fSnapshot.reset();fRecoveryTimer.reset(); }
        if(!tab->editor || !tab->editor->Dirty()) { ClearRecovery(*document);document->recoveryFile.clear(); }
        // Carry an existing crash snapshot across without deleting it first.
        if(!document->recoveryFile.empty()) {
            auto file=window->fRecoveryDirectory+"/"+window->fSessionToken+".draft";
            std::error_code error;
            if(std::filesystem::exists(document->recoveryFile,error)) {
                std::filesystem::rename(document->recoveryFile,file,error);
                if(error) { window->QuitRequested();window->Quit();Notice("Could not move the recovery snapshot: "+error.message());return nullptr; }
            }
            document->recoveryFile=file;
        }
        auto found=std::find_if(fDocuments.begin(),fDocuments.end(),[&](const auto& d){return d.get()==document;});
        detached=std::move(*found);fDocuments.erase(found);
    }
    int index=std::find_if(source->tabs.begin(),source->tabs.end(),[&](const auto& t){return t->id==id;})-source->tabs.begin();
    auto moved=TakeTab(*source,index);
    if(copy) { delete moved->view;moved->editor=copy.release();moved->view=moved->editor; }
    detached->id=window->fNextID++;detached->view=moved->view;detached->editor=moved->editor;
    detached->buffer=moved->editor?moved->editor->State():nullptr;
    detached->lastRecovery=0;detached->recoveryRevision=-1;detached->formatView=0;detached->closeView=0;
    moved->id=window->fNextView++;moved->document=detached->id;moved->preview=false;
    window->fDocuments.push_back(std::move(detached));auto* destination=window->fActivePane;
    destination->cards->AddView(moved->view);destination->tabs.push_back(std::move(moved));
    destination->selected=0;destination->cards->SetVisibleItem(int32(1));
    if(!last) RefreshRepresentative(*document);
    RemoveEmptyPane(source);ActivatePane(fActivePane,true);SaveSettings();
    window->ApplyTheme();window->ActivatePane(destination,true);
    window->UpdateIfNeeded();window->RestoreView(*window->CurrentTab(),state);
    window->StartRecovery();window->SaveSettings();window->Show();
    return window;
}

void Workspace::TrackTabDrag(BMessage& message) {
    fDetachTimer.reset();fDetachTab=0;
    for(auto& pane:fPanes) pane->strip->SetDropIndex(-1);
    if(message.what==kTabDragCancel) return;
    int64 id=0;BPoint point;
    if(message.FindInt64("tab_id",&id)!=B_OK || message.FindPoint("screen_point",&point)!=B_OK) return;
    Pane* source=nullptr;if(!FindTab(id,&source)) return;
    for(auto& pane:fPanes) {
        if(pane->panel->IsHidden() || !pane->panel->ConvertToScreen(pane->panel->Bounds()).Contains(point)) continue;
        auto local=pane->strip->ConvertFromScreen(point);bool strip=pane->strip->Bounds().Contains(local);
        // Returning to the source editor body leaves its order alone.
        if(!strip && pane.get()==source) return;
        int slot=strip?pane->strip->DropIndex(local,message.what==kTabDragUpdate):int(pane->tabs.size());
        if(message.what==kTabDragEnd) MoveTab(id,*pane,slot);else pane->strip->SetDropIndex(slot);
        return;
    }
    if(message.what==kTabDragEnd && !DecoratorFrame().Contains(point)) DetachTab(id,point);
}
}
