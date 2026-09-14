#include "core/LanguageProtocol.h"
#include "core/FileIO.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <stdexcept>

namespace kiri {
namespace {
size_t CharacterBytes(std::string_view text,size_t at) {
    unsigned char c=text[at];size_t length=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
    return std::min(length,text.size()-at);
}
size_t Units(size_t bytes,PositionEncoding encoding) { return encoding==PositionEncoding::UTF8?bytes:encoding==PositionEncoding::UTF16 && bytes==4?2:1; }
TextEdit ReadEdit(const Json& value,std::string_view text,PositionEncoding encoding) {
    const auto& range=value.at("range");
    return {OffsetAt(text,ReadPosition(range.at("start")),encoding),OffsetAt(text,ReadPosition(range.at("end")),encoding),value.at("newText").get<std::string>()};
}
}
TextPosition PositionAt(std::string_view text,size_t offset,PositionEncoding encoding) {
    TextPosition result;offset=std::min(offset,text.size());
    for(size_t at=0;at<offset;) {
        if(text[at]=='\r' || text[at]=='\n') {
            if(text[at]=='\r' && at+1<text.size() && text[at+1]=='\n') ++at;
            ++at;++result.line;result.character=0;
        } else {
            auto bytes=CharacterBytes(text,at);if(at+bytes>offset) break;
            result.character+=Units(bytes,encoding);at+=bytes;
        }
    }
    return result;
}
size_t OffsetAt(std::string_view text,TextPosition position,PositionEncoding encoding) {
    size_t at=0,line=0,character=0;
    while(at<text.size() && line<position.line) {
        if(text[at]=='\r') { ++line;++at;if(at<text.size() && text[at]=='\n') ++at; }
        else if(text[at++]=='\n') ++line;
    }
    while(at<text.size() && text[at]!='\n' && text[at]!='\r' && character<position.character) {
        auto bytes=CharacterBytes(text,at),units=Units(bytes,encoding);
        if(character+units>position.character) break;
        at+=bytes;character+=units;
    }
    return at;
}
Json PositionJSON(TextPosition value) { return {{"line",value.line},{"character",value.character}}; }
TextPosition ReadPosition(const Json& value) {
    auto line=value.at("line").get<int64_t>(),character=value.at("character").get<int64_t>();
    if(line<0 || character<0 || line>INT32_MAX || character>INT32_MAX) throw std::runtime_error("Invalid language server position");
    return {static_cast<size_t>(line),static_cast<size_t>(character)};
}
std::string FileURI(const std::string& path) {
    static const char hex[]="0123456789ABCDEF";std::string result="file://";
    for(unsigned char c:path) {
        if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='/' || c=='-' || c=='_' || c=='.' || c=='~') result+=c;
        else { result+='%';result+=hex[c>>4];result+=hex[c&15]; }
    }
    return result;
}
std::vector<DocumentSymbol> ReadSymbols(const Json& result,std::string_view text,const std::string& uri,PositionEncoding encoding) {
    std::vector<DocumentSymbol> symbols;if(!result.is_array()) return symbols;
    std::function<void(const Json&,int,const std::string&)> append=[&](const Json& rows,int depth,const std::string& container) {
        if(depth>64 || !rows.is_array()) return;
        for(const auto& row:rows) {
            if(symbols.size()>=10000) return;
            try {
                bool flat=row.contains("location");const auto& range=flat?row.at("location").at("range"):row.at("range");
                if(flat && row.at("location").at("uri")!=uri) continue;
                auto selected=flat?range:row.value("selectionRange",range);
                DocumentSymbol symbol{row.at("name").get<std::string>(),row.value("containerName",container),row.value("detail",std::string()),row.value("kind",0),depth,
                    OffsetAt(text,ReadPosition(range.at("start")),encoding),OffsetAt(text,ReadPosition(range.at("end")),encoding),
                    OffsetAt(text,ReadPosition(selected.at("start")),encoding)};
                if(symbol.end<symbol.start || symbol.selection<symbol.start || symbol.selection>symbol.end) continue;
                symbols.push_back(symbol);
                if(row.contains("children")) append(row["children"],depth+1,container.empty()?symbol.name:container+" › "+symbol.name);
            } catch(const std::exception&) { /* One malformed symbol must not hide the rest. */ }
        }
    };
    append(result,0,"");return symbols;
}
std::vector<CompletionItem> ReadCompletions(const Json& result) {
    auto rows=result.is_array()?result:result.is_object()?result.value("items",Json::array()):Json::array();
    std::vector<CompletionItem> items;if(!rows.is_array()) return items;
    for(const auto& row:rows) {
        if(items.size()>=10000) break;
        try {
            // We advertise plain text completion; never insert snippet syntax into source.
            if(row.value("insertTextFormat",1)!=1) continue;
            auto label=row.at("label").get<std::string>();if(label.empty()) continue;
            items.push_back({label,row.value("detail",std::string()),row.value("filterText",label),row.value("sortText",label),row});
        } catch(const std::exception&) {}
    }
    std::stable_sort(items.begin(),items.end(),[](const auto& a,const auto& b){return a.sort<b.sort;});return items;
}
std::vector<TextEdit> CompletionEdits(const CompletionItem& item,std::string_view text,size_t wordStart,size_t caret,PositionEncoding encoding) {
    auto value=item.value;std::vector<TextEdit> edits;
    if(value.value("insertTextFormat",1)!=1) throw std::runtime_error("The server returned a snippet despite plain-text completion capabilities");
    if(value.contains("textEdit")) {
        auto edit=value["textEdit"];if(!edit.contains("range")) edit["range"]=edit.at("replace");
        edits.push_back(ReadEdit(edit,text,encoding));
        if(edits[0].start>caret || edits[0].end<caret) throw std::runtime_error("Completion range does not contain the caret");
    } else edits.push_back({wordStart,caret,value.value("insertText",item.label)});
    if(value.contains("additionalTextEdits")) for(const auto& edit:value.at("additionalTextEdits")) edits.push_back(ReadEdit(edit,text,encoding));
    OrderedEdits(edits,text.size());return edits;
}
std::vector<TextEdit> OrderedEdits(std::vector<TextEdit> edits,size_t length) {
    std::stable_sort(edits.begin(),edits.end(),[](const auto& a,const auto& b){return a.start>b.start;});
    for(size_t i=0;i<edits.size();++i) {
        const auto& edit=edits[i];
        if(edit.start>edit.end || edit.end>length || !IsValidUTF8(edit.text) || edit.text.find('\0')!=std::string::npos) throw std::runtime_error("Invalid language server edit");
        if(i && (edit.end>edits[i-1].start || edit.start==edits[i-1].start)) throw std::runtime_error("Overlapping language server edits");
    }
    return edits;
}
size_t MapOffset(size_t offset,const std::vector<TextEdit>& edits) {
    auto ordered=OrderedEdits(edits,SIZE_MAX);int64_t delta=0;
    for(auto it=ordered.rbegin();it!=ordered.rend();++it) {
        if(offset<it->start) break;
        if(offset<=it->end) return static_cast<size_t>(static_cast<int64_t>(it->start)+delta+it->text.size());
        delta+=static_cast<int64_t>(it->text.size())-static_cast<int64_t>(it->end-it->start);
    }
    return static_cast<size_t>(static_cast<int64_t>(offset)+delta);
}
std::string ApplyTextEdits(std::string text,const std::vector<TextEdit>& edits) {
    for(const auto& edit:OrderedEdits(edits,text.size())) text.replace(edit.start,edit.end-edit.start,edit.text);
    return text;
}
const char* SymbolKindName(int kind) {
    static const char* names[]={"Symbol","File","Module","Namespace","Package","Class","Method","Property","Field","Constructor","Enum","Interface","Function","Variable","Constant","String","Number","Boolean","Array","Object","Key","Null","Enum member","Struct","Event","Operator","Type parameter"};
    return kind>=1 && kind<=26?names[kind]:names[0];
}
std::string RpcFramer::Frame(const Json& value) {
    auto body=value.dump();if(body.size()>Limit) throw std::runtime_error("Language server message exceeds 16 MiB");
    return "Content-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
}
std::vector<Json> RpcFramer::Feed(std::string_view bytes) {
    fBuffer.append(bytes);std::vector<Json> messages;
    while(true) {
        if(fHeader) {
            auto end=fBuffer.find("\r\n\r\n");
            if(end==std::string::npos) { if(fBuffer.size()>8192) throw std::runtime_error("Invalid language server header");break; }
            if(end>8192) throw std::runtime_error("Language server header is too long");
            bool found=false;
            for(size_t start=0;start<end;) {
                auto next=fBuffer.find("\r\n",start);auto line=fBuffer.substr(start,next-start);auto colon=line.find(':');
                if(colon==std::string::npos) throw std::runtime_error("Invalid language server header field");
                auto name=line.substr(0,colon);for(auto& c:name) c=std::tolower(static_cast<unsigned char>(c));
                auto value=line.substr(colon+1);while(!value.empty() && value.front()==' ') value.erase(value.begin());
                if(name=="content-length") {
                    if(found || value.empty() || value.find_first_not_of("0123456789")!=std::string::npos || value.size()>9) throw std::runtime_error("Invalid Content-Length");
                    fLength=std::stoul(value);if(fLength==0 || fLength>Limit) throw std::runtime_error("Language server message exceeds 16 MiB");found=true;
                }
                start=next+2;
            }
            if(!found) throw std::runtime_error("Missing Content-Length");
            fBuffer.erase(0,end+4);fHeader=false;
        }
        if(fBuffer.size()<fLength) break;
        auto message=Json::parse(fBuffer.begin(),fBuffer.begin()+fLength);
        if(!message.is_object() || message.value("jsonrpc",std::string())!="2.0") throw std::runtime_error("Invalid JSON-RPC message");
        messages.push_back(std::move(message));fBuffer.erase(0,fLength);fHeader=true;
    }
    return messages;
}
}
