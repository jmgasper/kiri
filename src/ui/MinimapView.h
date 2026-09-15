#pragma once
#include "ui/Theme.h"
#include <View.h>
#include <MessageRunner.h>
#include <memory>
#include <vector>
#include <array>
namespace kiri {
class Editor;
// Samples at most 1024 x 80 cells from the existing Scintilla document. It owns
// neither another text buffer nor another lexer/layout for that document.
class MinimapView:public BView {
public:
    explicit MinimapView(Editor* editor);
    void AttachedToWindow() override;
    void DetachedFromWindow() override;
    void MessageReceived(BMessage* message) override;
    void Draw(BRect update) override;
    void MouseDown(BPoint where) override;
    void MouseMoved(BPoint where,uint32 transit,const BMessage* message) override;
    void MouseUp(BPoint where) override;
    void SetEnabled(bool enabled);
    void ApplyTheme(const Theme& theme);
    void InvalidateContent() {fStale=true;}
    void Refresh(bool force=false);
    BRect Viewport() const;
    void Navigate(float y,bool center=true);
    bool Enabled() const {return fEnabled;}
    size_t CacheBytes() const {return fCells.capacity()*sizeof(uint16_t);}
    const std::string& Status() const {return fStatus;}
    int64 DisplayLines() const {return fDisplayLines;}
private:
    void Timer();
    Editor* fEditor;
    Theme fTheme=Theme::Builtins()[0];
    std::unique_ptr<BMessageRunner> fTimer;
    std::vector<uint16_t> fCells;
    std::array<rgb_color,256> fPalette;
    std::string fStatus;
    int64 fRevision=-1,fDisplayLines=1,fFirst=0,fVisible=1;
    int fRows=0,fWidth=0,fZoom=0,fWrap=0;
    bool fEnabled=false,fStale=true,fDragging=false;
    float fDragOffset=0;
};
}
