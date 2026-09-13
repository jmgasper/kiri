#pragma once
#include "ui/RecentItems.h"
#include <Window.h>
#include <memory>

class BButton;class BFilePanel;class BListView;class BCardLayout;class BStringView;
namespace kiri {
class LauncherWindow:public BWindow {
public:
    explicit LauncherWindow(const std::string& settingsDirectory);
    ~LauncherWindow() override;
    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
private:
    void Refresh();
    void OpenRecent();
    void ShowPicker(bool folder);
    std::string fSettings;
    std::vector<RecentItem> fItems;
    BListView* fRecents;
    BCardLayout* fRecentLayout;
    BButton* fOpen;
    BStringView* fHint;
    std::unique_ptr<BFilePanel> fFolderPanel,fFilePanel;
};
}
