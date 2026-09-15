#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/FileIcons.h"
#include "ui/PreviewView.h"
#include "ui/SymbolBar.h"
#include "ui/TabStrip.h"
#include <Alert.h>
#include <CardLayout.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <StringView.h>
#include <algorithm>
#include <functional>
#include <cmath>

namespace kiri {
BView* Workspace::LayoutNode::View() const { return pane?pane->panel:split; }
Workspace::Pane* Workspace::CreatePane() {
    auto pane=std::make_unique<Pane>();pane->id=fNextPane++;
    pane->strip=new TabStrip("documents",{kSelectTab,kCloseTab,kCloseAllTabs,kCloseOtherTabs,kNewFile},"New file");pane->strip->SetPane(pane->id);
    auto* host=new BView("document host",0);pane->cards=new BCardLayout();host->SetLayout(pane->cards);
    auto* welcome=new BView("welcome",B_WILL_DRAW);
    BLayoutBuilder::Group<>(welcome,B_VERTICAL,12).SetInsets(12).AddGlue()
        .Add(new BStringView("welcome title","Kiri"))
        .Add(new BStringView("welcome help","Open a file or create a new tab."))
        .AddGlue();
    pane->cards->AddView(welcome);pane->panel=new BView("editor pane",0);
    BLayoutBuilder::Group<>(pane->panel,B_VERTICAL,0).Add(pane->strip).Add(host);
    ThemeView(pane->panel,Theme::Builtins()[fEditorSettings.theme]);pane->strip->ApplyTheme(Theme::Builtins()[fEditorSettings.theme]);
    auto* result=pane.get();fPanes.push_back(std::move(pane));return result;
}
Workspace::Pane* Workspace::FindPane(int64 id) { for(auto& pane:fPanes) if(pane->id==id) return pane.get();return nullptr; }
Workspace::Tab* Workspace::FindTab(int64 id,Pane** owner) {
    for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->id==id) { if(owner) *owner=pane.get();return tab.get(); }return nullptr;
}
Workspace::Tab* Workspace::CurrentTab() {
    return fActivePane && fActivePane->selected>=0 && fActivePane->selected<int(fActivePane->tabs.size())?fActivePane->tabs[fActivePane->selected].get():nullptr;
}
Workspace::Document* Workspace::Current() {
    auto* tab=CurrentTab();auto* document=tab?ByID(tab->document):nullptr;
    if(document) { document->editor=tab->editor;document->view=tab->view; }return document;
}
Workspace::Document* Workspace::ByEditor(const void* editor) {
    if(!editor) return nullptr;
    for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->editor==editor) return ByID(tab->document);return nullptr;
}
void Workspace::SyncFocus() {
    auto* focus=CurrentFocus();
    for(auto& pane:fPanes) for(auto* view=focus;view;view=view->Parent())
        if(view==pane->panel) { if(fActivePane!=pane.get()) ActivatePane(pane.get());return; }
}
void Workspace::ActivatePane(Pane* pane,bool focus) {
    if(!pane) return;
    if(pane!=fActivePane) { CancelCompletion();++fFocusSerial; }
    fActivePane=pane;Current();
    if(focus) {
        fModeLayout->SetVisibleItem(int32(0));
        if(auto* tab=CurrentTab()) tab->view->MakeFocus();else pane->strip->MakeFocus();
    }
    UpdateTabs();UpdateSymbolBar();
}
int Workspace::AddTab(Pane& pane,Document& document,bool preview,bool firstView) {
    for(size_t i=0;i<pane.tabs.size();++i) if(pane.tabs[i]->document==document.id) { if(!preview) pane.tabs[i]->preview=false;return i; }
    auto tab=std::make_unique<Tab>();tab->id=fNextView++;tab->document=document.id;tab->preview=preview;
    if(firstView) { tab->view=document.view;tab->editor=document.editor;document.buffer=document.editor?document.editor->State():nullptr; }
    else if(document.editor) {
        tab->editor=new Editor();tab->editor->ShareDocument(*document.editor);tab->view=tab->editor;
        // Initial position is copied once; subsequent selection, scrolling,
        // folding, zoom and wrap belong to this view.
        Tab source;source.editor=document.editor;RestoreView(*tab,ViewState(source));
    } else tab->view=static_cast<PreviewView*>(document.view)->Clone();
    if(tab->editor) tab->previewChanges=tab->editor->State()->changes;
    pane.cards->AddView(tab->view);pane.tabs.push_back(std::move(tab));
    if(pane.selected<0) { pane.selected=0;pane.cards->SetVisibleItem(int32(1)); }
    return pane.tabs.size()-1;
}
void Workspace::SelectTab(int index) {
    if(!fActivePane || index<0 || index>=int(fActivePane->tabs.size())) return;
    CancelCompletion();++fFocusSerial;fActivePane->selected=index;fActivePane->cards->SetVisibleItem(index+1);ActivatePane(fActivePane,true);
}
void Workspace::ShowDocument(Pane& pane,Document& d,size_t line,size_t column,bool preview,bool activate,bool focusEditor,bool firstView) {
    PromotePreviews();
    bool exists=false;for(auto& tab:pane.tabs) if(tab->document==d.id) exists=true;
    // Install the new view before removing the old preview, so replacing the
    // only tab never collapses its pane or temporarily destroys a shared buffer.
    int64 replace=0;
    if(preview && !exists) for(auto& tab:pane.tabs) if(tab->preview) { replace=tab->id;break; }
    int index=AddTab(pane,d,preview,firstView);auto id=pane.tabs[index]->id;
    if(replace) CloseView(replace,false,false);
    for(size_t i=0;i<pane.tabs.size();++i) if(pane.tabs[i]->id==id) index=i;
    if(activate) {
        pane.selected=index;pane.cards->SetVisibleItem(index+1);ActivatePane(&pane,focusEditor);
        fModeLayout->SetVisibleItem(int32(0));if(pane.tabs[index]->editor) pane.tabs[index]->editor->GoTo(line,column,focusEditor);
    }
    UpdateTabs();
}
void Workspace::PromotePreviews() {
    for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->preview && tab->editor
            && (tab->editor->Dirty() || tab->editor->State()->changes!=tab->previewChanges || ByID(tab->document)->saving)) tab->preview=false;
}
void Workspace::UpdateTabs() {
    PromotePreviews();
    for(auto& pane:fPanes) {
        std::vector<TabLabel> labels;
        for(auto& tab:pane->tabs) if(auto* d=ByID(tab->document))
            labels.push_back({d->name+(d->external?" !":""),d->path+(tab->preview?" · Preview — double-click to keep open":""),tab->editor && tab->editor->Dirty(),tab->id,FileIcon(d->path.empty()?d->name:d->path),tab->preview});
        pane->strip->SetTabs(std::move(labels),pane->selected);pane->strip->SetActive(pane.get()==fActivePane);
    }
    UpdateStatus();
}
void Workspace::KeepTab(int index) {
    if(!fActivePane || index<0 || index>=int(fActivePane->tabs.size())) return;
    fActivePane->tabs[index]->preview=false;SelectTab(index);SaveSettings();
}
void Workspace::SplitPane(orientation direction) {
    if(!fActivePane) return;
    auto* source=Current();auto* old=fActivePane;
    std::function<LayoutNode*(LayoutNode&)> locate=[&](LayoutNode& node)->LayoutNode* {
        if(node.pane) return node.pane==old?&node:nullptr;
        if(auto* found=locate(*node.first)) return found;return locate(*node.second);
    };
    auto* node=locate(*fLayout);if(!node) return;
    auto* parent=node->View()->Parent();auto* layout=parent->GetLayout();int32 index=layout->IndexOfView(node->View());
    auto* parentSplit=dynamic_cast<BSplitView*>(parent);float weight=parentSplit?parentSplit->ItemWeight(index):1;
    old->panel->RemoveSelf();auto* pane=CreatePane();
    node->pane=nullptr;node->split=new BSplitView(direction,1);node->split->SetCollapsible(false);
    node->first=std::make_unique<LayoutNode>();node->first->pane=old;node->second=std::make_unique<LayoutNode>();node->second->pane=pane;
    node->split->AddChild(old->panel,1.f);node->split->AddChild(pane->panel,1.f);layout->AddView(index,node->split);
    if(parentSplit) parentSplit->SetItemWeight(index,weight,true);
    ThemeView(node->split,Theme::Builtins()[fEditorSettings.theme]);
    if(source) { if(auto* tab=CurrentTab()) tab->preview=false;AddTab(*pane,*source); }
    ActivatePane(pane,true);SaveSettings();
}
void Workspace::CollectPanes(LayoutNode& node,std::vector<Pane*>& panes) {
    if(node.pane) panes.push_back(node.pane);else { CollectPanes(*node.first,panes);CollectPanes(*node.second,panes); }
}
void Workspace::CyclePane(int step) {
    std::vector<Pane*> panes;CollectPanes(*fLayout,panes);auto found=std::find(panes.begin(),panes.end(),fActivePane);
    ActivatePane(panes[(found-panes.begin()+panes.size()+step)%panes.size()],true);SaveSettings();
}
void Workspace::RemoveEmptyPane(Pane* pane) {
    if(!pane || !pane->tabs.empty() || fPanes.size()==1) return;
    std::function<bool(std::unique_ptr<LayoutNode>&)> remove=[&](std::unique_ptr<LayoutNode>& node) {
        if(node->pane) return false;
        bool first=node->first->pane==pane,second=node->second->pane==pane;
        if(!first && !second) return remove(node->first)||remove(node->second);
        auto survivor=std::move(first?node->second:node->first);auto* split=node->split;
        auto* layout=split->Parent()->GetLayout();int32 index=layout->IndexOfView(split);
        auto* parentSplit=dynamic_cast<BSplitView*>(split->Parent());float weight=parentSplit?parentSplit->ItemWeight(index):1;
        survivor->View()->RemoveSelf();split->RemoveSelf();delete split;node=std::move(survivor);layout->AddView(index,node->View());
        if(parentSplit) parentSplit->SetItemWeight(index,weight,true);return true;
    };
    if(!remove(fLayout)) return;
    bool active=fActivePane==pane;
    fPanes.erase(std::remove_if(fPanes.begin(),fPanes.end(),[&](const auto& p){return p.get()==pane;}),fPanes.end());
    if(active) { fActivePane=nullptr;std::vector<Pane*> panes;CollectPanes(*fLayout,panes);ActivatePane(panes.front(),true); }
}
size_t Workspace::ViewCount(int64 document) const {
    size_t count=0;for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==document) ++count;return count;
}
void Workspace::RefreshRepresentative(Document& d) {
    for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==d.id) { d.editor=tab->editor;d.view=tab->view;return; }
    d.editor=nullptr;d.view=nullptr;
}
bool Workspace::CloseTab(int index) {
    return fActivePane && index>=0 && index<int(fActivePane->tabs.size()) && CloseView(fActivePane->tabs[index]->id);
}
bool Workspace::CloseView(int64 id,bool remember,bool collapse) {
    if(fApplyingEdit) { Notice("Wait for the project edit to finish before closing files.");return false; }
    Pane* pane=nullptr;auto* tab=FindTab(id,&pane);if(!tab) return false;
    auto* d=ByID(tab->document);bool last=ViewCount(d->id)==1;
    if(d && !d->path.empty() && EditPathBusy(d->path)) { Notice("Wait for the project edit to finish before closing this file.");return false; }
    if(last && d->saving) { d->closeAfterSave=true;d->closeView=id;return false; }
    if(last && tab->editor && tab->editor->Dirty()) {
        ActivatePane(pane);SelectTab(std::find_if(pane->tabs.begin(),pane->tabs.end(),[&](const auto& t){return t->id==id;})-pane->tabs.begin());
        int32 choice=(new BAlert("Unsaved Changes",("Save changes to "+d->name+"?").c_str(),"Cancel","Discard","Save",B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();
        if(choice==0) { fCloseQueue.clear();fClosingPane=0;return false; }
        if(choice==2) { d->closeAfterSave=true;d->closeView=id;Save(d);return false; }
    }
    if(remember && !d->path.empty()) { fClosedTabs.push_back({d->path,pane->id,ViewState(*tab)});if(fClosedTabs.size()>64) fClosedTabs.pop_front(); }
    CancelCompletion();
    if(d->formatView==id) { ++d->formatSerial;fJobs->Cancel("format-"+std::to_string(d->id)); }
    if(last) { ClearRecovery(*d);CloseLanguage(*d);if(d->externalWindow.IsValid()) d->externalWindow.SendMessage(B_QUIT_REQUESTED); }
    auto found=std::find_if(pane->tabs.begin(),pane->tabs.end(),[&](const auto& t){return t->id==id;});int index=found-pane->tabs.begin();
    tab->view->RemoveSelf();delete tab->view;pane->tabs.erase(found);
    pane->selected=pane->tabs.empty()?-1:std::clamp(pane->selected-(index<pane->selected?1:0),0,int(pane->tabs.size())-1);
    pane->cards->SetVisibleItem(pane->selected+1);
    if(last) fDocuments.erase(std::remove_if(fDocuments.begin(),fDocuments.end(),[&](const auto& document){return document.get()==d;}),fDocuments.end());
    else RefreshRepresentative(*d);
    if(collapse) RemoveEmptyPane(pane);
    ActivatePane(fActivePane,remember);SaveSettings();return true;
}
void Workspace::CloseTabs(int keep) {
    if(!fActivePane || keep<-1 || keep>=int(fActivePane->tabs.size())) return;
    if(fSavePanel && fSavePanel->IsShowing()) { fSavePanel->Window()->Activate();return; }
    for(auto& document:fDocuments) document->closeAfterSave=false;
    fCloseQueue.clear();if(keep>=0) SelectTab(keep);
    for(size_t i=0;i<fActivePane->tabs.size();++i) if(int(i)!=keep) fCloseQueue.push_back(fActivePane->tabs[i]->id);
    ContinueCloseTabs();
}
void Workspace::ContinueCloseTabs() {
    while(!fCloseQueue.empty()) {
        int64 id=fCloseQueue.front();
        if(FindTab(id) && !CloseView(id)) return;
        fCloseQueue.pop_front();
    }
    if(fClosingPane) { auto* pane=FindPane(fClosingPane);fClosingPane=0;RemoveEmptyPane(pane);SaveSettings(); }
}
BMessage Workspace::ViewState(const Tab& tab) const {
    BMessage state;if(auto* editor=tab.editor) {
        state.AddInt64("caret",editor->SendMessage(SCI_GETCURRENTPOS));state.AddInt64("anchor",editor->SendMessage(SCI_GETANCHOR));
        state.AddInt64("first",editor->SendMessage(SCI_GETFIRSTVISIBLELINE));state.AddInt32("zoom",editor->SendMessage(SCI_GETZOOM));
        state.AddInt32("wrap",editor->SendMessage(SCI_GETWRAPMODE));state.AddInt64("xoffset",editor->SendMessage(SCI_GETXOFFSET));
        state.AddInt32("selection_mode",editor->SendMessage(SCI_GETSELECTIONMODE));
        state.AddInt32("main_selection",editor->SendMessage(SCI_GETMAINSELECTION));
        for(sptr_t i=0;i<editor->SendMessage(SCI_GETSELECTIONS);++i) {
            BMessage selection;selection.AddInt64("caret",editor->SendMessage(SCI_GETSELECTIONNCARET,i));
            selection.AddInt64("anchor",editor->SendMessage(SCI_GETSELECTIONNANCHOR,i));
            selection.AddInt64("caret_space",editor->SendMessage(SCI_GETSELECTIONNCARETVIRTUALSPACE,i));
            selection.AddInt64("anchor_space",editor->SendMessage(SCI_GETSELECTIONNANCHORVIRTUALSPACE,i));state.AddMessage("selection",&selection);
        }
        state.AddInt64("rectangle_caret",editor->SendMessage(SCI_GETRECTANGULARSELECTIONCARET));
        state.AddInt64("rectangle_anchor",editor->SendMessage(SCI_GETRECTANGULARSELECTIONANCHOR));
        state.AddInt64("rectangle_caret_space",editor->SendMessage(SCI_GETRECTANGULARSELECTIONCARETVIRTUALSPACE));
        state.AddInt64("rectangle_anchor_space",editor->SendMessage(SCI_GETRECTANGULARSELECTIONANCHORVIRTUALSPACE));
        for(sptr_t line=editor->SendMessage(SCI_CONTRACTEDFOLDNEXT,0);line>=0;line=editor->SendMessage(SCI_CONTRACTEDFOLDNEXT,line+1)) state.AddInt64("fold",line);
    }return state;
}
void Workspace::RestoreView(Tab& tab,const BMessage& state) {
    auto* editor=tab.editor;if(!editor) return;
    int64 caret=0,anchor=0,first=0,xoffset=0;int32 zoom=0,wrap=0;
    state.FindInt64("caret",&caret);state.FindInt64("anchor",&anchor);state.FindInt64("first",&first);
    state.FindInt32("zoom",&zoom);state.FindInt32("wrap",&wrap);state.FindInt64("xoffset",&xoffset);
    editor->SendMessage(SCI_SETWRAPMODE,std::clamp(wrap,0,3));editor->SendMessage(SCI_SETZOOM,std::clamp(zoom,-10,20));
    int64 line=0;for(int32 i=0;state.FindInt64("fold",i,&line)==B_OK;++i) if(line>=0 && line<editor->SendMessage(SCI_GETLINECOUNT)) editor->SendMessage(SCI_FOLDLINE,line,SC_FOLDACTION_CONTRACT);
    auto length=editor->SendMessage(SCI_GETLENGTH);
    editor->SendMessage(SCI_SETSEL,std::clamp<int64>(anchor,0,length),std::clamp<int64>(caret,0,length));
    int32 mode=SC_SEL_STREAM,main=0;state.FindInt32("selection_mode",&mode);state.FindInt32("main_selection",&main);
    editor->SendMessage(SCI_SETSELECTIONMODE,std::clamp(mode,0,3));
    BMessage selection;
    for(int32 i=0;i<10000 && state.FindMessage("selection",i,&selection)==B_OK;++i) {
        int64 c=0,a=0,cs=0,as=0;selection.FindInt64("caret",&c);selection.FindInt64("anchor",&a);
        selection.FindInt64("caret_space",&cs);selection.FindInt64("anchor_space",&as);
        editor->SendMessage(i?SCI_ADDSELECTION:SCI_SETSELECTION,std::clamp<int64>(c,0,length),std::clamp<int64>(a,0,length));
        editor->SendMessage(SCI_SETSELECTIONNCARETVIRTUALSPACE,i,std::max<int64>(0,cs));
        editor->SendMessage(SCI_SETSELECTIONNANCHORVIRTUALSPACE,i,std::max<int64>(0,as));
    }
    editor->SendMessage(SCI_SETMAINSELECTION,std::clamp<sptr_t>(main,0,editor->SendMessage(SCI_GETSELECTIONS)-1));
    if(mode==SC_SEL_RECTANGLE || mode==SC_SEL_THIN) {
        int64 value=0;
        if(state.FindInt64("rectangle_caret",&value)==B_OK) editor->SendMessage(SCI_SETRECTANGULARSELECTIONCARET,std::clamp<int64>(value,0,length));
        if(state.FindInt64("rectangle_anchor",&value)==B_OK) editor->SendMessage(SCI_SETRECTANGULARSELECTIONANCHOR,std::clamp<int64>(value,0,length));
        if(state.FindInt64("rectangle_caret_space",&value)==B_OK) editor->SendMessage(SCI_SETRECTANGULARSELECTIONCARETVIRTUALSPACE,std::max<int64>(0,value));
        if(state.FindInt64("rectangle_anchor_space",&value)==B_OK) editor->SendMessage(SCI_SETRECTANGULARSELECTIONANCHORVIRTUALSPACE,std::max<int64>(0,value));
    }
    editor->SendMessage(SCI_SETFIRSTVISIBLELINE,std::max<int64>(0,first));editor->SendMessage(SCI_SETXOFFSET,std::max<int64>(0,xoffset));
}
void Workspace::ReopenTab() {
    while(!fClosedTabs.empty()) {
        auto closed=std::move(fClosedTabs.back());fClosedTabs.pop_back();
        if(!StatFile(closed.path).exists) { Notice("Cannot reopen missing file: "+closed.path);return; }
        auto* pane=FindPane(closed.pane);if(!pane) pane=fActivePane;auto paneID=pane->id;
        auto restore=[this,closed,paneID] {
            auto* pane=FindPane(paneID);if(!pane) return;
            for(auto& tab:pane->tabs) if(auto* d=ByID(tab->document);d && d->path==closed.path) { tab->preview=false;RestoreView(*tab,closed.state);SaveSettings();return; }
        };
        OpenFile(closed.path,1,1,true,false,paneID);
        if(fPendingOpen.count(closed.path)) fPendingRequests[closed.path].push_back(std::move(restore));else restore();return;
    }
    Notice("No saved tabs to reopen.");
}
BMessage Workspace::LayoutState(const LayoutNode& node) const {
    BMessage state;
    if(node.pane) {
        auto& pane=*node.pane;state.AddInt64("pane",pane.id);state.AddBool("active",node.pane==fActivePane);
        int selected=-1,count=0;
        for(size_t i=0;i<pane.tabs.size();++i) {
            auto& tab=*pane.tabs[i];Document* d=nullptr;for(auto& document:fDocuments) if(document->id==tab.document) d=document.get();
            if(!d || (d->path.empty() && d->recoveryFile.empty())) continue;
            auto view=ViewState(tab);view.AddString("path",d->path.c_str());view.AddString("draft",d->recoveryFile.c_str());view.AddBool("preview",tab.preview);
            state.AddMessage("tab",&view);if(int(i)==pane.selected) selected=count;++count;
        }
        state.AddInt32("selected",selected);
    } else {
        state.AddInt32("orientation",node.split->Orientation());
        // Actual sizes reflect dragged dividers even before weights normalize.
        bool horizontal=node.split->Orientation()==B_HORIZONTAL;
        float first=horizontal?node.first->View()->Frame().Width():node.first->View()->Frame().Height();
        float second=horizontal?node.second->View()->Frame().Width():node.second->View()->Frame().Height();
        state.AddFloat("ratio",first>0 && second>0?first/(first+second):.5f);
        state.AddFloat("first_weight",node.split->ItemWeight(int32(0)));state.AddFloat("second_weight",node.split->ItemWeight(int32(1)));
        auto a=LayoutState(*node.first),b=LayoutState(*node.second);state.AddMessage("first",&a);state.AddMessage("second",&b);
    }return state;
}
std::unique_ptr<Workspace::LayoutNode> Workspace::ReadLayout(const BMessage& state,int depth) {
    auto node=std::make_unique<LayoutNode>();BMessage first,second;
    // A corrupt settings file must not recurse indefinitely or allocate
    // unbounded views; valid user layouts are retained up to 64 panes.
    if(depth<32 && fPanes.size()<64 && state.FindMessage("first",&first)==B_OK && state.FindMessage("second",&second)==B_OK) {
        int32 direction=B_HORIZONTAL;float ratio=.5;state.FindInt32("orientation",&direction);state.FindFloat("ratio",&ratio);
        node->first=ReadLayout(first,depth+1);node->second=ReadLayout(second,depth+1);
        node->split=new BSplitView(direction==B_VERTICAL?B_VERTICAL:B_HORIZONTAL,1);node->split->SetCollapsible(false);
        ratio=std::isfinite(ratio)?std::clamp(ratio,.05f,.95f):.5f;
        float firstWeight=ratio,secondWeight=1-ratio,value;
        if(state.FindFloat("first_weight",&value)==B_OK && std::isfinite(value) && value>0) firstWeight=value;
        if(state.FindFloat("second_weight",&value)==B_OK && std::isfinite(value) && value>0) secondWeight=value;
        node->split->AddChild(node->first->View(),firstWeight);node->split->AddChild(node->second->View(),secondWeight);
        // The index overload also updates Haiku's cached layouter weights.
        node->split->SetItemWeight(int32(0),firstWeight,true);node->split->SetItemWeight(int32(1),secondWeight,true);
    } else {
        auto* pane=CreatePane();node->pane=pane;int64 id=0;
        if(state.FindInt64("pane",&id)==B_OK && id>0 && !FindPane(id)) { pane->id=id;pane->strip->SetPane(id);fNextPane=std::max(fNextPane,id+1); }
        bool active=false;state.FindBool("active",&active);if(active) fActivePane=pane;
        BMessage view;const char* path=nullptr;const char* draft=nullptr;
        int32 selected=-1;state.FindInt32("selected",&selected);
        for(int32 i=0;state.FindMessage("tab",i,&view)==B_OK;++i) {
            path=nullptr;draft=nullptr;view.FindString("path",&path);view.FindString("draft",&draft);
            for(auto& d:fDocuments) if((draft && *draft && d->recoveryFile==draft) || (path && *path && d->path==path)) {
                bool preview=false;view.FindBool("preview",&preview);
                if(preview) for(auto& tab:pane->tabs) if(tab->preview) preview=false;
                int index=AddTab(*pane,*d,preview && fPreviewTabs && !(d->editor && d->editor->Dirty()));RestoreView(*pane->tabs[index],view);
                fRestoreViews[pane->tabs[index]->id]=view;
                if(i==selected) pane->selected=index;break;
            }
        }
        pane->cards->SetVisibleItem(pane->selected+1);
    }return node;
}
void Workspace::RestoreLayout() {
    if(fRestoreLayout.IsEmpty()) return;
    auto oldPanes=std::move(fPanes);auto old=std::move(fLayout);fActivePane=nullptr;
    fLayout=ReadLayout(fRestoreLayout);if(!fActivePane) fActivePane=fPanes.front().get();
    // Recovered drafts not referenced by the last session remain accessible.
    for(auto& d:fDocuments) if(!ViewCount(d->id)) AddTab(*fActivePane,*d);
    old->View()->RemoveSelf();fPaneHost->GetLayout()->AddView(fLayout->View());
    for(auto& d:fDocuments) RefreshRepresentative(*d);
    delete old->View();ThemeView(fLayout->View(),Theme::Builtins()[fEditorSettings.theme]);ActivatePane(fActivePane,true);
    // Realize the new pane sizes and wrapping before restoring scroll offsets.
    // Focusing the selected view can also scroll its caret into view.
    UpdateIfNeeded();
    std::function<void(LayoutNode&,const BMessage&)> weights=[&](LayoutNode& node,const BMessage& state) {
        if(node.pane) return;
        float a=1,b=1;state.FindFloat("first_weight",&a);state.FindFloat("second_weight",&b);
        if(std::isfinite(a) && std::isfinite(b) && a>0 && b>0) {
            node.split->SetItemWeight(int32(0),a,true);node.split->SetItemWeight(int32(1),b,true);
        }
        BMessage first,second;
        if(state.FindMessage("first",&first)==B_OK) weights(*node.first,first);
        if(state.FindMessage("second",&second)==B_OK) weights(*node.second,second);
    };
    weights(*fLayout,fRestoreLayout);fRestoreLayout.MakeEmpty();GetLayout()->LayoutItems(true);UpdateIfNeeded();
    for(auto& state:fRestoreViews) if(auto* tab=FindTab(state.first)) RestoreView(*tab,state.second);
    fRestoreViews.clear();
}
}
