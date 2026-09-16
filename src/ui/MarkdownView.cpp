#include "ui/MarkdownView.h"
#include "ui/Editor.h"
#include "core/FileIO.h"
#include <BitmapStream.h>
#include <DataIO.h>
#include <Entry.h>
#include <Roster.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <TranslatorRoster.h>
#include <Window.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace kiri {
namespace {
constexpr uint32 kMarkdownTick='mdtk';
constexpr size_t kPieces=60000;
float LineHeight(const BFont& font) {font_height h;font.GetHeight(&h);return std::ceil(h.ascent+h.descent+h.leading+4);}
size_t NextChar(const std::string& text,size_t at) {++at;while(at<text.size() && (static_cast<unsigned char>(text[at])&0xc0)==0x80) ++at;return at;}
// Validate the translator's bitmap header before BBitmapStream allocates the
// decoded pixels. Cancellation is also checked as pixel rows are written.
class PreviewBitmapStream:public BBitmapStream {
public:
    PreviewBitmapStream(const std::atomic<bool>& cancel,size_t limit):fCancel(cancel),fLimit(limit) {}
    ssize_t WriteAt(off_t offset,const void* data,size_t size) override {
        if(fCancel) return B_CANCELED;
        if(offset<0 || !data || size>fLimit+sizeof(TranslatorBitmap) || uint64(offset)+size>fLimit+sizeof(TranslatorBitmap)) return B_BAD_VALUE;
        if(offset<off_t(sizeof(TranslatorBitmap))) {
            auto count=std::min(size,sizeof(TranslatorBitmap)-size_t(offset));std::memcpy(fRaw+offset,data,count);
            if(size_t(offset)+size>=sizeof(TranslatorBitmap)) {
                TranslatorBitmap raw,header;std::memcpy(static_cast<void*>(&raw),fRaw,sizeof(raw));
                if(B_HOST_IS_LENDIAN) SwapHeader(&raw,&header);else header=raw;
                double width=header.bounds.Width()+1,height=header.bounds.Height()+1;
                if(header.magic!=B_TRANSLATOR_BITMAP || header.bounds.left!=0 || header.bounds.top!=0 || !std::isfinite(width) || !std::isfinite(height)
                    || width<1 || height<1 || width>16384 || height>16384 || header.rowBytes==0
                    || header.rowBytes*height>fLimit || header.dataSize!=header.rowBytes*height) return B_BAD_VALUE;
            }
        }
        return BBitmapStream::WriteAt(offset,data,size);
    }
private:
    const std::atomic<bool>& fCancel;size_t fLimit;char fRaw[sizeof(TranslatorBitmap)]{};
};
std::shared_ptr<BBitmap> LoadImage(const std::string& path,const std::atomic<bool>& cancel,size_t limit) {
    std::error_code error;if(!std::filesystem::is_regular_file(path,error)) return {};
    auto file=ReadFile(path,&cancel,8*1024*1024);if(!file.ok() || cancel) return {};
    BMemoryIO input(file.bytes.data(),file.bytes.size());PreviewBitmapStream output(cancel,limit);
    if(BTranslatorRoster::Default()->Translate(&input,nullptr,nullptr,&output,B_TRANSLATOR_BITMAP)!=B_OK || cancel) return {};
    BBitmap* bitmap=nullptr;if(output.DetachBitmap(&bitmap)!=B_OK) return {};return std::shared_ptr<BBitmap>(bitmap);
}
}
MarkdownView::MarkdownView():BView("Markdown preview",B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE|B_FULL_UPDATE_ON_RESIZE) {
    SetExplicitMinSize(BSize(100,80));SetViewColor(fTheme.background);
    SetToolTip("Scroll either view to follow the source. Tab selects links; Enter opens a link; Escape returns to source.");
}
void MarkdownView::SetStatus(const std::string& status) {
    fStatus=status;fDocument.reset();fImages.clear();fPieces.clear();fLinks.clear();fMap.clear();fAnchors.clear();fFocusedLink=-1;Invalidate();
}
void MarkdownView::SetDocument(std::shared_ptr<MarkdownDocument> document,MarkdownImages images) {
    fStatus=document->error;fDocument=std::move(document);fImages=std::move(images);fFocusedLink=-1;Reflow();
}
void MarkdownView::ApplySettings(const EditorSettings& settings) {if(fSettings.fontSize!=settings.fontSize) fFonts.clear();fSettings=settings;fTheme=settings.Colors();SetViewColor(fTheme.background);Reflow();}
void MarkdownView::FrameResized(float,float) {Reflow();}
MarkdownView::FontMetrics& MarkdownView::Font(unsigned style,unsigned heading) {
    auto key=(heading<<8)|(style&(MarkdownBold|MarkdownItalic|MarkdownCode));auto found=fFonts.find(key);if(found!=fFonts.end()) return found->second;
    FontMetrics metrics;metrics.font=*(style&MarkdownCode?be_fixed_font:be_plain_font);
    float size=std::clamp(fSettings.fontSize,10,24)*(heading==1?1.85f:heading==2?1.5f:heading?1.18f:1.f);
    metrics.font.SetSize(size*(style&MarkdownCode?.94f:1.f));metrics.font.SetFace((style&MarkdownBold?B_BOLD_FACE:0)|(style&MarkdownItalic?B_ITALIC_FACE:0));metrics.height=LineHeight(metrics.font);
    return fFonts.emplace(key,std::move(metrics)).first->second;
}
float MarkdownView::Width(FontMetrics& font,const std::string& text) {
    auto found=font.widths.find(text);if(found!=font.widths.end()) return found->second;
    float width=font.font.StringWidth(text.c_str());if(font.widths.size()<8192) font.widths.emplace(text,width);return width;
}
void MarkdownView::Flow(const MarkdownBlock& block,float left,float width,float& y,bool bold) {
    const bool verbatim=block.type==MD_BLOCK_CODE || block.type==MD_BLOCK_HTML;
    const float top=y;float x=left,rowHeight=0;
    unsigned heading=block.type==MD_BLOCK_H?block.level:0;float baseHeight=Font(heading || bold?MarkdownBold:0,heading).height;
    auto newline=[&] {y+=std::max(rowHeight,baseHeight);x=left;rowHeight=0;};
    size_t firstPiece=fPieces.size();
    for(const auto& run:block.runs) {
        unsigned style=run.style;
        if(bold || block.type==MD_BLOCK_H) style|=MarkdownBold;
        auto& metrics=Font(style,heading);const auto& font=metrics.font;
        float height=metrics.height;size_t line=run.line;
        fMap.emplace_back(double(line),y);
        if(!run.image.empty()) {
            auto image=fImages.find(run.image);
            if(image!=fImages.end()) {
                if(x>left) newline();auto bounds=image->second->Bounds();float scale=std::min({1.f,width/(bounds.Width()+1),600.f/(bounds.Height()+1)});
                Piece piece;piece.rect=BRect(left,y,left+(bounds.Width()+1)*scale,y+(bounds.Height()+1)*scale);piece.image=run.image;piece.link=run.link;
                fPieces.push_back(std::move(piece));y+=fPieces.back().rect.Height()+10;continue;
            }
        }
        std::string text=run.image.empty()?run.text:"[Image: "+(run.text.empty()?run.image:run.text)+" — unavailable or blocked]";
        for(size_t at=0;at<text.size();) {
            if(fPieces.size()>=kPieces) throw std::runtime_error("Preview paused: layout exceeds 60,000 fragments.");
            if(text[at]=='\n' || text[at]=='\r') {if(text[at]=='\r' && at+1<text.size() && text[at+1]=='\n') ++at;++at;newline();if(verbatim) fMap.emplace_back(double(++line),y);continue;}
            bool space=text[at]==' ' || text[at]=='\t';size_t end=at;
            if(space) ++end;
            else while(end<text.size() && text[end]!=' ' && text[end]!='\t' && text[end]!='\n' && text[end]!='\r' && end-at<128) end=NextChar(text,end);
            std::string word=text.substr(at,end-at);if(word=="\t") word="    ";
            float length=Width(metrics,word);
            if(x>left && x+length>left+width) newline();
            if(!verbatim && space && x==left) {at=end;continue;}
            // Split very long words on UTF-8 boundaries; never measure an
            // unbounded line or allow a narrow pane to stall layout.
            if(length>width && !space) {
                end=NextChar(text,at);
                while(end<text.size()) {auto next=NextChar(text,end);if(next-at>128 || Width(metrics,text.substr(at,next-at))>width) break;end=next;}
                word=text.substr(at,end-at);length=Width(metrics,word);
            }
            Piece piece;piece.rect=BRect(x,y,x+length,y+height);piece.text=std::move(word);piece.link=run.link;piece.font=font;piece.style=style;
            if(!fPieces.empty() && !fPieces.back().text.empty() && fPieces.back().rect.top==y && fPieces.back().rect.right==x && fPieces.back().style==style && fPieces.back().link==run.link && fPieces.back().font==font) {
                fPieces.back().text+=piece.text;fPieces.back().rect.right+=length;
            } else fPieces.push_back(std::move(piece));
            x+=length;rowHeight=std::max(rowHeight,height);at=end;
        }
    }
    if(x>left || y==top) newline();
    if(block.align==MD_ALIGN_RIGHT || block.align==MD_ALIGN_CENTER) {
        for(size_t at=firstPiece;at<fPieces.size();) {
            size_t end=at+1;while(end<fPieces.size() && fPieces[end].rect.top==fPieces[at].rect.top) ++end;
            float delta=std::max(0.f,left+width-fPieces[end-1].rect.right)*(block.align==MD_ALIGN_CENTER?.5f:1.f);
            for(size_t i=at;i<end;++i) fPieces[i].rect.OffsetBy(delta,0);at=end;
        }
    }
    fMap.emplace_back(double(block.lastLine+1),y);
}
void MarkdownView::Block(size_t index,float x,float width,float& y) {
    auto& b=fDocument->blocks[index];if(fPieces.size()>=kPieces) throw std::runtime_error("Preview paused: layout exceeds 60,000 fragments.");
    width=std::max(24.f,width);float top=y;
    if(b.type==MD_BLOCK_TABLE) {
        for(auto section:b.children) for(auto row:fDocument->blocks[section].children) {
            auto& cells=fDocument->blocks[row].children;float cellWidth=width/std::max<size_t>(1,cells.size()),bottom=y;
            for(size_t c=0;c<cells.size();++c) {float cy=y+6;Flow(fDocument->blocks[cells[c]],x+c*cellWidth+6,std::max(12.f,cellWidth-12),cy,fDocument->blocks[cells[c]].type==MD_BLOCK_TH);bottom=std::max(bottom,cy+6);}
            // Borders are drawn after text, with transparent interiors.
            for(size_t c=0;c<cells.size();++c) {Piece p;p.rect=BRect(x+c*cellWidth,y,x+(c+1)*cellWidth,bottom);p.rule=true;fPieces.push_back(std::move(p));}
            y=bottom;
        }
        y+=12;return;
    }
    if(b.type==MD_BLOCK_HR) {Piece p;p.rect=BRect(x,y+8,x+width,y+8);p.rule=true;fPieces.push_back(std::move(p));y+=22;return;}
    if(b.type==MD_BLOCK_UL || b.type==MD_BLOCK_OL) {
        unsigned number=b.start;
        for(auto child:b.children) {
            const auto& item=fDocument->blocks[child];Piece p;auto& font=Font(0);p.font=font.font;
            p.text=item.task?(item.task==' '?"☐":"☑"):b.type==MD_BLOCK_UL?"•":std::to_string(number++)+".";p.rect=BRect(x,y,x+24,y+font.height);fPieces.push_back(std::move(p));Block(child,x+28,width-28,y);
        }
        y+=6;return;
    }
    if(b.type==MD_BLOCK_QUOTE) {
        for(auto child:b.children) Block(child,x+16,width-16,y);
        Piece p;p.rect=BRect(x+3,top,x+3,y-5);p.rule=true;fPieces.push_back(std::move(p));return;
    }
    bool code=b.type==MD_BLOCK_CODE || b.type==MD_BLOCK_HTML;
    size_t background=fPieces.size();
    if(code) {Piece p;p.panel=true;fPieces.push_back(std::move(p));y+=8;}
    if(!b.runs.empty() || b.type==MD_BLOCK_P || b.type==MD_BLOCK_H || code) {
        if(b.type==MD_BLOCK_H) {y+=8;fAnchors[b.anchor]=y;}
        fMap.emplace_back(double(b.firstLine),y);Flow(b,x+(code?8:0),width-(code?16:0),y);
        if(code) {y+=8;fPieces[background].rect=BRect(x,top,x+width,y);}
        y+=b.type==MD_BLOCK_H?10:8;
    }
    for(auto child:b.children) Block(child,x,width,y);
}
void MarkdownView::Reflow() {
    if(fLayout) return;fLayout=true;double line=LineForY(Bounds().top);
    fPieces.clear();fLinks.clear();fMap.clear();fAnchors.clear();float y=18;
    if(fDocument && fDocument->error.empty()) {
        fStatus.clear();
        try {if(!fDocument->blocks.empty()) Block(0,18,std::max(24.f,Bounds().Width()-36),y);}
        catch(const std::exception& error) {fStatus=error.what();fPieces.clear();fMap.clear();}
    }
    // A source line can appear in many styled runs or table cells. Retain its
    // earliest y and interpolate between distinct source lines in either view.
    std::sort(fMap.begin(),fMap.end());std::vector<std::pair<double,float>> map;
    for(auto point:fMap) {if(!map.empty() && point.first==map.back().first) continue;if(!map.empty()) point.second=std::max(point.second,map.back().second);map.push_back(point);}
    fMap=std::move(map);fHeight=y+18;
    for(size_t i=0;i<fPieces.size();++i) if(!fPieces[i].link.empty()) fLinks.push_back(i);
    fFocusedLink=-1;UpdateScrollBar();ScrollToLine(line);fLayout=false;Invalidate();
}
void MarkdownView::UpdateScrollBar() {
    if(auto* bar=ScrollBar(B_VERTICAL)) {bar->SetRange(0,std::max(0.f,fHeight-Bounds().Height()));bar->SetProportion(std::min(1.f,Bounds().Height()/std::max(1.f,fHeight)));bar->SetSteps(24,std::max(24.f,Bounds().Height()-24));}
}
void MarkdownView::Draw(BRect update) {
    SetHighColor(fTheme.background);FillRect(update);SetDrawingMode(B_OP_COPY);
    if(!fStatus.empty()) {BFont font(be_plain_font);font.SetSize(12);SetFont(&font);SetHighColor(fTheme.muted);BString message(fStatus.c_str());TruncateString(&message,B_TRUNCATE_END,std::max(20.f,Bounds().Width()-32));DrawString(message.String(),Bounds().LeftTop()+BPoint(16,28));return;}
    for(size_t i=0;i<fPieces.size();++i) {
        auto& p=fPieces[i];if(!update.Intersects(p.rect)) continue;
        if(p.panel) {SetHighColor(fTheme.panel);FillRect(p.rect);continue;}
        if(p.rule) {SetHighColor(fTheme.border);StrokeRect(p.rect);continue;}
        if(!p.image.empty()) {auto image=fImages.find(p.image);if(image!=fImages.end()) {SetDrawingMode(B_OP_ALPHA);DrawBitmap(image->second.get(),image->second->Bounds(),p.rect,B_FILTER_BITMAP_BILINEAR);SetDrawingMode(B_OP_COPY);}continue;}
        SetFont(&p.font);SetHighColor(!p.link.empty()?fTheme.accent:fTheme.text);SetLowColor(fTheme.background);
        font_height h;p.font.GetHeight(&h);DrawString(p.text.c_str(),BPoint(p.rect.left,p.rect.top+h.ascent+2));
        if(!p.link.empty()) StrokeLine(BPoint(p.rect.left,p.rect.bottom-3),BPoint(p.rect.right,p.rect.bottom-3));
        if(p.style&MarkdownStrike) StrokeLine(BPoint(p.rect.left,p.rect.top+p.rect.Height()*.55f),BPoint(p.rect.right,p.rect.top+p.rect.Height()*.55f));
        if(IsFocus() && fFocusedLink>=0 && size_t(fFocusedLink)<fLinks.size() && fLinks[fFocusedLink]==i) {SetHighColor(fTheme.accent);StrokeRect(p.rect);}
    }
    if(IsFocus()) {SetHighColor(fTheme.accent);StrokeRect(Bounds());}
}
void MarkdownView::ScrollTo(BPoint point) {
    point.x=0;point.y=std::clamp(point.y,0.f,std::max(0.f,fHeight-Bounds().Height()));bool changed=point!=Bounds().LeftTop();BView::ScrollTo(point);
    // The focus outline belongs to the viewport, so repaint its old pixels
    // after BView scrolls the underlying content.
    if(changed && IsFocus()) Invalidate();
    if(changed && !fLayout && scrolled) scrolled();
}
float MarkdownView::YForLine(double line) const {
    if(fMap.empty()) return 0;auto next=std::upper_bound(fMap.begin(),fMap.end(),line,[](double n,const auto& p){return n<p.first;});
    if(next==fMap.begin()) return next->second;if(next==fMap.end()) return fMap.back().second;auto prev=next-1;
    return prev->second+(next->second-prev->second)*float((line-prev->first)/(next->first-prev->first));
}
double MarkdownView::LineForY(float y) const {
    if(fMap.empty()) return 0;auto next=std::upper_bound(fMap.begin(),fMap.end(),y,[](float n,const auto& p){return n<p.second;});
    if(next==fMap.begin()) return next->first;if(next==fMap.end()) return fMap.back().first;auto prev=next-1;
    return prev->first+(next->first-prev->first)*(y-prev->second)/std::max(1.f,next->second-prev->second);
}
void MarkdownView::ScrollToLine(double line) {ScrollTo(BPoint(0,YForLine(line)));}
bool MarkdownView::GoToAnchor(const std::string& anchor) {auto found=fAnchors.find(anchor);if(found==fAnchors.end()) return false;ScrollTo(BPoint(0,found->second));return true;}
void MarkdownView::MakeFocus(bool focus) {BView::MakeFocus(focus);Invalidate();}
void MarkdownView::Activate(size_t index) {if(index<fPieces.size() && activate) activate(fPieces[index].link);}
void MarkdownView::MouseDown(BPoint point) {MakeFocus();for(auto index:fLinks) if(fPieces[index].rect.Contains(point)) {Activate(index);return;}}
void MarkdownView::MouseMoved(BPoint point,uint32,const BMessage*) {for(auto index:fLinks) if(fPieces[index].rect.Contains(point)) {SetToolTip(fPieces[index].link.c_str());return;}SetToolTip("Tab: links · Enter: open · Escape: source · Page Up/Down: scroll");}
void MarkdownView::KeyDown(const char* bytes,int32 count) {
    if(!count) return;
    float delta=0;
    switch(bytes[0]) {
        case B_ESCAPE:if(sourceFocus) sourceFocus();return;
        case B_TAB: {
            if(fLinks.empty()) {if(sourceFocus) sourceFocus();return;}
            int step=modifiers()&B_SHIFT_KEY?-1:1;
            int next=fFocusedLink+step;
            // A styled or wrapped link is one keyboard stop.
            while(fFocusedLink>=0 && next>=0 && size_t(next)<fLinks.size() && fPieces[fLinks[next]].link==fPieces[fLinks[fFocusedLink]].link) next+=step;
            if(next<0 || size_t(next)>=fLinks.size()) {fFocusedLink=-1;if(sourceFocus) sourceFocus();return;}
            fFocusedLink=next;auto r=fPieces[fLinks[next]].rect;if(!Bounds().Contains(r)) ScrollTo(BPoint(0,r.top-18));Invalidate();return;
        }
        case B_ENTER:case B_SPACE:if(fFocusedLink>=0) {Activate(fLinks[fFocusedLink]);return;}delta=Bounds().Height()-24;break;
        case B_UP_ARROW:delta=-24;break;case B_DOWN_ARROW:delta=24;break;
        case B_PAGE_UP:delta=-Bounds().Height()+24;break;case B_PAGE_DOWN:delta=Bounds().Height()-24;break;
        case B_HOME:ScrollTo(BPoint(0,0));return;case B_END:ScrollTo(BPoint(0,fHeight));return;
        default:BView::KeyDown(bytes,count);return;
    }
    ScrollTo(BPoint(0,Bounds().top+delta));
}

MarkdownPane::MarkdownPane(Editor* editor,const std::string& path,const EditorSettings& settings)
    :BSplitView(B_HORIZONTAL,1),fEditor(editor),fPreview(new MarkdownView()),fPath(path) {
    SetName("Markdown source and preview");SetCollapsible(false);AddChild(editor,1.f);
    AddChild(new BScrollView("Markdown scroll",fPreview,0,false,true,B_NO_BORDER),1.f);
    fPreview->scrolled=[this]{SyncSource();};fPreview->activate=[this](const auto& link){ActivateLink(link);};fPreview->sourceFocus=[this]{fEditor->MakeFocus();};ApplySettings(settings);
}
MarkdownPane::~MarkdownPane() {fTimer.reset();fJobs.reset();}
void MarkdownPane::AttachedToWindow() {
    BSplitView::AttachedToWindow();fJobs=std::make_unique<AsyncQueue>(BMessenger(this));BMessage tick(kMarkdownTick);fTimer=std::make_unique<BMessageRunner>(BMessenger(this),&tick,100000);fObserved=-1;Refresh();
}
void MarkdownPane::DetachedFromWindow() {fTimer.reset();fJobs.reset();++fSerial;fSubmitted=false;BSplitView::DetachedFromWindow();}
void MarkdownPane::MessageReceived(BMessage* message) {if(message->what==kMarkdownTick) Refresh();else if(message->what==kWorkDone) {if(fJobs) fJobs->Drain();}else BSplitView::MessageReceived(message);}
void MarkdownPane::MakeFocus(bool focus) {fEditor->MakeFocus(focus);}
void MarkdownPane::ApplySettings(const EditorSettings& settings) {SetViewColor(settings.Colors().panel);fPreview->ApplySettings(settings);fFirst=-1;}
void MarkdownPane::SetPath(const std::string& path) {if(path!=fPath) {fPath=path;fObserved=-1;Refresh();}}
void MarkdownPane::Refresh() {
    if(!fJobs) return;
    auto revision=fEditor->InputRevision();
    if(revision!=fObserved) {
        fObserved=revision;fRendered=-1;++fSerial;fJobs->Cancel("markdown");fSubmitted=false;fChangedAt=system_time();fPreview->SetStatus("Rendering Markdown…");
    }
    if(IsHidden()) return;
    if(!fSubmitted && system_time()-fChangedAt>=150000) Render();
    if(fRendered==revision) SyncPreview();
}
void MarkdownPane::Render() {
    fSubmitted=true;
    if(fEditor->SendMessage(SCI_GETLENGTH)>sptr_t(kMarkdownBytes)) {fPreview->SetStatus("Preview paused: document exceeds 2 MiB.");return;}
    auto text=fEditor->Text(),path=fPath;auto revision=fObserved,serial=fSerial;
    fJobs->Submit([this,text=std::move(text),path,revision,serial](const auto& cancel) {
        auto document=std::make_shared<MarkdownDocument>(ParseMarkdown(text,&cancel));MarkdownImages images;size_t bytes=0,attempts=0;
        if(document->error.empty()) for(const auto& block:document->blocks) for(const auto& run:block.runs) {
            if(cancel) return AsyncQueue::Callback{};
            if(run.image.empty() || images.count(run.image) || attempts>=32) continue;
            auto resource=ResolveMarkdownTarget(run.image,path,true);if(resource.kind!=MarkdownTargetKind::Local) continue;
            auto stamp=StatFile(resource.path);if(!stamp.exists || stamp.size>8*1024*1024) continue;++attempts;
            auto bitmap=LoadImage(resource.path,cancel,std::min(size_t(32*1024*1024),64*1024*1024-bytes));
            if(!bitmap || bitmap->InitCheck()!=B_OK || bitmap->BitsLength()>32*1024*1024 || bytes+bitmap->BitsLength()>64*1024*1024) continue;
            bytes+=bitmap->BitsLength();images[run.image]=std::move(bitmap);
        }
        return AsyncQueue::Callback([this,document,images=std::move(images),revision,serial]() mutable {
            if(serial!=fSerial || revision!=fEditor->InputRevision()) return;
            fSyncing=true;fPreview->SetDocument(document,std::move(images));fRendered=revision;fFirst=-1;fSyncing=false;SyncPreview();
        });
    },"markdown",[this,revision,serial](const auto& error){if(serial==fSerial && revision==fEditor->InputRevision()) fPreview->SetStatus("Preview failed: "+error);});
}
void MarkdownPane::SyncPreview() {
    if(fSyncing || !fPreview->Document()) return;auto first=fEditor->SendMessage(SCI_GETFIRSTVISIBLELINE);
    auto line=fEditor->SendMessage(SCI_DOCLINEFROMVISIBLE,first),start=fEditor->SendMessage(SCI_VISIBLEFROMDOCLINE,line),wrap=std::max<sptr_t>(1,fEditor->SendMessage(SCI_WRAPCOUNT,line));
    double source=line+double(first-start)/wrap;if(first==fFirst && source==fSourceLine) return;
    fSyncing=true;fPreview->ScrollToLine(source);fFirst=first;fSourceLine=source;fSyncing=false;
}
void MarkdownPane::SyncSource() {
    if(fSyncing || fRendered!=fEditor->InputRevision() || !fPreview->Document()) return;
    double source=fPreview->LineForY(fPreview->Bounds().top);auto line=std::clamp<sptr_t>(sptr_t(source),0,fEditor->SendMessage(SCI_GETLINECOUNT)-1);
    auto first=fEditor->SendMessage(SCI_VISIBLEFROMDOCLINE,line),wrap=std::max<sptr_t>(1,fEditor->SendMessage(SCI_WRAPCOUNT,line));
    fSyncing=true;fEditor->SendMessage(SCI_SETFIRSTVISIBLELINE,first+sptr_t((source-line)*wrap));fFirst=fEditor->SendMessage(SCI_GETFIRSTVISIBLELINE);
    line=fEditor->SendMessage(SCI_DOCLINEFROMVISIBLE,fFirst);first=fEditor->SendMessage(SCI_VISIBLEFROMDOCLINE,line);wrap=std::max<sptr_t>(1,fEditor->SendMessage(SCI_WRAPCOUNT,line));
    fSourceLine=line+double(fFirst-first)/wrap;fSyncing=false;
}
void MarkdownPane::ActivateLink(const std::string& link) {
    if(fRendered!=fEditor->InputRevision()) {Refresh();return;}
    auto target=ResolveMarkdownTarget(link,fPath);
    if(target.kind==MarkdownTargetKind::Anchor || (target.kind==MarkdownTargetKind::Local && CanonicalPath(target.path)==CanonicalPath(fPath))) {
        if(target.fragment.empty()) fPreview->ScrollToLine(0);else if(!fPreview->GoToAnchor(target.fragment)) fPreview->SetToolTip("Heading not found.");return;
    }
    if(target.kind==MarkdownTargetKind::Web) {
        const char* args[]={target.path.c_str()};auto status=be_roster->Launch(target.path.find(':')==5?"application/x-vnd.Be.URL.https":"application/x-vnd.Be.URL.http",1,args);
        if(status!=B_OK && status!=B_ALREADY_RUNNING) fPreview->SetToolTip("No application is available to open this web link.");return;
    }
    if(target.kind==MarkdownTargetKind::Local) {
        std::error_code error;if(!std::filesystem::is_regular_file(target.path,error)) {fPreview->SetToolTip("Local file not found.");return;}
        BMessage message(kMarkdownLink);message.AddString("path",target.path.c_str());message.AddString("fragment",target.fragment.c_str());Window()->PostMessage(&message);return;
    }
    fPreview->SetToolTip(target.reason.c_str());
}
}
