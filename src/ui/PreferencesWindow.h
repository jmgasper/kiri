#pragma once
#include "ui/EditorSettings.h"
#include <Messenger.h>
#include <Window.h>
#include <memory>

class BMenuField;class BTextControl;class BStringView;class BButton;class BFilePanel;class BCheckBox;
namespace kiri {
class Editor;
class PreferencesWindow:public BWindow {
public:
    PreferencesWindow(BMessenger target,const EditorSettings& settings,BRect parentFrame,const std::string& directory={});
    ~PreferencesWindow() override;
    void MessageReceived(BMessage* message) override;
private:
    void LoadControls();
    bool UpdatePreview();
    bool Apply();
    void ThemeChoices();
    void ReadTheme(BMessage& message);
    void ExportTheme(BMessage& message);
    BMessenger fTarget;
    BMessenger fEditingWindow;
    EditorSettings fApplied,fPending;
    BMenuField* fFont;
    BMenuField* fTheme;
    BTextControl* fSize;
    BStringView* fValidation;
    Editor* fPreview;
    BButton* fApply;
    BButton* fOK;
    BMenuField* fRole;
    BTextControl *fName,*fColor;
    BCheckBox *fDark,*fSemantic;
    std::string fDirectory,fColorRole="background",fThemeNotice;
    std::unique_ptr<BFilePanel> fThemePanel;
    bool fLoading=false;
};
}
