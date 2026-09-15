#include "ui/Theme.h"
#include "ui/Editor.h"
#include <Window.h>
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
#include <algorithm>
namespace kiri {
namespace { constexpr rgb_color C(unsigned value) { return {uint8(value>>16),uint8(value>>8),uint8(value),255}; } }
Theme Theme::FromDefinition(const ColorTheme& definition) {
    Theme theme;theme.name=definition.name;theme.dark=definition.dark;theme.definition=definition;
    auto color=[&](const char* role){return C(definition.colors.at(role));};
    theme.background=color("background");
    theme.panel=color("panel");
    theme.toolbar=color("toolbar");
    theme.text=color("text");
    theme.muted=color("muted");
    theme.border=color("border");
    theme.accent=color("accent");
    theme.selection=color("selection");
    theme.line=color("line");
    theme.comment=color("comment");
    theme.keyword=color("keyword");
    theme.string=color("string");
    theme.number=color("number");
    theme.type=color("type");
    theme.added=color("added");
    theme.removed=color("removed");
    theme.selectionText=color("selection.text");theme.terminalText=color("terminal.text");theme.terminalBackground=color("terminal.background");
    const char* roles[]={"type","parameter","property","function","variable","namespace","readonly","keyword"};
    for(int i=0;i<8;++i) theme.semantic[i]=color(roles[i]);
    const char* severities[]={"diagnostic.error","diagnostic.warning","diagnostic.info","diagnostic.hint"};
    for(int i=0;i<4;++i) theme.diagnostic[i]=color(severities[i]);
    for(int i=0;i<16;++i) theme.terminalANSI[i]=color(("terminal.ansi"+std::to_string(i)).c_str());
    return theme;
}
const std::vector<Theme>& Theme::Builtins() {
    static const auto themes=[] {std::vector<Theme> values;for(const auto& definition:BuiltinColorThemes()) values.push_back(FromDefinition(definition));return values;}();return themes;
}
int SciColor(rgb_color color) { return color.red|(color.green<<8)|(color.blue<<16); }
int ThemeIndex(const std::string& name) {
    const auto& themes=Theme::Builtins();
    for(size_t i=0;i<themes.size();++i) if(themes[i].name==name) return i;
    return 0;
}
BMenu* ThemeMenu(const char* name,uint32 command,int selected,const std::string& settings,const std::string& selectedID) {
    auto* menu=new BPopUpMenu(name,true,true);PopulateThemeMenu(menu,command,selected,settings,selectedID);return menu;
}
void PopulateThemeMenu(BMenu* menu,uint32 command,int selected,const std::string& settings,const std::string& selectedID) {
    while(auto* item=menu->RemoveItem(int32(0))) delete item;
    auto themes=LoadColorThemes(settings);
    for(bool dark:{true,false}) {
        if(!dark) menu->AddSeparatorItem();
        auto* heading=new BMenuItem(dark?"Dark themes":"Light themes",nullptr);heading->SetEnabled(false);menu->AddItem(heading);
        for(const auto& theme:themes) {
            if(theme.dark!=dark) continue;
            auto* message=new BMessage(command);message->AddString("theme_name",theme.name.c_str());message->AddString("theme_id",theme.id.c_str());
            menu->AddItem(new BMenuItem(theme.name.c_str(),message));
        }
    }
    MarkTheme(menu,selected,selectedID);
}
void MarkTheme(BMenu* menu,int selected,const std::string& selectedID) {
    auto id=selectedID.empty()?Theme::Builtins()[std::clamp(selected,0,int(Theme::Builtins().size())-1)].definition.id:selectedID;
    for(int32 i=0;i<menu->CountItems();++i) { auto* item=menu->ItemAt(i);const char* value=nullptr;
        if(item->Message() && item->Message()->FindString("theme_id",&value)==B_OK) item->SetMarked(id==value);
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
void ThemeWindow(BWindow* window,const BMessage& message) {
    EditorSettings settings;settings.ReadFrom(message);auto theme=settings.Colors();
    auto apply=[&](auto&& self,BView* view)->void {
        if(auto* editor=dynamic_cast<Editor*>(view)) {editor->ApplySettings(settings);return;}
        for(int32 i=0;i<view->CountChildren();++i) self(self,view->ChildAt(i));
    };
    for(int32 i=0;i<window->CountChildren();++i) {ThemeView(window->ChildAt(i),theme);apply(apply,window->ChildAt(i));}
}
}
