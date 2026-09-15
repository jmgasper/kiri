#include "core/LanguageAnalysis.h"
#include "core/FileIO.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace kiri {
namespace {
size_t Natural(const Json& value) {
    if(!value.is_number_integer() || value.get<int64_t>()<0 || value.get<uint64_t>()>INT32_MAX) throw std::runtime_error("Invalid token integer.");
    return value.get<size_t>();
}
std::string Text(const Json& value) {
    auto result=value.is_string()?value.get<std::string>():value.is_number_integer()?value.dump():std::string();
    if(result.size()>8192) {size_t end=8192;while(end && (static_cast<unsigned char>(result[end])&0xc0)==0x80) --end;result.resize(end);}
    for(auto& c:result) if((static_cast<unsigned char>(c)<32 && c!='\n' && c!='\t') || c==127) c=' ';
    return result;
}
}
SourcePositions::SourcePositions(std::string_view text,PositionEncoding encoding):fText(text),fEncoding(encoding) {
    if(text.size()>kLanguageBytes || !IsValidUTF8(text)) throw std::runtime_error("Language analysis requires UTF-8 text up to 8 MiB.");
    fLines.push_back({0,0});uint32_t units=0;
    for(size_t at=0;at<text.size();) {
        unsigned char c=text[at];size_t bytes=c<128?1:c<224?2:c<240?3:4;
        size_t count=encoding==PositionEncoding::UTF8?bytes:encoding==PositionEncoding::UTF16 && bytes==4?2:1;
        if(bytes>1 && encoding!=PositionEncoding::UTF8) fCharacters.push_back({uint32_t(at),units,uint8_t(bytes),uint8_t(count)});
        if(c=='\r' && at+1<text.size() && text[at+1]=='\n') { ++bytes;++count; }
        at+=bytes;units+=count;
        if(c=='\r' || c=='\n') fLines.push_back({uint32_t(at),units});
    }
}
size_t SourcePositions::Offset(TextPosition position) const {
    if(position.line>=fLines.size() || position.character>INT32_MAX) throw std::runtime_error("Position is outside the document.");
    auto line=fLines[position.line];size_t unit=size_t(line.unit)+position.character,byte=unit;
    if(fEncoding!=PositionEncoding::UTF8) {
        auto found=std::upper_bound(fCharacters.begin(),fCharacters.end(),unit,[](size_t u,const Character& c){return u<c.unit;});
        if(found!=fCharacters.begin()) {
            auto c=*--found;
            if(unit==c.unit) byte=c.byte;
            else if(unit<c.unit+c.units) throw std::runtime_error("Position splits a Unicode character.");
            else byte=unit+size_t(c.byte+c.bytes)-size_t(c.unit+c.units);
        }
    }
    size_t end=position.line+1<fLines.size()?fLines[position.line+1].byte:fText.size();
    if(end>line.byte && fText[end-1]=='\n') --end;
    if(end>line.byte && fText[end-1]=='\r') --end;
    if(byte<line.byte || byte>end || (byte<fText.size() && (static_cast<unsigned char>(fText[byte])&0xc0)==0x80)) throw std::runtime_error("Position is outside the line or splits a Unicode character.");
    return byte;
}
DiagnosticResult ReadDiagnostics(const Json& rows,std::string_view text,PositionEncoding encoding) {
    DiagnosticResult result;SourcePositions positions(text,encoding);
    if(!rows.is_array()) { result.warning="Invalid diagnostic list.";return result; }
    size_t rejected=0;
    for(const auto& row:rows) {
        if(result.items.size()==kDiagnosticLimit) { result.warning="Showing the first 2,000 problems in this file. ";break; }
        try {
            auto start=ReadPosition(row.at("range").at("start")),end=ReadPosition(row.at("range").at("end"));
            Diagnostic item;item.start=positions.Offset(start);item.end=positions.Offset(end);if(item.end<item.start) throw std::runtime_error("Reversed range.");
            item.line=start.line;item.column=start.character;item.severity=row.value("severity",1);
            if(item.severity<1 || item.severity>4) item.severity=1;
            item.message=Text(row.at("message"));item.source=Text(row.value("source",Json()));item.code=Text(row.value("code",Json()));
            if(item.message.empty()) throw std::runtime_error("Empty diagnostic.");result.items.push_back(std::move(item));
        }catch(const std::exception&) { ++rejected; }
    }
    if(rejected) result.warning+="Ignored "+std::to_string(rejected)+" invalid diagnostics.";
    std::stable_sort(result.items.begin(),result.items.end(),[](const auto& a,const auto& b){return a.start==b.start?a.severity<b.severity:a.start<b.start;});return result;
}
SemanticResult ReadSemanticTokens(const Json& result,const Json& legend,std::string_view text,PositionEncoding encoding) {
    SemanticResult parsed;
    try {
        if(result.is_null()) return parsed;
        SourcePositions positions(text,encoding);const auto& data=result.at("data");const auto& types=legend.at("tokenTypes");auto modifiers=legend.value("tokenModifiers",Json::array());
        if(!data.is_array() || data.size()%5 || !types.is_array() || !modifiers.is_array()) throw std::runtime_error("Invalid semantic-token response or legend.");
        if(data.size()/5>kSemanticLimit) throw std::runtime_error("Semantic highlighting limit: 250,000 tokens.");
        size_t line=0,column=0,lastEnd=0;
        for(size_t i=0;i<data.size();i+=5) {
            auto delta=Natural(data[i]);line+=delta;column=delta?Natural(data[i+1]):column+Natural(data[i+1]);
            auto length=Natural(data[i+2]),type=Natural(data[i+3]),bits=Natural(data[i+4]);
            if(!length) throw std::runtime_error("Empty semantic token.");
            auto start=positions.Offset({line,column}),end=positions.Offset({line,column+length});
            if(start<lastEnd) throw std::runtime_error("Overlapping semantic tokens are unsupported.");lastEnd=end;
            if(type>=types.size() || !types[type].is_string()) continue;
            auto name=types[type].get<std::string>();SemanticRole role;
            if(name=="class" || name=="interface" || name=="struct" || name=="enum" || name=="type" || name=="typeParameter") role=SemanticRole::Type;
            else if(name=="parameter") role=SemanticRole::Parameter;
            else if(name=="property" || name=="event") role=SemanticRole::Property;
            else if(name=="function" || name=="method") role=SemanticRole::Function;
            else if(name=="namespace") role=SemanticRole::Namespace;
            else if(name=="variable") role=SemanticRole::Variable;
            else if(name=="enumMember") role=SemanticRole::Readonly;
            else if(name=="keyword" || name=="modifier") role=SemanticRole::Keyword;
            else continue;
            bool deprecated=false;
            for(size_t bit=0;bit<std::min<size_t>(31,modifiers.size());++bit) if(bits&(size_t(1)<<bit)) {
                if(modifiers[bit]=="readonly" && (role==SemanticRole::Variable || role==SemanticRole::Property)) role=SemanticRole::Readonly;
                if(modifiers[bit]=="deprecated") deprecated=true;
            }
            parsed.tokens.push_back({start,end,role,deprecated});
        }
    }catch(const std::exception& e) { parsed.tokens.clear();parsed.error=e.what(); }
    return parsed;
}
const char* SeverityName(int severity) { return severity==1?"Error":severity==2?"Warning":severity==3?"Information":"Hint"; }
const char* SemanticColorRole(SemanticRole role) {
    static const char* roles[]={"type","parameter","property","function","variable","namespace","readonly","keyword"};return roles[int(role)];
}
}
