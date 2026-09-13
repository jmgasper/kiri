#include "core/Project.h"
#include "core/Process.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <queue>
#include <set>

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
ProjectIndex IndexProject(const std::string& root, const std::atomic<bool>* cancel, size_t limit) {
    ProjectIndex result;
    ProcessOptions opts;opts.directory=root;opts.cancel=cancel;opts.outputLimit=64*1024*1024;
    auto git=RunProcess({"git","ls-files","-z","--cached","--others","--exclude-standard"},opts);
    if(git.ok()) {
        size_t offset=0;
        while(offset<git.output.size()) {
            auto end=git.output.find('\0',offset);
            if(end==std::string::npos) break;
            if(result.paths.size()>=limit) { result.truncated=true;break; }
            result.paths.emplace_back(git.output.substr(offset,end-offset));offset=end+1;
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
            if(entry.directory) { if(!excluded.count(entry.name)) pending.emplace_back(entry.path); }
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
SearchResult SearchProject(const std::string& root,const ProjectIndex& index,const std::string& query,
    bool matchCase,const std::atomic<bool>* cancel,size_t limit) {
    SearchResult result;
    if(query.empty()) return result;
    std::string needle=matchCase?query:Lower(query);
    for(const auto& relative:index.paths) {
        if(cancel && cancel->load()) break;
        fs::path path=fs::path(root)/relative;
        std::error_code error;
        if(fs::is_symlink(path,error) || !fs::is_regular_file(path,error) || fs::file_size(path,error)>32*1024*1024) { ++result.skipped;continue; }
        std::ifstream file(path,std::ios::binary);
        if(!file) { ++result.skipped;continue; }
        char sniff[4096];file.read(sniff,sizeof(sniff));
        if(std::find(sniff,sniff+file.gcount(),'\0')!=sniff+file.gcount()) { ++result.skipped;continue; }
        file.clear();file.seekg(0);
        std::string text;size_t line=0;
        while(std::getline(file,text)) {
            if(cancel && cancel->load()) return result;
            ++line;
            std::string haystack=matchCase?text:Lower(text);
            auto column=haystack.find(needle);
            if(column==std::string::npos) continue;
            size_t begin=column>120?column-120:0;
            result.matches.push_back({relative,text.substr(begin,400),line,column+1});
            if(result.matches.size()>=limit) { result.truncated=true;return result; }
        }
    }
    return result;
}
}
