#include "ui/PreviewView.h"
#include <Font.h>
#include <Message.h>
#include <Window.h>
#include <algorithm>
#include <cmath>
namespace kiri {
PreviewView::PreviewView(std::unique_ptr<BBitmap> bitmap,std::string name,uint64_t bytes)
    :BView("image preview",B_WILL_DRAW|B_FULL_UPDATE_ON_RESIZE),fBitmap(std::move(bitmap)),fName(std::move(name)),fBytes(bytes) {
    SetExplicitMinSize(BSize(100,100));SetViewColor(fTheme.background);
    SetToolTip("Double-click for Fit / Actual Size. Drag to pan at Actual Size.");
}
void PreviewView::Draw(BRect update) {
    SetHighColor(fTheme.background);FillRect(update);
    if(!fBitmap || fBitmap->InitCheck()!=B_OK) return;
    BRect source=fBitmap->Bounds();
    float width=source.Width()+1,height=source.Height()+1;
    float scale=fActualSize?1.f:std::min(1.f,std::min((Bounds().Width()-64)/width,(Bounds().Height()-100)/height));
    scale=std::max(.001f,scale);
    BRect destination(0,0,width*scale-1,height*scale-1);
    destination.OffsetTo(floorf((Bounds().Width()-destination.Width())/2),floorf((Bounds().Height()-destination.Height()-30)/2));
    if(fActualSize) destination.OffsetBy(fPan);
    // Checkerboard makes alpha visible without flattening the decoded bitmap.
    PushState();ClipToRect(destination);
    BRect visible=destination & Bounds();
    float startX=destination.left+floorf((visible.left-destination.left)/12)*12,startY=destination.top+floorf((visible.top-destination.top)/12)*12;
    for(float y=startY;y<=visible.bottom;y+=12) for(float x=startX;x<=visible.right;x+=12) {
        bool alternate=(static_cast<int>((x-destination.left)/12)+static_cast<int>((y-destination.top)/12))%2;
        SetHighColor(alternate?rgb_color{185,189,196,255}:rgb_color{220,223,228,255});FillRect(BRect(x,y,x+11,y+11));
    }
    SetDrawingMode(B_OP_ALPHA);SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);
    DrawBitmap(fBitmap.get(),source,destination,B_FILTER_BITMAP_BILINEAR);PopState();
    SetHighColor(fTheme.border);StrokeRect(destination);
    std::string label=std::to_string(static_cast<int>(width))+" × "+std::to_string(static_cast<int>(height))+" px   ·   "+std::to_string(fBytes/1024)+" KiB   ·   "+std::to_string(static_cast<int>(scale*100))+"%";
    SetHighColor(fTheme.muted);SetLowColor(fTheme.background);
    DrawString(label.c_str(),BPoint((Bounds().Width()-StringWidth(label.c_str()))/2,Bounds().bottom-27));
}
void PreviewView::MouseDown(BPoint point) {
    int32 clicks=1;Window()->CurrentMessage()->FindInt32("clicks",&clicks);
    if(clicks==2) { fActualSize=!fActualSize;fPan=BPoint();Invalidate(); }
    else if(fActualSize) { fDragging=true;fDragPoint=point;SetMouseEventMask(B_POINTER_EVENTS,B_LOCK_WINDOW_FOCUS); }
}
void PreviewView::MouseMoved(BPoint point,uint32,const BMessage*) {
    if(!fDragging || !fBitmap) return;
    fPan+=point-fDragPoint;fDragPoint=point;
    float dx=std::max(0.f,(fBitmap->Bounds().Width()-Bounds().Width()+64)/2),dy=std::max(0.f,(fBitmap->Bounds().Height()-Bounds().Height()+100)/2);
    fPan.x=std::clamp(fPan.x,-dx,dx);fPan.y=std::clamp(fPan.y,-dy,dy);Invalidate();
}
void PreviewView::MouseUp(BPoint) { fDragging=false; }
void PreviewView::MessageReceived(BMessage* message) { BView::MessageReceived(message); }
}
