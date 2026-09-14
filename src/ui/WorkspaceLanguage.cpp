#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/LanguageToolsWindow.h"
#include "ui/SymbolBar.h"
#include <Alert.h>
#include <CardLayout.h>
#include <OS.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unistd.h>

namespace kiri {
namespace fs=std::filesystem;
namespace {
constexpr size_t LanguageLimit=8*1024*1024;
bool HasCapability(const Json& capabilities,const char* name) {
    auto found=capabilities.find(name);return found!=capabilities.end() && !found->is_null() && *found!=false;
}
bool Prefix(const std::string& candidate,const std::string& typed) {
    if(typed.size()>candidate.size()) return false;
    for(size_t i=0;i<typed.size();++i)
        if(std::tolower(static_cast<unsigned char>(candidate[i]))!=std::tolower(static_cast<unsigned char>(typed[i]))) return false;
    return true;
}
std::string OneLine(std::string text) { for(auto& c:text) if(static_cast<unsigned char>(c)<32 || c==127) c=' ';return text; }
}
void Workspace::ShowLanguageTools() {
    if(fLanguageToolsWindow.IsValid()) { fLanguageToolsWindow.SendMessage(kShowLanguageTools);return; }
    auto error=fLanguageTools.Load(fSettings);if(!error.empty()) Notice(error);
    auto* window=new LanguageToolsWindow(BMessenger(this),fLanguageTools,fSettings,Frame());fLanguageToolsWindow=BMessenger(window);window->Show();
}
void Workspace::FormatDocument(Editor* source) {
    Document* document=ByEditor(source);
    if(!document || !source || source->SendMessage(SCI_GETREADONLY)) return;
    if(document->path.empty()) { Notice("Save this file with an extension before formatting with Prettier.");return; }
    if(source->SendMessage(SCI_GETLENGTH)>static_cast<sptr_t>(LanguageLimit)) { Notice("Prettier formatting is limited to files up to 8 MiB.");return; }
    int64 view=0;for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->editor==source) view=tab->id;
    document->formatView=view;
    auto id=document->id,serial=++document->formatSerial;auto path=document->path,project=fProject,settings=fSettings,command=fLanguageTools.prettier;
    auto text=std::make_shared<std::string>(source->Text());Notice("Formatting "+document->name+" with Prettier…");
    fJobs->Submit([this,id,view,serial,path,project,settings,command,text](const auto& cancel) {
        auto result=FormatWithPrettier(command,path,project,settings,*text,&cancel);
        return [this,id,view,serial,path,text,result=std::move(result)] {
            auto* d=ByID(id);auto* tab=FindTab(view);if(!d || !tab || !tab->editor || d->path!=path || d->formatSerial!=serial) return;
            auto* editor=tab->editor;
            if(!result.ok()) {
                Notice("Prettier: "+result.diagnostic());
                (new BAlert("Prettier",result.diagnostic().c_str(),"OK"))->Go();return;
            }
            if(!editor->Matches(*text)) { Notice("Formatting was not applied because the file changed. Run Format with Prettier again.");return; }
            if(result.output==*text) { Notice("Prettier: no changes needed.");return; }
            editor->ApplyEdits({{0,text->size(),result.output}},true);d->languageDirty=true;d->textChangedAt=0;
            UpdateTabs();Notice("Formatted "+d->name+" with Prettier. Undo restores the previous text.");
        };
    },"format-"+std::to_string(id),[this](const std::string& error){Notice("Prettier: "+error);});
}
void Workspace::CancelCompletion() {
    ++fCompletionSerial;fTypedDocument=0;
    if(auto* document=ByID(fCompletionDocument)) {
        if(auto* tab=FindTab(fCompletionView);tab && tab->editor) tab->editor->CancelCompletions();
        auto found=fServers.find(document->serverKey);
        if(found!=fServers.end() && found->second.client) found->second.client->Cancel(document->completionRequest);
        document->completionRequest=0;
    }
    fCompletionDocument=0;fCompletionView=0;fCompletionText.clear();fCompletions.clear();fCompletionLabels.clear();
}
void Workspace::CloseLanguage(Document& document) {
    if(fCompletionDocument==document.id || fTypedDocument==document.id) CancelCompletion();
    auto found=fServers.find(document.serverKey);
    if(found!=fServers.end() && found->second.client && found->second.client->Ready()) {
        auto& server=*found->second.client;server.Cancel(document.symbolRequest);server.Cancel(document.completionRequest);
        if(document.serverVersion) server.Close(document.serverURI);
    }
    document.serverKey.clear();document.serverURI.clear();document.serverText.clear();document.symbols.clear();document.languageStatus.clear();document.symbolError.clear();
    document.serverVersion=0;document.symbolVersion=-1;document.symbolRequest=0;document.completionRequest=0;document.languageDirty=true;
}
void Workspace::ResetLanguages() {
    CancelCompletion();++fLanguageGeneration;
    for(auto& document:fDocuments) CloseLanguage(*document);
    for(auto& entry:fServers) if(entry.second.client) entry.second.client->Stop();
    fServers.clear();fSymbolBar->SetSymbols({},0,0);fSymbolBar->SetStatus("Updating language tools…");
}
LanguageServer* Workspace::EnsureLanguage(Document& document) {
    auto* editor=document.editor;
    if(!editor || editor->SendMessage(SCI_GETREADONLY)) { document.languageStatus="Source text only";return nullptr; }
    if(document.path.empty()) { document.languageStatus="Save to enable language tools";return nullptr; }
    if(editor->SendMessage(SCI_GETLENGTH)>static_cast<sptr_t>(LanguageLimit)) {
        if(!document.serverKey.empty()) CloseLanguage(document);
        document.languageStatus="Language tools paused above 8 MiB";return nullptr;
    }
    const auto* profile=fLanguageTools.ForFile(document.path);
    if(!profile) { document.languageStatus="No language server configured";return nullptr; }
    if(profile->command.empty()) { document.languageStatus="Language server disabled";return nullptr; }
    if(!document.serverKey.empty()) {
        auto found=fServers.find(document.serverKey);
        if(found!=fServers.end()) { document.languageStatus=found->second.status;return found->second.client.get(); }
    }
    std::string root=fProject;
    if(root.empty() || (document.path!=root && document.path.rfind(root+"/",0)!=0)) root=fs::path(document.path).parent_path().string();
    try {
        auto command=ToolCommand(profile->command,document.path,root,fSettings);
        if(command.empty()) return nullptr;
        std::string key=root+"\n";for(const auto& arg:command) { key+=arg;key.push_back('\0'); }
        key+=profile->initializationOptions;key.push_back('\0');key+=profile->configuration;
        document.serverKey=key;document.serverURI=FileURI(document.path);document.serverLanguage=profile->language;
        auto inserted=fServers.try_emplace(key);auto& entry=inserted.first->second;
        if(inserted.second) {
            auto executable=command[0];
            if(access(executable.c_str(),X_OK)!=0) entry.status="Install "+fs::path(executable).filename().string()+" · Language Tools…";
            else {
                entry.status="Starting language server…";auto generation=fLanguageGeneration;
                entry.client=std::make_unique<LanguageServer>(command,root,[this,key,generation](const std::string& error) {
                    fJobs->Post([this,key,generation,error] {
                        if(generation!=fLanguageGeneration) return;
                        auto found=fServers.find(key);if(found==fServers.end()) return;
                        found->second.status=error.empty()?"Language server ready":error;
                        for(auto& d:fDocuments) if(d->serverKey==key) d->languageStatus=found->second.status;
                        UpdateSymbolBar();
                    });
                },Json::parse(profile->initializationOptions),Json::parse(profile->configuration));
            }
        }
        document.languageStatus=entry.status;return entry.client.get();
    } catch(const std::exception& error) { document.languageStatus=error.what();return nullptr; }
}
bool Workspace::SyncLanguage(Document& document,LanguageServer& server) {
    if(!server.Ready()) return false;
    auto text=document.editor->Text();
    if(!document.serverVersion) {
        document.serverVersion=1;server.Open(document.serverURI,document.serverLanguage,1,text);
    } else if(document.serverText!=text) {
        server.Cancel(document.symbolRequest);document.symbolRequest=0;document.symbolVersion=-1;
        server.Change(document.serverURI,++document.serverVersion,document.serverText,text);
    } else { document.languageDirty=false;return false; }
    document.serverText=std::move(text);document.languageDirty=false;RequestSymbols(document,server);return true;
}
void Workspace::RequestSymbols(Document& document,LanguageServer& server) {
    document.symbolError.clear();
    if(!HasCapability(server.Capabilities(),"documentSymbolProvider")) { document.languageStatus="Server does not provide file symbols";return; }
    auto id=document.id,generation=fLanguageGeneration;int version=document.serverVersion;auto uri=document.serverURI;
    auto text=std::make_shared<std::string>(document.serverText);auto encoding=server.Encoding();
    document.symbolRequest=server.Request("textDocument/documentSymbol",{{"textDocument",{{"uri",uri}}}},
        [this,id,generation,version,uri,text,encoding](RpcReply reply) {
            auto symbols=reply.ok()?ReadSymbols(reply.result,*text,uri,encoding):std::vector<DocumentSymbol>();
            fJobs->Post([this,id,generation,version,uri,text,reply=std::move(reply),symbols=std::move(symbols)]() mutable {
                auto* d=ByID(id);if(!d || generation!=fLanguageGeneration || d->serverURI!=uri || d->serverVersion!=version || !d->editor->Matches(*text)) return;
                d->symbolRequest=0;d->symbolVersion=version;d->symbols=std::move(symbols);
                if(!reply.ok()) d->symbolError=reply.error;
                if(Current()==d) UpdateSymbolBar();
            });
        });
}
void Workspace::LanguageTick() {
    fJobs->Drain();
    for(auto& document:fDocuments) {
        auto* server=EnsureLanguage(*document);
        if(server && server->Ready() && (!document->serverVersion || (document->languageDirty && system_time()-document->textChangedAt>250000))) {
            try { SyncLanguage(*document,*server);if(Current()==document.get()) UpdateSymbolBar(); }
            catch(const std::exception& error) { document->languageStatus=error.what();UpdateSymbolBar(); }
        }
    }
    if(fTypedDocument && system_time()-fTypedAt>180000) {
        auto id=fTypedDocument;auto character=fTypedCharacter;fTypedDocument=0;
        if(auto* document=ByID(id);document && document==Current()) Complete(document->editor,false,character);
    }
}
void Workspace::UpdateSymbolBar() {
    auto* d=Current();
    if(!d || !d->editor) { fSymbolBar->SetSymbols({},0,0);fSymbolBar->SetStatus(d?"Source text only":"Open a source file");return; }
    auto* server=EnsureLanguage(*d);
    bool fresh=d->symbolVersion==d->serverVersion && !d->languageDirty;
    fSymbolBar->SetSymbols(fresh?d->symbols:std::vector<DocumentSymbol>(),d->id,d->symbolVersion);
    std::string status=d->languageStatus;
    if(!d->symbolError.empty()) status=d->symbolError;
    else if(server && server->Ready() && HasCapability(server->Capabilities(),"documentSymbolProvider"))
        status=fresh?(d->symbols.empty()?"No symbols in this file":std::to_string(d->symbols.size())+" symbols"):"Updating symbols…";
    fSymbolBar->SetStatus(OneLine(status),d->symbolError.empty()?d->languageStatus:d->symbolError);fSymbolBar->SetPosition(d->editor->SendMessage(SCI_GETCURRENTPOS));
}
void Workspace::Complete(Editor* source,bool manual,int character) {
    auto* d=Current();if(!d || !source || d->editor!=source || source->SendMessage(SCI_GETREADONLY)) return;
    if(!manual && !fLanguageTools.completion) return;
    if(manual) { ++fFocusSerial;fModeLayout->SetVisibleItem(int32(0));source->MakeFocus(); }
    auto* server=EnsureLanguage(*d);
    if(!server || !server->Ready()) { if(manual) { UpdateSymbolBar();Notice(d->languageStatus); }return; }
    auto capabilities=server->Capabilities();
    if(!HasCapability(capabilities,"completionProvider")) { if(manual) Notice("This language server does not provide completion.");return; }
    if(source->SendMessage(SCI_GETSELECTIONSTART)!=source->SendMessage(SCI_GETSELECTIONEND) || source->SendMessage(SCI_GETSELECTIONS)!=1) return;
    CancelCompletion();SyncLanguage(*d,*server);
    auto caret=source->SendMessage(SCI_GETCURRENTPOS),start=source->SendMessage(SCI_WORDSTARTPOSITION,caret,true);
    auto text=std::make_shared<std::string>(source->Text());
    Json context={{"triggerKind",1}};
    auto triggers=capabilities["completionProvider"].is_object()?capabilities["completionProvider"].value("triggerCharacters",Json::array()):Json::array();
    if(character>0 && character<128 && std::find(triggers.begin(),triggers.end(),std::string(1,char(character)))!=triggers.end()) context={{"triggerKind",2},{"triggerCharacter",std::string(1,char(character))}};
    else if(!manual && !(std::isalnum(static_cast<unsigned char>(character)) || character=='_' || character>=128)) return;
    fCompletionDocument=d->id;fCompletionView=CurrentTab()->id;fCompletionText=*text;fCompletionCaret=caret;fCompletionWordStart=start;
    auto id=d->id,serial=fCompletionSerial,generation=fLanguageGeneration;auto uri=d->serverURI;
    d->completionRequest=server->Request("textDocument/completion",{{"textDocument",{{"uri",uri}}},{"position",PositionJSON(PositionAt(*text,caret,server->Encoding()))},{"context",context}},
        [this,id,serial,generation,caret,start,text,manual](RpcReply reply) {
            auto items=reply.ok()?ReadCompletions(reply.result):std::vector<CompletionItem>();
            fJobs->Post([this,id,serial,generation,caret,start,text,manual,reply=std::move(reply),items=std::move(items)]() mutable {
                auto* d=ByID(id);
                if(!d || d!=Current() || !CurrentTab() || CurrentTab()->id!=fCompletionView || generation!=fLanguageGeneration || serial!=fCompletionSerial || !d->editor->Matches(*text) || d->editor->SendMessage(SCI_GETCURRENTPOS)!=caret) return;
                d->completionRequest=0;
                if(!reply.ok()) { if(manual) Notice(reply.error);return; }
                auto typed=text->substr(start,caret-start);std::set<std::string> labels;
                for(auto& item:items) {
                    if(!Prefix(item.filter,typed) && !Prefix(item.label,typed)) continue;
                    auto label=OneLine(item.label);
                    if(!item.detail.empty()) label+="   "+OneLine(item.detail);
                    if(!labels.insert(label).second) label+="  ["+std::to_string(fCompletions.size()+1)+"]";
                    fCompletionLabels.push_back(label);fCompletions.push_back(std::move(item));
                    if(fCompletions.size()>=200) break;
                }
                if(fCompletions.empty()) { if(manual) Notice("No completions at this position.");return; }
                d->editor->ShowCompletions(fCompletionLabels);
                if(manual) Notice("Choose a completion with ↑ / ↓ and Enter or Tab. Esc closes the list.");
            });
        });
}
void Workspace::AcceptCompletion(Editor* source,const std::string& label) {
    auto* d=ByID(fCompletionDocument);if(!d || d!=Current() || d->editor!=source || !source->Matches(fCompletionText) || source->SendMessage(SCI_GETCURRENTPOS)!=fCompletionCaret) { CancelCompletion();return; }
    auto found=std::find(fCompletionLabels.begin(),fCompletionLabels.end(),label);if(found==fCompletionLabels.end()) return;
    auto item=fCompletions[found-fCompletionLabels.begin()];auto server=fServers.find(d->serverKey);
    if(server==fServers.end() || !server->second.client || !server->second.client->Ready()) return;
    auto& client=*server->second.client;
    auto provider=client.Capabilities().value("completionProvider",Json::object());
    if(provider.is_object() && provider.value("resolveProvider",false)) {
        auto id=d->id,serial=fCompletionSerial;
        d->completionRequest=client.Request("completionItem/resolve",item.value,[this,id,serial,item](RpcReply reply) mutable {
            fJobs->Post([this,id,serial,item=std::move(item),reply=std::move(reply)]() mutable {
                if(!reply.ok()) { Notice("Cannot resolve completion: "+reply.error);return; }
                if(reply.result.is_object()) item.value.update(reply.result);
                ApplyCompletion(id,serial,std::move(item));
            });
        });
    } else ApplyCompletion(d->id,fCompletionSerial,std::move(item));
}
void Workspace::ApplyCompletion(int64 id,int64 serial,CompletionItem item) {
    auto* d=ByID(id);
    if(!d || d!=Current() || serial!=fCompletionSerial || !d->editor->Matches(fCompletionText) || d->editor->SendMessage(SCI_GETCURRENTPOS)!=fCompletionCaret) return;
    auto found=fServers.find(d->serverKey);if(found==fServers.end() || !found->second.client) return;
    auto& server=*found->second.client;
    try {
        auto edits=CompletionEdits(item,fCompletionText,fCompletionWordStart,fCompletionCaret,server.Encoding());
        d->editor->ApplyEdits(edits);d->editor->MakeFocus();d->languageDirty=true;d->textChangedAt=0;
        SyncLanguage(*d,server);
        if(item.value.contains("command")) {
            auto command=item.value["command"];
            server.Request("workspace/executeCommand",{{"command",command.at("command")},{"arguments",command.value("arguments",Json::array())}},[](RpcReply){});
        }
        CancelCompletion();UpdateTabs();
    } catch(const std::exception& error) { Notice("Cannot apply completion: "+std::string(error.what()));CancelCompletion(); }
}
}
