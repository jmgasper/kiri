#include "core/Project.h"
#include "core/Process.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <queue>
#include <set>
#include <fnmatch.h>

namespace kiri {
namespace fs=std::filesystem;
namespace {
std::string Lower(std::string s) {
    for(auto& c:s) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
}
DirectoryResult ListDirectory(const std::string& path, const std::atomic<bool>* cancel) {
    DirectoryResult result;
    std::error_code error;
    fs::directory_iterator iterator(path,error);
    if(error) { result.error=error.message();return result; }
    for(;iterator!=fs::directory_iterator();iterator.increment(error)) {
        if(error) { result.error=error.message();break; }
        if(cancel && cancel->load()) break;
        auto& entry=*iterator;
        DirectoryEntry item;
        item.name=entry.path().filename().string();item.path=entry.path().string();
        item.symlink=entry.is_symlink(error);error.clear();
        item.directory=entry.is_directory(error);error.clear();
        result.entries.push_back(std::move(item));
    }
    std::sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b) {
        if(a.directory!=b.directory) return a.directory;
        auto al=Lower(a.name),bl=Lower(b.name);
        return al==bl ? a.name<b.name : al<bl;
    });
    return result;
}
ProjectIndex IndexProject(const std::string& root, const std::atomic<bool>* cancel, size_t limit,bool includeIgnored) {
    ProjectIndex result;
    ProcessOptions opts;opts.directory=root;opts.cancel=cancel;opts.outputLimit=64*1024*1024;
    std::vector<std::string> command={"git","ls-files","-z","--cached","--others"};
    if(!includeIgnored) command.push_back("--exclude-standard");
    auto git=RunProcess(command,opts);
    if(git.ok()) {
        size_t offset=0;
        while(offset<git.output.size()) {
            auto end=git.output.find('\0',offset);
            if(end==std::string::npos) break;
            if(result.paths.size()>=limit) { result.truncated=true;break; }
            auto path=git.output.substr(offset,end-offset);offset=end+1;
            std::error_code error;auto status=fs::symlink_status(fs::path(root)/path,error);
            if(!error && (fs::is_regular_file(status) || fs::is_symlink(status))) result.paths.push_back(std::move(path));
        }
        std::sort(result.paths.begin(),result.paths.end());
        result.paths.erase(std::unique(result.paths.begin(),result.paths.end()),result.paths.end());
        return result;
    }
    if(cancel && cancel->load()) return result;
    const std::set<std::string> excluded={".git",".hg",".svn","node_modules",".cache",".vm","__pycache__",".venv","vendor","build","dist","target"};
    std::vector<fs::path> pending{root};
    while(!pending.empty() && !(cancel && cancel->load())) {
        auto current=pending.back();pending.pop_back();
        auto listing=ListDirectory(current.string(),cancel);
        if(!listing.error.empty()) { if(current==fs::path(root)) result.error=listing.error;continue; }
        for(const auto& entry:listing.entries) {
            if(entry.symlink) continue; // no cycles, escaping project, or device reads during indexing
            if(entry.directory) { if(entry.name!=".git" && entry.name!=".hg" && entry.name!=".svn" && (includeIgnored || !excluded.count(entry.name))) pending.emplace_back(entry.path); }
            else {
                if(result.paths.size()>=limit) { result.truncated=true;return result; }
                result.paths.push_back(fs::path(entry.path).lexically_relative(root).string());
            }
        }
    }
    return result;
}
int FuzzyScore(const std::string& query, const std::string& path) {
    if(query.empty()) return -static_cast<int>(path.size());
    size_t pos=0;int score=0;int previous=-2;
    for(unsigned char q:query) {
        bool found=false;
        for(;pos<path.size();++pos) {
            if(std::tolower(static_cast<unsigned char>(path[pos]))!=std::tolower(q)) continue;
            score+=10;
            if(static_cast<int>(pos)==previous+1) score+=12;
            if(pos==0 || path[pos-1]=='/' || path[pos-1]=='_' || path[pos-1]=='-') score+=20;
            if(path[pos]==q) score+=2;
            previous=static_cast<int>(pos++);found=true;break;
        }
        if(!found) return -1000000;
    }
    return score-static_cast<int>(path.size());
}
std::vector<std::string> QuickOpen(const ProjectIndex& index,const std::string& query,size_t limit) {
    using Candidate=std::pair<int,size_t>;
    std::priority_queue<Candidate,std::vector<Candidate>,std::greater<Candidate>> best;
    for(size_t i=0;i<index.paths.size();++i) {
        int score=FuzzyScore(query,index.paths[i]);
        if(score<=-1000000) continue;
        best.emplace(score,i);if(best.size()>limit) best.pop();
    }
    std::vector<std::string> result(best.size());
    for(size_t n=best.size();n>0;--n) { result[n-1]=index.paths[best.top().second];best.pop(); }
    return result;
}

namespace {
std::vector<std::string> Patterns(const std::string& text) {
    std::vector<std::string> parts;
    for(size_t start=0;start<text.size();) {
        auto end=text.find(';',start);if(end==std::string::npos) end=text.size();
        auto part=text.substr(start,end-start);auto first=part.find_first_not_of(" \t");
        if(first!=std::string::npos) parts.push_back(part.substr(first,part.find_last_not_of(" \t")-first+1));
        start=end+1;
    }return parts;
}
bool Glob(const std::string& pattern,const std::string& path) {
    // * spans directory separators; **/ additionally matches zero directories.
    if(fnmatch(pattern.c_str(),path.c_str(),0)==0) return true;
    auto recursive=pattern.find("**/");
    if(recursive!=std::string::npos && Glob(pattern.substr(0,recursive)+pattern.substr(recursive+3),path)) return true;
    return pattern.find('/')==std::string::npos && fnmatch(pattern.c_str(),fs::path(path).filename().c_str(),0)==0;
}
}
bool MatchesProjectPath(const std::string& path,const ProjectSearchOptions& options) {
    auto folders=Patterns(options.folders),include=Patterns(options.include),exclude=Patterns(options.exclude);
    bool scoped=folders.empty();
    for(auto folder:folders) {
        while(!folder.empty() && folder.back()=='/') folder.pop_back();
        if(folder.empty() || folder=="." || path==folder || path.rfind(folder+"/",0)==0) scoped=true;
    }
    if(!scoped) return false;
    if(!include.empty() && std::none_of(include.begin(),include.end(),[&](const auto& p){return Glob(p,path);})) return false;
    return std::none_of(exclude.begin(),exclude.end(),[&](const auto& p){return Glob(p,path);});
}
std::string SearchResult::Summary() const {
    if(!error.empty()) return error;
    return std::to_string(matches.size())+" matches"+(truncated?" (result limit reached)":"")+(cancelled?" · cancelled":"")
        +" · skipped: "+std::to_string(binary)+" binary/encoding, "+std::to_string(large)+" size, "
        +std::to_string(unreadable)+" unreadable, "+std::to_string(symlinks)+" symlinks, "+std::to_string(filtered)+" filtered";
}
SearchResult SearchProject(const std::string& root,const ProjectIndex& index,const std::string& query,
    bool matchCase,const std::atomic<bool>* cancel,size_t limit) {
    ProjectSearchOptions options;options.query=query;options.matchCase=matchCase;
    return SearchProject(root,index,options,cancel,limit);
}
SearchResult SearchProject(const std::string& root,const ProjectIndex& index,const ProjectSearchOptions& options,
    const std::atomic<bool>* cancel,size_t limit,const std::map<std::string,FileData>* buffers,const std::string* replacement) {
    SearchResult result;TextQuery query(options);result.error=replacement?query.ValidateReplacement(*replacement):query.Error();
    if(!result.error.empty() || options.query.empty()) return result;
    std::set<std::string> paths(index.paths.begin(),index.paths.end());
    // The index decides ignored-file inclusion. Unsaved buffers replace the
    // contents of indexed files without silently widening the selected scope.
    size_t snapshotBytes=0;
    for(const auto& relative:paths) {
        if(cancel && cancel->load()) { result.cancelled=true;break; }
        if(!MatchesProjectPath(relative,options)) { ++result.filtered;continue; }
        fs::path path=fs::path(root)/relative;std::error_code error;
        if(fs::path(relative).is_absolute() || relative==".." || relative.rfind("../",0)==0 || CanonicalPath(path.string()).rfind(CanonicalPath(root)+"/",0)!=0
                || fs::is_symlink(path,error)) { ++result.symlinks;++result.skipped;continue; }
        FileData data;bool fromBuffer=buffers && buffers->count(relative);
        if(fromBuffer) data=buffers->at(relative);
        else {
            auto size=fs::file_size(path,error);
            if(!error && size>32*1024*1024) { ++result.large;++result.skipped;continue; }
            data=ReadFile(path.string(),cancel,32*1024*1024);
        }
        if(!data.ok()) { ++result.unreadable;++result.skipped;continue; }
        if(data.binary || !data.utf8) { ++result.binary;++result.skipped;continue; }
        if(!fromBuffer && data.bom) data.bytes.erase(0,3);
        if(data.bytes.size()>32*1024*1024) { ++result.large;++result.skipped;continue; }
        size_t first=result.matches.size(),line=1;
        for(size_t start=0;start<=data.bytes.size();) {
            if(cancel && cancel->load()) { result.cancelled=true;break; }
            auto end=data.bytes.find_first_of("\r\n",start);if(end==std::string::npos) end=data.bytes.size();
            auto found=query.Find(data.bytes,start,end,limit-result.matches.size(),cancel,replacement);
            if(!found.error.empty()) { result.error=relative+": "+found.error;result.matches.clear();result.snapshots.clear();return result; }
            for(auto& match:found.matches) {
                auto begin=match.start>start+120?match.start-120:start;
                while(begin>start && (static_cast<unsigned char>(data.bytes[begin])&0xc0)==0x80) --begin;
                auto excerptEnd=std::min(end,begin+400);
                while(excerptEnd<end && (static_cast<unsigned char>(data.bytes[excerptEnd])&0xc0)==0x80) ++excerptEnd;
                auto column=PositionAt(std::string_view(data.bytes).substr(start,end-start),match.start-start,PositionEncoding::UTF32).character+1;
                result.matches.push_back({relative,data.bytes.substr(begin,excerptEnd-begin),line,column,match.start,match.end,std::move(match.text)});
            }
            if(found.truncated) { result.truncated=true;break; }
            if(found.cancelled) { result.cancelled=true;break; }
            if(end==data.bytes.size()) break;
            start=end+1;if(data.bytes[end]=='\r' && start<data.bytes.size() && data.bytes[start]=='\n') ++start;
            ++line;
        }
        if(replacement && first!=result.matches.size()) {
            snapshotBytes+=data.bytes.size();
            if(snapshotBytes>128*1024*1024) { result.error="Replacement preview exceeds 128 MiB. Narrow the folders or filters.";result.matches.clear();result.snapshots.clear();return result; }
            result.snapshots.emplace(relative,std::move(data));
        }
        if(result.truncated || result.cancelled) break;
    }
    return result;
}
}
