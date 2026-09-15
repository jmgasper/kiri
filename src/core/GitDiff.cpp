#include "core/Git.h"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace kiri {
namespace {
bool Hash(const std::string& hash) { return (hash.size()==40 || hash.size()==64) && std::all_of(hash.begin(),hash.end(),[](unsigned char c){return std::isxdigit(c);}); }
std::string TrimLine(std::string text) { while(!text.empty() && (text.back()=='\n' || text.back()=='\r')) text.pop_back();return text; }
DiffSource Blob(const GitRepository& git,const std::string& hash,const std::string& path,const std::string& label,const std::atomic<bool>* cancel) {
    DiffSource source;source.path=path;source.label=label;
    source.exists=!hash.empty() && hash.find_first_not_of('0')!=std::string::npos;if(!source.exists) return source;
    if(!Hash(hash)) { source.error="Invalid blob identifier.";return source; }
    auto size=git.Run({"cat-file","-s",hash},cancel);if(!size.ok()) { source.error=size.diagnostic();return source; }
    try { source.size=std::stoull(size.output); }catch(...) { source.error="Invalid blob size.";return source; }
    if(source.size>kDiffBytes) { source.error="Comparison limit: 8 MiB per side.";return source; }
    auto data=git.Run({"cat-file","blob",hash},cancel,{},kDiffBytes+1);
    if(!data.ok()) source.error=data.diagnostic();else source.text=std::move(data.output);
    source.binary=source.text.find('\0')!=std::string::npos || !IsValidUTF8(source.text);return source;
}
DiffSource RevisionPath(const GitRepository& git,const std::string& revision,const std::string& path,const std::string& label,const std::atomic<bool>* cancel) {
    auto entry=revision.empty()?git.Run({"ls-files","--stage","-z","--",path},cancel):git.Run({"ls-tree","-z",revision,"--",path},cancel);
    if(!entry.ok()) { DiffSource source;source.error=entry.diagnostic();return source; }
    auto tab=entry.output.find('\t');std::istringstream header(entry.output.substr(0,tab));std::string mode,hash,type;
    if(revision.empty()) { header>>mode>>hash>>type;if(!hash.empty() && type!="0") { DiffSource source;source.error="Unmerged index entries cannot be compared as one file.";return source; } }
    else header>>mode>>type>>hash;
    if(mode=="160000") { DiffSource source;source.path=path;source.label=label+" · submodule";source.text=hash+"\n";return source; }
    return Blob(git,hash,path,label+(mode=="120000"?" · symbolic link":""),cancel);
}
}
DiffModel GitRepository::CompareFile(const GitFile& file,bool staged,const std::atomic<bool>* cancel) const {
    DiffSource before,after;
    if(staged) {
        auto head=Run({"rev-parse","--verify","HEAD"},cancel);auto revision=head.ok()?TrimLine(head.output):std::string();
        auto prior=file.originalPath.empty()?file.path:file.originalPath;
        if(revision.empty()) { before.path=prior;before.label="HEAD (unborn)";before.exists=false; }
        else before=RevisionPath(*this,revision,prior,"HEAD "+revision.substr(0,8),cancel);
        after=RevisionPath(*this,"",file.path,"Index snapshot",cancel);
    } else {
        before=RevisionPath(*this,"",file.path,"Index snapshot",cancel);
        after=DiskDiffSource((std::filesystem::path(fDirectory)/file.path).string(),"Working tree · disk snapshot",cancel);after.path=file.path;
    }
    return CompareText(std::move(before),std::move(after),cancel);
}
GitDiffFiles GitRepository::CommitFiles(const std::string& hash,const std::atomic<bool>* cancel) const {
    GitDiffFiles result;if(!Hash(hash)) { result.error="Invalid commit identifier.";return result; }
    auto parents=Run({"rev-list","--parents","-n","1",hash},cancel);if(!parents.ok()) { result.error=parents.diagnostic();return result; }
    std::istringstream parentLine(parents.output);std::string current,parent;parentLine>>current>>parent;
    auto raw=parent.empty()?Run({"diff-tree","--root","--no-commit-id","--raw","-r","-z","--no-abbrev","--find-renames",hash,"--"},cancel):
        Run({"diff","--raw","-z","--no-abbrev","--find-renames",parent,hash,"--"},cancel);
    if(!raw.ok()) { result.error=raw.diagnostic();return result; }
    size_t at=0;auto token=[&]() { auto end=raw.output.find('\0',at);if(end==std::string::npos) { at=raw.output.size();return std::string(); }auto text=raw.output.substr(at,end-at);at=end+1;return text; };
    while(at<raw.output.size()) {
        auto text=token();if(text.empty() || text[0]!=':') { result.error="Invalid Git change list.";break; }
        GitDiffFile file;std::string status;std::istringstream header(text.substr(1));header>>file.beforeMode>>file.afterMode>>file.beforeBlob>>file.afterBlob>>status;
        file.path=token();file.originalPath=file.path;
        if(!status.empty() && (status[0]=='R' || status[0]=='C')) file.path=token();
        if(file.path.empty() || !Hash(file.beforeBlob) || !Hash(file.afterBlob)) { result.error="Incomplete Git change list.";break; }
        result.files.push_back(std::move(file));
        if(result.files.size()>2000) { result.error="Comparison limit: 2,000 changed files per commit.";break; }
    }
    if(!result.error.empty()) result.files.clear();
    return result;
}
DiffModel GitRepository::CompareCommitFile(const GitDiffFile& file,const std::string& hash,const std::atomic<bool>* cancel) const {
    auto read=[&](bool before) {
        auto blob=before?file.beforeBlob:file.afterBlob,mode=before?file.beforeMode:file.afterMode,path=before?file.originalPath:file.path;
        auto label=before?"Parent of "+hash.substr(0,8):"Commit "+hash.substr(0,8);
        if(mode=="160000") { DiffSource source;source.path=path;source.label=label+" · submodule";source.text=blob+"\n";return source; }
        return Blob(*this,blob,path,label+(mode=="120000"?" · symbolic link":""),cancel);
    };
    return CompareText(read(true),read(false),cancel);
}
}
