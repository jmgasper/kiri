#include "ui/TerminalPanel.h"
#include "ui/TabStrip.h"
#include "ui/TerminalView.h"
#include <CardLayout.h>
#include <LayoutBuilder.h>
#include <Window.h>
#include <algorithm>

namespace kiri {
TerminalPanel::TerminalPanel():BView("terminal panel",B_WILL_DRAW) {
    fTabs=new TabStrip("terminal tabs",{kSelectTerminal,kCloseTerminal,kCloseAllTerminals,kCloseOtherTerminals,kNewTerminal},"New terminal (Alt+Shift+T)");
    auto* host=new BView("terminal host",0);fLayout=new BCardLayout();host->SetLayout(fLayout);
    auto* empty=new BView("no terminal",0);empty->SetExplicitMinSize(BSize(180,80));fLayout->AddView(empty);
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(fTabs).Add(host);
}
void TerminalPanel::NewTerminal(const std::string& directory,bool focus) {
    auto* view=new TerminalView();int64 id=fNextID++;
    view->SetName(("terminal "+std::to_string(id)).c_str());
    fLayout->AddView(view);view->ApplyTheme(fTheme);
    fSessions.push_back({id,directory,view});
    view->Start(directory);SelectTerminal(fSessions.size()-1,focus);
}
void TerminalPanel::SelectTerminal(int index,bool focus) {
    if(index<0 || index>=static_cast<int>(fSessions.size())) return;
    fSelected=index;fLayout->SetVisibleItem(index+1);
    if(focus) fSessions[index].view->MakeFocus();UpdateTabs();
}
void TerminalPanel::CloseTerminal(int index) {
    if(index<0 || index>=static_cast<int>(fSessions.size())) return;
    auto* view=fSessions[index].view;
    bool focused=OwnsFocus();
    fLayout->RemoveView(view);view->RemoveSelf();delete view;
    fSessions.erase(fSessions.begin()+index);
    if(index<fSelected) --fSelected;
    if(fSessions.empty()) { fSelected=-1;fLayout->SetVisibleItem(int32(0));UpdateTabs(); }
    else SelectTerminal(std::min(fSelected,static_cast<int>(fSessions.size())-1),focused);
}
void TerminalPanel::CloseTerminals(int keep) {
    if(keep<-1 || keep>=static_cast<int>(fSessions.size())) return;
    if(keep>=0) SelectTerminal(keep);
    for(int i=static_cast<int>(fSessions.size())-1;i>=0;--i) if(i!=keep) CloseTerminal(i);
}
void TerminalPanel::UpdateTabs() {
    std::vector<TabLabel> labels;
    for(const auto& session:fSessions) {
        auto name="Terminal "+std::to_string(session.id);
        if(session.view->Exited()) name+=" · exited";
        labels.push_back({name,session.directory,false,session.id});
    }
    fTabs->SetTabs(std::move(labels),fSelected);
}
void TerminalPanel::ApplyTheme(const Theme& theme) {
    fTheme=theme;ThemeView(this,theme);fTabs->ApplyTheme(theme);
    for(auto& session:fSessions) session.view->ApplyTheme(theme);
}
int TerminalPanel::IndexForMessage(const BMessage& message) const {
    int64 id;
    if(message.FindInt64("tab_id",&id)==B_OK) {
        for(size_t i=0;i<fSessions.size();++i) if(fSessions[i].id==id) return i;
        return -1;
    }
    int32 index=fSelected;message.FindInt32("index",&index);return index;
}
bool TerminalPanel::OwnsFocus() const {
    for(auto* view=Window()?Window()->CurrentFocus():nullptr;view;view=view->Parent()) if(view==this) return true;
    return false;
}
}
