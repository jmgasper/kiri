#include "core/Diff.h"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace kiri {
namespace {
std::vector<DiffLine> Lines(const std::string& text) {
    std::vector<DiffLine> result;
    for(size_t at=0;at<text.size();) {
        auto end=text.find_first_of("\r\n",at);if(end==std::string::npos) end=text.size();
        size_t ending=end==text.size()?0:text[end]=='\r' && end+1<text.size() && text[end+1]=='\n'?2:1;
        result.push_back({at,end-at,ending});at=end+ending;
        if(result.size()>kDiffLines) break;
    }
    return result;
}
std::string_view Line(const DiffSource& source,const DiffLine& line) { return std::string_view(source.text).substr(line.start,line.length+line.ending); }
bool Continuation(char c) { return (static_cast<unsigned char>(c)&0xc0)==0x80; }
}
DiffSource DiskDiffSource(const std::string& path,const std::string& label,const std::atomic<bool>* cancel) {
    DiffSource source;source.path=path;source.label=label;source.stamp=StatFile(path);source.exists=source.stamp.exists;source.size=source.stamp.size;
    std::error_code error;
    if(std::filesystem::is_symlink(std::filesystem::symlink_status(path,error))) {
        source.text=std::filesystem::read_symlink(path,error).string();source.exists=true;source.size=source.text.size();
        source.label+=" · symbolic link";if(error) source.error=error.message();return source;
    }
    if(!source.exists) return source;
    if(source.size>kDiffBytes) { source.error="Comparison limit: 8 MiB per side.";return source; }
    auto data=ReadFile(path,cancel,kDiffBytes);source.error=data.error;source.stamp=data.stamp;
    source.binary=data.binary || !data.utf8;source.text=std::move(data.bytes);
    if(StatFile(path)!=source.stamp) source.error="File changed while reading. Refresh the comparison.";
    return source;
}
ChangedText ChangedMiddle(std::string_view before,std::string_view after) {
    ChangedText result;result.beforeEnd=before.size();result.afterEnd=after.size();
    while(result.prefix<std::min(before.size(),after.size()) && before[result.prefix]==after[result.prefix]) ++result.prefix;
    while(result.prefix && ((result.prefix<before.size() && Continuation(before[result.prefix])) || (result.prefix<after.size() && Continuation(after[result.prefix])))) --result.prefix;
    while(result.beforeEnd>result.prefix && result.afterEnd>result.prefix && before[result.beforeEnd-1]==after[result.afterEnd-1]) { --result.beforeEnd;--result.afterEnd; }
    while(result.beforeEnd<before.size() && Continuation(before[result.beforeEnd])) { ++result.beforeEnd;++result.afterEnd; }
    return result;
}
DiffModel CompareText(DiffSource before,DiffSource after,const std::atomic<bool>* cancel,size_t workLimit) {
    DiffModel model;model.before=std::move(before);model.after=std::move(after);
    if(!model.before.error.empty() || !model.after.error.empty()) { model.error=!model.before.error.empty()?model.before.error:model.after.error;return model; }
    if(model.before.text.size()>kDiffBytes || model.after.text.size()>kDiffBytes) { model.error="Comparison limit: 8 MiB per side.";return model; }
    model.binary=model.before.binary || model.after.binary || model.before.text.find('\0')!=std::string::npos || model.after.text.find('\0')!=std::string::npos || !IsValidUTF8(model.before.text) || !IsValidUTF8(model.after.text);
    if(model.binary) return model;
    model.leftLines=Lines(model.before.text);model.rightLines=Lines(model.after.text);
    int n=model.leftLines.size(),m=model.rightLines.size();
    if(size_t(n)>kDiffLines || size_t(m)>kDiffLines) { model.error="Comparison limit: 100,000 lines per side.";return model; }
    auto equal=[&](int x,int y) { return Line(model.before,model.leftLines[x])==Line(model.after,model.rightLines[y]); };
    int maximum=n+m,offset=maximum+1;std::vector<int> v(2*maximum+3,0);std::vector<std::vector<int>> trace;
    size_t work=0;int distance=-1;
    for(int d=0;d<=maximum;++d) {
        if((cancel && *cancel) || work>workLimit || d>2048) { model.error=cancel && *cancel?"Comparison cancelled.":"Comparison work limit reached. Use smaller files or changes.";return model; }
        for(int k=-d;k<=d;k+=2) {
            int x=k==-d || (k!=d && v[offset+k-1]<v[offset+k+1])?v[offset+k+1]:v[offset+k-1]+1;
            int y=x-k;++work;
            while(x<n && y<m && equal(x,y)) { ++x;++y;++work; }
            v[offset+k]=x;
            if(x>=n && y>=m) { distance=d;break; }
        }
        trace.emplace_back(v.begin()+offset-d,v.begin()+offset+d+1);
        if(distance>=0) break;
    }
    std::vector<char> operations;int x=n,y=m;
    for(int d=distance;d>0;--d) {
        auto& previous=trace[d-1];int k=x-y;
        auto at=[&](int diagonal) { return previous[diagonal+d-1]; };
        int prior=k==-d || (k!=d && at(k-1)<at(k+1))?k+1:k-1;
        int px=at(prior),py=px-prior;
        while(x>px && y>py) { operations.push_back('=');--x;--y; }
        if(x==px) { operations.push_back('+');--y; }else { operations.push_back('-');--x; }
    }
    while(x>0 && y>0) { operations.push_back('=');--x;--y; }
    std::reverse(operations.begin(),operations.end());x=y=0;
    for(size_t i=0;i<operations.size();) {
        if(operations[i]=='=') { model.rows.push_back({x++,y++,-1});++i;continue; }
        DiffHunk h;h.row=model.rows.size();h.before=x;h.after=y;
        while(i<operations.size() && operations[i]!='=') { if(operations[i++]=='-') { ++h.removed;++x; }else { ++h.added;++y; } }
        h.rows=std::max(h.removed,h.added);int index=model.hunks.size();
        for(size_t j=0;j<h.rows;++j) model.rows.push_back({j<h.removed?int(h.before+j):-1,j<h.added?int(h.after+j):-1,index});
        model.hunks.push_back(h);
    }
    return model;
}
std::string DiffDescription(const DiffSource& source) {
    std::string label=source.label+" · "+source.path;
    if(!source.exists) return label+" · absent";
    if(source.binary) return label+" · binary · "+std::to_string(source.size)+" bytes";
    size_t crlf=0,cr=0,lf=0;for(size_t i=0;i<source.text.size();++i) if(source.text[i]=='\r') { if(i+1<source.text.size() && source.text[i+1]=='\n') { ++crlf;++i; }else ++cr; }else if(source.text[i]=='\n') ++lf;
    label+=" · ";label+=(int(crlf>0)+int(cr>0)+int(lf>0)>1)?"mixed EOL":crlf?"CRLF":cr?"CR":"LF";
    if(!source.text.empty() && source.text.back()!='\n' && source.text.back()!='\r') label+=" · no final newline";
    return label;
}
std::string DiffModel::Unified(std::vector<size_t>* hunkLines) const {
    if(hunkLines) hunkLines->assign(hunks.size(),0);
    if(!error.empty()) return error+"\n";
    if(binary) return "Binary or non-UTF-8 comparison. Text alignment is unavailable.\n"+DiffDescription(before)+"\n"+DiffDescription(after)+"\n";
    if(hunks.empty()) return "No text differences.\n";
    std::string result="--- "+before.label+" / "+before.path+"\n+++ "+after.label+" / "+after.path+"\n";
    size_t outputLine=2;
    auto append=[&](char mark,const DiffSource& source,const DiffLine& line) { result+=mark;result.append(source.text,line.start,line.length);result+='\n';++outputLine;if(!line.ending) { result+="\\ No newline at end of file\n";++outputLine; } };
    for(size_t h=0;h<hunks.size();) {
        size_t first=hunks[h].row>3?hunks[h].row-3:0,end=std::min(rows.size(),hunks[h].row+hunks[h].rows+3);
        while(++h<hunks.size() && hunks[h].row<=end+3) end=std::min(rows.size(),hunks[h].row+hunks[h].rows+3);
        size_t left=0,right=0,removed=0,added=0;
        for(size_t i=0;i<end;++i) { if(i<first) { left+=rows[i].before>=0;right+=rows[i].after>=0; }else { removed+=rows[i].before>=0;added+=rows[i].after>=0; } }
        result+="@@ -"+std::to_string(removed?left+1:left)+","+std::to_string(removed)+" +"+std::to_string(added?right+1:right)+","+std::to_string(added)+" @@\n";
        ++outputLine;
        for(size_t i=first;i<end;) {
            if(rows[i].hunk<0) { append(' ',before,leftLines[rows[i].before]);++i;continue; }
            size_t last=i+1;while(last<end && rows[last].hunk==rows[i].hunk) ++last;
            if(hunkLines) (*hunkLines)[rows[i].hunk]=outputLine;
            for(size_t j=i;j<last;++j) if(rows[j].before>=0) append('-',before,leftLines[rows[j].before]);
            for(size_t j=i;j<last;++j) if(rows[j].after>=0) append('+',after,rightLines[rows[j].after]);
            i=last;
        }
    }
    return result;
}
}
