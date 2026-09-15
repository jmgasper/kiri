#include "ui/EditPreviewWindow.h"
#include "ui/Messages.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextView.h>
#include <algorithm>

namespace kiri {
namespace {
std::string Visible(std::string text,size_t limit=180) {
    if(text.size()>limit) { auto end=limit;while(end && (static_cast<unsigned char>(text[end])&0xc0)==0x80) --end;text.resize(end);text+="…"; }
    std::string result;for(char c:text) { if(c=='\n') result+="\\n";else if(c=='\r') result+="\\r";else if(c=='\t') result+="\\t";else result+=c; }return result;
}
}
EditPreviewWindow::EditPreviewWindow(BWindow* owner,const EditPlan& plan,const Theme& theme,bool restore)
    :BWindow(BRect(0,0,880,650),restore?"Restore Project Edit":plan.title.c_str(),B_TITLED_WINDOW_LOOK,B_FLOATING_APP_WINDOW_FEEL,B_AUTO_UPDATE_SIZE_LIMITS),
    fPlan(plan),fTarget(owner),fRestore(restore) {
    fList=new BListView("edit files");fList->SetSelectionMessage(new BMessage(kEditSelect));fList->SetInvocationMessage(new BMessage(kEditToggle));
    fList->SetExplicitMinSize(BSize(640,250));
    fDetail=new BTextView("edit detail");fDetail->MakeEditable(false);fDetail->SetWordWrap(true);fDetail->SetExplicitMinSize(BSize(600,150));
    fStatus=new BStringView("edit status",plan.summary.c_str());fStatus->SetTruncation(B_TRUNCATE_END);fStatus->SetExplicitMinSize(BSize(200,24));
    fApply=new BButton("apply edits",restore?"Restore Changed Files":"Apply Reviewed Changes",new BMessage(kEditApply));
    fToggle=new BButton("toggle edits","Include / Exclude",new BMessage(kEditToggle));fToggle->SetEnabled(!plan.rename && !restore);
    fRefresh=new BButton("refresh preview","Refresh Preview",new BMessage(kEditRefresh));fRefresh->SetEnabled(!restore);
    fCancel=new BButton("cancel edits","Close",new BMessage(kEditCancel));
    auto* policy=new BStringView("edit policy",restore?"Restore checks current text first. Newer changes are kept and reported.":
        "Open files: one undo step, left unsaved. Closed files: saved after Apply. Edit → Undo Last Project Edit restores the operation.");
    auto* panel=new BView("edit preview",B_WILL_DRAW);
    BLayoutBuilder::Group<>(panel,B_VERTICAL,8).SetInsets(12).Add(policy)
        .Add(new BScrollView("edit list scroll",fList,0,false,true,B_NO_BORDER))
        .Add(new BScrollView("edit detail scroll",fDetail,0,false,true,B_NO_BORDER)).Add(fStatus)
        .AddGroup(B_HORIZONTAL,8).Add(fToggle).Add(fRefresh).AddGlue().Add(fCancel).Add(fApply);
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);
    for(auto* button:{fApply,fToggle,fRefresh,fCancel}) button->SetTarget(this);fList->SetTarget(this);
    ThemeView(panel,theme);Rebuild();ResizeTo(960,690);CenterIn(owner->Frame());Show();
}
EditPreviewWindow::~EditPreviewWindow() { while(auto* item=fList->RemoveItem(int32(0))) delete item; }
bool EditPreviewWindow::QuitRequested() { if(fBusy) { fTarget.SendMessage(kEditCancel);fStatus->SetText("Cancelling after the current file finishes…");return false; }return true; }
void EditPreviewWindow::Rebuild() {
    auto selection=fList->CurrentSelection();while(auto* item=fList->RemoveItem(int32(0))) delete item;fRows.clear();
    size_t selected=0;
    for(size_t i=0;i<fPlan.files.size();++i) {
        auto& file=fPlan.files[i];auto count=std::count_if(file.edits.begin(),file.edits.end(),[](const auto& e){return e.selected;});selected+=count;
        auto label=std::string(count?"[x] ":"[ ] ")+file.before.path+(file.before.open?"  (open buffer)":"  (disk)")+" · "+file.status;
        if(!file.error.empty()) label+=" · "+file.error;
        fList->AddItem(new BStringItem(label.c_str()));fRows.emplace_back(i,-1);
        if(!fRestore) for(size_t j=0;j<file.edits.size();++j) {
            auto& item=file.edits[j];auto position=PositionAt(file.before.text,item.edit.start,PositionEncoding::UTF32);
            label=std::string(item.selected?"    [x] ":"    [ ] ")+std::to_string(position.line+1)+":"+std::to_string(position.character+1)+"  "
                +Visible(file.before.text.substr(item.edit.start,item.edit.end-item.edit.start))+" → "+Visible(item.edit.text);
            fList->AddItem(new BStringItem(label.c_str()));fRows.emplace_back(i,int(j));
        }
    }
    fApply->SetEnabled(!fBusy && (fRestore?!fPlan.files.empty():selected>0));
    if(!fRows.empty()) fList->Select(std::clamp(selection,int32(0),int32(fRows.size()-1)));
    Detail();
}
void EditPreviewWindow::Detail() {
    auto row=fList->CurrentSelection();if(row<0 || size_t(row)>=fRows.size()) return;
    auto [i,j]=fRows[row];auto& file=fPlan.files[i];std::string detail=file.before.path+"\n";
    if(j>=0) {
        auto& edit=file.edits[j].edit;auto start=edit.start?file.before.text.rfind('\n',edit.start-1):std::string::npos;start=start==std::string::npos?0:start+1;
        auto end=file.before.text.find('\n',edit.end);if(end==std::string::npos) end=file.before.text.size();
        detail+="Before:\n"+file.before.text.substr(start,std::min<size_t>(end-start,12000))+"\n\nAfter:\n";
        auto context=file.before.text.substr(start,end-start);context.replace(edit.start-start,edit.end-edit.start,edit.text);detail+=context.substr(0,12000);
    } else { if(!fRestore) file.Prepare();detail+="Before:\n"+file.before.text.substr(0,12000)+"\n\nAfter:\n"+file.after.substr(0,12000); }
    if(!file.error.empty()) detail+="\n\n"+file.error;
    fDetail->SetText(detail.c_str());
}
void EditPreviewWindow::MessageReceived(BMessage* message) {
    if(message->what==kEditSelect) Detail();
    else if(message->what==kEditToggle && !fBusy && !fRestore && !fPlan.rename) {
        auto row=fList->CurrentSelection();if(row<0 || size_t(row)>=fRows.size()) return;
        auto [i,j]=fRows[row];auto& file=fPlan.files[i];
        if(j>=0) file.edits[j].selected=!file.edits[j].selected;
        else { bool selected=std::any_of(file.edits.begin(),file.edits.end(),[](const auto& e){return e.selected;});for(auto& item:file.edits) item.selected=!selected; }
        Rebuild();
    } else if(message->what==kEditApply && !fBusy) {
        BMessage apply(kEditApply);apply.AddBool("restore",fRestore);
        for(size_t i=0;i<fPlan.files.size();++i) for(size_t j=0;j<fPlan.files[i].edits.size();++j)
            if(!fPlan.files[i].edits[j].selected) { BMessage omit;omit.AddInt32("file",i);omit.AddInt32("match",j);apply.AddMessage("omit",&omit); }
        fTarget.SendMessage(&apply);
    } else if(message->what==kEditRefresh && !fBusy) fTarget.SendMessage(kEditRefresh);
    else if(message->what==kEditCancel) { if(fBusy) fTarget.SendMessage(kEditCancel);else PostMessage(B_QUIT_REQUESTED); }
    else if(message->what==kEditPreview) {
        message->FindBool("busy",&fBusy);const char* status=nullptr;if(message->FindString("status",&status)==B_OK) { fStatus->SetText(status);fStatus->SetToolTip(status); }
        int32 file=-1;message->FindInt32("file",&file);
        if(file>=0 && size_t(file)<fPlan.files.size()) { if(message->FindString("file_status",&status)==B_OK) fPlan.files[file].status=status;if(message->FindString("error",&status)==B_OK) fPlan.files[file].error=status; }
        fCancel->SetLabel(fBusy?"Cancel remaining":"Close");fToggle->SetEnabled(!fBusy && !fPlan.rename && !fRestore);fRefresh->SetEnabled(!fBusy && !fRestore);Rebuild();
        bool finished=false;message->FindBool("finished",&finished);if(finished) fApply->SetEnabled(false);
    } else BWindow::MessageReceived(message);
}
}
