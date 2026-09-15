#include "ui/DiffView.h"
#include "ui/Editor.h"
#include <Button.h>
#include <CardLayout.h>
#include <CheckBox.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <StringView.h>
#include <Window.h>
#include <algorithm>

namespace kiri {
namespace { constexpr uint32 kMode='dfmd',kWrap='dfwr',kPrevious='dfpr',kNext='dfnx',kAlign='dfal';constexpr int kMarker=20,kIndicator=20,kPaddingStyle=254; }
class DiffNavigationButton:public BButton {
public:
    using BButton::BButton;
    void Draw(BRect update) override {
        BRect rect=Bounds();auto base=ui_color(B_CONTROL_BACKGROUND_COLOR);
        rgb_color text=int(base.red)+base.green+base.blue>384?rgb_color{0,0,0,255}:rgb_color{255,255,255,255};
        auto flags=be_control_look->Flags(this);
        be_control_look->DrawButtonFrame(this,rect,update,base,Parent()?Parent()->ViewColor():ViewColor(),flags);
        be_control_look->DrawButtonBackground(this,rect,update,base,flags);
        // Beta 5's default button label can inherit the dark source palette.
        be_control_look->DrawLabel(this,Label(),nullptr,rect,update,base,flags,BAlignment(B_ALIGN_CENTER,B_ALIGN_MIDDLE),&text);
    }
};
class DiffEditor:public Editor {
public:
    explicit DiffEditor(DiffView* owner):fOwner(owner) {}
    void NotificationReceived(SCNotification* notification) override {
        Editor::NotificationReceived(notification);
        if(notification->nmhdr.code==SCN_UPDATEUI) fOwner->Scrolled(this,notification->updated);
        else if(notification->nmhdr.code==SCN_PAINTED) { if(SendMessage(SCI_GETWRAPMODE)!=SC_WRAP_NONE) fOwner->Align();fOwner->Scrolled(this,0); }
    }
    void FrameResized(float w,float h) override { Editor::FrameResized(w,h);if(fOwner->Window()) fOwner->Window()->PostMessage(kAlign,fOwner); }
private:DiffView* fOwner;
};
DiffView::DiffView():BView("comparison",B_FRAME_EVENTS) {
    fLeft=new DiffEditor(this);fLeft->SetName("original source");fRight=new DiffEditor(this);fRight->SetName("modified source");
    fUnified=new Editor();fUnified->SetName("unified diff");fUnified->SetLanguage("changes.diff");
    fLeftLabel=new BStringView("original revision","");fRightLabel=new BStringView("modified revision","");
    for(auto* label:{fLeftLabel,fRightLabel}) { label->SetTruncation(B_TRUNCATE_MIDDLE);label->SetExplicitMinSize(BSize(100,24)); }
    auto* left=new BView("original panel",0);auto* right=new BView("modified panel",0);
    BLayoutBuilder::Group<>(left,B_VERTICAL,2).Add(fLeftLabel).Add(fLeft);
    BLayoutBuilder::Group<>(right,B_VERTICAL,2).Add(fRightLabel).Add(fRight);
    auto* split=new BSplitView(B_HORIZONTAL,1);split->AddChild(left);split->AddChild(right);
    split->SetItemWeight(split->GetLayout()->ItemAt(0),.5f);split->SetItemWeight(split->GetLayout()->ItemAt(1),.5f);
    auto* content=new BView("diff modes",0);fCards=new BCardLayout();content->SetLayout(fCards);fCards->AddView(fUnified);fCards->AddView(split);
    fSideBySide=new BCheckBox("side by side","Side by side",new BMessage(kMode));fSideBySide->SetValue(1);
    fWrap=new BCheckBox("wrap diff","Wrap lines",new BMessage(kWrap));
    fPrevious=new DiffNavigationButton("previous difference","Previous",new BMessage(kPrevious));fNext=new DiffNavigationButton("next difference","Next",new BMessage(kNext));
    fStatus=new BStringView("comparison status","");fStatus->SetTruncation(B_TRUNCATE_END);fStatus->SetExplicitMinSize(BSize(100,24));
    BLayoutBuilder::Group<>(this,B_VERTICAL,3)
        .AddGroup(B_HORIZONTAL,6).SetInsets(6,4,6,4).Add(fSideBySide).Add(fWrap).AddGlue().Add(fPrevious).Add(fNext).End()
        .Add(content).Add(fStatus);
    Clear("Select a file to compare.");
}
void DiffView::AttachedToWindow() { BView::AttachedToWindow();for(auto* control:std::vector<BControl*>{fSideBySide,fWrap,fPrevious,fNext}) control->SetTarget(this);QueueAlignment(); }
void DiffView::FrameResized(float w,float h) { BView::FrameResized(w,h);QueueAlignment(); }
void DiffView::QueueAlignment() { if(Window() && !fAlignQueued) { fAlignQueued=true;Window()->PostMessage(kAlign,this); } }
Editor* DiffView::Left() const { return fLeft; }Editor* DiffView::Right() const { return fRight; }
void DiffView::Clear(const std::string& message) {
    fModel.reset();fHunk=-1;fLeftPadding.clear();fRightPadding.clear();
    for(auto* editor:std::vector<Editor*>{fLeft,fRight,fUnified}) { editor->SendMessage(SCI_ANNOTATIONCLEARALL);editor->SetText("",true);editor->SendMessage(SCI_SETFIRSTVISIBLELINE,0);editor->SendMessage(SCI_SETXOFFSET,0); }
    fLastTop[0]=fLastTop[1]=fLastHorizontal[0]=fLastHorizontal[1]=0;
    fUnified->SetText(message+"\n",true);fLeftLabel->SetText("");fRightLabel->SetText("");fStatus->SetText(message.c_str());fStatus->SetToolTip(message.c_str());
    fPrevious->SetEnabled(false);fNext->SetEnabled(false);fCards->SetVisibleItem(int32(0));
}
void DiffView::SetModel(std::shared_ptr<const DiffModel> model) {
    Clear("Loading comparison…");fModel=std::move(model);if(!fModel) return;
    fUnified->SetText(fModel->Unified(&fUnifiedHunks),true);
    auto load=[](Editor* editor,const DiffSource& source) {
        std::string display="\n"+source.text;if(!source.text.empty() && source.text.back()!='\n' && source.text.back()!='\r') display+='\n';
        editor->SetLanguage(source.path);editor->SetText(display,true);
        editor->SendMessage(SCI_SETMARGINTYPEN,0,SC_MARGIN_TEXT);editor->SendMessage(SCI_SETMARGINWIDTHN,1,0);
        editor->SendMessage(SCI_SETPROPERTY,reinterpret_cast<uptr_t>("fold"),reinterpret_cast<sptr_t>("0"));
    };
    if(fModel->error.empty() && !fModel->binary) { load(fLeft,fModel->before);load(fRight,fModel->after); }
    auto left=DiffDescription(fModel->before),right=DiffDescription(fModel->after);
    fLeftLabel->SetText(left.c_str());fLeftLabel->SetToolTip(left.c_str());fRightLabel->SetText(right.c_str());fRightLabel->SetToolTip(right.c_str());
    auto status=!fModel->error.empty()?fModel->error:fModel->binary?"Binary or non-UTF-8 data · text comparison unavailable":std::to_string(fModel->hunks.size())+" changed blocks · read-only snapshots";
    fStatus->SetText(status.c_str());fStatus->SetToolTip(status.c_str());fPrevious->SetEnabled(!fModel->hunks.empty());fNext->SetEnabled(!fModel->hunks.empty());
    Decorate();SetSideBySide(fSideBySide->Value());QueueAlignment();
}
void DiffView::ApplyTheme(const Theme& theme) { fTheme=theme;ThemeView(this,theme);for(auto* editor:std::vector<Editor*>{fLeft,fRight,fUnified}) editor->ApplyTheme(theme);Decorate();QueueAlignment(); }
void DiffView::ApplySettings(const EditorSettings& settings) { for(auto* editor:std::vector<Editor*>{fLeft,fRight,fUnified}) editor->ApplySettings(settings);ApplyTheme(settings.Colors()); }
void DiffView::SetSideBySide(bool split) { fSideBySide->SetValue(split);fCards->SetVisibleItem(int32(split && fModel && fModel->error.empty() && !fModel->binary));QueueAlignment(); }
void DiffView::SetWrap(bool wrap) { fWrap->SetValue(wrap);for(auto* editor:std::vector<Editor*>{fLeft,fRight,fUnified}) editor->SendMessage(SCI_SETWRAPMODE,wrap?SC_WRAP_WORD:SC_WRAP_NONE);QueueAlignment(); }
void DiffView::Decorate() {
    if(!fModel || fModel->binary || !fModel->error.empty()) return;
    for(int side=0;side<2;++side) {
        auto* editor=side?fRight:fLeft;auto& lines=side?fModel->rightLines:fModel->leftLines;auto color=side?fTheme.added:fTheme.removed;
        editor->SendMessage(SCI_MARKERDELETEALL,kMarker);editor->SendMessage(SCI_MARKERDEFINE,kMarker,SC_MARK_BACKGROUND);editor->SendMessage(SCI_MARKERSETBACK,kMarker,SciColor(color));editor->SendMessage(SCI_MARKERSETALPHA,kMarker,45);
        editor->SendMessage(SCI_INDICSETSTYLE,kIndicator,INDIC_ROUNDBOX);editor->SendMessage(SCI_INDICSETFORE,kIndicator,SciColor(color));editor->SendMessage(SCI_INDICSETALPHA,kIndicator,90);
        editor->SendMessage(SCI_STYLESETBACK,kPaddingStyle,SciColor(fTheme.panel));editor->SendMessage(SCI_ANNOTATIONSETVISIBLE,ANNOTATION_STANDARD);
        for(size_t i=0;i<lines.size();++i) { auto number=std::to_string(i+1);editor->SendMessage(SCI_MARGINSETTEXT,i+1,reinterpret_cast<sptr_t>(number.c_str()));editor->SendMessage(SCI_MARGINSETSTYLE,i+1,STYLE_LINENUMBER); }
    }
    for(auto& row:fModel->rows) if(row.hunk>=0) {
        std::string_view left,right;if(row.before>=0) { auto line=fModel->leftLines[row.before];left=std::string_view(fModel->before.text).substr(line.start,line.length); }
        if(row.after>=0) { auto line=fModel->rightLines[row.after];right=std::string_view(fModel->after.text).substr(line.start,line.length); }
        auto changed=ChangedMiddle(left,right);
        for(int side=0;side<2;++side) { int line=side?row.after:row.before;if(line<0) continue;auto* editor=side?fRight:fLeft;
            editor->SendMessage(SCI_MARKERADD,line+1,kMarker);auto start=editor->SendMessage(SCI_POSITIONFROMLINE,line+1)+changed.prefix;auto end=side?changed.afterEnd:changed.beforeEnd;
            if(end>changed.prefix) { editor->SendMessage(SCI_SETINDICATORCURRENT,kIndicator);editor->SendMessage(SCI_INDICATORFILLRANGE,start,end-changed.prefix); }
        }
    }
}
void DiffView::Align() {
    if(fSyncing || !fModel || fModel->binary || !fModel->error.empty()) return;
    fSyncing=true;std::vector<int> left(fModel->leftLines.size()+1,0),right(fModel->rightLines.size()+1,0);int lastLeft=0,lastRight=0;
    for(auto& row:fModel->rows) {
        int a=row.before>=0?std::max<sptr_t>(1,fLeft->SendMessage(SCI_WRAPCOUNT,row.before+1)):0;
        int b=row.after>=0?std::max<sptr_t>(1,fRight->SendMessage(SCI_WRAPCOUNT,row.after+1)):0;
        if(row.before>=0) lastLeft=row.before+1;if(row.after>=0) lastRight=row.after+1;
        left[lastLeft]+=std::max(a,b)-a;right[lastRight]+=std::max(a,b)-b;
    }
    auto pad=[](Editor* editor,const std::vector<int>& counts,std::vector<int>& previous) {
        if(counts==previous) return;
        for(size_t i=0;i<counts.size();++i) if(i>=previous.size() || counts[i]!=previous[i]) {
            std::string text;if(counts[i]) { text=" ";for(int n=1;n<counts[i];++n) text+="\n "; }
            editor->SendMessage(SCI_ANNOTATIONSETTEXT,i,reinterpret_cast<sptr_t>(counts[i]?text.c_str():nullptr));editor->SendMessage(SCI_ANNOTATIONSETSTYLE,i,kPaddingStyle);
        }
        previous=counts;
    };
    pad(fLeft,left,fLeftPadding);pad(fRight,right,fRightPadding);fSyncing=false;
}
void DiffView::Scrolled(DiffEditor* editor,int) {
    if(fSyncing || !fModel) return;fSyncing=true;auto* other=editor==fLeft?fRight:fLeft;
    int side=editor==fLeft?0:1;
    auto first=editor->SendMessage(SCI_GETFIRSTVISIBLELINE),horizontal=editor->SendMessage(SCI_GETXOFFSET);
    // Haiku delivers notifications asynchronously, and programmatic scrolling
    // may only be observable after painting. Remember both resulting positions
    // so a delayed notification cannot echo the scroll back to its source.
    if(fLastTop[side]!=first) { fLastTop[side]=first;other->SendMessage(SCI_SETFIRSTVISIBLELINE,first);fLastTop[1-side]=other->SendMessage(SCI_GETFIRSTVISIBLELINE); }
    if(fLastHorizontal[side]!=horizontal) { fLastHorizontal[side]=horizontal;other->SendMessage(SCI_SETXOFFSET,horizontal);fLastHorizontal[1-side]=other->SendMessage(SCI_GETXOFFSET); }fSyncing=false;
}
void DiffView::Navigate(int direction) {
    if(!fModel || fModel->hunks.empty()) return;
    fHunk=fHunk<0?(direction>0?0:int(fModel->hunks.size())-1):(fHunk+direction+int(fModel->hunks.size()))%fModel->hunks.size();auto& h=fModel->hunks[fHunk];Align();
    auto& row=fModel->rows[h.row];auto* editor=row.before>=0?fLeft:fRight;int line=(row.before>=0?row.before:row.after)+1;
    auto visible=editor->SendMessage(SCI_VISIBLEFROMDOCLINE,line);for(auto* side:{fLeft,fRight}) side->SendMessage(SCI_SETFIRSTVISIBLELINE,std::max<sptr_t>(0,visible-2));
    fStatus->SetText(("Changed block "+std::to_string(fHunk+1)+" of "+std::to_string(fModel->hunks.size())+" · read-only snapshots").c_str());
    if(!fSideBySide->Value() && size_t(fHunk)<fUnifiedHunks.size()) fUnified->GoTo(fUnifiedHunks[fHunk]+1,1,false);
}
void DiffView::MessageReceived(BMessage* message) {
    switch(message->what) { case kMode:SetSideBySide(fSideBySide->Value());break;case kWrap:SetWrap(fWrap->Value());break;case kPrevious:Navigate(-1);break;case kNext:Navigate(1);break;case kAlign:fAlignQueued=false;Align();break;default:BView::MessageReceived(message); }
}
}
