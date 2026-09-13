#include "ui/RefreshButton.h"
#include <Window.h>

namespace kiri {
RefreshButton::RefreshButton(BMessage* message)
    :BControl("refresh","Refresh files",message,B_WILL_DRAW|B_NAVIGABLE) {
    SetExplicitMinSize(BSize(24,24));
    SetExplicitMaxSize(BSize(24,24));
    SetToolTip("Refresh files");
}
void RefreshButton::ApplyTheme(const Theme& theme) {
    fTheme=theme;SetViewColor(theme.panel);Invalidate();
}
void RefreshButton::Draw(BRect) {
    SetHighColor(fTheme.panel);FillRect(Bounds());
    if(fHovered || fPressed) {
        SetHighColor(fPressed?fTheme.selection:fTheme.toolbar);
        FillRoundRect(Bounds().InsetByCopy(1,1),4,4);
    }
    SetHighColor(IsEnabled()?fTheme.muted:fTheme.border);SetPenSize(1.5);
    BPoint center(Bounds().Width()/2,Bounds().Height()/2);
    StrokeArc(center,6,6,35,290);
    BPoint tip(center.x+5,center.y-4);
    StrokeLine(tip+BPoint(-5,0),tip);StrokeLine(tip,tip+BPoint(0,-5));
    SetPenSize(1);
    if(IsFocus()) { SetHighColor(fTheme.accent);StrokeRoundRect(Bounds().InsetByCopy(1,1),4,4); }
}
void RefreshButton::MouseDown(BPoint) {
    int32 buttons=B_PRIMARY_MOUSE_BUTTON;Window()->CurrentMessage()->FindInt32("buttons",&buttons);
    if(!IsEnabled() || !(buttons&B_PRIMARY_MOUSE_BUTTON)) return;
    fPressed=true;SetMouseEventMask(B_POINTER_EVENTS,B_LOCK_WINDOW_FOCUS);Invalidate();
}
void RefreshButton::MouseUp(BPoint where) {
    bool invoke=fPressed && IsEnabled() && Bounds().Contains(where);
    fPressed=false;Invalidate();if(invoke) Invoke();
}
void RefreshButton::MouseMoved(BPoint where,uint32,const BMessage*) {
    bool hovered=Bounds().Contains(where);
    if(hovered!=fHovered) { fHovered=hovered;Invalidate(); }
}
void RefreshButton::KeyDown(const char* bytes,int32 count) {
    if(IsEnabled() && count==1 && (bytes[0]==B_ENTER || bytes[0]==' ')) Invoke();
    else BControl::KeyDown(bytes,count);
}
}
