#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/MarkdownView.h"
#include <CardLayout.h>

namespace kiri {
void Workspace::SetMarkdownPreview(Tab& tab,bool enabled) {
    auto* preview=dynamic_cast<MarkdownPane*>(tab.view);
    if(!tab.editor || bool(preview)==enabled) return;
    auto* d=ByID(tab.document);if(!d) return;
    auto* layout=tab.view->Parent()?tab.view->Parent()->GetLayout():nullptr;
    if(!layout) return;
    auto index=layout->IndexOfView(tab.view);tab.view->RemoveSelf();
    if(enabled) {
        // Removing a card hides its view. It must be visible again before it
        // becomes an item in the source/preview split.
        if(tab.editor->IsHidden()) tab.editor->Show();
        tab.view=new MarkdownPane(tab.editor,d->path,fEditorSettings);
    } else {tab.editor->RemoveSelf();delete preview;if(tab.editor->IsHidden()) tab.editor->Show();tab.view=tab.editor;}
    layout->AddView(index,tab.view);
    for(auto& pane:fPanes) if(pane->cards==layout) pane->cards->SetVisibleItem(pane->selected+1);
    RefreshRepresentative(*d);if(CurrentTab()==&tab) tab.editor->MakeFocus();
}
void Workspace::OpenMarkdownLink(const BMessage& message) {
    const char *raw=nullptr,*anchor=nullptr;if(message.FindString("path",&raw)!=B_OK) return;message.FindString("fragment",&anchor);
    std::string path=CanonicalPath(raw),fragment=anchor?anchor:"";auto paneID=fActivePane->id;
    auto finish=[this,path,fragment,paneID] {
        auto* pane=FindPane(paneID);if(!pane) return;
        for(auto& tab:pane->tabs) if(auto* d=ByID(tab->document);d && d->path==path && tab->editor) {
            if(!fragment.empty() && tab->editor->SendMessage(SCI_GETLENGTH)<=sptr_t(kMarkdownBytes)) {
                auto document=ParseMarkdown(tab->editor->Text());
                for(const auto& block:document.blocks) if(block.type==MD_BLOCK_H && block.anchor==fragment) {tab->editor->GoTo(block.firstLine+1);break;}
            }
            if(tab->editor->Language()=="Markdown") SetMarkdownPreview(*tab,true);
            UpdateTabs();SaveSettings();return;
        }
    };
    OpenFile(path,1,1,true,false,paneID);
    if(fPendingOpen.count(path)) fPendingRequests[path].push_back(std::move(finish));else finish();
}
}
