#pragma once
#include "core/LanguageTools.h"
#include <Messenger.h>
#include <Window.h>

class BTextControl;class BCheckBox;class BPopUpMenu;class BStringView;
namespace kiri {
class LanguageToolsWindow:public BWindow {
public:
    LanguageToolsWindow(BMessenger target,LanguageTools tools,std::string settings,BRect parent);
    void MessageReceived(BMessage* message) override;
private:
    void Select(int index);
    bool Store();
    BMessenger fTarget;
    LanguageTools fTools;
    std::string fSettings;
    BTextControl* fPrettier;
    BTextControl* fCommand;
    BCheckBox* fAutomatic;
    BPopUpMenu* fLanguages;
    BStringView* fHint;
    int fSelected=0;
};
}
