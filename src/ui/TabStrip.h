#pragma once
#include <View.h>
#include "ui/Theme.h"
#include "ui/Messages.h"
#include <string>
#include <vector>
#include <memory>
class BBitmap;
class BMessageRunner;
namespace kiri {
struct TabLabel { std::string name,tooltip; bool dirty=false; int64 id=0; std::shared_ptr<const BBitmap> icon;bool preview=false; };
struct TabActions {
    uint32 select=kSelectTab,close=kCloseTab,closeAll=kCloseAllTabs,closeOthers=kCloseOtherTabs,create=0;
};
class TabStrip:public BView {
public:
    explicit TabStrip(const char* name="documents",TabActions actions={},const char* createLabel="New tab");
    ~TabStrip() override;
    void Draw(BRect update) override;
    void FrameResized(float width,float height) override;
    void MouseDown(BPoint where) override;
    void MouseMoved(BPoint where,uint32 transit,const BMessage* drag) override;
    void MouseUp(BPoint where) override;
    void KeyDown(const char* bytes,int32 count) override;
    void DetachedFromWindow() override;
    void MessageReceived(BMessage* message) override;
    void SetTabs(std::vector<TabLabel> tabs,int selected);
    void SetPane(int64 pane) { fPane=pane; }
    void SetActive(bool active) { if(fActive!=active) { fActive=active;Invalidate(); } }
    int DropIndex(BPoint where,bool scroll=false);
    void SetDropIndex(int index);
    void ApplyTheme(const Theme& theme) { fTheme=theme;SetViewColor(theme.toolbar);Invalidate(); }
private:
    float Width(size_t index) const;
    float AvailableWidth() const;
    void EnsureSelectedVisible();
    BRect NewButtonRect() const;
    int HitTab(BPoint where) const;
    void SendAction(uint32 command,int index);
    void ContextMenu(BPoint where,int index);
    void DragUpdate(BPoint where);
    void EndDrag(bool drop,BPoint where={});
    std::vector<TabLabel> fTabs;
    int fSelected=-1;
    int64 fPane=0;
    bool fActive=true;
    float fOffset=0;
    int fDropIndex=-1;
    int64 fPressedTab=0;
    BPoint fPressPoint;
    bool fDragging=false;
    std::unique_ptr<BMessageRunner> fDragTimer;
    TabActions fActions;
    std::string fCreateLabel;
    Theme fTheme=Theme::Builtins()[0];
};
}
