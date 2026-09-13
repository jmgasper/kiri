#include "ui/TabStrip.h"
#include "ui/Messages.h"
#include <String.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>
#include <algorithm>
namespace kiri {
TabStrip::TabStrip(const char* name,TabActions actions,const char* createLabel)
    :BView(name,B_WILL_DRAW|B_FULL_UPDATE_ON_RESIZE|B_FRAME_EVENTS),fActions(actions),fCreateLabel(createLabel) {
    SetExplicitMinSize(BSize(80,34));SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,34));
}
float TabStrip::Width(size_t index) const { return std::clamp(StringWidth(fTabs[index].name.c_str())+(fTabs[index].icon?74:54),100.f,260.f); }
float TabStrip::AvailableWidth() const { return std::max(0.f,Bounds().Width()-(fActions.create?34:0)); }
BRect TabStrip::NewButtonRect() const {
    float total=0;for(size_t i=0;i<fTabs.size();++i) total+=Width(i);
    float x=std::clamp(total-fOffset,0.f,AvailableWidth());
    return BRect(x,0,x+33,Bounds().bottom);
}
int TabStrip::HitTab(BPoint where) const {
    if(!Bounds().Contains(where) || (fActions.create && NewButtonRect().Contains(where))) return -1;
    float x=-fOffset;
    for(size_t i=0;i<fTabs.size();++i) {
        float width=Width(i);if(where.x>=x && where.x<x+width) return i;x+=width;
    }
    return -1;
}
void TabStrip::SetTabs(std::vector<TabLabel> tabs,int selected) {
    fTabs=std::move(tabs);fSelected=selected>=0 && selected<static_cast<int>(fTabs.size())?selected:-1;
    EnsureSelectedVisible();Invalidate();
}
void TabStrip::EnsureSelectedVisible() {
    float total=0;for(size_t i=0;i<fTabs.size();++i) total+=Width(i);
    fOffset=std::clamp(fOffset,0.f,std::max(0.f,total-AvailableWidth()));
    float x=0;for(int i=0;i<fSelected;++i) x+=Width(i);
    if(x<fOffset) fOffset=x;
    if(fSelected>=0 && x+Width(fSelected)>fOffset+AvailableWidth()) fOffset=std::max(0.f,x+Width(fSelected)-AvailableWidth());
}
void TabStrip::FrameResized(float,float) {
    EnsureSelectedVisible();Invalidate();
}
void TabStrip::Draw(BRect) {
    SetHighColor(fTheme.toolbar);FillRect(Bounds());
    float x=-fOffset;
    for(size_t i=0;i<fTabs.size();++i) {
        float width=Width(i);BRect rect(x,0,x+width-1,Bounds().bottom);
        bool selected=static_cast<int>(i)==fSelected;
        SetHighColor(selected?fTheme.background:fTheme.toolbar);FillRect(rect);
        if(selected) { SetHighColor(fTheme.accent);FillRect(BRect(x,0,x+width-1,2)); }
        SetHighColor(fTheme.border);StrokeLine(rect.RightTop(),rect.RightBottom());
        SetLowColor(selected?fTheme.background:fTheme.toolbar);SetHighColor(selected?fTheme.text:fTheme.muted);
        float labelX=x+14;
        if(fTabs[i].icon) {
            PushState();SetDrawingMode(B_OP_ALPHA);SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);
            DrawBitmap(fTabs[i].icon.get(),BPoint(x+10,9));PopState();labelX+=20;
        }
        BString name(fTabs[i].name.c_str());TruncateString(&name,B_TRUNCATE_MIDDLE,width-(fTabs[i].icon?70:50));DrawString(name.String(),BPoint(labelX,22));
        if(fTabs[i].dirty) FillEllipse(BPoint(x+width-17,17),3,3);
        else { StrokeLine(BPoint(x+width-21,13),BPoint(x+width-13,21));StrokeLine(BPoint(x+width-13,13),BPoint(x+width-21,21)); }
        x+=width;
    }
    SetHighColor(fTheme.border);StrokeLine(Bounds().LeftBottom(),Bounds().RightBottom());
    if(fActions.create) {
        BRect button=NewButtonRect();SetHighColor(fTheme.toolbar);FillRect(button);
        BPoint center((button.left+button.right)/2,17);
        SetHighColor(fTheme.muted);SetPenSize(1.5);
        StrokeLine(center+BPoint(-5,0),center+BPoint(5,0));
        StrokeLine(center+BPoint(0,-5),center+BPoint(0,5));SetPenSize(1);
        SetHighColor(fTheme.border);StrokeLine(button.LeftBottom(),button.RightBottom());
    }
}
void TabStrip::MouseDown(BPoint where) {
    int32 buttons=B_PRIMARY_MOUSE_BUTTON;Window()->CurrentMessage()->FindInt32("buttons",&buttons);
    int index=HitTab(where);
    if(buttons&B_SECONDARY_MOUSE_BUTTON) { if(index>=0) ContextMenu(where,index);return; }
    if(!(buttons&B_PRIMARY_MOUSE_BUTTON)) return;
    if(fActions.create && NewButtonRect().Contains(where)) { SendAction(fActions.create,-1);return; }
    if(index<0) return;
    float x=-fOffset;
    for(int i=0;i<index;++i) x+=Width(i);
    SendAction(where.x>x+Width(index)-30?fActions.close:fActions.select,index);
}
void TabStrip::MouseMoved(BPoint where,uint32 transit,const BMessage*) {
    if(transit==B_EXITED_VIEW) { SetToolTip("");return; }
    if(fActions.create && NewButtonRect().Contains(where)) { SetToolTip(fCreateLabel.c_str());return; }
    int index=HitTab(where);if(index<0) { SetToolTip("");return; }
    float right=-fOffset;for(int i=0;i<=index;++i) right+=Width(i);
    std::string tip=where.x>right-30?"Close "+fTabs[index].name:fTabs[index].tooltip;
    SetToolTip(tip.c_str());
}
void TabStrip::SendAction(uint32 command,int index) {
    if(!command || !Window()) return;
    BMessage message(command);
    if(index>=0) { message.AddInt32("index",index);message.AddInt64("tab_id",fTabs[index].id); }
    Window()->PostMessage(&message);
}
void TabStrip::ContextMenu(BPoint where,int index) {
    BPopUpMenu menu("tab actions",false,false);
    auto add=[&](const char* label,uint32 command) {
        auto* message=new BMessage(command);message->AddInt32("index",index);message->AddInt64("tab_id",fTabs[index].id);
        auto* item=new BMenuItem(label,message);menu.AddItem(item);return item;
    };
    add("Close all",fActions.closeAll);
    add("Close others",fActions.closeOthers)->SetEnabled(fTabs.size()>1);
    menu.SetTargetForItems(Window());menu.Go(ConvertToScreen(where),true,true);
}
void TabStrip::MessageReceived(BMessage* message) {
    if(message->what==B_MOUSE_WHEEL_CHANGED) {
        float dy=0,dx=0;message->FindFloat("be:wheel_delta_y",&dy);message->FindFloat("be:wheel_delta_x",&dx);
        float total=0;for(size_t i=0;i<fTabs.size();++i) total+=Width(i);
        fOffset=std::clamp(fOffset+(dy+dx)*50,0.f,std::max(0.f,total-AvailableWidth()));Invalidate();
    } else BView::MessageReceived(message);
}
}
