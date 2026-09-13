#pragma once
#include "ui/Theme.h"
#include <Control.h>

namespace kiri {
class RefreshButton : public BControl {
public:
    explicit RefreshButton(BMessage* message);
    void Draw(BRect update) override;
    void MouseDown(BPoint where) override;
    void MouseUp(BPoint where) override;
    void MouseMoved(BPoint where,uint32 transit,const BMessage* drag) override;
    void KeyDown(const char* bytes,int32 count) override;
    void ApplyTheme(const Theme& theme);
private:
    Theme fTheme=Theme::Builtins()[0];
    bool fPressed=false,fHovered=false;
};
}
