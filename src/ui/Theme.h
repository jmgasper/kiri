#pragma once
#include <GraphicsDefs.h>
#include <string>
#include <vector>
class BView;
namespace kiri {
struct Theme {
    std::string name;
    rgb_color background,panel,toolbar,text,muted,border,accent,selection,line,
        comment,keyword,string,number,type,added,removed;
    bool dark=true;
    static const std::vector<Theme>& Builtins();
};
int SciColor(rgb_color color);
void ThemeView(BView* view,const Theme& theme);
}
