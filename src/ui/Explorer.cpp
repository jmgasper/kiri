#include "ui/Explorer.h"
#include "ui/Messages.h"
#include <Font.h>
#include <Message.h>
#include <String.h>
#include <Window.h>
namespace kiri {
FileItem::FileItem(DirectoryEntry value,uint32 level,bool isPlaceholder)
    :BListItem(level,false),entry(std::move(value)),placeholder(isPlaceholder) {
    if(!placeholder) fIcon=FileIcon(entry.path,entry.directory);
}
void FileItem::Update(BView* owner,const BFont* font) { BListItem::Update(owner,font);SetHeight(25); }
void FileItem::DrawItem(BView* owner,BRect rect,bool) {
    const auto& t=static_cast<Explorer*>(owner)->Colors();
    owner->SetHighColor(IsSelected()?t.selection:t.panel);owner->FillRect(rect);
    owner->SetLowColor(IsSelected()?t.selection:t.panel);
    float x=rect.left+5,y=rect.top+6;
    owner->SetHighColor(entry.directory?t.accent:t.muted);
    if(fIcon) {
        owner->PushState();owner->SetDrawingMode(B_OP_ALPHA);owner->SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);
        owner->DrawBitmap(fIcon.get(),BPoint(x,rect.top+4));owner->PopState();
    }
    else if(entry.directory) { owner->FillRoundRect(BRect(x,y+3,x+13,y+12),2,2);owner->FillRect(BRect(x+1,y,x+6,y+4)); }
    else if(!placeholder) { owner->StrokeRect(BRect(x+2,y,x+11,y+13));owner->StrokeLine(BPoint(x+4,y+5),BPoint(x+9,y+5)); }
    owner->SetHighColor(IsSelected()?t.selectionText:placeholder?t.muted:t.text);
    BString label(entry.name.c_str());owner->TruncateString(&label,B_TRUNCATE_MIDDLE,rect.Width()-29);
    owner->DrawString(label.String(),BPoint(x+22,rect.top+18));
}
Explorer::Explorer():BOutlineListView("files",B_SINGLE_SELECTION_LIST,B_WILL_DRAW|B_NAVIGABLE|B_FRAME_EVENTS) {
    SetExplicitMinSize(BSize(150,100));SetInvocationMessage(new BMessage(kOpenFile));
}
Explorer::~Explorer() { Clear(); }
void Explorer::Clear() {
    ClearItems();fRoot.clear();fListings.clear();fExpanded.clear();
}
void Explorer::ClearItems() {
    std::vector<BListItem*> items;
    for(int32 i=0;i<FullListCountItems();++i) items.push_back(FullListItemAt(i));
    MakeEmpty();for(auto* item:items) delete item;
}
void Explorer::AddEntries(const DirectoryResult& listing,FileItem* parent) {
    for(size_t i=0;i<listing.entries.size();++i) {
        // AddUnder inserts immediately after the parent; reverse the input to
        // keep the same directories-first ordering as the project root.
        const auto& entry=listing.entries[parent?listing.entries.size()-1-i:i];
        auto* item=new FileItem(entry,parent?parent->OutlineLevel()+1:0);
        if(parent) AddUnder(item,parent);else AddItem(item);
        if(entry.directory) {
            auto found=fListings.find(entry.path);
            if(found!=fListings.end()) { item->loaded=true;AddEntries(found->second,item); }
            else AddUnder(new FileItem({"Loading…",entry.path,false,false,{}},item->OutlineLevel()+1,true),item);
            if(fExpanded.count(entry.path)) Expand(item);
        }
    }
    if(!listing.error.empty()) {
        auto* error=new FileItem({listing.error,"",false,false,{}},parent?parent->OutlineLevel()+1:0,true);
        if(parent) AddUnder(error,parent);else AddItem(error);
    }
}
void Explorer::SetRoot(const std::string& path,const DirectoryResult& listing) { if(fRoot!=path) Clear();fRoot=path;fListings[path]=listing;Rebuild(); }
void Explorer::AddListing(const std::string& path,const DirectoryResult& listing) {
    fListings[path]=listing;Rebuild();
}
std::vector<std::string> Explorer::LoadedDirectories() const { std::vector<std::string> result;for(auto& listing:fListings) result.push_back(listing.first);return result; }
void Explorer::Rebuild() {
    fExpanded.clear();std::string selection;auto* selected=static_cast<FileItem*>(ItemAt(CurrentSelection()));if(selected) selection=selected->entry.path;
    auto scroll=Bounds().LeftTop();
    for(int32 i=0;i<FullListCountItems();++i) { auto* item=static_cast<FileItem*>(FullListItemAt(i));if(item->entry.directory && item->IsExpanded()) fExpanded.insert(item->entry.path); }
    ClearItems();auto root=fListings.find(fRoot);if(root!=fListings.end()) AddEntries(root->second,nullptr);
    for(int32 i=0;i<CountItems();++i) if(static_cast<FileItem*>(ItemAt(i))->entry.path==selection) { Select(i);break; }
    ScrollTo(scroll);Invalidate();
}
void Explorer::CheckExpanded() {
    for(int32 i=0;i<FullListCountItems();++i) {
        auto* item=static_cast<FileItem*>(FullListItemAt(i));
        if(item->entry.directory && !item->placeholder && !item->loaded && item->IsExpanded()) {
            item->loaded=true;if(requestDirectory) requestDirectory(item->entry.path);
        }
    }
}
void Explorer::OpenSelected(bool preview) {
    auto* item=static_cast<FileItem*>(ItemAt(CurrentSelection()));
    if(!item || item->placeholder) return;
    if(item->entry.directory) { if(!preview) { if(item->IsExpanded()) Collapse(item);else Expand(item);CheckExpanded(); } }
    else { BMessage msg(kOpenFile);msg.AddString("path",item->entry.path.c_str());msg.AddBool("preview",preview);Window()->PostMessage(&msg); }
}
void Explorer::MouseDown(BPoint where) {
    BOutlineListView::MouseDown(where);CheckExpanded();
    int32 clicks=1;Window()->CurrentMessage()->FindInt32("clicks",&clicks);
    int32 buttons=B_PRIMARY_MOUSE_BUTTON;Window()->CurrentMessage()->FindInt32("buttons",&buttons);
    if(buttons&B_PRIMARY_MOUSE_BUTTON) OpenSelected(clicks!=2);
}
void Explorer::KeyDown(const char* bytes,int32 count) {
    if(count==1 && (bytes[0]==B_ENTER || bytes[0]==' ')) OpenSelected();
    else { BOutlineListView::KeyDown(bytes,count);CheckExpanded();if(count==1 && (bytes[0]==B_UP_ARROW || bytes[0]==B_DOWN_ARROW)) OpenSelected(true); }
}
void Explorer::MessageReceived(BMessage* message) { BOutlineListView::MessageReceived(message); }
}
