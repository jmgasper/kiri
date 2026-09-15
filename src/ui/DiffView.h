#pragma once
#include "core/Diff.h"
#include "ui/Theme.h"
#include "ui/EditorSettings.h"
#include <View.h>
#include <memory>
class BCardLayout;class BStringView;class BCheckBox;class BButton;
namespace kiri {
class Editor;class DiffEditor;
class DiffView:public BView {
public:
    DiffView();
    void AttachedToWindow() override;
    void AllAttached() override;
    void FrameResized(float width,float height) override;
    void MessageReceived(BMessage* message) override;
    void SetModel(std::shared_ptr<const DiffModel> model);
    void Clear(const std::string& message);
    void ApplySettings(const EditorSettings& settings);
    void ApplyTheme(const Theme& theme);
    void SetSideBySide(bool split);
    void SetWrap(bool wrap);
    void Navigate(int direction);
    void Align();
    void Scrolled(DiffEditor* editor,int updated);
    Editor* Left() const;Editor* Right() const;
    std::shared_ptr<const DiffModel> Model() const { return fModel; }
private:
    void Decorate();
    void StyleNavigation();
    void QueueAlignment();
    DiffEditor *fLeft,*fRight;
    Editor* fUnified;
    BCardLayout* fCards;
    BStringView *fLeftLabel,*fRightLabel,*fStatus;
    BCheckBox *fSideBySide,*fWrap;
    BButton *fPrevious,*fNext;
    std::shared_ptr<const DiffModel> fModel;
    std::vector<int> fLeftPadding,fRightPadding;
    std::vector<size_t> fUnifiedHunks;
    Theme fTheme=Theme::Builtins()[0];
    bool fSyncing=false,fAlignQueued=false;
    int64 fLastTop[2]={0,0},fLastHorizontal[2]={0,0};
    int fHunk=-1;
};
}
