#include "ui/GitView.h"
#include "ui/Editor.h"
#include "ui/Messages.h"
#include <Alert.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <algorithm>
namespace kiri {
namespace {
void ClearList(BListView* list) { while(auto* item=list->RemoveItem(int32(0))) delete item; }
class ChangeItem:public BListItem {
public:
    ChangeItem(GitFile value,const Theme* theme):fFile(std::move(value)),fTheme(theme) {}
    void Update(BView* owner,const BFont* font) override { BListItem::Update(owner,font);SetHeight(27); }
    void DrawItem(BView* owner,BRect rect,bool) override {
        const auto& t=*fTheme;owner->SetHighColor(IsSelected()?t.selection:t.panel);owner->FillRect(rect);owner->SetLowColor(IsSelected()?t.selection:t.panel);
        std::string status;status+=fFile.index;status+=fFile.worktree;
        owner->SetHighColor(fFile.index!=' ' && fFile.index!='?'?t.added:t.number);owner->DrawString(status.c_str(),BPoint(rect.left+8,rect.top+19));
        owner->SetHighColor(t.text);BString path(fFile.path.c_str());owner->TruncateString(&path,B_TRUNCATE_MIDDLE,rect.Width()-47);owner->DrawString(path.String(),BPoint(rect.left+38,rect.top+19));
    }
private:GitFile fFile;const Theme* fTheme;
};
class HistoryItem:public BListItem {
public:
    HistoryItem(Commit commit,GraphRow graph,const Theme* theme):fCommit(std::move(commit)),fGraph(std::move(graph)),fTheme(theme) {}
    void Update(BView* owner,const BFont* font) override { BListItem::Update(owner,font);SetHeight(47); }
    void DrawItem(BView* owner,BRect rect,bool) override {
        const auto& t=*fTheme;owner->SetHighColor(IsSelected()?t.selection:t.panel);owner->FillRect(rect);owner->SetLowColor(IsSelected()?t.selection:t.panel);
        rgb_color colors[]={t.accent,t.keyword,t.added,t.number,t.type,t.removed};
        auto x=[&](int lane){return rect.left+13+lane*13;};float mid=rect.top+16;
        owner->SetPenSize(1.5);
        for(auto edge:fGraph.edges) {
            owner->SetHighColor(colors[edge.color%6]);
            if(edge.from!=fGraph.lane) owner->StrokeLine(BPoint(x(edge.from),rect.top),BPoint(x(edge.from),mid));
            owner->StrokeLine(BPoint(x(edge.from),mid),BPoint(x(edge.to),rect.bottom+1));
        }
        owner->SetHighColor(colors[fGraph.color%6]);owner->StrokeLine(BPoint(x(fGraph.lane),rect.top),BPoint(x(fGraph.lane),mid));
        owner->FillEllipse(BPoint(x(fGraph.lane),mid),3.5,3.5);owner->SetPenSize(1);
        float textX=x(std::max(3,fGraph.width))+5;
        owner->SetHighColor(t.text);BString subject(fCommit.subject.c_str());
        if(!fCommit.refs.empty()) subject.Prepend(("["+fCommit.refs+"] ").c_str());
        owner->TruncateString(&subject,B_TRUNCATE_END,rect.right-textX-8);owner->DrawString(subject.String(),BPoint(textX,rect.top+17));
        owner->SetHighColor(t.muted);std::string detail=fCommit.hash.substr(0,8)+"  "+fCommit.author+"  "+fCommit.date.substr(0,10);
        BString label(detail.c_str());owner->TruncateString(&label,B_TRUNCATE_END,rect.right-textX-8);owner->DrawString(label.String(),BPoint(textX,rect.top+36));
    }
private:Commit fCommit;GraphRow fGraph;const Theme* fTheme;
};
}
GitView::GitView():BView("source control",B_WILL_DRAW) {
    fChanges=new BListView("changes",B_MULTIPLE_SELECTION_LIST);
    fChanges->SetSelectionMessage(new BMessage(kGitSelection));
    fHistory=new BListView("history");fHistory->SetSelectionMessage(new BMessage(kGitHistorySelection));
    auto* changesScroll=new BScrollView("changes scroll",fChanges,0,false,true,B_NO_BORDER);
    auto* historyScroll=new BScrollView("history scroll",fHistory,0,false,true,B_NO_BORDER);
    fCommitMessage=new BTextControl("commit","", "",new BMessage(kGitCommit));
    fCommitMessage->TextView()->SetToolTip("Commit message. Only staged changes are committed.");
    fStatus=new BStringView("git status","Open a Git project to view changes and history.");
    fHistoryTitle=new BStringView("history title","HISTORY · ALL BRANCHES");
    fMore=new BButton("more","Load More",new BMessage(kGitMore));
    auto* stage=new BButton("stage","Stage",new BMessage(kGitStage));
    auto* unstage=new BButton("unstage","Unstage",new BMessage(kGitUnstage));
    auto* commit=new BButton("commit button","Commit Staged",new BMessage(kGitCommit));
    commit->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNSET));
    fMore->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNSET));
    auto* refresh=new BButton("refresh git","Refresh",new BMessage(kGitRefresh));
    auto* staged=new BButton("index diff","Staged",new BMessage(kGitStagedDiff));
    auto* working=new BButton("working diff","Working Tree",new BMessage(kGitWorktreeDiff));
    BView* changes=new BView("changes panel",0);BView* history=new BView("history panel",0);
    BLayoutBuilder::Group<>(changes,B_VERTICAL,5).SetInsets(8)
        .AddGroup(B_HORIZONTAL,8).Add(new BStringView("changes label","CHANGES")).AddGlue().Add(refresh).End()
        .Add(changesScroll)
        .AddGroup(B_HORIZONTAL,5).Add(stage).Add(unstage).AddGlue().End()
        .Add(fCommitMessage).Add(commit);
    BLayoutBuilder::Group<>(history,B_VERTICAL,5).SetInsets(8)
        .Add(fHistoryTitle).Add(historyScroll).Add(fMore);
    auto* left=new BSplitView(B_VERTICAL,5);left->AddChild(changes);left->AddChild(history);left->SetItemWeight(left->GetLayout()->ItemAt(0),.45f);left->SetItemWeight(left->GetLayout()->ItemAt(1),.55f);
    fDiff=new Editor();fDiff->SetLanguage("changes.diff");fDiff->SetText("Select a changed file or commit to inspect its diff.\n",true);
    auto* right=new BView("diff panel",0);
    BLayoutBuilder::Group<>(right,B_VERTICAL,0)
        .AddGroup(B_HORIZONTAL,6).SetInsets(8,5,8,5).Add(working).Add(staged).AddGlue().End().Add(fDiff);
    auto* split=new BSplitView(B_HORIZONTAL,1);split->AddChild(left);split->AddChild(right);split->SetItemWeight(split->GetLayout()->ItemAt(0),.4f);split->SetItemWeight(split->GetLayout()->ItemAt(1),.6f);
    left->SetExplicitMinSize(BSize(300,200));right->SetExplicitMinSize(BSize(150,100));
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(split).Add(fStatus);
}
GitView::~GitView() { fJobs.reset();ClearList(fChanges);ClearList(fHistory); }
void GitView::AttachedToWindow() {
    BView::AttachedToWindow();fJobs=std::make_unique<AsyncQueue>(BMessenger(this),2);
    std::function<void(BView*)> targets=[&](BView* view) {
        if(auto* control=dynamic_cast<BControl*>(view)) control->SetTarget(this);
        for(int32 i=0;i<view->CountChildren();++i) targets(view->ChildAt(i));
    };targets(this);fChanges->SetTarget(this);fHistory->SetTarget(this);
    Refresh();
}
void GitView::ApplyTheme(const Theme& t) { fTheme=t;ThemeView(this,t);fDiff->ApplyTheme(t); }
void GitView::SetRepository(std::string root,const std::string& error) {
    ++fGeneration;++fDiffRequest;++fHistoryRequest;++fStatusRequest;fRoot=std::move(root);fHistoryPath.clear();fBusy=fHistoryBusy=false;
    if(fJobs) { fJobs->Cancel("status");fJobs->Cancel("history");fJobs->Cancel("diff"); }
    fFiles.clear();fCommits.clear();fGraph.Clear();ClearList(fChanges);ClearList(fHistory);
    fDiff->SetText("Select a changed file or commit to inspect its diff.\n",true);
    fStatus->SetText(error.empty()?"This folder is not a Git repository.":error.c_str());
    fStatus->SetToolTip(error.c_str());fMore->SetEnabled(false);
    Refresh();
}
void GitView::Refresh() {
    if(!fJobs || fRoot.empty()) return;
    auto root=fRoot;auto generation=++fStatusRequest;
    fStatus->SetText("Reading working tree…");
    fJobs->Submit([this,root,generation](const auto& cancel) {
        GitRepository git(root);auto status=git.Status(&cancel);auto branch=git.Run({"branch","--show-current"},&cancel);
        return [this,generation,status=std::move(status),branch=std::move(branch)] {
            if(generation!=fStatusRequest) return;
            if(!status.ok()) { fStatus->SetText(status.diagnostic().c_str());return; }
            fFiles=ParseGitStatus(status.output);ClearList(fChanges);
            for(auto& file:fFiles) fChanges->AddItem(new ChangeItem(file,&fTheme));
            std::string name=branch.output;while(!name.empty() && name.back()=='\n') name.pop_back();
            fStatus->SetText((name+" · "+std::to_string(fFiles.size())+" changed files").c_str());
        };
    },"status");
    LoadHistory(false);
}
void GitView::ShowFileHistory(const std::string& path) { fHistoryPath=path;LoadHistory(false); }
void GitView::LoadHistory(bool more) {
    if(!fJobs || fRoot.empty() || (more && fHistoryBusy)) return;
    fHistoryBusy=true;fMore->SetEnabled(false);
    if(!more) { fCommits.clear();fGraph.Clear();ClearList(fHistory); }
    auto root=fRoot,path=fHistoryPath;auto generation=++fHistoryRequest;size_t skip=fCommits.size();
    fHistoryTitle->SetText(path.empty()?"HISTORY · ALL BRANCHES":("HISTORY · "+path).c_str());
    fJobs->Submit([this,root,path,generation,skip](const auto& cancel) {
        auto result=GitRepository(root).History(skip,200,path,&cancel);
        return [this,result=std::move(result),generation] {
            if(generation!=fHistoryRequest) return;
            fHistoryBusy=false;
            if(!result.ok()) { fStatus->SetText(result.diagnostic().c_str());fMore->SetEnabled(true);return; }
            auto commits=ParseGitLog(result.output);
            for(auto& commit:commits) { fHistory->AddItem(new HistoryItem(commit,fGraph.Append(commit),&fTheme));fCommits.push_back(std::move(commit)); }
            fMore->SetEnabled(commits.size()==200);fMore->SetLabel(commits.size()==200?"Load More":("End of History · "+std::to_string(fCommits.size())+" commits").c_str());
        };
    },"history",[this](const std::string& error){fHistoryBusy=false;fMore->SetEnabled(true);fStatus->SetText(error.c_str());});
}
void GitView::LoadDiff() {
    int32 selected=fChanges->CurrentSelection();if(selected<0 || selected>=static_cast<int32>(fFiles.size())) return;
    auto file=fFiles[selected];auto root=fRoot;bool staged=fStaged;auto request=++fDiffRequest;
    fJobs->Submit([this,root,file,staged,request](const auto& cancel) {
        auto result=GitRepository(root).Diff(file.path,staged,&cancel);
        if(file.index=='?' && result.ok() && result.output.empty()) {
            result.output="Untracked file: "+file.path+"\n\nStage this file to inspect its added-line diff.\n";
        }
        return [this,result=std::move(result),request] {
            if(request!=fDiffRequest) return;
            fDiff->SetText(result.ok()?result.output:result.diagnostic(),true);
        };
    },"diff");
}
void GitView::Operate(uint32 command) {
    if(fBusy || fRoot.empty() || !fJobs) return;
    std::vector<std::string> paths;
    for(int32 n=0;;++n) { int32 selected=fChanges->CurrentSelection(n);if(selected<0) break;if(selected<static_cast<int32>(fFiles.size())) paths.push_back(fFiles[selected].path); }
    std::string message=fCommitMessage->Text();auto root=fRoot;auto generation=fGeneration;
    fBusy=true;fStatus->SetText("Running Git…");
    fJobs->Submit([this,root,paths,message,generation,command](const auto& cancel) {
        GitRepository git(root);ProcessResult result;
        if(command==kGitStage) result=git.Stage(paths);
        else if(command==kGitUnstage) result=git.Unstage(paths);
        else if(command==kGitCommit) result=git.CommitIndex(message);
        else if(command==kGitFetch) result=git.Run({"fetch","--all","--prune"},&cancel);
        else if(command==kGitPull) result=git.Run({"pull","--ff-only"},&cancel);
        else if(command==kGitPush) result=git.Run({"push"},&cancel);
        return [this,result=std::move(result),generation,command] {
            if(generation!=fGeneration) return;
            fBusy=false;
            if(!result.ok()) { (new BAlert("Git",result.diagnostic().c_str(),"OK"))->Go();return; }
            if(command==kGitCommit) fCommitMessage->SetText("");
            Refresh();
        };
    });
}
void GitView::MessageReceived(BMessage* message) {
    switch(message->what) {
        case kWorkDone:if(fJobs) fJobs->Drain();break;
        case kGitRefresh:fHistoryPath.clear();Refresh();break;
        case kGitMore:LoadHistory(true);break;
        case kGitSelection:LoadDiff();break;
        case kGitStagedDiff:fStaged=true;LoadDiff();break;
        case kGitWorktreeDiff:fStaged=false;LoadDiff();break;
        case kGitStage:case kGitUnstage:case kGitCommit:case kGitFetch:case kGitPull:case kGitPush:Operate(message->what);break;
        case kGitHistorySelection: {
            int32 selected=fHistory->CurrentSelection();if(selected<0 || selected>=static_cast<int32>(fCommits.size())) break;
            auto hash=fCommits[selected].hash,root=fRoot;auto request=++fDiffRequest;
            fJobs->Submit([this,root,hash,request](const auto& cancel) {
                auto result=GitRepository(root).CommitDiff(hash,&cancel);
                return [this,result=std::move(result),request] { if(request==fDiffRequest) fDiff->SetText(result.ok()?result.output:result.diagnostic(),true); };
            },"diff");
            break;
        }
        default:BView::MessageReceived(message);
    }
}
}
