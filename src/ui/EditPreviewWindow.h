#pragma once
#include "core/EditTransaction.h"
#include "ui/Theme.h"
#include <Window.h>
#include <Messenger.h>
class BListView;class BTextView;class BStringView;class BButton;
namespace kiri {
class EditPreviewWindow : public BWindow {
public:
    EditPreviewWindow(BWindow* owner,const EditPlan& plan,const Theme& theme,bool restore,int64 serial);
    ~EditPreviewWindow() override;
    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
private:
    void Rebuild();
    void Detail();
    void SendAction(uint32 command);
    EditPlan fPlan;
    BMessenger fTarget;
    BListView* fList;
    BTextView* fDetail;
    BStringView* fStatus;
    BButton *fApply,*fToggle,*fRefresh,*fCancel;
    bool fRestore=false,fBusy=false;
    int64 fSerial=0;
    std::vector<std::pair<size_t,int>> fRows;
};
}
