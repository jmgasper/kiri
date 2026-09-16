#pragma once
#include "core/Markdown.h"
#include "ui/Async.h"
#include "ui/EditorSettings.h"
#include <Bitmap.h>
#include <Font.h>
#include <MessageRunner.h>
#include <SplitView.h>
#include <functional>
#include <map>
#include <unordered_map>

namespace kiri {
class Editor;
using MarkdownImages=std::map<std::string,std::shared_ptr<BBitmap>>;
class MarkdownView:public BView {
public:
    MarkdownView();
    void SetDocument(std::shared_ptr<MarkdownDocument> document,MarkdownImages images={});
    void SetStatus(const std::string& status);
    void ApplySettings(const EditorSettings& settings);
    void Draw(BRect update) override;
    void FrameResized(float width,float height) override;
    void ScrollTo(BPoint point) override;
    void KeyDown(const char* bytes,int32 count) override;
    void MouseDown(BPoint point) override;
    void MouseMoved(BPoint point,uint32 transit,const BMessage* drag) override;
    void MakeFocus(bool focus=true) override;
    float YForLine(double line) const;
    double LineForY(float y) const;
    bool GoToAnchor(const std::string& anchor);
    void ScrollToLine(double line);
    const MarkdownDocument* Document() const {return fDocument.get();}
    size_t ImageCount() const {return fImages.size();}
    size_t LinkCount() const {return fLinks.size();}
    float ContentHeight() const {return fHeight;}
    const std::string& Status() const {return fStatus;}
    std::function<void()> scrolled;
    std::function<void(const std::string&)> activate;
    std::function<void()> sourceFocus;
private:
    struct Piece {BRect rect;std::string text,link,image;BFont font;unsigned style=0;bool panel=false,rule=false;};
    struct FontMetrics {BFont font;float height=0;std::unordered_map<std::string,float> widths;};
    FontMetrics& Font(unsigned style,unsigned heading=0);
    float Width(FontMetrics& font,const std::string& text);
    void Reflow();
    void Block(size_t index,float x,float width,float& y);
    void Flow(const MarkdownBlock& block,float x,float width,float& y,bool bold=false);
    void UpdateScrollBar();
    void Activate(size_t index);
    std::shared_ptr<MarkdownDocument> fDocument;
    MarkdownImages fImages;
    std::vector<Piece> fPieces;
    std::vector<size_t> fLinks;
    std::vector<std::pair<double,float>> fMap;
    std::map<std::string,float> fAnchors;
    std::map<unsigned,FontMetrics> fFonts;
    EditorSettings fSettings;
    Theme fTheme=Theme::Builtins()[0];
    std::string fStatus="Rendering Markdown…";
    float fHeight=0;
    int fFocusedLink=-1;
    bool fLayout=false;
};

// One preview belongs to one source view. Its text/undo buffer remains the
// existing Editor, including shared documents in other workspace panes.
class MarkdownPane:public BSplitView {
public:
    MarkdownPane(Editor* editor,const std::string& path,const EditorSettings& settings);
    ~MarkdownPane() override;
    void AttachedToWindow() override;
    void DetachedFromWindow() override;
    void MessageReceived(BMessage* message) override;
    void MakeFocus(bool focus=true) override;
    void ApplySettings(const EditorSettings& settings);
    void SetPath(const std::string& path);
    void Refresh();
    MarkdownView* Preview() const {return fPreview;}
    int64 RenderedRevision() const {return fRendered;}
    void ActivateLink(const std::string& link);
private:
    void Render();
    void SyncSource();
    void SyncPreview();
    Editor* fEditor;
    MarkdownView* fPreview;
    std::string fPath;
    std::unique_ptr<AsyncQueue> fJobs;
    std::unique_ptr<BMessageRunner> fTimer;
    int64 fObserved=-1,fRendered=-1,fSerial=0,fChangedAt=0,fFirst=-1;
    double fSourceLine=-1;
    bool fSubmitted=false,fSyncing=false;
};
}
