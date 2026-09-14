#pragma once
#include "ui/Theme.h"
#include <Bitmap.h>
#include <View.h>
#include <memory>
namespace kiri {
class PreviewView:public BView {
public:
    PreviewView(std::shared_ptr<BBitmap> bitmap,std::string name,uint64_t bytes);
    PreviewView* Clone() const;
    void Draw(BRect update) override;
    void MouseDown(BPoint point) override;
    void MouseMoved(BPoint point,uint32 transit,const BMessage* drag) override;
    void MouseUp(BPoint point) override;
    void MessageReceived(BMessage* message) override;
    void ApplyTheme(const Theme& theme) { fTheme=theme;SetViewColor(theme.background);Invalidate(); }
private:
    std::shared_ptr<BBitmap> fBitmap;
    std::string fName;
    uint64_t fBytes;
    bool fActualSize=false;
    bool fDragging=false;
    BPoint fPan,fDragPoint;
    Theme fTheme=Theme::Builtins()[0];
};
}
