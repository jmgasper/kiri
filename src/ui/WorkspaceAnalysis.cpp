#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/ProblemsView.h"
#include <CardLayout.h>
#include <OS.h>
#include <algorithm>

namespace kiri {
void Workspace::InvalidateAnalysis(Document& d) {
    ++d.diagnosticSerial;++d.semanticSerial;d.diagnostics.clear();d.semanticTokens.clear();d.semanticVersion=-1;
    if(d.editor) {d.editor->ClearAnalysis();d.analysisRevision=d.editor->InputRevision();}
    auto found=fServers.find(d.serverKey);
    if(found!=fServers.end()) {
        if(found->second.client) found->second.client->Cancel(d.semanticRequest);
        if(found->second.unversionedDiagnostics) {
            found->second.snapshotDirty=true;found->second.snapshotChangedAt=system_time();
            // A snapshot can depend on any open document, including imports.
            for(auto& other:fDocuments) if(other.get()!=&d && other->serverKey==d.serverKey) {
                ++other->diagnosticSerial;other->diagnostics.clear();if(other->editor) other->editor->SetDiagnostics({});
                other->diagnosticStatus="Waiting for a stable document snapshot";
            }
        }
    }
    d.semanticRequest=0;d.diagnosticStatus="Waiting for diagnostics";d.semanticStatus.clear();fProblemsDirty=true;
}
void Workspace::RequestSemantic(Document& d,LanguageServer& server) {
    if(!fEditorSettings.semanticHighlighting) {d.semanticStatus="Semantic highlighting disabled";d.semanticVersion=d.serverVersion;d.semanticTokens.clear();d.editor->SetSemanticTokens({});return;}
    auto capabilities=server.Capabilities();auto provider=capabilities.value("semanticTokensProvider",Json());
    if(!provider.is_object() || !provider.contains("legend") || !provider["legend"].is_object() || !provider.contains("full") || (provider["full"]!=true && !provider["full"].is_object())) {
        d.semanticStatus="Lexical highlighting · server has no full semantic tokens";d.semanticVersion=d.serverVersion;fProblemsDirty=true;return;
    }
    server.Cancel(d.semanticRequest);auto id=d.id,generation=fLanguageGeneration,serial=++d.semanticSerial;auto version=d.serverVersion;auto revision=d.editor->InputRevision();
    auto text=std::make_shared<std::string>(d.serverText);auto uri=d.serverURI;auto legend=provider["legend"];auto encoding=server.Encoding();d.semanticStatus="Updating semantic highlighting…";
    d.semanticRequest=server.Request("textDocument/semanticTokens/full",{{"textDocument",{{"uri",uri}}}},[this,id,generation,serial,version,revision,text,uri,legend,encoding](RpcReply reply) {
        auto result=reply.ok()?ReadSemanticTokens(reply.result,legend,*text,encoding):SemanticResult{{},reply.error};
        fJobs->Post([this,id,generation,serial,version,revision,text,uri,result=std::move(result)] {
            auto* d=ByID(id);if(!d || !d->editor || generation!=fLanguageGeneration || serial!=d->semanticSerial) return;
            d->semanticRequest=0;if(d->serverURI!=uri || d->serverVersion!=version || d->editor->InputRevision()!=revision || !d->editor->Matches(*text)) return;
            d->semanticVersion=version;d->semanticTokens=result.tokens;d->editor->SetSemanticTokens(result.tokens);
            d->semanticStatus=result.error.empty()?std::to_string(result.tokens.size())+" semantic tokens":"Lexical highlighting · "+result.error;
            fProblemsDirty=true;UpdateProblems();
        });
    });
}
void Workspace::LanguageNotification(const std::string& key,int64 generation,const std::string& method,const Json& params) {
    if(generation!=fLanguageGeneration) return;auto found=fServers.find(key);if(found==fServers.end()) return;
    if(method=="workspace/semanticTokens/refresh") {
        for(auto& d:fDocuments) if(d->serverKey==key) {
            if(found->second.client) found->second.client->Cancel(d->semanticRequest);
            ++d->semanticSerial;d->semanticRequest=0;d->semanticVersion=-1;d->semanticTokens.clear();if(d->editor) d->editor->SetSemanticTokens({});
        }return;
    }
    if(method!="textDocument/publishDiagnostics" || !params.is_object()) return;
    if(!params.contains("version") || params["version"].is_null()) {
        auto& entry=found->second;
        if(!entry.unversionedDiagnostics) {
            entry.unversionedDiagnostics=true;entry.snapshotDirty=true;entry.snapshotChangedAt=system_time();
            for(auto& d:fDocuments) if(d->serverKey==key) {d->diagnosticStatus="Preparing a version-safe diagnostic snapshot…";++d->diagnosticSerial;d->diagnostics.clear();if(d->editor) d->editor->SetDiagnostics({});}
            fProblemsDirty=true;UpdateProblems();
        }
        return;
    }
    if(!found->second.unversionedDiagnostics) AcceptDiagnostics(key,generation,params);
}
bool Workspace::DiagnosticSnapshotCurrent(const std::string& key,int64 serial) {
    auto found=fServers.find(key);if(found==fServers.end()) return false;auto& entry=found->second;
    if(serial!=entry.snapshotSerial || entry.snapshotDirty || !entry.snapshotClient) return false;
    for(const auto& record:entry.snapshots) {
        auto* d=ByID(record.document);
        if(!d || !d->editor || d->serverKey!=key || d->serverURI!=record.uri || d->editor->InputRevision()!=record.revision) return false;
    }
    // New files are dependencies too; do not accept a partial old workspace.
    for(const auto& d:fDocuments) if(d->editor && d->serverKey==key &&
        std::none_of(entry.snapshots.begin(),entry.snapshots.end(),[&](const auto& record){return record.document==d->id;})) return false;
    return true;
}
void Workspace::AcceptDiagnostics(const std::string& key,int64 generation,const Json& params,int64 snapshot) {
    auto found=fServers.find(key);if(generation!=fLanguageGeneration || found==fServers.end() || !params.is_object()) return;
    auto& entry=found->second;std::string uri;
    try {uri=params.at("uri").get<std::string>();}catch(...) {return;}
    Document* d=nullptr;std::shared_ptr<std::string> text;int64 revision=0;PositionEncoding encoding=PositionEncoding::UTF16;
    if(snapshot) {
        if(!DiagnosticSnapshotCurrent(key,snapshot)) return;
        auto record=std::find_if(entry.snapshots.begin(),entry.snapshots.end(),[&](const auto& item){return item.uri==uri;});if(record==entry.snapshots.end()) return;
        d=ByID(record->document);if(!d || !d->editor || d->serverKey!=key || d->editor->InputRevision()!=record->revision || !d->editor->Matches(record->text)) return;
        if(params.contains("version") && !params["version"].is_null() && params["version"]!=record->version) return;
        text=std::make_shared<std::string>(record->text);revision=record->revision;encoding=entry.snapshotClient->Encoding();
    }else {
        for(auto& candidate:fDocuments) if(candidate->serverKey==key && candidate->serverURI==uri) {d=candidate.get();break;}
        // Haiku may deliver SCN_MODIFIED after didChange has already been sent.
        // The synchronized input revision and bytes are authoritative here.
        if(!d || !d->editor || !entry.client || !entry.client->Ready() || d->serverRevision!=d->editor->InputRevision() || !params.contains("version") || params["version"]!=d->serverVersion || !d->editor->Matches(d->serverText)) return;
        text=std::make_shared<std::string>(d->serverText);revision=d->editor->InputRevision();encoding=entry.client->Encoding();
    }
    auto id=d->id,serial=++d->diagnosticSerial;auto rows=params.value("diagnostics",Json::array());
    fJobs->Submit([this,id,serial,generation,key,snapshot,revision,text,rows=std::move(rows),encoding](const auto&) {
        auto result=ReadDiagnostics(rows,*text,encoding);
        return [this,id,serial,generation,key,snapshot,revision,text,result=std::move(result)] {
            auto* d=ByID(id);
            if(!d || !d->editor || generation!=fLanguageGeneration || d->diagnosticSerial!=serial || d->serverKey!=key || d->editor->InputRevision()!=revision || !d->editor->Matches(*text)) return;
            if(snapshot && !DiagnosticSnapshotCurrent(key,snapshot)) return;
            d->diagnostics=result.items;d->editor->SetDiagnostics(result.items);d->diagnosticStatus=result.warning.empty()?"Diagnostics current":result.warning;fProblemsDirty=true;UpdateProblems();
        };
    },"diagnostics-"+std::to_string(id),[this,id,serial](const std::string& error){if(auto* d=ByID(id);d && d->diagnosticSerial==serial) {d->diagnosticStatus=error;fProblemsDirty=true;UpdateProblems();}});
}
void Workspace::StartDiagnosticSnapshot(const std::string& key) {
    auto found=fServers.find(key);if(found==fServers.end()) return;auto& entry=found->second;
    if(entry.snapshotClient) {
        auto old=std::move(entry.snapshotClient);
        fJobs->Submit([old=std::move(old)](const auto&) mutable {old->Stop();old.reset();return AsyncQueue::Callback();});
    }
    entry.snapshots.clear();entry.snapshotDirty=false;auto serial=++entry.snapshotSerial,generation=fLanguageGeneration;
    for(auto& d:fDocuments) if(d->serverKey==key && d->editor && d->serverVersion && !d->languageDirty && d->serverRevision==d->editor->InputRevision() && size_t(d->editor->SendMessage(SCI_GETLENGTH))<=kLanguageBytes) {
        entry.snapshots.push_back({d->id,d->editor->InputRevision(),d->serverVersion,d->serverURI,d->serverLanguage,d->serverText});d->diagnosticStatus="Analyzing a stable document snapshot…";
    }
    if(entry.snapshots.empty()) return;
    entry.snapshotClient=std::make_shared<LanguageServer>(entry.command,entry.root,[this,key,generation,serial](const std::string& error) {
        fJobs->Post([this,key,generation,serial,error] {
            auto found=fServers.find(key);if(generation!=fLanguageGeneration || found==fServers.end() || found->second.snapshotSerial!=serial || found->second.snapshotDirty) return;
            auto& entry=found->second;if(!entry.snapshotClient) return;
            if(error.empty()) {if(DiagnosticSnapshotCurrent(key,serial)) for(const auto& document:entry.snapshots) entry.snapshotClient->Open(document.uri,document.language,document.version,document.text);}
            else for(auto& d:fDocuments) if(d->serverKey==key) d->diagnosticStatus="Diagnostics unavailable: "+error;
            fProblemsDirty=true;UpdateProblems();
        });
    },entry.initialization,entry.configuration,[this,key,generation,serial](const std::string& method,const Json& params) {
        if(method=="textDocument/publishDiagnostics") fJobs->Post([this,key,generation,serial,params]{AcceptDiagnostics(key,generation,params,serial);});
    });
    fProblemsDirty=true;
}
void Workspace::AnalysisTick() {
    for(auto& d:fDocuments) if(d->editor) {
        if(d->analysisRevision!=d->editor->InputRevision()) InvalidateAnalysis(*d);
        if(d->diagnosticWaitAt && d->diagnosticStatus=="Waiting for diagnostics" && system_time()-d->diagnosticWaitAt>5000000) {d->diagnosticStatus="No diagnostic report received from this server";fProblemsDirty=true;}
        auto found=fServers.find(d->serverKey);
        if(found!=fServers.end() && found->second.client && found->second.client->Ready() && d->serverVersion && d->serverRevision==d->editor->InputRevision() && !d->languageDirty && !d->semanticRequest && d->semanticVersion!=d->serverVersion) RequestSemantic(*d,*found->second.client);
    }
    for(auto& pair:fServers) if(pair.second.unversionedDiagnostics && pair.second.snapshotDirty && system_time()-pair.second.snapshotChangedAt>600000) StartDiagnosticSnapshot(pair.first);
    UpdateProblems();
}
void Workspace::UpdateProblems() {
    if(!fProblemsDirty || !fProblems) return;fProblemsDirty=false;
    std::vector<ProblemEntry> entries;int errors=0,warnings=0;std::string states;
    for(auto& d:fDocuments) if(d->editor) {
        for(auto& problem:d->diagnostics) {if(entries.size()==10000) break;entries.push_back({d->id,d->diagnosticSerial,d->path,problem});errors+=problem.severity==1;warnings+=problem.severity==2;}
        if(!states.empty()) states+="\n";states+=d->name+": "+d->languageStatus+" · "+d->diagnosticStatus+" · "+d->semanticStatus;
    }
    std::string status=std::to_string(entries.size())+" problems · "+std::to_string(errors)+" errors · "+std::to_string(warnings)+" warnings";
    if(entries.size()==10000) status+=" · panel limit reached";
    if(entries.empty() && Current()) status+=" · "+Current()->languageStatus+" · "+Current()->diagnosticStatus;
    if(!states.empty()) status+="\n"+states;
    fProblems->SetProblems(std::move(entries),status);
}
void Workspace::JumpProblem(int64 id,int64 serial,size_t offset) {
    auto* d=ByID(id);if(!d || !d->editor || d->diagnosticSerial!=serial || d->analysisRevision!=d->editor->InputRevision()) return;
    auto found=std::find_if(d->diagnostics.begin(),d->diagnostics.end(),[&](const auto& item){return item.start==offset;});if(found==d->diagnostics.end()) return;
    fModeLayout->SetVisibleItem(int32(0));ShowDocument(*fActivePane,*d,1,1,false,true,true);
    auto* editor=Current()->editor;editor->SendMessage(SCI_ENSUREVISIBLEENFORCEPOLICY,editor->SendMessage(SCI_LINEFROMPOSITION,offset));editor->SendMessage(SCI_SETSEL,found->start,found->end);editor->SendMessage(SCI_SCROLLCARET);editor->MakeFocus();editor->ShowDiagnostic(offset);fProblems->SelectProblem(id,offset);
}
void Workspace::NavigateProblem(int direction) {
    std::vector<ProblemEntry> entries;for(const auto& entry:fProblems->Entries()) if(!fProblems->SeverityFilter() || fProblems->SeverityFilter()==entry.diagnostic.severity) entries.push_back(entry);
    if(entries.empty()) {Notice("No current problems match this severity in open files.");return;}
    auto* d=Current();int index=-1;auto offset=d && d->editor?size_t(d->editor->SendMessage(SCI_GETSELECTIONSTART)):0;
    for(size_t i=0;i<entries.size();++i) if(d && entries[i].document==d->id && entries[i].diagnostic.start==offset) {index=i;break;}
    index=index<0?(direction>0?0:int(entries.size())-1):(index+direction+int(entries.size()))%entries.size();auto entry=entries[index];JumpProblem(entry.document,entry.serial,entry.diagnostic.start);
}
}
