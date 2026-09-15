#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/EditPreviewWindow.h"
#include <CheckBox.h>
#include <Directory.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <StringView.h>
#include <TextControl.h>
#include <filesystem>
#include <algorithm>
#include <mutex>

namespace kiri {
namespace fs=std::filesystem;
namespace {
std::mutex sWorkspaceMutex;
std::vector<BMessenger> sEditWorkspaces;
std::map<std::string,BMessenger> sReservedPaths;
struct WindowLock {
    BLooper* looper=nullptr;
    explicit WindowLock(const BMessenger& target) {
        if(target.LockTargetWithTimeout(50000)!=B_OK) throw std::runtime_error("Another workspace is busy. Retry the preview when it is ready.");
        target.Target(&looper);
    }
    ~WindowLock() { if(looper) looper->Unlock(); }
};
std::vector<BMessenger> EditWorkspaces() { std::lock_guard<std::mutex> lock(sWorkspaceMutex);return sEditWorkspaces; }
std::string String(const BMessage& message,const char* key) { const char* value=nullptr;message.FindString(key,&value);return value?value:""; }
bool Flag(const BMessage& message,const char* key) { bool value=false;message.FindBool(key,&value);return value; }
void Progress(BMessenger target,const std::string& text,bool busy,int file=-1,const FileEdit* edit=nullptr,bool finished=false) {
    BMessage status(kEditPreview);status.AddString("status",text.c_str());status.AddBool("busy",busy);status.AddBool("finished",finished);
    if(edit) { status.AddInt32("file",file);status.AddString("file_status",edit->status.c_str());status.AddString("error",edit->error.c_str()); }
    target.SendMessage(&status);
}
}
SearchOptions Workspace::FindOptions() const { return {fFindText->Text(),bool(fMatchCase->Value()),bool(fRegex->Value()),bool(fWholeWord->Value())}; }
void Workspace::RememberQuery() {
    auto query=std::string(fFindText->Text());if(query.empty()) return;
    fRecentQueries.erase(std::remove(fRecentQueries.begin(),fRecentQueries.end(),query),fRecentQueries.end());fRecentQueries.push_front(query);
    while(fRecentQueries.size()>20) fRecentQueries.pop_back();
    auto* menu=fFindHistory->Menu();while(auto* item=menu->RemoveItem(int32(0))) delete item;
    for(const auto& recent:fRecentQueries) { auto* message=new BMessage(kFindHistory);message->AddString("query",recent.c_str());auto* item=new BMenuItem(recent.c_str(),message);item->SetTarget(this);menu->AddItem(item); }
    Json history=fRecentQueries;SaveFile(fSettings+"/search-history.json",history.dump(),StatFile(fSettings+"/search-history.json"));
}
void Workspace::RunFind(uint32 command) {
    auto* d=Current();auto* tab=CurrentTab();if(!d || !d->editor || !tab || !tab->editor) { fFindStatus->SetText("No text document");return; }
    auto* editor=tab->editor;
    if(command==kFindScope) { editor->SetSearchSelection(fInSelection->Value());fFindScopeView=tab->id; }
    else if(fFindScopeView!=tab->id) { fInSelection->SetValue(B_CONTROL_OFF);editor->SetSearchSelection(false);fFindScopeView=tab->id; }
    if(command==kFindChanged) { BMessage refresh(kFindRefresh);fFindTimer=std::make_unique<BMessageRunner>(BMessenger(this),&refresh,120000,1);return; }
    auto options=FindOptions();std::string status;
    if(command==kFindNext || command==kFindPrevious) { RememberQuery();if(!editor->Find(options,command==kFindPrevious)) status="No match"; }
    else if(command==kReplace || command==kReplaceAll) { RememberQuery();auto count=editor->Replace(options,fReplaceText->Text(),command==kReplaceAll);status=std::to_string(count)+" replaced";UpdateTabs(); }
    auto error=(command==kFindNext || command==kFindPrevious || command==kReplace || command==kReplaceAll)?editor->SearchError():std::string();auto found=editor->SearchMatches(options);
    if(!error.empty()) status=error;else if(!found.error.empty()) status=found.error;
    else { if(!status.empty()) status+=" · ";status+=std::to_string(found.matches.size())+(found.truncated?"+ matches":" matches"); }
    fFindStatus->SetText(status.c_str());fFindStatus->SetToolTip(status.c_str());
}
void Workspace::RegisterEditWorkspace(bool add) {
    std::lock_guard<std::mutex> lock(sWorkspaceMutex);BMessenger self(this);
    if(add) sEditWorkspaces.push_back(self);
    else { sEditWorkspaces.erase(std::remove(sEditWorkspaces.begin(),sEditWorkspaces.end(),self),sEditWorkspaces.end());
        for(auto it=sReservedPaths.begin();it!=sReservedPaths.end();) { if(it->second==self) it=sReservedPaths.erase(it);else ++it; } }
}
bool Workspace::EditPathBusy(const std::string& path) const {
    std::lock_guard<std::mutex> lock(sWorkspaceMutex);return sReservedPaths.count(CanonicalPath(path));
}
bool Workspace::ReserveEditPaths(bool reserve) {
    std::lock_guard<std::mutex> lock(sWorkspaceMutex);BMessenger self(this);
    if(reserve) {
        for(const auto& file:fEditPlan->files) if(sReservedPaths.count(file.before.path) && sReservedPaths.at(file.before.path)!=self) return false;
        for(const auto& file:fEditPlan->files) sReservedPaths[file.before.path]=self;
    } else for(auto it=sReservedPaths.begin();it!=sReservedPaths.end();) { if(it->second==self) it=sReservedPaths.erase(it);else ++it; }
    return true;
}
EditSnapshot Workspace::BufferSnapshot(Document& document) {
    EditSnapshot snapshot;snapshot.path=document.path;snapshot.text=document.editor->Text();snapshot.stamp=StatFile(snapshot.path);
    snapshot.bom=document.bom;snapshot.open=true;snapshot.readOnly=document.saving || document.editor->SendMessage(SCI_GETREADONLY);
    snapshot.document=document.id;snapshot.revision=document.editor->InputRevision();snapshot.version=document.serverVersion;snapshot.owner=fSessionToken;return snapshot;
}
SnapshotMap Workspace::OpenSnapshots() {
    SnapshotMap result;
    for(const auto& target:EditWorkspaces()) {
        if(!target.IsValid()) continue;WindowLock lock(target);auto* owner=dynamic_cast<Workspace*>(lock.looper);if(!owner) continue;
        for(auto& document:owner->fDocuments) {
            if(document->path.empty() || !document->editor) continue;
            auto snapshot=owner->BufferSnapshot(*document);
            if(result.count(snapshot.path)) throw std::runtime_error("This file has independent buffers in multiple windows: "+snapshot.path+". Close one before a project edit.");
            result.emplace(snapshot.path,std::move(snapshot));
        }
    }
    return result;
}
bool Workspace::VisitOpenDocument(const std::string& path,const std::function<void(Workspace&,Document&)>& action) {
    bool visited=false;
    for(const auto& target:EditWorkspaces()) {
        if(!target.IsValid()) continue;WindowLock lock(target);auto* owner=dynamic_cast<Workspace*>(lock.looper);if(!owner) continue;
        if(owner->fPendingOpen.count(path)) throw std::runtime_error("This file is still opening: "+path+". Retry when it is ready.");
        for(auto& d:owner->fDocuments) if(d->path==path && d->editor) {
            if(visited) throw std::runtime_error("Independent buffers for this file exist in multiple windows. Close one and refresh the preview.");
            action(*owner,*d);visited=true;
        }
    }
    return visited;
}
EditSnapshot Workspace::CurrentSnapshot(const EditSnapshot& before) {
    EditSnapshot result;
    bool found=VisitOpenDocument(before.path,[&](Workspace& owner,Document& d){result=owner.BufferSnapshot(d);});
    return found?result:DiskSnapshot(before.path);
}
std::string Workspace::ApplyBufferEdit(FileEdit& file,bool restore) {
    std::string error;
    try {
        bool found=VisitOpenDocument(file.before.path,[&](Workspace& owner,Document& d) {
            error=ValidateEdit(file,owner.BufferSnapshot(d),restore);if(!error.empty()) return;
            if(restore) {
                if(file.appliedRevision && d.editor->InputRevision()==file.appliedRevision && d.editor->SendMessage(SCI_CANUNDO) && d.editor->Matches(file.after)) d.editor->SendMessage(SCI_UNDO);
                if(!d.editor->Matches(file.before.text)) d.editor->ApplyEdits({{0,size_t(d.editor->SendMessage(SCI_GETLENGTH)),file.before.text}});
                file.restored=true;
            } else { d.editor->ApplyEdits(file.Selected());file.appliedRevision=d.editor->InputRevision();file.applied=true; }
            file.pending=false;file.status=restore?"Restored buffer":"Changed buffer (unsaved)";file.error.clear();d.languageDirty=true;owner.UpdateTabs();
        });
        if(!found) error="The original buffer was closed. Refresh the preview.";
    }catch(const std::exception& e) { error=e.what(); }
    return error;
}
void Workspace::PreviewReplacement(const BMessage& message) {
    if(fApplyingEdit) { Notice("Wait for the current project edit to finish.");return; }
    if(fProject.empty() || String(message,"root")!=fProject) return;
    fReplaceRequest=message;auto root=fProject, replacement=String(message,"replacement");
    ProjectSearchOptions options;options.query=String(message,"query");options.folders=String(message,"folders");options.include=String(message,"include");options.exclude=String(message,"exclude");
    options.matchCase=Flag(message,"case");options.regex=Flag(message,"regex");options.wholeWord=Flag(message,"word");options.includeIgnored=Flag(message,"ignored");
    auto error=TextQuery(options).ValidateReplacement(replacement);if(!error.empty()) { Notice(error);return; }
    SnapshotMap open;try { open=OpenSnapshots(); }catch(const std::exception& e) { Notice(e.what());return; }auto serial=++fEditSerial;Notice("Building replacement preview from open buffers and files on disk…");
    fJobs->Submit([this,root,options,replacement,open=std::move(open),serial](const auto& cancel) {
        auto index=IndexProject(root,&cancel,500000,options.includeIgnored);
        auto plan=std::make_shared<EditPlan>(ReplacementPlan(root,index,options,replacement,open,&cancel));
        return [this,plan,serial] { if(serial==fEditSerial) ShowEditPreview(plan); };
    },"replace-preview",[this,serial](const std::string& error){if(serial==fEditSerial) Notice(error);});
}
void Workspace::ShowEditPreview(std::shared_ptr<EditPlan> plan,bool restore) {
    if(fApplyingEdit) return;
    if(fEditWindow.IsValid()) fEditWindow.SendMessage(B_QUIT_REQUESTED);
    fEditPlan=plan;fRestoringEdit=restore;++fEditSerial;
    if(plan->files.empty()) { Notice("No changes to preview. "+plan->summary);return; }
    auto* preview=new EditPreviewWindow(this,*plan,Theme::Builtins()[fEditorSettings.theme],restore,fEditSerial);fEditWindow=BMessenger(preview);
    Notice(restore?"Review the files to restore.":"Review the edits and choose Apply Reviewed Changes.");
}
void Workspace::ApplyProjectEdit(bool restore) {
    if(fApplyingEdit || !fEditPlan || restore!=fRestoringEdit) return;
    auto plan=fEditPlan;
    try {
        for(auto& file:plan->files) {
            if(!restore) file.Prepare();
            if((restore && (!file.applied || file.restored)) || (!restore && file.before.text==file.after)) continue;
            // Validate open buffers together before the disk preflight.
            if(file.before.open) {
                auto current=CurrentSnapshot(file.before);auto error=ValidateEdit(file,current,restore);
                if(restore && current.text==file.before.text && current.bom==file.before.bom) { file.restored=true;file.status="Already undone";continue; }
                if(!error.empty()) throw std::runtime_error(file.before.path+": "+error+". Refresh Preview before applying.");
            }
        }
    }catch(const std::exception& e) { Notice(e.what());Progress(fEditWindow,e.what(),false);return; }
    if(!ReserveEditPaths(true)) { Notice("Another project edit is using these files. Retry when it finishes.");return; }
    fApplyingEdit=true;fRestoringEdit=restore;fCancelEdit=false;fEditFile=0;
    Progress(fEditWindow,"Checking every affected snapshot…",true);
    fJobs->Submit([this,plan,restore](const auto&) {
        std::string error;
        try {
            for(auto& file:plan->files) {
                if(file.before.open || (restore && (!file.applied || file.restored)) || (!restore && file.before.text==file.after)) continue;
                auto current=DiskSnapshot(file.before.path);
                if(restore && current.text==file.before.text && current.bom==file.before.bom) { file.restored=true;file.status="Already restored";continue; }
                error=ValidateEdit(file,current,restore);if(!error.empty()) { error=file.before.path+": "+error;break; }
            }
            if(error.empty() && !restore) {
                auto directory=fSessionDirectory+"/project-edits";create_directory(directory.c_str(),0700);
                plan->journal=directory+"/"+std::to_string(system_time())+".json";
                error=WriteEditJournal(*plan);
                if(error.empty()) error=SaveFile(directory+"/latest",plan->journal,StatFile(directory+"/latest"));
            }
        }catch(const std::exception& e) { error=e.what(); }
        return [this,plan,error] {
            if(!error.empty()) { fApplyingEdit=false;ReserveEditPaths(false);Notice(error+". Refresh the preview.");Progress(fEditWindow,error+". Refresh the preview.",false);return; }
            fLastEdit=plan;ProjectEditStep();
        };
    },"project-edit");
}
void Workspace::ProjectEditStep() {
    auto plan=fEditPlan;bool restore=fRestoringEdit;
    while(fEditFile<plan->files.size()) {
        auto& file=plan->files[fEditFile];
        if((restore && (!file.applied || file.restored)) || (!restore && file.before.text==file.after)) { if(!restore) file.status=file.Selected().empty()?"Excluded; unchanged":"No text change";++fEditFile;continue; }
        break;
    }
    if(fCancelEdit || fEditFile==plan->files.size()) {
        size_t changed=0,failed=0;
        for(size_t i=0;i<plan->files.size();++i) {
            auto& file=plan->files[i];if(restore?file.restored:file.applied) ++changed;if(!file.error.empty() && (!restore || (file.applied && !file.restored))) ++failed;
            if(fCancelEdit && i>=fEditFile && file.error.empty() && !(restore?file.restored:file.applied)) file.status="Cancelled; unchanged";
            Progress(fEditWindow,"",false,int(i),&file);
        }
        auto summary=std::to_string(changed)+(restore?" files restored":" files changed")+", "+std::to_string(failed)+" failed"+(fCancelEdit?" · cancelled":"")+". Recovery: "+plan->journal;
        fJobs->Submit([this,plan,summary](const auto&){auto error=WriteEditJournal(*plan);return [this,summary,error] {
            fApplyingEdit=false;ReserveEditPaths(false);
            for(const auto& file:fEditPlan->files) if(file.applied) {
                try { VisitOpenDocument(file.before.path,[&](Workspace& owner,Document&){owner.fLastEdit=fEditPlan;}); }catch(...) {}
            }
            auto status=summary;if(!error.empty()) status+=" · Recovery journal: "+error;
            Progress(fEditWindow,status,false,-1,nullptr,true);Notice(status);UpdateTabs();RefreshIndex();
        };},"edit-journal-final");return;
    }
    auto index=fEditFile;auto& file=plan->files[index];
    try {
        auto current=CurrentSnapshot(file.before);auto error=ValidateEdit(file,current,restore);
        if(!error.empty()) { file.error=error;file.status="Stale; unchanged";fCancelEdit=true;ProjectEditStep();return; }
    }catch(const std::exception& e) { file.error=e.what();file.status="Failed; unchanged";fCancelEdit=true;ProjectEditStep();return; }
    Progress(fEditWindow,std::string(restore?"Restoring ":"Applying ")+file.before.path,true,int(index),&file);
    bool diskOperation=!file.before.open;
    if(restore && file.before.open) {
        bool stillOpen=VisitOpenDocument(file.before.path,[](Workspace&,Document&){});
        diskOperation=!stillOpen;
    }
    // The write-ahead journal must reach disk before any corresponding edit.
    file.pending=true;
    fJobs->Submit([this,plan,index,restore,diskOperation](const auto&) {
        auto error=WriteEditJournal(*plan);auto& file=plan->files[index];
        if(error.empty() && diskOperation) error=ApplyDiskEdit(file,restore);
        if(error.empty() && diskOperation) error=WriteEditJournal(*plan);
        return [this,plan,index,restore,diskOperation,error]() mutable {
            auto& file=plan->files[index];
            if(error.empty() && !diskOperation) error=ApplyBufferEdit(file,restore);
            if(error.empty() && diskOperation && restore) {
                try { VisitOpenDocument(file.before.path,[&](Workspace& owner,Document& d) {
                    if(!d.editor->Matches(file.after) || d.editor->SendMessage(SCI_GETREADONLY)) { error="Disk restored; open buffer has newer text and was kept.";return; }
                    d.editor->ApplyEdits({{0,file.after.size(),file.before.text}});d.stamp=StatFile(d.path);d.external=false;d.editor->MarkSaved();d.languageDirty=true;owner.UpdateTabs();
                }); }catch(const std::exception& e) { error=e.what(); }
            }
            if(!error.empty()) { file.error=error;file.status=file.applied?"Changed; recovery needs attention":"Failed; unchanged";fCancelEdit=true; }
            Progress(fEditWindow,file.status,true,int(index),&file);++fEditFile;ProjectEditStep();
        };
    },"project-edit");
}
void Workspace::UndoProjectEdit() {
    if(fApplyingEdit) { Notice("Wait for the current project edit to finish.");return; }
    if(fLastEdit) { for(const auto& file:fLastEdit->files) if(EditPathBusy(file.before.path)) { Notice("Wait for the current project edit to finish.");return; }ShowEditPreview(fLastEdit,true);return; }
    auto latest=fSessionDirectory+"/project-edits/latest";
    if(!StatFile(latest).exists) { Notice("There is no project edit to restore.");return; }
    fJobs->Submit([this,latest](const auto&) {
        auto path=ReadFile(latest);if(!path.ok()) throw std::runtime_error(path.error);
        auto plan=std::make_shared<EditPlan>(ReadEditJournal(path.bytes));
        return [this,plan]{fLastEdit=plan;ShowEditPreview(plan,true);};
    },"edit-restore",[this](const std::string& error){Notice(error);});
}
}
