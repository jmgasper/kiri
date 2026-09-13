#pragma once
#include <View.h>
#include "ui/Theme.h"
#include <string>
#include <vector>
namespace kiri {
struct TabLabel { std::string name,tooltip; bool dirty=false; };
class TabStrip:public BView {
public:
    TabStrip();
    void Draw(BRect update) override;
    void MouseDown(BPoint where) override;
    void MessageReceived(BMessage* message) override;
    void SetTabs(std::vector<TabLabel> tabs,int selected);
    void ApplyTheme(const Theme& theme) { fTheme=theme;SetViewColor(theme.toolbar);Invalidate(); }
private:
    float Width(size_t index) const;
    std::vector<TabLabel> fTabs;
    int fSelected=-1;
    float fOffset=0;
    Theme fTheme=Theme::Builtins()[0];
};
}
