#include "core/DocumentSettings.h"
#include <algorithm>
#include <charconv>
#include <sstream>

namespace kiri {
namespace {
bool Number(const std::string& value,int& number,int maximum) {
    auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
    return !value.empty() && parsed.ec==std::errc() && parsed.ptr==value.data()+value.size() && number>=1 && number<=maximum;
}
std::string Lower(std::string value) { for(auto& c:value) if(c>='A' && c<='Z') c+=32;return value; }
}
void Indentation::Normalize() { tabWidth=std::clamp(tabWidth,1,16);indentWidth=std::clamp(indentWidth,1,16); }
bool Indentation::operator==(const Indentation& other) const { return tabs==other.tabs && tabWidth==other.tabWidth && indentWidth==other.indentWidth; }
bool ParseGuideColumns(const std::string& text,std::vector<int>& columns,std::string& error) {
    auto input=Lower(text);std::replace(input.begin(),input.end(),',',' ');std::istringstream stream(input);std::string word;std::vector<int> result;
    while(stream>>word) {
        if(word=="off" && result.empty()) { if(stream>>word) {error="Use off by itself, or enter guide columns.";return false;}columns.clear();error.clear();return true; }
        int column=0;if(!Number(word,column,1000) || result.size()>=8) { error="Enter up to eight columns from 1 to 1000, separated by commas, or leave empty.";return false; }
        result.push_back(column);
    }
    if(!stream.eof() && stream.fail()) {error="Invalid guide columns.";return false;}
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());columns=std::move(result);error.clear();return true;
}
std::string GuideColumnsText(const std::vector<int>& columns) {
    std::string text;for(int column:columns) { if(!text.empty()) text+=", ";text+=std::to_string(column); }return text;
}
DocumentStyle ResolveDocumentStyle(const Indentation& defaults,const std::vector<int>& guides,const DocumentConfig& config,const DocumentOverrides& overrides) {
    DocumentStyle result{defaults,guides,{},{},{}};result.indentation.Normalize();
    std::vector<std::string> warnings=config.warnings;
    auto get=[&](const char* key)->const ConfigValue* { auto found=config.properties.find(key);return found==config.properties.end() || Lower(found->second.value)=="unset"?nullptr:&found->second; };
    auto source=[](const ConfigValue& value) { return value.file+":"+std::to_string(value.line); };
    auto invalid=[&](const char* key,const ConfigValue& value) { warnings.push_back(source(value)+": ignored invalid "+key+" = "+value.value); };
    auto* style=get("indent_style");auto* indent=get("indent_size");auto* tab=get("tab_width");auto* width=get("max_line_length");
    std::string styleOrigin="Defaults",indentOrigin="Defaults",tabOrigin="Defaults",guideOrigin="Defaults";
    if(style) { auto value=Lower(style->value);if(value=="tab" || value=="space") {result.indentation.tabs=value=="tab";styleOrigin=source(*style);}else invalid("indent_style",*style); }
    int indentNumber=0,tabNumber=0;bool indentIsTab=indent && Lower(indent->value)=="tab";
    if(indent && !indentIsTab && !Number(indent->value,indentNumber,16)) {invalid("indent_size",*indent);indentNumber=0;}
    if(tab && !Number(tab->value,tabNumber,16)) {invalid("tab_width",*tab);tabNumber=0;}
    if(indentNumber) {result.indentation.indentWidth=indentNumber;indentOrigin=source(*indent);}
    if(tabNumber) {result.indentation.tabWidth=tabNumber;tabOrigin=source(*tab);}
    else if(indentNumber && !config.properties.count("tab_width")) {result.indentation.tabWidth=indentNumber;tabOrigin=source(*indent)+" (indent_size)";}
    if(indentIsTab || (!config.properties.count("indent_size") && style && Lower(style->value)=="tab")) {
        result.indentation.indentWidth=result.indentation.tabWidth;indentOrigin=indent?source(*indent):styleOrigin;indentOrigin+=" (tab width)";
    }
    if(width) {int column=0;if(Lower(width->value)=="off") {result.guides.clear();guideOrigin=source(*width);}else if(Number(width->value,column,1000)) {result.guides={column};guideOrigin=source(*width);}else invalid("max_line_length",*width);}
    if(overrides.indentation) {result.indentation=*overrides.indentation;result.indentation.Normalize();styleOrigin=indentOrigin=tabOrigin="Document override";}
    if(overrides.guides) {result.guides=*overrides.guides;guideOrigin="Document override";}
    result.origin=std::string(result.indentation.tabs?"Tabs":"Spaces")+": "+styleOrigin+"\nIndent width "+std::to_string(result.indentation.indentWidth)+": "+indentOrigin+"\nTab width "+std::to_string(result.indentation.tabWidth)+": "+tabOrigin+"\nGuide columns "+(result.guides.empty()?"off":GuideColumnsText(result.guides))+": "+guideOrigin;
    for(const auto& key:{"end_of_line","charset","trim_trailing_whitespace","insert_final_newline"}) if(get(key)) {if(!result.notes.empty()) result.notes+='\n';result.notes+=std::string(key)+" is not applied by this indentation feature.";}
    for(const auto& warning:warnings) {if(!result.warning.empty()) result.warning+='\n';result.warning+=warning;}
    return result;
}
}
