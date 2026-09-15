#include "core/DocumentSettings.h"
#include "core/FileIO.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <sstream>
#include <memory>

namespace kiri {
namespace {
std::string Trim(std::string value) { auto first=value.find_first_not_of(" \t\r\n");return first==std::string::npos?"":value.substr(first,value.find_last_not_of(" \t\r\n")-first+1); }
std::string Lower(std::string value) {for(auto& c:value) if(c>='A' && c<='Z') c+=32;return value;}
bool Integer(std::string_view text,int64_t& value) {if(!text.empty() && text.front()=='+') text.remove_prefix(1);auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);return !text.empty() && parsed.ec==std::errc() && parsed.ptr==text.data()+text.size();}
std::string Literal(char c) { return std::string("\\.^$|()[]{}*+?-").find(c)==std::string::npos?std::string(1,c):"\\"+std::string(1,c); }
struct Glob {
    struct Range {size_t group;int64_t low,high;};
    std::vector<Range> ranges;
    size_t groups=0;
    std::string Convert(std::string_view text,int depth=0) {
        if(depth>32) throw std::runtime_error("Glob nesting exceeds 32 levels");
        std::string out;
        for(size_t i=0;i<text.size();++i) {
            char c=text[i];
            if(c=='\\') {out+=Literal(i+1<text.size()?text[++i]:'\\');continue;}
            if(c=='*') {if(i+1<text.size() && text[i+1]=='*') {++i;if(i+1<text.size() && text[i+1]=='/') {++i;out+="(?:.*/)?";}else out+=".*";}else out+="[^/]*";continue;}
            if(c=='?') {out+="[^/]";continue;}
            if(c=='[') {
                size_t end=i+1;bool slash=false;
                for(;end<text.size() && text[end]!=']';++end) {if(text[end]=='\\' && end+1<text.size()) ++end;else if(text[end]=='/') slash=true;}
                if(end<text.size() && !slash && end>i+1) {
                    out+='[';size_t at=i+1;if(text[at]=='!') {out+='^';++at;}
                    for(;at<end;++at) {char ch=text[at];if(ch=='\\' && at+1<end) {out+='\\';out+=text[++at];}else if(ch=='[' || ch=='^') {out+='\\';out+=ch;}else out+=ch;}
                    out+=']';i=end;continue;
                }
            }
            if(c=='{') {
                size_t end=i+1,start=i+1;int nested=0;std::vector<std::string_view> alternatives;
                for(;end<text.size();++end) {
                    if(text[end]=='\\' && end+1<text.size()) {++end;continue;}
                    if(text[end]=='{') ++nested;else if(text[end]=='}') {if(!nested) break;--nested;}
                    else if(text[end]==',' && !nested) {alternatives.push_back(text.substr(start,end-start));start=end+1;}
                }
                if(end<text.size()) {
                    if(!alternatives.empty()) {alternatives.push_back(text.substr(start,end-start));out+="(?:";for(size_t a=0;a<alternatives.size();++a) {if(a) out+='|';out+=Convert(alternatives[a],depth+1);}out+=')';i=end;continue;}
                    auto inside=text.substr(i+1,end-i-1);auto dots=inside.find("..");int64_t low=0,high=0;
                    if(dots!=std::string_view::npos && Integer(inside.substr(0,dots),low) && Integer(inside.substr(dots+2),high) && low<=high) {if(groups>=128) throw std::runtime_error("Too many numeric ranges in glob");ranges.push_back({++groups,low,high});out+="([+-]?(?:0|[1-9][0-9]*))(?C"+std::to_string(groups)+")";i=end;continue;}
                }
            }
            out+=Literal(c);
        }
        return out;
    }
};
void Warn(DocumentConfig& result,const std::string& text) {if(result.warnings.size()<32) result.warnings.push_back(text);}
}
bool MatchConfigGlob(const std::string& pattern,const std::string& relativePath,std::string& error) {
    error.clear();if(pattern.size()>4096 || relativePath.size()>16384) {error="EditorConfig pattern or path is too long";return false;}
    try {
        bool slash=false,inClass=false;for(size_t i=0;i<pattern.size();++i) {if(pattern[i]=='\\' && i+1<pattern.size()) {if(pattern[i+1]=='/') slash=true;++i;}else if(pattern[i]=='[') inClass=true;else if(pattern[i]==']') inClass=false;else if(pattern[i]=='/' && !inClass) slash=true;}
        auto input=std::string_view(pattern);if(!input.empty() && input.front()=='/') input.remove_prefix(1);
        Glob glob;auto regex=std::string("\\A")+(slash?"":"(?:.*/)?")+glob.Convert(input)+"\\z";
        int code=0;PCRE2_SIZE offset=0;
        std::unique_ptr<pcre2_code,decltype(&pcre2_code_free)> compiled(pcre2_compile(reinterpret_cast<PCRE2_SPTR>(regex.data()),regex.size(),PCRE2_UTF|PCRE2_DOTALL,&code,&offset,nullptr),pcre2_code_free);
        if(!compiled) {error="Invalid EditorConfig glob";return false;}
        std::unique_ptr<pcre2_match_data,decltype(&pcre2_match_data_free)> data(pcre2_match_data_create_from_pattern(compiled.get(),nullptr),pcre2_match_data_free);
        std::unique_ptr<pcre2_match_context,decltype(&pcre2_match_context_free)> context(pcre2_match_context_create(nullptr),pcre2_match_context_free);
        if(!data || !context) {error="Not enough memory to match EditorConfig glob";return false;}
        pcre2_set_match_limit(context.get(),100000);pcre2_set_depth_limit(context.get(),100);pcre2_set_heap_limit(context.get(),1024);
        // Range checks must participate in backtracking: a numeric alternative
        // outside its interval must not hide a later matching alternative.
        pcre2_set_callout(context.get(),[](pcre2_callout_block* block,void* data)->int {
            auto* glob=static_cast<Glob*>(data);auto group=block->callout_number;if(!group || group>glob->ranges.size()) return 1;
            const auto& range=glob->ranges[group-1];auto start=block->offset_vector[group*2],end=block->offset_vector[group*2+1];int64_t number=0;
            return start!=PCRE2_UNSET && Integer(std::string_view(reinterpret_cast<const char*>(block->subject)+start,end-start),number) && number>=range.low && number<=range.high?0:1;
        },&glob);
        auto match=pcre2_match(compiled.get(),reinterpret_cast<PCRE2_SPTR>(relativePath.data()),relativePath.size(),0,0,data.get(),context.get());
        if(match==PCRE2_ERROR_NOMATCH) return false;if(match<0) {error="EditorConfig glob exceeded matching limits or contains invalid UTF-8";return false;}
        auto* positions=pcre2_get_ovector_pointer(data.get());
        for(auto& range:glob.ranges) {auto start=positions[range.group*2],end=positions[range.group*2+1];if(start==PCRE2_UNSET) continue;int64_t number=0;if(!Integer(std::string_view(relativePath).substr(start,end-start),number) || number<range.low || number>range.high) return false;}
        return true;
    }catch(const std::exception& exception) {error=exception.what();return false;}
}
void ReadConfigText(DocumentConfig& result,const std::string& text,const std::string& file,const std::string& relativePath,bool& root) {
    root=false;bool preamble=true,matching=false;std::istringstream stream(text);std::string line;int number=0,sections=0;
    while(std::getline(stream,line)) {
        ++number;if(number==1 && line.compare(0,3,"\xef\xbb\xbf")==0) line.erase(0,3);line=Trim(line);
        if(line.empty() || line[0]=='#' || line[0]==';') continue;
        auto location=file+":"+std::to_string(number);
        if(line.front()=='[') {
            preamble=false;matching=false;if(++sections>4096) {Warn(result,location+": section limit reached");break;}
            if(line.back()!=']') {Warn(result,location+": invalid section header");continue;}
            std::string error;matching=MatchConfigGlob(line.substr(1,line.size()-2),relativePath,error);if(!error.empty()) Warn(result,location+": "+error);continue;
        }
        auto separator=line.find('=');if(separator==std::string::npos) {Warn(result,location+": expected key = value");continue;}
        auto key=Lower(Trim(line.substr(0,separator))),value=Trim(line.substr(separator+1));
        if(key.empty() || key.size()>1024 || value.size()>4096) {Warn(result,location+": invalid or overlong key/value");continue;}
        if(preamble) {if(key=="root") {auto flag=Lower(value);if(flag=="true" || flag=="false") root=flag=="true";else Warn(result,location+": root must be true or false");}continue;}
        if(matching) {if(result.properties.count(key) || result.properties.size()<4096) result.properties[key]={value,file,number};else Warn(result,location+": property limit reached (4096)");}
    }
}
DocumentConfig ReadDocumentConfig(const std::string& path,const std::atomic<bool>* cancel) {
    DocumentConfig result;if(path.empty()) return result;namespace fs=std::filesystem;
    std::error_code error;auto absolute=fs::absolute(path,error).lexically_normal();if(error) {Warn(result,error.message());return result;}
    struct Part {std::string file,text,relative;};std::vector<Part> parts;size_t bytes=0;auto directory=absolute.parent_path();
    for(int depth=0;depth<128;++depth) {
        if(cancel && *cancel) return {};
        auto file=(directory/".editorconfig").string();result.files.push_back(file);auto stamp=StatFile(file);result.stamps[file]=stamp;
        if(stamp.exists) {
            if(!fs::is_regular_file(file,error)) {Warn(result,file+": EditorConfig must be a regular file");}
            else if(stamp.size>1024*1024 || bytes+stamp.size>8*1024*1024) {Warn(result,file+": EditorConfig size limit reached (1 MiB per file, 8 MiB total)");}
            else {auto data=ReadFile(file,cancel,1024*1024);bytes+=data.bytes.size();if(!data.ok() || data.binary || !data.utf8) Warn(result,file+": "+(data.ok()?"EditorConfig must be UTF-8 text":data.error));
                else {Part part{file,std::move(data.bytes),absolute.lexically_relative(directory).generic_string()};DocumentConfig parsed;bool root=false;ReadConfigText(parsed,part.text,file,part.relative,root);parts.push_back(std::move(part));if(root) break;}}
        }
        if(directory==directory.root_path()) break;directory=directory.parent_path();if(depth==127) Warn(result,"EditorConfig directory depth limit reached (128)");
    }
    for(auto it=parts.rbegin();it!=parts.rend();++it) {bool root=false;ReadConfigText(result,it->text,it->file,it->relative,root);}
    return result;
}
}
