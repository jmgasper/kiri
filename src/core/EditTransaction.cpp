#include "core/EditTransaction.h"
#include <algorithm>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace kiri {
namespace fs=std::filesystem;
std::vector<TextEdit> FileEdit::Selected() const {
    std::vector<TextEdit> result;for(const auto& item:edits) if(item.selected) result.push_back(item.edit);return result;
}
void FileEdit::Prepare() {
    auto selected=Selected();auto ordered=OrderedEdits(selected,before.text.size());size_t size=before.text.size();
    for(const auto& edit:ordered) { size-=edit.end-edit.start;if(edit.text.size()>32*1024*1024 || size>32*1024*1024-edit.text.size()) throw std::runtime_error("Edited file would exceed 32 MiB. Narrow the replacement.");size+=edit.text.size(); }
    after=ApplyTextEdits(before.text,ordered);
}
EditSnapshot DiskSnapshot(const std::string& path,const std::atomic<bool>* cancel) {
    auto data=ReadFile(path,cancel,32*1024*1024);
    if(!data.ok()) throw std::runtime_error(path+": "+data.error);
    if(data.binary || !data.utf8) throw std::runtime_error(path+": only UTF-8 text files can be edited.");
    if(data.bom) data.bytes.erase(0,3);
    EditSnapshot snapshot;snapshot.path=CanonicalPath(path);snapshot.text=std::move(data.bytes);snapshot.stamp=data.stamp;snapshot.bom=data.bom;return snapshot;
}
EditPlan ReplacementPlan(const std::string& root,const ProjectIndex& index,const ProjectSearchOptions& options,
    const std::string& replacement,const SnapshotMap& open,const std::atomic<bool>* cancel) {
    std::map<std::string,FileData> buffers;
    for(const auto& [path,snapshot]:open) {
        auto relative=fs::path(path).lexically_relative(root).string();if(relative.empty() || relative.rfind("../",0)==0) continue;
        FileData data;data.bytes=snapshot.text;data.stamp=snapshot.stamp;data.bom=snapshot.bom;buffers.emplace(relative,std::move(data));
    }
    auto result=SearchProject(root,index,options,cancel,20000,&buffers,&replacement);
    if(!result.error.empty()) throw std::runtime_error(result.error);
    if(result.truncated || index.truncated) throw std::runtime_error("Replacement preview is incomplete. Narrow the scope (20,000 match / 500,000 file limit).");
    EditPlan plan;plan.title="Replace in Project";plan.root=root;plan.summary=result.Summary();
    if(result.cancelled) return plan;
    size_t totalBytes=0;
    for(auto& [relative,data]:result.snapshots) {
        FileEdit file;auto path=CanonicalPath((fs::path(root)/relative).string());
        if(open.count(path)) file.before=open.at(path);
        else { file.before.path=path;file.before.text=std::move(data.bytes);file.before.stamp=data.stamp;file.before.bom=data.bom; }
        for(auto& match:result.matches) if(match.path==relative) file.edits.push_back({{match.start,match.end,std::move(match.replacement)},true});
        file.Prepare();totalBytes+=file.before.text.size()+file.after.size();if(totalBytes>128*1024*1024) throw std::runtime_error("Preview exceeds 128 MiB of before/after text. Narrow the scope.");plan.files.push_back(std::move(file));
    }
    return plan;
}
std::string PathFromURI(const std::string& uri) {
    std::string encoded;
    if(uri.rfind("file:///",0)==0) encoded=uri.substr(7);
    else if(uri.rfind("file://localhost/",0)==0) encoded=uri.substr(16);
    else throw std::runtime_error("Unsupported document URI: "+uri+". Rename requires local files.");
    std::string path;
    auto hex=[](char c)->int { if(c>='0'&&c<='9') return c-'0';if(c>='a'&&c<='f') return c-'a'+10;if(c>='A'&&c<='F') return c-'A'+10;return -1; };
    for(size_t i=0;i<encoded.size();++i) {
        char c=encoded[i];
        if(c=='%' && i+2<encoded.size() && hex(encoded[i+1])>=0 && hex(encoded[i+2])>=0) { c=char(hex(encoded[i+1])*16+hex(encoded[i+2]));i+=2; }
        else if(c=='%' || c=='?' || c=='#') throw std::runtime_error("Malformed file URI: "+uri);
        if(!c) throw std::runtime_error("NUL in file URI");
        path+=c;
    }
    if(path.empty() || path[0]!='/' || !IsValidUTF8(path)) throw std::runtime_error("Invalid local file URI");
    return CanonicalPath(path);
}
std::string ValidateRenameName(const std::string& name,const std::string& language) {
    SearchOptions options;options.regex=true;options.matchCase=true;options.query="\\A[$_\\p{L}][$_\\p{L}\\p{N}\\p{M}]*\\z";
    auto found=TextQuery(options).Find(name);
    if(name.size()>1024 || found.matches.empty()) return "Enter a single identifier: start with a letter, _ or $, followed by letters, digits, _ or $.";
    const std::set<std::string> common={"break","case","catch","class","const","continue","default","delete","do","else","enum","export","false","for","if","import","new","return","switch","this","throw","true","try","void","while"};
    const std::set<std::string> js={"await","debugger","extends","finally","function","implements","in","instanceof","interface","let","null","package","private","protected","public","static","super","typeof","var","with","yield"};
    const std::set<std::string> cpp={"alignas","alignof","and","and_eq","asm","auto","bitand","bitor","bool","char","char8_t","char16_t","char32_t","compl","concept","consteval","constexpr","constinit","const_cast","co_await","co_return","co_yield","decltype","double","dynamic_cast","explicit","extern","float","friend","goto","inline","int","long","mutable","namespace","noexcept","not","not_eq","nullptr","operator","or","or_eq","private","protected","public","register","reinterpret_cast","requires","short","signed","sizeof","static","static_assert","static_cast","struct","template","thread_local","typedef","typeid","typename","union","unsigned","using","virtual","volatile","wchar_t","xor","xor_eq"};
    bool javascript=language.rfind("javascript",0)==0 || language.rfind("typescript",0)==0;
    bool cfamily=language=="cpp";
    if((javascript || cfamily) && (common.count(name) || (javascript?js.count(name):cpp.count(name)))) return "Choose a name that is not a reserved language keyword.";

    return {};
}
EditPlan WorkspaceEditPlan(const Json& edit,PositionEncoding encoding,const std::function<EditSnapshot(const std::string&)>& snapshot) {
    EditPlan plan;plan.title="Rename Symbol";plan.rename=true;
    if(edit.is_null()) return plan;
    if(!edit.is_object()) throw std::runtime_error("Invalid workspace edit.");
    if(edit.contains("changeAnnotations") && !edit["changeAnnotations"].empty()) throw std::runtime_error("Annotated workspace edits are not supported. No files were changed.");
    std::set<std::string> seen;size_t totalBytes=0;
    auto add=[&](const std::string& uri,const Json& version,const Json& edits) {
        auto path=PathFromURI(uri);
        if(!seen.insert(path).second) throw std::runtime_error("Repeated document in workspace edit: "+path);
        FileEdit file;file.before=snapshot(path);file.checkVersion=true;
        if(file.before.readOnly) throw std::runtime_error("Read-only document: "+path);
        if(!version.is_null() && (!version.is_number_integer() || !file.before.open || version.get<int>()!=file.before.version))
            throw std::runtime_error("Language server document version is stale: "+path);
        if(!edits.is_array()) throw std::runtime_error("Invalid document edits: "+path);
        for(const auto& change:edits) {
            if(change.contains("annotationId")) throw std::runtime_error("Annotated edits are not supported. No files were changed.");
            const auto& range=change.at("range");auto a=ReadPosition(range.at("start")),b=ReadPosition(range.at("end"));
            auto start=OffsetAt(file.before.text,a,encoding),end=OffsetAt(file.before.text,b,encoding);
            auto actualA=PositionAt(file.before.text,start,encoding),actualB=PositionAt(file.before.text,end,encoding);
            if(actualA.line!=a.line || actualA.character!=a.character || actualB.line!=b.line || actualB.character!=b.character)
                throw std::runtime_error("Invalid or stale Unicode edit range: "+path);
            file.edits.push_back({{start,end,change.at("newText").get<std::string>()},true});
        }
        file.Prepare();totalBytes+=file.before.text.size()+file.after.size();if(totalBytes>128*1024*1024) throw std::runtime_error("Rename preview exceeds 128 MiB of before/after text.");if(!file.edits.empty()) plan.files.push_back(std::move(file));
    };
    if(edit.contains("documentChanges")) {
        if(!edit["documentChanges"].is_array()) throw std::runtime_error("Invalid documentChanges.");
        // Reject the entire operation before resolving or applying any easy subset.
        for(const auto& change:edit["documentChanges"]) if(change.contains("kind"))
            throw std::runtime_error("Unsupported file operation: "+change.at("kind").get<std::string>()+". No files were changed.");
        for(const auto& change:edit["documentChanges"]) {
            auto& document=change.at("textDocument");add(document.at("uri"),document.value("version",Json()),change.at("edits"));
        }
    } else if(edit.contains("changes")) {
        if(!edit["changes"].is_object()) throw std::runtime_error("Invalid workspace changes.");
        for(auto it=edit["changes"].begin();it!=edit["changes"].end();++it) add(it.key(),nullptr,it.value());
    } else if(!edit.empty()) throw std::runtime_error("Unsupported workspace edit.");
    return plan;
}
std::string ValidateEdit(const FileEdit& file,const EditSnapshot& current,bool restore) {
    if(current.readOnly) return "Read-only document";
    if(current.path!=file.before.path) return "Document path changed";
    if(current.text!=(restore?file.after:file.before.text) || current.bom!=file.before.bom) return "Text changed since "+std::string(restore?"Apply":"preview");
    if(!restore && current.open!=file.before.open) return "Document opened or closed since preview; refresh preview";
    if(!restore && current.open && (current.owner!=file.before.owner || current.document!=file.before.document || current.revision!=file.before.revision
            || (file.checkVersion && current.version!=file.before.version))) return "Buffer version changed since preview";
    if(!restore && !current.open && current.stamp!=file.before.stamp) return "File changed on disk";
    if(restore && !file.before.open && current.stamp!=file.writtenStamp) return "File changed on disk";
    if(!restore && current.open && current.stamp!=file.before.stamp) return "File changed on disk since preview";
    return {};
}
std::string ApplyDiskEdit(FileEdit& file,bool restore) {
    try {
        auto current=DiskSnapshot(file.before.path);auto error=ValidateEdit(file,current,restore);
        if(!error.empty()) return error;
        auto& text=restore?file.before.text:file.after;
        error=SaveFileParts(file.before.path,{file.before.bom?std::string_view("\xef\xbb\xbf",3):std::string_view(),text},current.stamp);
        if(!error.empty()) return error;
        if(restore) file.restored=true;
        else { file.applied=true;file.writtenStamp=StatFile(file.before.path); }
        file.pending=false;file.status=restore?"Restored":"Changed on disk";file.error.clear();return {};
    }catch(const std::exception& e) { return e.what(); }
}
namespace {
Json StampJSON(const FileStamp& s) { return Json::array({s.device,s.inode,s.size,s.seconds,s.nanoseconds,s.exists}); }
FileStamp ReadStamp(const Json& s) { return {s.at(0),s.at(1),s.at(2),s.at(3),s.at(4),s.at(5)}; }
}
std::string WriteEditJournal(const EditPlan& plan) {
    try {
        Json data={{"format",1},{"title",plan.title},{"root",plan.root},{"rename",plan.rename},{"files",Json::array()}};
        for(const auto& file:plan.files) data["files"].push_back({{"path",file.before.path},{"before",file.before.text},{"after",file.after},
            {"bom",file.before.bom},{"open",file.before.open},{"stamp",StampJSON(file.before.stamp)},{"writtenStamp",StampJSON(file.writtenStamp)},
            {"applied",file.applied},{"restored",file.restored},{"pending",file.pending},{"status",file.status},{"error",file.error}});
        return SaveFile(plan.journal,data.dump(),StatFile(plan.journal));
    }catch(const std::exception& e) { return e.what(); }
}
EditPlan ReadEditJournal(const std::string& path) {
    auto data=ReadFile(path,nullptr,512*1024*1024);if(!data.ok()) throw std::runtime_error(data.error);
    auto json=Json::parse(data.bytes);if(json.at("format")!=1) throw std::runtime_error("Unknown project edit recovery format");
    EditPlan plan;plan.title=json.at("title");plan.root=json.at("root");plan.rename=json.at("rename");plan.journal=path;
    for(auto& item:json.at("files")) {
        FileEdit file;file.before.path=item.at("path");file.before.text=item.at("before");file.after=item.at("after");file.before.bom=item.at("bom");file.before.open=item.at("open");
        file.before.stamp=ReadStamp(item.at("stamp"));file.writtenStamp=ReadStamp(item.at("writtenStamp"));
        file.applied=item.at("applied");file.restored=item.at("restored");file.pending=item.at("pending");file.status=item.at("status");file.error=item.at("error");
        if(file.pending && file.before.open) file.applied=true;
        if(file.pending && !file.before.open) {
            // A crash may occur after rename(2) but before the journal update.
            try { auto current=DiskSnapshot(file.before.path);if(current.text==file.after && current.bom==file.before.bom) { file.applied=true;file.writtenStamp=current.stamp; } }catch(...) {}
        }
        plan.files.push_back(std::move(file));
    }
    return plan;
}
}
