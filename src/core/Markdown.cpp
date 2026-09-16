#include "core/Markdown.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
extern "C" {
#include "md4c/entity.h"
}

namespace kiri {
namespace {
std::string UTF8(unsigned cp) {
    if(!cp || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) cp=0xfffd;
    std::string s;
    if(cp<0x80) s+=char(cp);
    else if(cp<0x800) {s+=char(0xc0|(cp>>6));s+=char(0x80|(cp&63));}
    else if(cp<0x10000) {s+=char(0xe0|(cp>>12));s+=char(0x80|((cp>>6)&63));s+=char(0x80|(cp&63));}
    else {s+=char(0xf0|(cp>>18));s+=char(0x80|((cp>>12)&63));s+=char(0x80|((cp>>6)&63));s+=char(0x80|(cp&63));}
    return s;
}
std::string Entity(std::string s) {
    if(s.size()>3 && s[1]=='#') {
        unsigned value=0;size_t at=2;int base=10;
        if(s[at]=='x' || s[at]=='X') {base=16;++at;}
        for(;at+1<s.size();++at) {unsigned c=std::tolower(static_cast<unsigned char>(s[at]));unsigned digit=c<='9'?c-'0':c-'a'+10;if(digit>=unsigned(base) || value>0x10ffff/unsigned(base)) return UTF8(0xfffd);value=value*base+digit;}
        return UTF8(value);
    }
    if(auto* entity=entity_lookup(s.data(),s.size())) return UTF8(entity->codepoints[0])+(entity->codepoints[1]?UTF8(entity->codepoints[1]):"");
    return s;
}
std::string Attribute(const MD_ATTRIBUTE& a) {
    std::string result;
    for(size_t i=0;a.size && a.substr_offsets[i]<a.size;++i) {
        std::string part(a.text+a.substr_offsets[i],a.substr_offsets[i+1]-a.substr_offsets[i]);
        result+=a.substr_types[i]==MD_TEXT_ENTITY?Entity(part):a.substr_types[i]==MD_TEXT_NULLCHAR?UTF8(0xfffd):part;
    }
    return result;
}
struct Parser {
    Parser(const std::string& source,const std::atomic<bool>* stop):text(source),cancel(stop) {}
    const std::string& text;
    const std::atomic<bool>* cancel;
    MarkdownDocument result;
    std::vector<size_t> starts{0},stack;
    std::vector<MarkdownRun> spans{MarkdownRun{}};
    size_t cursor=0,items=0;
    int imageDepth=0;
    bool Stop() {
        if(cancel && cancel->load()) {result.error="Preview cancelled";return true;}
        if(++items>150000 || stack.size()>128 || spans.size()>128) {result.error="Preview paused: Markdown is too complex.";return true;}
        return false;
    }
    void Locate(const char* bytes,size_t size) {
        auto at=reinterpret_cast<uintptr_t>(bytes),base=reinterpret_cast<uintptr_t>(text.data());
        if(at<base || at>=base+text.size()) return;
        size_t offset=at-base;
        cursor=std::upper_bound(starts.begin(),starts.end(),offset)-starts.begin()-1;
        auto end=std::upper_bound(starts.begin(),starts.end(),std::min(text.size(),offset+(size?size-1:0)))-starts.begin()-1;
        for(auto index:stack) {auto& b=result.blocks[index];b.firstLine=std::min(b.firstLine,cursor);b.lastLine=std::max(b.lastLine,size_t(end));}
    }
    static int EnterBlock(MD_BLOCKTYPE type,void* detail,void* data) {
        auto& p=*static_cast<Parser*>(data);if(p.Stop()) return 1;
        MarkdownBlock block;block.type=type;block.firstLine=std::numeric_limits<size_t>::max();block.lastLine=p.cursor;
        if(type==MD_BLOCK_H) block.level=static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
        if(type==MD_BLOCK_OL) block.start=static_cast<MD_BLOCK_OL_DETAIL*>(detail)->start;
        if(type==MD_BLOCK_LI) {auto* d=static_cast<MD_BLOCK_LI_DETAIL*>(detail);if(d->is_task) block.task=d->task_mark;}
        if(type==MD_BLOCK_TH || type==MD_BLOCK_TD) block.align=static_cast<MD_BLOCK_TD_DETAIL*>(detail)->align;
        size_t index=p.result.blocks.size();if(!p.stack.empty()) p.result.blocks[p.stack.back()].children.push_back(index);
        p.result.blocks.push_back(std::move(block));p.stack.push_back(index);return 0;
    }
    static int LeaveBlock(MD_BLOCKTYPE,void*,void* data) {
        auto& p=*static_cast<Parser*>(data);if(p.Stop()) return 1;
        auto& b=p.result.blocks[p.stack.back()];if(b.firstLine==std::numeric_limits<size_t>::max()) b.firstLine=p.cursor;
        p.stack.pop_back();return 0;
    }
    static int EnterSpan(MD_SPANTYPE type,void* detail,void* data) {
        auto& p=*static_cast<Parser*>(data);if(p.Stop()) return 1;
        auto span=p.spans.back();
        if(type==MD_SPAN_STRONG) span.style|=MarkdownBold;
        if(type==MD_SPAN_EM) span.style|=MarkdownItalic;
        if(type==MD_SPAN_CODE) span.style|=MarkdownCode;
        if(type==MD_SPAN_DEL) span.style|=MarkdownStrike;
        if(type==MD_SPAN_A && !p.imageDepth) span.link=Attribute(static_cast<MD_SPAN_A_DETAIL*>(detail)->href);
        if(type==MD_SPAN_IMG) {
            if(!p.imageDepth) {
                auto* d=static_cast<MD_SPAN_IMG_DETAIL*>(detail);
                span.image=Attribute(d->src);span.text.clear();span.line=p.cursor;
                p.result.blocks[p.stack.back()].runs.push_back(span);
            }
            ++p.imageDepth;
        }
        p.spans.push_back(std::move(span));return 0;
    }
    static int LeaveSpan(MD_SPANTYPE type,void*,void* data) {
        auto& p=*static_cast<Parser*>(data);if(type==MD_SPAN_IMG) --p.imageDepth;p.spans.pop_back();return p.Stop()?1:0;
    }
    static int Text(MD_TEXTTYPE type,const char* bytes,MD_SIZE size,void* data) {
        auto& p=*static_cast<Parser*>(data);if(p.Stop()) return 1;p.Locate(bytes,size);
        std::string text(bytes,size);
        if(type==MD_TEXT_ENTITY) text=Entity(text);
        else if(type==MD_TEXT_NULLCHAR) text=UTF8(0xfffd);
        else if(type==MD_TEXT_BR) text="\n";
        else if(type==MD_TEXT_SOFTBR) text=" ";
        auto& b=p.result.blocks[p.stack.back()];
        if(p.imageDepth) {auto& image=b.runs.back();if(image.text.empty()) image.line=p.cursor;image.text+=text;return 0;}
        auto run=p.spans.back();run.text=std::move(text);run.line=p.cursor;
        if(type==MD_TEXT_HTML || b.type==MD_BLOCK_CODE || b.type==MD_BLOCK_HTML) run.style|=MarkdownCode;
        b.runs.push_back(std::move(run));return 0;
    }
};
int Hex(char c) {if(c>='0' && c<='9') return c-'0';if(c>='a' && c<='f') return c-'a'+10;if(c>='A' && c<='F') return c-'A'+10;return -1;}
bool Decode(std::string& value) {
    std::string decoded;
    for(size_t i=0;i<value.size();++i) {
        unsigned char c=value[i];
        if(c=='%') {if(i+2>=value.size() || Hex(value[i+1])<0 || Hex(value[i+2])<0) return false;c=Hex(value[i+1])*16+Hex(value[i+2]);i+=2;}
        if(c<32 || c==127) return false;
        decoded+=char(c);
    }
    value=std::move(decoded);return true;
}
}
std::string MarkdownAnchor(const std::string& text) {
    std::string out;for(unsigned char c:text) {if(c==' ' || c=='\n') out+='-';else if(c>=128 || std::isalnum(c) || c=='_' || c=='-') out+=char(c<128?std::tolower(c):c);}return out;
}
MarkdownDocument ParseMarkdown(const std::string& text,const std::atomic<bool>* cancel) {
    if(text.size()>kMarkdownBytes) return {{},1,"Preview paused: document exceeds 2 MiB."};
    Parser p{text,cancel};
    for(size_t i=0;i<text.size();++i) if(text[i]=='\r' || text[i]=='\n') {if(text[i]=='\r' && i+1<text.size() && text[i+1]=='\n') ++i;p.starts.push_back(i+1);}
    p.result.lines=p.starts.size();if(p.result.lines>kMarkdownLines) return {{},p.result.lines,"Preview paused: document exceeds 30,000 lines."};
    MD_PARSER parser{};parser.flags=MD_FLAG_TABLES|MD_FLAG_STRIKETHROUGH|MD_FLAG_TASKLISTS;
    parser.enter_block=Parser::EnterBlock;parser.leave_block=Parser::LeaveBlock;parser.enter_span=Parser::EnterSpan;parser.leave_span=Parser::LeaveSpan;parser.text=Parser::Text;
    if(md_parse(text.data(),text.size(),&parser,&p)!=0) {if(p.result.error.empty()) p.result.error="Unable to parse Markdown.";p.result.blocks.clear();}
    std::map<std::string,int> anchors;std::set<std::string> used;
    for(auto& block:p.result.blocks) if(block.type==MD_BLOCK_H) {
        std::string title;for(const auto& run:block.runs) title+=run.text;auto base=MarkdownAnchor(title);auto& count=anchors[base];
        do {block.anchor=base+(count?"-"+std::to_string(count):"");++count;} while(!used.insert(block.anchor).second);
    }
    return std::move(p.result);
}
MarkdownTarget ResolveMarkdownTarget(const std::string& target,const std::string& documentPath,bool image) {
    MarkdownTarget out;out.reason="Blocked resource or link scheme.";
    if(target.empty() || target.size()>8192) return out;
    for(unsigned char c:target) if(c<32 || c==127) return out;
    auto colon=target.find(':'),slash=target.find_first_of("/#?");
    if(colon!=std::string::npos && (slash==std::string::npos || colon<slash)) {
        auto scheme=target.substr(0,colon);std::transform(scheme.begin(),scheme.end(),scheme.begin(),[](unsigned char c){return std::tolower(c);});
        if(!image && (scheme=="http" || scheme=="https") && target.substr(colon,3)=="://" && target.size()>colon+3) {out.kind=MarkdownTargetKind::Web;out.path=target;out.reason.clear();}
        else if(image) out.reason="Remote images are disabled.";
        return out;
    }
    if(target.compare(0,2,"//")==0 || target.find('\\')!=std::string::npos) return out;
    auto hash=target.find('#');out.path=target.substr(0,hash);if(hash!=std::string::npos) out.fragment=target.substr(hash+1);
    if(!Decode(out.path) || !Decode(out.fragment) || out.path.find('\\')!=std::string::npos || out.path.find(':')!=std::string::npos || out.path.compare(0,2,"//")==0) return out;
    if(out.path.empty()) {if(!image) {out.kind=MarkdownTargetKind::Anchor;out.reason.clear();}return out;}
    std::filesystem::path path(out.path);
    if(path.is_relative()) {
        if(documentPath.empty()) {out.reason="Save this document to resolve relative resources.";return out;}
        path=std::filesystem::path(documentPath).parent_path()/path;
    }
    out.path=path.lexically_normal().string();out.kind=MarkdownTargetKind::Local;out.reason.clear();return out;
}
}
