#include "core/Search.h"
#include "core/FileIO.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace kiri {
namespace {
std::string RegexError(int code) {
    unsigned char bytes[256];pcre2_get_error_message(code,bytes,sizeof(bytes));
    return reinterpret_cast<char*>(bytes);
}
std::string Quote(std::string_view text) {
    std::string result;
    for(char c:text) { if(std::string_view("\\.^$|()[]{}*+?").find(c)!=std::string_view::npos) result+='\\';result+=c; }
    return result;
}
struct Budget {
    const std::atomic<bool>* cancel;
    std::chrono::steady_clock::time_point until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    size_t calls=0;
    static int Check(pcre2_callout_block*,void* data) {
        auto& b=*static_cast<Budget*>(data);
        if((++b.calls&1023)==0 && ((b.cancel && b.cancel->load()) || std::chrono::steady_clock::now()>b.until)) return PCRE2_ERROR_CALLOUT;
        return 0;
    }
};
}
struct TextQuery::Impl {
    pcre2_code* code=nullptr;
    std::string error;
    bool regex=false,empty=false;
    uint32_t captures=0;
    ~Impl() { if(code) pcre2_code_free(code); }
    std::string Expand(const std::string& replacement,std::string_view subject,PCRE2_SIZE* offsets) const {
        if(!regex) return replacement;
        std::string result;
        for(size_t i=0;i<replacement.size();++i) {
            char c=replacement[i];
            if(c=='\\') {
                if(++i==replacement.size()) throw std::runtime_error("Trailing replacement backslash; use \\\\ for a literal backslash.");
                c=replacement[i];
                if(c=='n') result+='\n';else if(c=='r') result+='\r';else if(c=='t') result+='\t';else if(c=='\\') result+='\\';
                else throw std::runtime_error("Unknown replacement escape; use \\n, \\r, \\t or \\\\.");
            } else if(c=='$') {
                if(++i==replacement.size()) throw std::runtime_error("Use $$ for a literal dollar sign.");
                if(replacement[i]=='$') { result+='$';continue; }
                int group=-1;
                if(replacement[i]>='0' && replacement[i]<='9') {
                    group=replacement[i]-'0';
                    if(i+1<replacement.size() && replacement[i+1]>='0' && replacement[i+1]<='9') group=group*10+replacement[++i]-'0';
                } else if(replacement[i]=='{') {
                    auto end=replacement.find('}',i+1);if(end==std::string::npos) throw std::runtime_error("Missing } in replacement capture.");
                    auto name=replacement.substr(i+1,end-i-1);group=pcre2_substring_number_from_name(code,reinterpret_cast<PCRE2_SPTR>(name.c_str()));i=end;
                }
                if(group<0 || uint32_t(group)>captures) throw std::runtime_error("Unknown replacement capture; use $0..$99 or ${name}.");
                if(offsets && offsets[group*2]!=PCRE2_UNSET) result.append(subject.substr(offsets[group*2],offsets[group*2+1]-offsets[group*2]));
            } else result+=c;
        }
        return result;
    }
};
TextQuery::TextQuery(const SearchOptions& options):fImpl(std::make_unique<Impl>()) {
    auto& q=*fImpl;q.regex=options.regex;q.empty=options.query.empty();
    std::string pattern=options.regex?options.query:Quote(options.query);

    auto* context=pcre2_compile_context_create(nullptr);pcre2_set_newline(context,PCRE2_NEWLINE_ANYCRLF);
    int error=0;PCRE2_SIZE offset=0;
    q.code=pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()),pattern.size(),
        PCRE2_UTF|PCRE2_UCP|PCRE2_MULTILINE|PCRE2_NEVER_BACKSLASH_C|PCRE2_AUTO_CALLOUT|(options.matchCase?0:PCRE2_CASELESS),&error,&offset,context);
    pcre2_compile_context_free(context);
    if(!q.code) q.error="Pattern at byte "+std::to_string(offset+1)+": "+RegexError(error);
    else {
        if(options.wholeWord) {
            pcre2_code_free(q.code);
            pattern="(?<![\\p{L}\\p{N}\\p{M}_])(?:"+pattern+")(?![\\p{L}\\p{N}\\p{M}_])";
            auto* wrappedContext=pcre2_compile_context_create(nullptr);pcre2_set_newline(wrappedContext,PCRE2_NEWLINE_ANYCRLF);
            q.code=pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()),pattern.size(),PCRE2_UTF|PCRE2_UCP|PCRE2_MULTILINE|PCRE2_NEVER_BACKSLASH_C|PCRE2_AUTO_CALLOUT|(options.matchCase?0:PCRE2_CASELESS),&error,&offset,wrappedContext);
            pcre2_compile_context_free(wrappedContext);
            if(!q.code) q.error="Whole-word pattern: "+RegexError(error);
        }
        if(q.code) pcre2_pattern_info(q.code,PCRE2_INFO_CAPTURECOUNT,&q.captures);
    }
}
TextQuery::~TextQuery()=default;
const std::string& TextQuery::Error() const { return fImpl->error; }
std::string TextQuery::ValidateReplacement(const std::string& replacement) const {
    if(!Error().empty()) return Error();
    if(!IsValidUTF8(replacement) || replacement.find('\0')!=std::string::npos) return "Replacement must be UTF-8 text without NUL bytes.";
    try { fImpl->Expand(replacement,{},nullptr);return {}; }catch(const std::exception& e) { return e.what(); }
}
TextMatches TextQuery::Find(std::string_view text,size_t start,size_t end,size_t limit,const std::atomic<bool>* cancel,const std::string* replacement) const {
    TextMatches result;auto& q=*fImpl;result.error=replacement?ValidateReplacement(*replacement):Error();
    if(!result.error.empty() || q.empty) return result;
    end=std::min(end,text.size());
    if(start>end) { result.error="Invalid search selection.";return result; }
    auto subject=text.substr(start,end-start);
    if(!IsValidUTF8(subject)) { result.error="Search scope must contain complete UTF-8 characters.";return result; }
    auto* data=pcre2_match_data_create_from_pattern(q.code,nullptr);auto* context=pcre2_match_context_create(nullptr);
    pcre2_set_match_limit(context,1000000);pcre2_set_depth_limit(context,1000);pcre2_set_heap_limit(context,16384);
    Budget budget{cancel};pcre2_set_callout(context,Budget::Check,&budget);
    size_t offset=0;
    while(offset<=subject.size()) {
        if(cancel && cancel->load()) { result.cancelled=true;break; }
        int count=pcre2_match(q.code,reinterpret_cast<PCRE2_SPTR>(subject.data()),subject.size(),offset,PCRE2_NO_UTF_CHECK,data,context);
        if(count==PCRE2_ERROR_NOMATCH) break;
        if(count<0) {
            result.cancelled=cancel && cancel->load();
            if(!result.cancelled) result.error="Search stopped: "+RegexError(count)+". Simplify the pattern.";
            break;
        }
        auto* offsets=pcre2_get_ovector_pointer(data);
        if(result.matches.size()==limit) { result.truncated=true;break; }
        result.matches.push_back({start+offsets[0],start+offsets[1],replacement?q.Expand(*replacement,subject,offsets):std::string()});
        offset=offsets[1];
        if(offsets[0]==offsets[1]) {
            if(offset==subject.size()) break;
            ++offset;while(offset<subject.size() && (static_cast<unsigned char>(subject[offset])&0xc0)==0x80) ++offset;
            if(offset<subject.size() && subject[offset-1]=='\r' && subject[offset]=='\n') ++offset;
        }
    }
    pcre2_match_context_free(context);pcre2_match_data_free(data);
    if(!result.error.empty() || result.cancelled) result.matches.clear();
    return result;
}
}
