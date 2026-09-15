#pragma once
#include "ui/EditorSettings.h"
#include <Window.h>
#include <Messenger.h>
class BTextControl;class BCheckBox;class BStringView;class BButton;
namespace kiri {
class DocumentSettingsWindow:public BWindow {
public:
    DocumentSettingsWindow(BMessenger target,const EditorSettings& defaults,const DocumentStyle& style,
        const DocumentOverrides& overrides,BRect parent,int64 document=-1,const std::string& name={});
    void MessageReceived(BMessage* message) override;
private:
    bool Validate();
    BMessenger fTarget;
    EditorSettings fDefaults;
    int64 fDocument;
    BCheckBox *fOverrideIndent,*fOverrideGuides,*fTabs,*fMinimap;
    BTextControl *fIndent,*fTabWidth,*fGuides;
    BStringView* fValidation;
    BButton* fApply;
};
}
