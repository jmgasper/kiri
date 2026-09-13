#include "ui/Theme.h"
#include <Button.h>
#include <Control.h>
#include <ListView.h>
#include <MenuBar.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>
namespace kiri {
namespace { constexpr rgb_color C(unsigned value) { return {uint8(value>>16),uint8(value>>8),uint8(value),255}; } }
const std::vector<Theme>& Theme::Builtins() {
    static const std::vector<Theme> themes={
        {"Obsidian",C(0x171b22),C(0x202630),C(0x282f3a),C(0xdce3ed),C(0x8c9bad),C(0x343e4b),C(0x78b7ff),C(0x334e70),C(0x202936),C(0x8195a6),C(0xc6a0f6),C(0xa6da95),C(0xf5a97f),C(0x8bd5ca),C(0xa6da95),C(0xed8796),true},
        {"Daylight",C(0xfafbfD),C(0xf0f2f6),C(0xe5e9f0),C(0x273448),C(0x68788d),C(0xcfd6e0),C(0x2567c6),C(0xc8ddf8),C(0xf0f4fa),C(0x718096),C(0x8047a8),C(0x3d7a40),C(0xa55028),C(0x187f86),C(0x24723b),C(0xc1384c),false},
        {"Nord",C(0x2e3440),C(0x343c49),C(0x3b4252),C(0xeceff4),C(0x9caec6),C(0x4c566a),C(0x88c0d0),C(0x4c566a),C(0x353e4e),C(0x899db7),C(0xb48ead),C(0xa3be8c),C(0xd08770),C(0x8fbcbb),C(0xa3be8c),C(0xbf616a),true}
    };
    return themes;
}
int SciColor(rgb_color color) { return color.red|(color.green<<8)|(color.blue<<16); }
void ThemeView(BView* view,const Theme& t) {
    if(!view) return;
    if(auto* label=dynamic_cast<BStringView*>(view)) {
        label->SetExplicitMinSize(BSize(0,B_SIZE_UNSET));
        label->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNSET));
    }
    if(dynamic_cast<BMenuBar*>(view)) {
        // Haiku's menu text and shortcuts follow the desktop's menu palette.
        // Keep that native surface readable when only the document theme is dark.
        view->SetViewColor(ui_color(B_MENU_BACKGROUND_COLOR));view->SetLowColor(ui_color(B_MENU_BACKGROUND_COLOR));view->Invalidate();return;
    }
    view->SetViewColor(t.panel);view->SetLowColor(t.panel);view->SetHighColor(t.text);
    view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR,B_NO_TINT);
    view->SetViewColor(t.panel); // explicit per-window colors, independent of global preferences
    if(auto text=dynamic_cast<BTextView*>(view)) {
        text->SetViewColor(t.background);text->SetLowColor(t.background);
        text->SetFontAndColor(nullptr,0,&t.text);
    }
    if(auto list=dynamic_cast<BListView*>(view)) { list->SetViewColor(t.panel);list->SetLowColor(t.panel); }
    for(int32 i=0;i<view->CountChildren();++i) ThemeView(view->ChildAt(i),t);
    view->Invalidate();
}
}
