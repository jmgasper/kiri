#pragma once
#include <GraphicsDefs.h>
#include <string>
#include <vector>
class BView;class BMenu;
namespace kiri {
struct Theme {
    std::string name;
    rgb_color background,panel,toolbar,text,muted,border,accent,selection,line,
        comment,keyword,string,number,type,added,removed;
    bool dark=true;
    static const std::vector<Theme>& Builtins();
};
int SciColor(rgb_color color);
int ThemeIndex(const std::string& name);
BMenu* ThemeMenu(const char* name,uint32 command,int selected);
void MarkTheme(BMenu* menu,int selected);
void ThemeView(BView* view,const Theme& theme);
}
