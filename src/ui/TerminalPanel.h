#pragma once
#include "ui/Theme.h"
#include <View.h>
#include <string>
#include <vector>

class BCardLayout;
namespace kiri {
class TabStrip;
class TerminalView;
class TerminalPanel : public BView {
public:
    TerminalPanel();
    void NewTerminal(const std::string& directory,bool focus=true);
    void SelectTerminal(int index,bool focus=true);
    void CloseTerminal(int index);
    void CloseTerminals(int keep=-1);
    void UpdateTabs();
    void ApplyTheme(const Theme& theme);
    int IndexForMessage(const BMessage& message) const;
    bool Empty() const { return fSessions.empty(); }
    bool OwnsFocus() const;
private:
    struct Session { int64 id;std::string directory;TerminalView* view; };
    std::vector<Session> fSessions;
    int fSelected=-1;
    int64 fNextID=1;
    TabStrip* fTabs;
    BCardLayout* fLayout;
    Theme fTheme=Theme::Builtins()[0];
};
}
