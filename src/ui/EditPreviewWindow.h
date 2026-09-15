#pragma once
#include "core/EditTransaction.h"
#include "ui/Theme.h"
#include <Window.h>
#include <Messenger.h>
class BListView;class BTextView;class BStringView;class BButton;
namespace kiri {
class EditPreviewWindow : public BWindow {
public:
    EditPreviewWindow(BWindow* owner,const EditPlan& plan,const Theme& theme,bool restore);
    ~EditPreviewWindow() override;
    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
private:
    void Rebuild();
    void Detail();
    EditPlan fPlan;
    BMessenger fTarget;
    BListView* fList;
    BTextView* fDetail;
    BStringView* fStatus;
    BButton *fApply,*fToggle,*fRefresh,*fCancel;
    bool fRestore=false,fBusy=false;
    std::vector<std::pair<size_t,int>> fRows;
};
}
