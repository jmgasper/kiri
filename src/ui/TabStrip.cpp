#include "ui/TabStrip.h"
#include "ui/Messages.h"
#include <String.h>
#include <Window.h>
#include <algorithm>
namespace kiri {
TabStrip::TabStrip():BView("documents",B_WILL_DRAW|B_FULL_UPDATE_ON_RESIZE) {
    SetExplicitMinSize(BSize(80,34));SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,34));
}
float TabStrip::Width(size_t index) const { return std::clamp(StringWidth(fTabs[index].name.c_str())+54,100.f,240.f); }
void TabStrip::SetTabs(std::vector<TabLabel> tabs,int selected) {
    fTabs=std::move(tabs);fSelected=selected;
    float x=0;for(int i=0;i<selected;++i) x+=Width(i);
    if(x<fOffset) fOffset=x;
    if(selected>=0 && x+Width(selected)>fOffset+Bounds().Width()) fOffset=std::max(0.f,x+Width(selected)-Bounds().Width());
    Invalidate();
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
        BString name(fTabs[i].name.c_str());TruncateString(&name,B_TRUNCATE_MIDDLE,width-50);DrawString(name.String(),BPoint(x+14,22));
        if(fTabs[i].dirty) FillEllipse(BPoint(x+width-17,17),3,3);
        else { StrokeLine(BPoint(x+width-21,13),BPoint(x+width-13,21));StrokeLine(BPoint(x+width-13,13),BPoint(x+width-21,21)); }
        x+=width;
    }
    SetHighColor(fTheme.border);StrokeLine(Bounds().LeftBottom(),Bounds().RightBottom());
}
void TabStrip::MouseDown(BPoint where) {
    float x=-fOffset;
    for(size_t i=0;i<fTabs.size();++i) {
        float width=Width(i);
        if(where.x>=x && where.x<x+width) {
            BMessage msg(where.x>x+width-30?kCloseTab:kSelectTab);msg.AddInt32("index",i);Window()->PostMessage(&msg);return;
        }
        x+=width;
    }
}
void TabStrip::MessageReceived(BMessage* message) {
    if(message->what==B_MOUSE_WHEEL_CHANGED) {
        float dy=0,dx=0;message->FindFloat("be:wheel_delta_y",&dy);message->FindFloat("be:wheel_delta_x",&dx);
        float total=0;for(size_t i=0;i<fTabs.size();++i) total+=Width(i);
        fOffset=std::clamp(fOffset+(dy+dx)*50,0.f,std::max(0.f,total-Bounds().Width()));Invalidate();
    } else BView::MessageReceived(message);
}
}
