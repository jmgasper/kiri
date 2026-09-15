#include "core/ColorTheme.h"
#include "core/LanguageAnalysis.h"
#include "core/FileIO.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace kiri;
namespace fs=std::filesystem;
static int checks=0;
#define CHECK(value) do { ++checks;if(!(value)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #value); } while(false)
template<class F> static bool Throws(F function) { try { function();return false; }catch(...) {return true;} }
static void Themes(const std::string& root) {
    CHECK(BuiltinColorThemes().size()==10);CHECK(ColorRoles().size()==45);
    for(const auto& theme:BuiltinColorThemes()) {auto read=ParseTheme(SerializeTheme(theme));CHECK(read.ok());CHECK(read.theme==theme);}
    auto custom=BuiltinColorThemes()[0];custom.id=NewThemeID();custom.name="Night garden 日本語";custom.colors["property"]=0xfedabc;
    CHECK(SaveColorTheme(root,custom).empty());CHECK(ResolveColorTheme(root,custom.id).theme==custom);CHECK(LoadColorThemes(root).size()==11);
    auto light=BuiltinColorThemes()[1];light.id=NewThemeID();light.name="Morning garden";CHECK(SaveColorTheme(root,light).empty());CHECK(ResolveColorTheme(root,light.id).theme==light);
    auto value=Json::parse(SerializeTheme(custom));value["colors"].erase("property");auto read=ParseTheme(value.dump());CHECK(read.ok());CHECK(!read.warning.empty());CHECK(read.theme.colors["property"]==BuiltinColorThemes()[0].colors.at("property"));
    value["colors"]["new.future.role"]="#123456";read=ParseTheme(value.dump());CHECK(read.ok());CHECK(read.warning.find("Unknown")!=std::string::npos);
    value["colors"]["text"]="#bad";CHECK(!ParseTheme(value.dump()).ok());
    value=Json::parse(SerializeTheme(custom));value["version"]=2;CHECK(!ParseTheme(value.dump()).ok());
    value["version"]=1;value["id"]="../escape";CHECK(!ParseTheme(value.dump()).ok());
    CHECK(!ParseTheme("{broken").ok());CHECK(!ParseTheme(std::string(1024*1024+1,'x')).ok());CHECK(!SaveColorTheme(root,BuiltinColorThemes()[0]).empty());
    custom.colors["text"]=custom.colors["background"];CHECK(!ThemeContrastWarning(custom).empty());
    fs::remove(fs::path(root)/"themes"/(light.id+".json"));read=ResolveColorTheme(root,light.id);CHECK(read.ok());CHECK(read.theme.id=="kiri.obsidian" && !read.warning.empty());
    uint32_t color=9;CHECK(ParseColor("#aB01fF",color) && color==0xab01ff);CHECK(!ParseColor("#a0000z",color) && color==0xab01ff);
    CHECK(HexColor(0x123)=="#000123");CHECK(LoadColorThemes(root+"/absent").size()==10);
}
static void Analysis() {
    std::string text="😀 const 日本 = 1;\r\nnext\rfinal\n";
    SourcePositions utf16(text,PositionEncoding::UTF16),utf8(text,PositionEncoding::UTF8),utf32(text,PositionEncoding::UTF32);
    CHECK(utf16.Offset({0,2})==4);CHECK(utf8.Offset({0,4})==4);CHECK(utf32.Offset({0,1})==4);
    CHECK(utf16.Offset({1,0})==text.find("next"));CHECK(utf16.Offset({2,0})==text.find("final"));CHECK(utf16.Offset({3,0})==text.size());
    CHECK(Throws([&]{utf16.Offset({0,1});}));CHECK(Throws([&]{utf8.Offset({0,2});}));CHECK(Throws([&]{utf16.Offset({1,5});}));CHECK(Throws([&]{utf16.Offset({99,0});}));
    CHECK(Throws([]{SourcePositions broken("\xff",PositionEncoding::UTF16);}));
    Json range={{"start",{{"line",0},{"character",9}}},{"end",{{"line",0},{"character",11}}}};
    Json rows=Json::array({{{"range",range},{"severity",2},{"message","Unknown 日本"},{"source","fixture"},{"code",42}}});
    auto diagnostics=ReadDiagnostics(rows,text,PositionEncoding::UTF16);CHECK(diagnostics.items.size()==1);CHECK(text.substr(diagnostics.items[0].start,diagnostics.items[0].end-diagnostics.items[0].start)=="日本");CHECK(diagnostics.items[0].code=="42");
    rows.push_back({{"range",{{"start",{{"line",0},{"character",1}}},{"end",{{"line",0},{"character",2}}}}},{"message","Split surrogate"}});
    diagnostics=ReadDiagnostics(rows,text,PositionEncoding::UTF16);CHECK(diagnostics.items.size()==1);CHECK(!diagnostics.warning.empty());
    auto empty=ReadDiagnostics(Json::array(),text,PositionEncoding::UTF16);CHECK(empty.items.empty() && empty.warning.empty());
    rows=Json::array();for(size_t i=0;i<kDiagnosticLimit+1;++i) rows.push_back({{"range",range},{"message","Repeated problem"}});
    auto bounded=ReadDiagnostics(rows,text,PositionEncoding::UTF16);CHECK(bounded.items.size()==kDiagnosticLimit && bounded.warning.find("2,000")!=std::string::npos);
    rows=Json::array({{{"range",range},{"message",std::string(8191,'a')+"😀"}}});bounded=ReadDiagnostics(rows,text,PositionEncoding::UTF16);CHECK(bounded.items.size()==1 && IsValidUTF8(bounded.items[0].message) && bounded.items[0].message.size()==8191);
    Json legend={{"tokenTypes",Json::array({"type","parameter","property","variable","futureType"})},{"tokenModifiers",Json::array({"readonly","deprecated","futureModifier"})}};
    text="Type param field var x\n  field";
    Json tokens={{"data",Json::array({0,0,4,0,0,0,5,5,1,4,0,6,5,2,0,0,6,3,3,3,0,4,1,4,0,1,2,5,2,0})}};
    auto semantic=ReadSemanticTokens(tokens,legend,text,PositionEncoding::UTF16);CHECK(semantic.error.empty());CHECK(semantic.tokens.size()==5);
    CHECK(semantic.tokens[0].role==SemanticRole::Type && semantic.tokens[1].role==SemanticRole::Parameter && semantic.tokens[2].role==SemanticRole::Property);
    CHECK(semantic.tokens[3].role==SemanticRole::Readonly && semantic.tokens[3].deprecated);CHECK(semantic.tokens.back().start==text.rfind("field"));
    CHECK(ReadSemanticTokens({{"data",Json::array({0,0,4,0})}},legend,text,PositionEncoding::UTF16).tokens.empty());
    CHECK(!ReadSemanticTokens({{"data",Json::array({0,0,99,0,0})}},legend,text,PositionEncoding::UTF16).error.empty());
    CHECK(!ReadSemanticTokens({{"data",Json::array({0,0,4,0,0,0,2,4,1,0})}},legend,text,PositionEncoding::UTF16).error.empty());
    for(auto encoding:{PositionEncoding::UTF8,PositionEncoding::UTF16,PositionEncoding::UTF32}) {
        text="// 😀\n日本";auto units=encoding==PositionEncoding::UTF8?6:2;
        semantic=ReadSemanticTokens({{"data",Json::array({1,0,units,0,0})}},legend,text,encoding);CHECK(semantic.error.empty());CHECK(semantic.tokens.size()==1 && semantic.tokens[0].end==text.size());
    }
}
static void Benchmark() {
    std::string text;Json data=Json::array();const std::string line="Type parameter property; // 😀 benchmark padding.........................................\n";
    while(text.size()+line.size()<=kLanguageBytes) {data.push_back(text.empty()?0:1);data.push_back(0);data.push_back(4);data.push_back(0);data.push_back(0);text+=line;}
    text.resize(kLanguageBytes,' ');Json legend={{"tokenTypes",Json::array({"type"})}};
    auto begin=std::chrono::steady_clock::now();auto parsed=ReadSemanticTokens({{"data",std::move(data)}},legend,text,PositionEncoding::UTF16);
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    CHECK(parsed.error.empty());CHECK(parsed.tokens.size()==kLanguageBytes/line.size());CHECK(parsed.tokens.back().end<=text.size());
    text+=' ';CHECK(!ReadSemanticTokens({{"data",Json::array()}},legend,text,PositionEncoding::UTF16).error.empty());
    std::cout<<"8 MiB semantic response: "<<parsed.tokens.size()<<" tokens, "<<elapsed<<" ms\n";
}
int main() {
    char directory[]="/tmp/kiri-analysis-theme-XXXXXX";auto* root=mkdtemp(directory);if(!root) return 1;
    try {Themes(root);Analysis();Benchmark();fs::remove_all(root);std::cout<<"Passed "<<checks<<" analysis and theme checks.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<" (files: "<<root<<")\n";return 1;}
}
