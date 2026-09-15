#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/Explorer.h"
#include "ui/GitView.h"
#include "ui/DiffView.h"
#include "ui/PreviewView.h"
#include "ui/Messages.h"
#include <Button.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <NodeMonitor.h>
#include <OS.h>
#include <StringView.h>
#include <TranslationUtils.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

namespace kiri {
namespace fs=std::filesystem;
namespace {
class ExternalWindow:public BWindow {
public:
    ExternalWindow(Workspace* owner,int64 document,int64 serial,const std::shared_ptr<DiffModel>& model,const EditorSettings& settings,const std::string& backups,bool exists)
        :BWindow(BRect(0,0,1060,660),"External File Changes",B_TITLED_WINDOW_LOOK,B_FLOATING_APP_WINDOW_FEEL,B_AUTO_UPDATE_SIZE_LIMITS),fTarget(owner),fDocument(document),fSerial(serial),fBackups(backups) {
        auto* diff=new DiffView();fDiff=diff;diff->ApplySettings(settings);diff->SetModel(model);
        auto* label=new BStringView("external policy","Both versions are backed up. Keep Editing lets Save replace this disk version.");
        auto* reload=new BButton("reload disk","Reload Disk",new BMessage(kExternalReload));reload->SetEnabled(exists);
        auto* keep=new BButton("keep editing","Keep Editing",new BMessage(kExternalKeep));
        auto* copies=new BButton("open backups","Open Backups…",new BMessage(kExternalBackups));
        auto* close=new BButton("close comparison","Close",new BMessage(B_QUIT_REQUESTED));
        auto* panel=new BView("external comparison panel",B_WILL_DRAW);
        BLayoutBuilder::Group<>(panel,B_VERTICAL,6).SetInsets(8).Add(label).Add(diff)
            .AddGroup(B_HORIZONTAL,6).Add(copies).AddGlue().Add(close).Add(keep).Add(reload);
        BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);
        for(auto* button:{reload,keep,copies,close}) button->SetTarget(this);
        ThemeView(this->ChildAt(0),settings.Colors());CenterIn(owner->Frame());Show();
    }
    void MessageReceived(BMessage* message) override {
        if(message->what==kWindowTheme) {ThemeWindow(this,*message);EditorSettings settings;settings.ReadFrom(*message);fDiff->ApplySettings(settings);}
        else if(message->what==kExternalReload || message->what==kExternalKeep || message->what==kExternalBackups) {
            BMessage action(message->what);action.AddInt64("document",fDocument);action.AddInt64("external_serial",fSerial);action.AddString("directory",fBackups.c_str());fTarget.SendMessage(&action);
        }else BWindow::MessageReceived(message);
    }
private:BMessenger fTarget;int64 fDocument,fSerial;std::string fBackups;DiffView* fDiff;
};
}
void Workspace::UpdateExternalBar() {
    auto* d=Current();bool visible=d && d->external;
    if(visible) {
        auto message=d->externalBusy?"Reading disk changes and preserving both versions…":d->externalChange && !d->externalChange->error.empty()?d->externalChange->error:
            !d->externalObserved.exists?"File removed or moved on disk. The editor text is preserved.":"File changed on disk. Compare, reload, or keep your edits.";
        fExternalStatus->SetText(message.c_str());fExternalStatus->SetToolTip(message.c_str());
        bool ready=!d->externalBusy;fExternalCompare->SetEnabled(ready && d->editor);fExternalReload->SetEnabled(ready && d->externalObserved.exists);fExternalKeep->SetEnabled(ready && d->editor);
    }
    if(visible && fExternalBar->IsHidden()) fExternalBar->Show();else if(!visible && !fExternalBar->IsHidden()) fExternalBar->Hide();
}
void Workspace::CheckExternalFiles() {
    if(fDiskCheckPending) return;
    struct Check { int64 id;std::string path;FileStamp before,current; };
    std::vector<Check> files;
    for(auto& d:fDocuments) if(!d->path.empty() && !d->saving && !d->externalBusy && !EditPathBusy(d->path)) files.push_back({d->id,d->path,d->stamp,{}});
    if(files.empty()) return;fDiskCheckPending=true;
    fJobs->Submit([this,files=std::move(files)](const auto&) mutable {
        for(auto& file:files) file.current=StatFile(file.path);
        return [this,files=std::move(files)] {
            fDiskCheckPending=false;
            for(auto& file:files) {
                auto* d=ByID(file.id);if(!d || d->path!=file.path || d->stamp!=file.before || d->saving || d->externalBusy || EditPathBusy(file.path)) continue;
                if(file.current==d->stamp) continue;
                if(d->external && d->externalObserved==file.current && d->externalChange && d->externalChange->diskStamp==file.current) continue;
                d->external=true;d->externalObserved=file.current;
                if(d->editor && !d->editor->Dirty() && file.current.exists) ReloadExternal(*d);
                else if(d->editor) { if(!file.current.exists && !d->editor->Dirty()) d->editor->MarkRecovered();CaptureExternal(*d); }
                else if(file.current.exists) ReloadImage(*d);
            }
            UpdateTabs();
        };
    },{},[this](const std::string& error){fDiskCheckPending=false;Notice(error);});
}
void Workspace::ReloadImage(Document& d) {
    if(d.externalBusy || !dynamic_cast<PreviewView*>(d.view)) return;
    d.externalBusy=true;auto id=d.id,serial=++d.externalSerial;auto path=d.path;
    fJobs->Submit([this,id,serial,path](const auto&) {
        auto stamp=StatFile(path);auto bitmap=std::shared_ptr<BBitmap>(BTranslationUtils::GetBitmap(path.c_str()));bool stable=stamp==StatFile(path);
        return [this,id,serial,path,stamp,bitmap,stable] {
            auto* d=ByID(id);if(!d || d->externalSerial!=serial) return;d->externalBusy=false;
            if(d->path!=path || !stable || !bitmap || bitmap->InitCheck()!=B_OK || stamp!=StatFile(path)) { Notice("The changed image could not be reloaded. Its previous preview is retained.");UpdateTabs();return; }
            for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==id) if(auto* preview=dynamic_cast<PreviewView*>(tab->view)) preview->SetBitmap(bitmap,stamp.size);
            d->stamp=stamp;d->external=false;UpdateTabs();
        };
    },{},[this,id,serial](const std::string& error){if(auto* d=ByID(id);d && d->externalSerial==serial) { d->externalBusy=false;Notice(error);UpdateTabs(); }});
}
void Workspace::CaptureExternal(Document& d,uint32 action) {
    if(d.externalBusy || d.saving || !d.editor || EditPathBusy(d.path)) return;
    d.external=true;d.externalBusy=true;auto id=d.id,serial=++d.externalSerial,revision=d.editor->InputRevision();auto path=d.path;
    auto text=std::make_shared<std::string>(d.editor->Text());bool bom=d.bom;
    auto directory=fSettings+"/external-changes/"+std::to_string(real_time_clock())+"-"+std::to_string(system_time())+"-"+std::to_string(id);
    if(d.externalWindow.IsValid()) d.externalWindow.SendMessage(B_QUIT_REQUESTED);d.externalWindow=BMessenger();UpdateExternalBar();
    fJobs->Submit([this,id,serial,revision,path,text,bom,directory,action](const auto& cancel) {
        auto snapshot=std::make_shared<ExternalChange>();snapshot->revision=revision;snapshot->backupDirectory=directory;snapshot->diskStamp=StatFile(path);
        std::error_code error;fs::create_directories(directory,error);chmod(directory.c_str(),0700);
        auto name=fs::path(path).filename().string();auto bufferPath=directory+"/buffer-"+name,diskPath=directory+"/disk-"+name;
        if(error) snapshot->error="Cannot create external-change backups: "+error.message();
        else { std::vector<std::string_view> parts;if(bom) parts.push_back("\xef\xbb\xbf");parts.push_back(*text);snapshot->error=SaveFileParts(bufferPath,parts,{}); }
        if(snapshot->error.empty() && snapshot->diskStamp.exists) {
            fs::copy_file(path,diskPath,fs::copy_options::none,error);if(error) snapshot->error="Cannot back up disk version: "+error.message();
            if(StatFile(path)!=snapshot->diskStamp) snapshot->error="Disk changed during backup. Compare again to read the latest version.";
        }
        if(snapshot->error.empty()) {
            DiffSource before;if(snapshot->diskStamp.exists) before=DiskDiffSource(diskPath,"Disk snapshot",&cancel);else { before.exists=false;before.label="Disk (removed)"; }
            before.path=path;before.stamp=snapshot->diskStamp;
            DiffSource after;after.path=path;after.label="Unsaved editor snapshot";after.size=text->size();
            if(text->size()<=kDiffBytes) { if(bom) after.text="\xef\xbb\xbf";after.text+=*text; }else after.error="Comparison limit: 8 MiB per side. Full versions are preserved in the backup folder.";
            snapshot->comparison=std::make_shared<DiffModel>(CompareText(std::move(before),std::move(after),&cancel));
            std::ofstream info(directory+"/README.txt");info<<"Original file: "<<path<<"\nBuffer: "<<bufferPath<<"\nDisk: "<<(snapshot->diskStamp.exists?diskPath:"absent")<<"\nThese copies are retained after reload, save, and restart.\n";
        }
        return [this,id,serial,snapshot,action] {
            auto* d=ByID(id);if(!d || d->externalSerial!=serial) return;d->externalBusy=false;d->externalChange=snapshot;d->externalObserved=snapshot->diskStamp;
            if(snapshot->error.empty() && action) {
                BMessage message(action);message.AddInt64("document",id);message.AddInt64("external_serial",serial);ResolveExternal(message);
            }
            UpdateTabs();
        };
    },{},[this,id,serial](const std::string& error){auto* d=ByID(id);if(d && d->externalSerial==serial) { d->externalBusy=false;d->externalChange=std::make_shared<ExternalChange>();d->externalChange->error=error;UpdateTabs(); }});
}
void Workspace::ReloadExternal(Document& d,bool reviewed) {
    if(d.externalBusy || d.saving || !d.editor || EditPathBusy(d.path)) return;
    if(d.editor->Dirty() && !reviewed) { CaptureExternal(d);return; }
    auto id=d.id,serial=++d.externalSerial,revision=d.editor->InputRevision();auto path=d.path;auto base=d.stamp;
    d.externalBusy=true;auto loader=fLoaderFactory->CreateLoader(StatFile(path).size>8*1024*1024);UpdateExternalBar();
    fJobs->Submit([this,id,serial,revision,path,base,reviewed,loader](const auto& cancel) {
        FileData data;
        if(!loader->loader) data.error="Not enough memory to reload the document.";
        else data=ReadFile(path,&cancel,1024ULL*1024*1024,[&](std::string_view bytes){return loader->loader->AddData(bytes.data(),bytes.size())==SC_STATUS_OK;});
        if(data.ok() && StatFile(path)!=data.stamp) data.error="Disk changed during reload; waiting for the next refresh.";
        return [this,id,serial,revision,path,base,reviewed,loader,data=std::move(data)] {
            auto* d=ByID(id);if(!d || d->externalSerial!=serial) return;d->externalBusy=false;
            if(d->path!=path || d->saving || d->stamp!=base || !d->editor || d->editor->InputRevision()!=revision || (!reviewed && d->editor->Dirty()) || EditPathBusy(path)) { d->externalObserved={};UpdateTabs();return; }
            if(!data.ok() || StatFile(path)!=data.stamp) { d->externalObserved={};Notice(data.ok()?"Disk changed again; waiting for the next refresh.":data.error);UpdateTabs();return; }
            if(reviewed && (!d->externalChange || data.stamp!=d->externalChange->diskStamp)) { CaptureExternal(*d,kExternalCompare);return; }
            Editor replacement;bool binary=data.binary || !data.utf8;
            if(binary) replacement.SetText(HexPreview(data.bytes,data.stamp.size),true);else replacement.Adopt(*loader,data.eol);
            replacement.SetLanguage(binary?"preview.txt":path,data.stamp.size>8*1024*1024);replacement.ApplySettings(fEditorSettings);replacement.State()->revision=revision+1;replacement.State()->inputRevision=revision+1;
            CancelCompletion();CloseLanguage(*d);ClearRecovery(*d);++d->formatSerial;
            for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==id && tab->editor) {
                auto view=ViewState(*tab);auto* editor=tab->editor;
                auto remap=[&](BMessage& state,const char* field) { int64 position=0;if(state.FindInt64(field,&position)!=B_OK) return;
                    auto line=editor->SendMessage(SCI_LINEFROMPOSITION,position),column=editor->SendMessage(SCI_GETCOLUMN,position);
                    line=std::min(line,replacement.SendMessage(SCI_GETLINECOUNT)-1);state.ReplaceInt64(field,replacement.SendMessage(SCI_FINDCOLUMN,line,column)); };
                for(auto field:{"caret","anchor","rectangle_caret","rectangle_anchor"}) remap(view,field);
                BMessage selection;for(int32 i=0;view.FindMessage("selection",i,&selection)==B_OK;++i) { remap(selection,"caret");remap(selection,"anchor");view.ReplaceMessage("selection",i,&selection); }
                auto first=editor->SendMessage(SCI_DOCLINEFROMVISIBLE,editor->SendMessage(SCI_GETFIRSTVISIBLELINE));
                editor->ShareDocument(replacement);editor->SetLanguage(binary?"preview.txt":path,data.stamp.size>8*1024*1024);RestoreView(*tab,view);
                editor->SendMessage(SCI_SETFIRSTVISIBLELINE,editor->SendMessage(SCI_VISIBLEFROMDOCLINE,std::min(first,editor->SendMessage(SCI_GETLINECOUNT)-1)));
            }
            d->buffer=replacement.State();d->stamp=data.stamp;d->bom=data.bom;d->external=false;d->externalObserved=data.stamp;
            if(d->externalWindow.IsValid()) d->externalWindow.SendMessage(B_QUIT_REQUESTED);d->externalWindow=BMessenger();
            d->languageDirty=true;d->textChangedAt=0;d->symbols.clear();UpdateTabs();UpdateSymbolBar();SaveSettings();
        };
    },{},[this,id,serial](const std::string& error){if(auto* d=ByID(id);d && d->externalSerial==serial) { d->externalBusy=false;d->externalObserved={};Notice(error);UpdateTabs(); }});
}
void Workspace::ResolveExternal(BMessage& message) {
    int64 id=0,serial=0;message.FindInt64("document",&id);bool hasSerial=message.FindInt64("external_serial",&serial)==B_OK;auto* d=id?ByID(id):Current();
    if(message.what==kExternalBackups) { const char* path=nullptr;message.FindString("directory",&path);OpenExternalBackups(path?path:d && d->externalChange?d->externalChange->backupDirectory:fSettings+"/external-changes");return; }
    if(!d || !d->external || d->externalBusy || d->saving || EditPathBusy(d->path)) return;
    if(hasSerial && serial!=d->externalSerial) return;
    if(!d->editor) { if(message.what==kExternalReload) ReloadImage(*d);return; }
    auto snapshot=d->externalChange;
    if(!snapshot || !snapshot->error.empty() || snapshot->revision!=d->editor->InputRevision() || snapshot->diskStamp!=StatFile(d->path)) { CaptureExternal(*d,message.what);return; }
    if(message.what==kExternalCompare) {
        if(d->externalWindow.IsValid()) d->externalWindow.SendMessage(B_QUIT_REQUESTED);
        d->externalWindow=BMessenger(new ExternalWindow(this,d->id,d->externalSerial,snapshot->comparison,fEditorSettings,snapshot->backupDirectory,snapshot->diskStamp.exists));
    }else if(message.what==kExternalReload) { if(snapshot->diskStamp.exists) ReloadExternal(*d,true); }
    else if(message.what==kExternalKeep) {
        d->stamp=snapshot->diskStamp;d->external=false;if(!d->editor->Dirty()) d->editor->MarkRecovered();
        if(d->externalWindow.IsValid()) d->externalWindow.SendMessage(B_QUIT_REQUESTED);d->externalWindow=BMessenger();UpdateTabs();SaveSettings();
    }
}
void Workspace::OpenExternalBackups(const std::string& directory) {
    if(fOpenPanel && fOpenPanel->IsShowing()) { fOpenPanel->Window()->Activate();return; }
    std::error_code error;fs::create_directories(directory,error);if(error) { Notice(error.message());return; }BMessenger target(this);BMessage message(kFileChosen);
    fOpenPanel=std::make_unique<BFilePanel>(B_OPEN_PANEL,&target,nullptr,B_FILE_NODE,true,&message);fOpenPanel->SetPanelDirectory(directory.c_str());fOpenPanel->Show();
}
void Workspace::PollProject() {
    if(!fProjectMonitor || fMonitorPending) return;fMonitorPending=true;auto monitor=fProjectMonitor;auto generation=fGeneration;auto priority=std::move(fMonitorPriority);fMonitorPriority.clear();
    fJobs->Submit([this,monitor,generation,priority](const auto& cancel) {
        auto changes=monitor->Poll(priority,&cancel);
        return [this,monitor,generation,changes=std::move(changes)] {
            if(generation!=fGeneration || monitor!=fProjectMonitor) return;fMonitorPending=false;
            UpdateNodeWatches(changes.directories);
            if(!changes.changedDirectories.empty()) {
                auto loaded=fExplorer->LoadedDirectories();for(auto& path:changes.changedDirectories) if(std::find(loaded.begin(),loaded.end(),path)!=loaded.end()) LoadDirectory(path);
                RefreshIndex();
            }
            if(changes.contents) { RefreshSearchWindows();fGit->Refresh(); }
            if(changes.limited) Notice("Automatic directory monitoring limit: 50,000 directories. Use Refresh for the remaining paths.");
        };
    },{},[this,generation](const std::string& error){if(generation==fGeneration) { fMonitorPending=false;Notice(error); }});
}
void Workspace::UpdateNodeWatches(const std::vector<std::string>& directories) {
    std::vector<std::string> candidates=fExplorer->LoadedDirectories();candidates.insert(candidates.end(),directories.begin(),directories.end());
    for(auto& d:fDocuments) if(!d->path.empty()) candidates.insert(candidates.begin(),fs::path(d->path).parent_path().string());
    std::map<std::pair<uint64_t,uint64_t>,std::string> watched;
    for(auto& path:candidates) { auto stamp=StatFile(path);if(!stamp.exists) continue;auto key=std::make_pair(stamp.device,stamp.inode);if(watched.size()>=512) break;
        if(fWatchedNodes.count(key)) watched[key]=path;
        else { node_ref node{dev_t(stamp.device),ino_t(stamp.inode)};if(watch_node(&node,B_WATCH_DIRECTORY|B_WATCH_STAT|B_WATCH_CHILDREN,BMessenger(this))==B_OK) watched[key]=path; }
    }
    for(auto& item:fWatchedNodes) if(!watched.count(item.first)) { node_ref node{dev_t(item.first.first),ino_t(item.first.second)};watch_node(&node,B_STOP_WATCHING,BMessenger(this)); }
    fWatchedNodes=std::move(watched);
}
void Workspace::RefreshSearchWindows() {
    for(auto it=fSearchWindows.begin();it!=fSearchWindows.end();) { if(!it->IsValid()) it=fSearchWindows.erase(it);else { it->SendMessage(kProjectUpdated);++it; } }
}
}
