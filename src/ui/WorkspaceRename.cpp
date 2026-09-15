#include "ui/Workspace.h"
#include "ui/Editor.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <filesystem>

namespace kiri {
namespace fs=std::filesystem;
namespace {
class RenamePrompt:public BWindow {
public:
    RenamePrompt(BWindow* owner,const std::string& name,int64 serial,const Theme& theme)
        :BWindow(BRect(0,0,580,150),"Rename Symbol",B_TITLED_WINDOW_LOOK,B_FLOATING_APP_WINDOW_FEEL,B_AUTO_UPDATE_SIZE_LIMITS),fTarget(owner),fSerial(serial) {
        fName=new BTextControl("rename name","New name",name.c_str(),new BMessage(kRenameSubmit));fName->SetTarget(this);
        fStatus=new BStringView("rename status","The language server will prepare a preview. No changes are applied yet.");
        auto* preview=new BButton("preview rename","Preview Rename…",new BMessage(kRenameSubmit));preview->SetTarget(this);
        auto* cancel=new BButton("cancel rename","Cancel",new BMessage(B_QUIT_REQUESTED));cancel->SetTarget(this);
        auto* panel=new BView("rename panel",0);
        BLayoutBuilder::Group<>(panel,B_VERTICAL,10).SetInsets(15).Add(fName).Add(fStatus).AddGroup(B_HORIZONTAL,8).AddGlue().Add(cancel).Add(preview);
        BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);ThemeView(panel,theme);SetDefaultButton(preview);CenterIn(owner->Frame());fName->MakeFocus();fName->TextView()->SelectAll();Show();
    }
    bool QuitRequested() override { BMessage message(kRenameDismiss);message.AddInt64("serial",fSerial);fTarget.SendMessage(&message);return true; }
    void MessageReceived(BMessage* message) override {
        if(message->what==kWindowTheme) ThemeWindow(this,*message);
        else if(message->what==kRenameSubmit) {
            auto error=ValidateRenameName(fName->Text());if(!error.empty()) { fStatus->SetText(error.c_str());return; }
            BMessage request(kRenameSubmit);request.AddString("name",fName->Text());request.AddInt64("serial",fSerial);fTarget.SendMessage(&request);fStatus->SetText("Requesting rename preview…");
        } else if(message->what==kEditPreview) { const char* error=nullptr;if(message->FindString("status",&error)==B_OK) { fStatus->SetText(error);fStatus->SetToolTip(error); } }
        else BWindow::MessageReceived(message);
    }
private:BMessenger fTarget;int64 fSerial;BTextControl* fName;BStringView* fStatus;
};
}
void Workspace::ClearRenameBorrowed() {
    auto found=fServers.find(fRenameServerKey);
    if(found!=fServers.end() && found->second.client) for(const auto& uri:fRenameBorrowed) found->second.client->Close(uri);
    fRenameBorrowed.clear();fRenameServerKey.clear();
}
void Workspace::RenameSymbol(Editor* source) {
    if(fApplyingEdit) { Notice("Wait for the current project edit to finish.");return; }
    auto* d=ByEditor(source);if(!d || !source || source->SendMessage(SCI_GETREADONLY)) { Notice("Open an editable source file to rename a symbol.");return; }
    auto* server=EnsureLanguage(*d);
    if(!server || !server->Ready()) { Notice(d->languageStatus.empty()?"Language server is not ready.":d->languageStatus);return; }
    auto provider=server->Capabilities().value("renameProvider",Json(false));
    if(provider==false || provider.is_null()) { Notice("This language server does not support Rename Symbol.");return; }
    for(auto& document:fDocuments) if(document->editor) {
        auto* other=EnsureLanguage(*document);if(other==server) {
            SyncLanguage(*document,*server);
            if(document->serverRevision!=document->editor->InputRevision()) {Notice(document->diagnosticStatus);return;}
        }
    }
    ClearRenameBorrowed();
    auto serial=++fRenameSerial;fRenameDocument=d->id;fRenameCaret=source->SendMessage(SCI_GETCURRENTPOS);fRenameText=source->Text();
    try { fRenameOpen=OpenSnapshots(); }catch(const std::exception& e) { Notice(e.what());return; }auto root=fProject.empty()?fs::path(d->path).parent_path().string():fProject;
    fRenameServerKey=d->serverKey;
    const auto* sourceProfile=fLanguageTools.ForFile(d->path);
    for(const auto& [path,snapshot]:fRenameOpen) if(snapshot.owner!=fSessionToken) {
        const auto* profile=fLanguageTools.ForFile(path);
        if(profile && sourceProfile && profile->command==sourceProfile->command) {
            auto uri=FileURI(path);server->Open(uri,profile->language,snapshot.version,snapshot.text);fRenameBorrowed.push_back(uri);
        }
    }
    if(fRenameWindow.IsValid()) fRenameWindow.SendMessage(B_QUIT_REQUESTED);
    auto start=source->SendMessage(SCI_WORDSTARTPOSITION,fRenameCaret,1),end=source->SendMessage(SCI_WORDENDPOSITION,fRenameCaret,1);
    auto name=fRenameText.substr(start,end-start);auto generation=fLanguageGeneration;auto key=d->serverKey,uri=d->serverURI;
    Notice("Checking symbol rename and capturing project file versions…");
    fJobs->Submit([this,serial,root,name,key,uri,provider,generation](const auto& cancel) {
        auto index=IndexProject(root,&cancel,500000,true);if(index.truncated || !index.error.empty()) throw std::runtime_error("Cannot capture the complete project for rename: "+index.error);
        std::map<std::string,FileStamp> stamps;
        for(const auto& path:index.paths) { if(cancel) break;auto full=CanonicalPath((fs::path(root)/path).string());stamps.emplace(full,StatFile(full)); }
        return [this,serial,root,name,key,uri,provider,generation,stamps=std::move(stamps)]() mutable {
            if(serial!=fRenameSerial || generation!=fLanguageGeneration) return;
            auto* document=ByID(fRenameDocument);auto found=fServers.find(key);
            if(!document || !document->editor->Matches(fRenameText) || found==fServers.end() || !found->second.client || !found->second.client->Ready()) { Notice("The source changed. Run Rename Symbol again.");return; }
            fRenameStamps=std::move(stamps);auto* server=found->second.client.get();
            auto show=[this,serial,name](const std::string& placeholder) {
                if(serial!=fRenameSerial) return;
                auto* prompt=new RenamePrompt(this,placeholder.empty()?name:placeholder,serial,fEditorSettings.Colors());fRenameWindow=BMessenger(prompt);
            };
            if(!provider.is_object() || !provider.value("prepareProvider",false)) { show(name);return; }
            auto encoding=server->Encoding();auto text=fRenameText;
            server->Request("textDocument/prepareRename",{{"textDocument",{{"uri",uri}}},{"position",PositionJSON(PositionAt(text,fRenameCaret,encoding))}},
                [this,serial,generation,show,text,encoding](RpcReply reply) {
                    fJobs->Post([this,serial,generation,show,text,encoding,reply=std::move(reply)] {
                        if(serial!=fRenameSerial || generation!=fLanguageGeneration) return;
                        auto* d=ByID(fRenameDocument);if(!d || !d->editor->Matches(text)) { Notice("The source changed. Run Rename Symbol again.");return; }
                        if(!reply.ok() || reply.result.is_null()) { Notice(reply.ok()?"The symbol at the caret cannot be renamed.":reply.error);return; }
                        try {
                            std::string placeholder;
                            if(reply.result.contains("placeholder")) placeholder=reply.result.at("placeholder");
                            else if(!reply.result.value("defaultBehavior",false)) {
                                const auto& range=reply.result.contains("range")?reply.result.at("range"):reply.result;
                                auto start=OffsetAt(text,ReadPosition(range.at("start")),encoding),end=OffsetAt(text,ReadPosition(range.at("end")),encoding);
                                if(end<start) throw std::runtime_error("Invalid prepareRename range");placeholder=text.substr(start,end-start);
                            }
                            show(placeholder);
                        }catch(const std::exception& e) { Notice(e.what()); }
                    });
                });
        };
    },"rename-snapshots",[this,serial](const std::string& error){if(serial==fRenameSerial) Notice(error);});
}
void Workspace::SubmitRename(const BMessage& message) {
    int64 serial=0;message.FindInt64("serial",&serial);if(serial!=fRenameSerial || fApplyingEdit) return;
    const char* name=nullptr;if(message.FindString("name",&name)!=B_OK) return;
    auto fail=[this](const std::string& error) { Notice(error);BMessage status(kEditPreview);status.AddString("status",error.c_str());fRenameWindow.SendMessage(&status); };
    auto error=ValidateRenameName(name);if(!error.empty()) { fail(error);return; }
    auto* d=ByID(fRenameDocument);if(!d || !d->editor || !d->editor->Matches(fRenameText)) { fail("The source changed. Run Rename Symbol again.");return; }
    error=ValidateRenameName(name,d->serverLanguage);if(!error.empty()) { fail(error);return; }
    auto found=fServers.find(d->serverKey);if(found==fServers.end() || !found->second.client || !found->second.client->Ready()) { fail("The language server stopped. Run Rename Symbol again.");return; }
    for(const auto& [path,before]:fRenameOpen) {
        try { FileEdit file;file.before=before;error=ValidateEdit(file,CurrentSnapshot(before)); }catch(const std::exception& e) { error=e.what(); }
        if(!error.empty()) { fail(path+": "+error+". Run Rename Symbol again.");return; }
    }
    auto* server=found->second.client.get();auto encoding=server->Encoding();auto open=fRenameOpen;auto stamps=fRenameStamps;
    auto generation=fLanguageGeneration,editSerial=++fEditSerial;auto root=fProject;auto newName=std::string(name);
    server->Request("textDocument/rename",{{"textDocument",{{"uri",d->serverURI}}},{"position",PositionJSON(PositionAt(fRenameText,fRenameCaret,encoding))},{"newName",newName}},
        [this,serial,generation,editSerial,encoding,root,open,stamps,newName,fail](RpcReply reply) {
            fJobs->Post([this,serial,generation,editSerial,encoding,root,open,stamps,newName,fail,reply=std::move(reply)] {
                if(serial!=fRenameSerial || generation!=fLanguageGeneration || editSerial!=fEditSerial) return;
                if(!reply.ok()) { fail(reply.error);return; }
                fJobs->Submit([this,serial,generation,editSerial,encoding,root,open,stamps,newName,reply](const auto& cancel) {
                    auto plan=std::make_shared<EditPlan>(WorkspaceEditPlan(reply.result,encoding,[&](const std::string& path) {
                        if(open.count(path)) return open.at(path);
                        auto stamp=stamps.find(path);if(stamp==stamps.end()) throw std::runtime_error("Rename affects a file outside the captured project: "+path);
                        auto snapshot=DiskSnapshot(path,&cancel);if(snapshot.stamp!=stamp->second) throw std::runtime_error("File changed during rename: "+path+". Run Rename Symbol again.");return snapshot;
                    }));
                    plan->title="Rename Symbol to "+newName;plan->root=root;plan->summary=std::to_string(plan->files.size())+" affected files · all symbol edits are applied together";
                    return [this,serial,generation,editSerial,plan] {
                        if(serial!=fRenameSerial || generation!=fLanguageGeneration || editSerial!=fEditSerial) return;
                        if(fRenameWindow.IsValid()) fRenameWindow.SendMessage(B_QUIT_REQUESTED);ShowEditPreview(plan);
                    };
                },"rename-preview",fail);
            });
        });
}
}
