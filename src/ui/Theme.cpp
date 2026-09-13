#include "ui/Theme.h"
#include <Button.h>
#include <Control.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
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
        {"Nord",C(0x2e3440),C(0x343c49),C(0x3b4252),C(0xeceff4),C(0x9caec6),C(0x4c566a),C(0x88c0d0),C(0x4c566a),C(0x353e4e),C(0x899db7),C(0xb48ead),C(0xa3be8c),C(0xd08770),C(0x8fbcbb),C(0xa3be8c),C(0xbf616a),true},
        {"Midnight",C(0x101827),C(0x162135),C(0x1c2a42),C(0xdde7ff),C(0x8fa2c2),C(0x30415f),C(0x91b4ff),C(0x2f4269),C(0x19253d),C(0x889ab5),C(0xc4a7ff),C(0x9cdaa0),C(0xffcc85),C(0x75cfe1),C(0x91ce9c),C(0xff9bac),true},
        {"Forest",C(0x17231e),C(0x1e2d26),C(0x26392f),C(0xe0ece3),C(0x9bb1a2),C(0x3d5145),C(0xb4d594),C(0x355546),C(0x21362c),C(0x93aa97),C(0xceb0de),C(0xc6d88c),C(0xebba91),C(0x8acdbf),C(0xaad389),C(0xefadac),true},
        {"Ember",C(0x241c19),C(0x2f2420),C(0x3a2c26),C(0xefe0d3),C(0xb5a092),C(0x564136),C(0xe9b47e),C(0x67452e),C(0x342820),C(0xaf9b8a),C(0xdcb2d5),C(0xbed293),C(0xedb497),C(0x85c9c0),C(0xbdd892),C(0xf6a29c),true},
        {"Linen",C(0xfffaf0),C(0xf5eddf),C(0xeae0cf),C(0x40362b),C(0x7a6d5d),C(0xd9cbb6),C(0x986333),C(0xe9d4ad),C(0xf7eddc),C(0x81705e),C(0x864775),C(0x527038),C(0xa15131),C(0x316d79),C(0x417342),C(0xa1393f),false},
        {"Glacier",C(0xf5fbff),C(0xe9f2f8),C(0xdce8f1),C(0x233d50),C(0x5f7888),C(0xbfd0dd),C(0x2068a2),C(0xc1def3),C(0xe9f4fb),C(0x617b8b),C(0x6e4898),C(0x267257),C(0x9a5125),C(0x216d82),C(0x267141),C(0xbf3a52),false},
        {"Rose",C(0xfff8fa),C(0xf6edf1),C(0xf0dfe7),C(0x47323e),C(0x856a79),C(0xdec9d3),C(0x985373),C(0xebc7d9),C(0xf9eaf0),C(0x8b7080),C(0x84499b),C(0x427849),C(0xa0572b),C(0x3a7586),C(0x327344),C(0xc13e56),false},
        {"Meadow",C(0xf7fcf5),C(0xebf3e6),C(0xdeecd9),C(0x2f4231),C(0x647c65),C(0xc5d8bd),C(0x477b35),C(0xcbe3c1),C(0xeaf5e5),C(0x657c64),C(0x7c519b),C(0x37743e),C(0xa25a2c),C(0x256f81),C(0x307739),C(0xa93f4d),false}
    };
    return themes;
}
int SciColor(rgb_color color) { return color.red|(color.green<<8)|(color.blue<<16); }
int ThemeIndex(const std::string& name) {
    const auto& themes=Theme::Builtins();
    for(size_t i=0;i<themes.size();++i) if(themes[i].name==name) return i;
    return 0;
}
BMenu* ThemeMenu(const char* name,uint32 command,int selected) {
    auto* menu=new BPopUpMenu(name,true,true);
    for(bool dark:{true,false}) {
        if(!dark) menu->AddSeparatorItem();
        auto* heading=new BMenuItem(dark?"Dark themes":"Light themes",nullptr);
        heading->SetEnabled(false);menu->AddItem(heading);
        for(size_t i=0;i<Theme::Builtins().size();++i) {
            const auto& theme=Theme::Builtins()[i];
            if(theme.dark!=dark) continue;
            auto* message=new BMessage(command);message->AddString("theme_name",theme.name.c_str());
            auto* item=new BMenuItem(theme.name.c_str(),message);menu->AddItem(item);
            item->SetMarked(static_cast<int>(i)==selected);
        }
    }
    return menu;
}
void MarkTheme(BMenu* menu,int selected) {
    for(int32 i=0;i<menu->CountItems();++i) {
        auto* item=menu->ItemAt(i);const char* name;
        if(item->Message() && item->Message()->FindString("theme_name",&name)==B_OK)
            item->SetMarked(ThemeIndex(name)==selected);
    }
}
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
