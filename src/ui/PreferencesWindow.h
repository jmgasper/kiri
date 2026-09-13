#pragma once
#include "ui/EditorSettings.h"
#include <Messenger.h>
#include <Window.h>

class BMenuField;class BTextControl;class BStringView;class BButton;
namespace kiri {
class Editor;
class PreferencesWindow:public BWindow {
public:
    PreferencesWindow(BMessenger target,const EditorSettings& settings,BRect parentFrame);
    void MessageReceived(BMessage* message) override;
private:
    void LoadControls();
    bool UpdatePreview();
    bool Apply();
    BMessenger fTarget;
    EditorSettings fApplied,fPending;
    BMenuField* fFont;
    BMenuField* fTheme;
    BTextControl* fSize;
    BStringView* fValidation;
    Editor* fPreview;
    BButton* fApply;
    BButton* fOK;
};
}
