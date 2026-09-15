#include "ui/ProblemsView.h"
#include "ui/Messages.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>
#include <Window.h>
#include <Font.h>
#include <String.h>
#include <algorithm>

namespace kiri {
namespace {
constexpr uint32 kSelected='pbsl',kInvoked='pbiv',kSeverity='pbsv';
class ProblemItem:public BStringItem {
public:
    ProblemItem(const std::string& text,const Theme* theme,bool heading=false):BStringItem(text.c_str()),fTheme(theme),fHeading(heading) {}
    void DrawItem(BView* owner,BRect frame,bool) override {
        owner->SetHighColor(IsSelected()?fTheme->selection:fTheme->background);owner->FillRect(frame);owner->SetLowColor(owner->HighColor());
        owner->SetHighColor(IsSelected()?fTheme->selectionText:fTheme->text);BFont font;owner->GetFont(&font);BFont saved(font);if(fHeading) font.SetFace(B_BOLD_FACE);owner->SetFont(&font);
        font_height metrics;font.GetHeight(&metrics);BString label(Text());owner->TruncateString(&label,B_TRUNCATE_END,frame.Width()-12);
        owner->DrawString(label.String(),BPoint(frame.left+6,frame.top+metrics.ascent+2));owner->SetFont(&saved);
    }
private:const Theme* fTheme;bool fHeading;
};
}
ProblemsView::ProblemsView():BView("problems panel",B_WILL_DRAW) {
    fList=new BListView("problems");fList->SetSelectionMessage(new BMessage(kSelected));fList->SetInvocationMessage(new BMessage(kInvoked));
    fDetails=new BTextView("problem details");fDetails->MakeEditable(false);fDetails->SetWordWrap(true);fDetails->SetExplicitMinSize(BSize(160,40));fDetails->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,70));
    fStatus=new BStringView("problems status","Open a source file to receive problems.");fStatus->SetTruncation(B_TRUNCATE_END);fStatus->SetExplicitMinSize(BSize(100,22));
    auto* menu=new BPopUpMenu("severity",true,true);
    auto* details=new BScrollView("problem details scroll",fDetails,0,false,true,B_NO_BORDER);
    details->SetExplicitMinSize(BSize(160,50));details->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,70));
    const char* choices[]={"All severities","Errors","Warnings","Information","Hints"};
    for(int i=0;i<5;++i) {auto* message=new BMessage(kSeverity);message->AddInt32("severity",i);auto* item=new BMenuItem(choices[i],message);menu->AddItem(item);item->SetMarked(i==0);}
    fSeverity=new BMenuField("problem severity","",menu);
    BLayoutBuilder::Group<>(this,B_VERTICAL,4).SetInsets(8)
        .AddGroup(B_HORIZONTAL,6).Add(new BStringView("problems title","PROBLEMS · OPEN FILES")).Add(fSeverity).AddGlue()
            .Add(new BButton("previous problem","Previous",new BMessage(kPreviousProblem)))
            .Add(new BButton("next problem","Next",new BMessage(kNextProblem)))
            .Add(new BButton("hide problems","Close",new BMessage(kToggleProblems))).End()
        .Add(new BScrollView("problems scroll",fList,0,false,true,B_NO_BORDER)).Add(details).Add(fStatus);
    SetExplicitMinSize(BSize(350,230));
}
ProblemsView::~ProblemsView() {while(auto* item=fList->RemoveItem(int32(0))) delete item;}
void ProblemsView::AttachedToWindow() {
    BView::AttachedToWindow();fList->SetTarget(this);fSeverity->Menu()->SetTargetForItems(this);
    for(const char* name:{"previous problem","next problem","hide problems"}) static_cast<BButton*>(FindView(name))->SetTarget(Window());
}
void ProblemsView::ApplyTheme(const Theme& theme) {fTheme=theme;ThemeView(this,theme);fList->SetViewColor(theme.background);fList->SetLowColor(theme.background);}
const ProblemEntry* ProblemsView::Selected() const {auto selected=fList->CurrentSelection();return selected>=0 && size_t(selected)<fRows.size() && fRows[selected]>=0?&fEntries[fRows[selected]]:nullptr;}
void ProblemsView::SetProblems(std::vector<ProblemEntry> entries,const std::string& status) {
    int64 document=0;size_t position=0;if(auto* selected=Selected()) {document=selected->document;position=selected->diagnostic.start;}
    auto scroll=fList->Bounds().LeftTop();fEntries=std::move(entries);
    std::stable_sort(fEntries.begin(),fEntries.end(),[](const auto& a,const auto& b){return a.path==b.path?(a.diagnostic.start==b.diagnostic.start?a.diagnostic.severity<b.diagnostic.severity:a.diagnostic.start<b.diagnostic.start):a.path<b.path;});
    Rebuild();SelectProblem(document,position);fList->ScrollTo(scroll);fStatus->SetText(status.substr(0,status.find('\n')).c_str());fStatus->SetToolTip(status.c_str());
}
void ProblemsView::Rebuild() {
    while(auto* item=fList->RemoveItem(int32(0))) delete item;fRows.clear();fDetails->SetText("");std::string previous;
    for(size_t i=0;i<fEntries.size();++i) {
        const auto& entry=fEntries[i];auto& problem=entry.diagnostic;if(fFilter && problem.severity!=fFilter) continue;
        if(previous!=entry.path) {previous=entry.path;fList->AddItem(new ProblemItem(previous,&fTheme,true));fRows.push_back(-1);}
        auto message=problem.message;std::replace(message.begin(),message.end(),'\n',' ');if(message.size()>300) {size_t end=300;while(end && (static_cast<unsigned char>(message[end])&0xc0)==0x80) --end;message=message.substr(0,end)+"…";}
        auto label=std::string("    ")+SeverityName(problem.severity)+" · "+std::to_string(problem.line+1)+":"+std::to_string(problem.column+1)+" · "+message;
        fList->AddItem(new ProblemItem(label,&fTheme));fRows.push_back(i);
    }
}
void ProblemsView::SelectProblem(int64 document,size_t start) {
    for(size_t i=0;i<fRows.size();++i) if(fRows[i]>=0) {auto& item=fEntries[fRows[i]];if(item.document==document && item.diagnostic.start==start) {fList->Select(i);break;}}
}
void ProblemsView::MessageReceived(BMessage* message) {
    if(message->what==kSeverity) {message->FindInt32("severity",&fFilter);Rebuild();}
    else if(message->what==kSelected || message->what==kInvoked) {
        if(auto* entry=Selected()) {
            auto& item=entry->diagnostic;auto detail=std::string(SeverityName(item.severity))+" · "+item.source+(item.code.empty()?"":" "+item.code)+"\n"+item.message;fDetails->SetText(detail.c_str());
            if(message->what==kInvoked) {BMessage jump(kJumpProblem);jump.AddInt64("document",entry->document);jump.AddInt64("serial",entry->serial);jump.AddInt64("offset",item.start);Window()->PostMessage(&jump);}
        }
    }else BView::MessageReceived(message);
}
}
