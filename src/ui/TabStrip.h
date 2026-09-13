#pragma once
#include <View.h>
#include "ui/Theme.h"
#include "ui/Messages.h"
#include <string>
#include <vector>
namespace kiri {
struct TabLabel { std::string name,tooltip; bool dirty=false; int64 id=0; };
struct TabActions {
    uint32 select=kSelectTab,close=kCloseTab,closeAll=kCloseAllTabs,closeOthers=kCloseOtherTabs,create=0;
};
class TabStrip:public BView {
public:
    explicit TabStrip(const char* name="documents",TabActions actions={},const char* createLabel="New tab");
    void Draw(BRect update) override;
    void FrameResized(float width,float height) override;
    void MouseDown(BPoint where) override;
    void MouseMoved(BPoint where,uint32 transit,const BMessage* drag) override;
    void MessageReceived(BMessage* message) override;
    void SetTabs(std::vector<TabLabel> tabs,int selected);
    void ApplyTheme(const Theme& theme) { fTheme=theme;SetViewColor(theme.toolbar);Invalidate(); }
private:
    float Width(size_t index) const;
    float AvailableWidth() const;
    void EnsureSelectedVisible();
    BRect NewButtonRect() const;
    int HitTab(BPoint where) const;
    void SendAction(uint32 command,int index);
    void ContextMenu(BPoint where,int index);
    std::vector<TabLabel> fTabs;
    int fSelected=-1;
    float fOffset=0;
    TabActions fActions;
    std::string fCreateLabel;
    Theme fTheme=Theme::Builtins()[0];
};
}
