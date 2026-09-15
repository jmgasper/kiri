#include "ui/MinimapView.h"
#include "ui/Editor.h"
#include <Font.h>
#include <Window.h>
#include <algorithm>
#include <cmath>

namespace kiri {
namespace {constexpr uint32 kRefresh='mmrf';constexpr int kColumns=80,kRows=1024;constexpr int64 kMaxBytes=32*1024*1024,kMaxLines=500000;}
MinimapView::MinimapView(Editor* editor):BView(BRect(0,0,95,99),"document minimap",B_FOLLOW_NONE,B_WILL_DRAW|B_FRAME_EVENTS),fEditor(editor) {Hide();}
void MinimapView::Timer() {fTimer.reset();if(fEnabled && Window()) {BMessage tick(kRefresh);fTimer=std::make_unique<BMessageRunner>(BMessenger(this),&tick,150000);}}
void MinimapView::AttachedToWindow() {BView::AttachedToWindow();Timer();}
void MinimapView::DetachedFromWindow() {fTimer.reset();BView::DetachedFromWindow();}
void MinimapView::SetEnabled(bool enabled) {
    if(fEnabled==enabled) return;fEnabled=enabled;
    if(enabled) {Show();fStale=true;}else {Hide();std::vector<uint16_t>().swap(fCells);fRows=0;fStatus.clear();fDragging=false;}
    Timer();
}
void MinimapView::ApplyTheme(const Theme& theme) {fTheme=theme;SetViewColor(theme.background);fStale=true;Invalidate();}
void MinimapView::MessageReceived(BMessage* message) {if(message->what==kRefresh) {Refresh();return;}BView::MessageReceived(message);}
void MinimapView::Refresh(bool force) {
    if(!fEnabled || !Window() || IsHidden()) return;
    auto length=fEditor->SendMessage(SCI_GETLENGTH),lines=fEditor->SendMessage(SCI_GETLINECOUNT);
    auto status=length>kMaxBytes?"Minimap paused: file exceeds 32 MiB":lines>kMaxLines?"Minimap paused: more than 500,000 lines":std::string();
    if(status!=fStatus) {fStatus=status;SetToolTip(status.empty()?"Click or drag to scroll. The outline marks this pane's visible region.":status.c_str());fStale=true;Invalidate();}
    if(!status.empty()) {std::vector<uint16_t>().swap(fCells);fRows=0;return;}
    int64 total=std::max<sptr_t>(1,fEditor->SendMessage(SCI_VISIBLEFROMDOCLINE,lines-1)+fEditor->SendMessage(SCI_WRAPCOUNT,lines-1));
    auto first=fEditor->SendMessage(SCI_GETFIRSTVISIBLELINE),visible=fEditor->SendMessage(SCI_LINESONSCREEN);
    int width=int(fEditor->Target()->Bounds().Width()),zoom=fEditor->SendMessage(SCI_GETZOOM),wrap=fEditor->SendMessage(SCI_GETWRAPMODE);
    int rows=std::min<int64>(std::clamp(int(Bounds().Height()-4),1,kRows),total);
    bool rebuild=force || fStale || fRevision!=fEditor->InputRevision() || total!=fDisplayLines || rows!=fRows || width!=fWidth || zoom!=fZoom || wrap!=fWrap;
    if(first!=fFirst || visible!=fVisible || rebuild) Invalidate();
    fFirst=first;fVisible=visible;fDisplayLines=total;
    if(!rebuild) return;
    fRevision=fEditor->InputRevision();fRows=rows;fWidth=width;fZoom=zoom;fWrap=wrap;fStale=false;
    fCells.assign(size_t(rows)*kColumns,0);
    for(size_t style=0;style<fPalette.size();++style) {auto color=fEditor->SendMessage(SCI_STYLEGETFORE,style);fPalette[style]={uint8(color),uint8(color>>8),uint8(color>>16),255};}
    auto indentation=fEditor->EffectiveStyle().indentation;
    for(int row=0;row<rows;++row) {
        int64 display=int64(double(row)*total/rows);auto line=fEditor->SendMessage(SCI_DOCLINEFROMVISIBLE,display);
        auto start=fEditor->SendMessage(SCI_POSITIONFROMLINE,line),end=std::min(start+256,fEditor->SendMessage(SCI_GETLINEENDPOSITION,line));
        if(start<0 || end<=start) continue;
        char bytes[514]{};Sci_TextRangeFull range{{start,end},bytes};fEditor->SendMessage(SCI_GETSTYLEDTEXTFULL,0,reinterpret_cast<sptr_t>(&range));
        int column=0;for(sptr_t i=0;i<end-start && column<kColumns;++i) {
            auto c=static_cast<uint8_t>(bytes[2*i]);if((c&0xc0)==0x80) continue;
            if(c=='\t') {column+=indentation.tabWidth-column%indentation.tabWidth;continue;}
            if(c!=' ' && c>=32) fCells[size_t(row)*kColumns+column]=uint16_t(static_cast<uint8_t>(bytes[2*i+1]))+1;
            ++column;
        }
    }
}
BRect MinimapView::Viewport() const {
    float height=std::max(1.f,Bounds().Height()-4),top=2+height*std::clamp(double(fFirst)/fDisplayLines,0.,1.);
    float size=std::max(6.f,float(height*std::min(1.,double(std::max<int64>(1,fVisible))/fDisplayLines)));
    top=std::clamp(top,2.f,std::max(2.f,Bounds().bottom-2-size));return BRect(1,top,Bounds().right-1,std::min(Bounds().bottom-2,top+size));
}
void MinimapView::Draw(BRect) {
    SetDrawingMode(B_OP_COPY);SetHighColor(fTheme.background);FillRect(Bounds());SetHighColor(fTheme.border);StrokeLine(BPoint(0,0),BPoint(0,Bounds().bottom));
    if(!fStatus.empty()) {BFont font(be_plain_font);font.SetSize(9);SetFont(&font);SetHighColor(fTheme.muted);DrawString("Minimap paused",BPoint(5,18));DrawString(fEditor->SendMessage(SCI_GETLENGTH)>kMaxBytes?"File > 32 MiB":"Over 500k lines",BPoint(5,34));return;}
    if(fRows) {
        float step=(Bounds().Height()-4)/fRows,xscale=(Bounds().Width()-8)/kColumns;
        for(int row=0;row<fRows;++row) for(int col=0;col<kColumns;) {
            auto style=fCells[size_t(row)*kColumns+col];if(!style) {++col;continue;}int end=col+1;while(end<kColumns && fCells[size_t(row)*kColumns+end]==style) ++end;
            SetHighColor(fPalette[style-1]);FillRect(BRect(4+col*xscale,2+row*step,4+end*xscale-1,2+row*step+std::min(1.f,step-1)));col=end;
        }
    }
    auto viewport=Viewport();auto fill=fTheme.selection;fill.alpha=75;SetDrawingMode(B_OP_ALPHA);SetHighColor(fill);FillRect(viewport);SetDrawingMode(B_OP_COPY);
    SetHighColor(fEditor->Target()->IsFocus()?fTheme.accent:fTheme.muted);StrokeRect(viewport);
}
void MinimapView::Navigate(float y,bool center) {
    Refresh();if(!fEnabled || !fStatus.empty()) return;
    double fraction=(y-2-(center?Viewport().Height()/2:fDragOffset))/std::max(1.f,Bounds().Height()-4);
    auto first=int64(std::round(std::clamp(fraction,0.,1.)*fDisplayLines));first=std::min(first,std::max<int64>(0,fDisplayLines-fVisible));
    fEditor->SendMessage(SCI_SETFIRSTVISIBLELINE,first);fFirst=fEditor->SendMessage(SCI_GETFIRSTVISIBLELINE);Invalidate();
}
void MinimapView::MouseDown(BPoint point) {
    Refresh();if(!fEnabled || !fStatus.empty()) return;fEditor->MakeFocus();auto viewport=Viewport();fDragOffset=viewport.Contains(point)?point.y-viewport.top:viewport.Height()/2;
    fDragging=true;SetMouseEventMask(B_POINTER_EVENTS,B_LOCK_WINDOW_FOCUS);Navigate(point.y,false);
}
void MinimapView::MouseMoved(BPoint point,uint32,const BMessage*) {if(fDragging) Navigate(point.y,false);}
void MinimapView::MouseUp(BPoint point) {if(fDragging) Navigate(point.y,false);fDragging=false;}
}
