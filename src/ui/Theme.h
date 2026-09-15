#pragma once
#include <GraphicsDefs.h>
#include "core/ColorTheme.h"
#include <array>
#include <string>
#include <vector>
class BView;class BMenu;class BWindow;class BMessage;
namespace kiri {
struct Theme {
    std::string name;
    rgb_color background,panel,toolbar,text,muted,border,accent,selection,line,
        comment,keyword,string,number,type,added,removed;
    bool dark=true;
    ColorTheme definition;
    rgb_color selectionText,terminalText,terminalBackground;
    std::array<rgb_color,8> semantic;
    std::array<rgb_color,4> diagnostic;
    std::array<rgb_color,16> terminalANSI;
    static const std::vector<Theme>& Builtins();
    static Theme FromDefinition(const ColorTheme& definition);
};
int SciColor(rgb_color color);
int ThemeIndex(const std::string& name);
BMenu* ThemeMenu(const char* name,uint32 command,int selected,const std::string& settings={},const std::string& selectedID={});
void PopulateThemeMenu(BMenu* menu,uint32 command,int selected,const std::string& settings={},const std::string& selectedID={});
void MarkTheme(BMenu* menu,int selected,const std::string& selectedID={});
void ThemeView(BView* view,const Theme& theme);
void ThemeWindow(BWindow* window,const BMessage& settings);
}
