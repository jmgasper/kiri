#include "core/ColorTheme.h"
#include "core/FileIO.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace kiri {
namespace {
const std::vector<std::string> baseRoles={"background","panel","toolbar","text","muted","border","accent","selection","line","comment","keyword","string","number","type","added","removed"};
bool ValidID(const std::string& id) {
    return !id.empty() && id.size()<=100 && std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='-' || c=='_';});
}
void Extend(ColorTheme& t) {
    auto& c=t.colors;
    for(auto role:{"variable","parameter","property","function","namespace","readonly"}) c[role]=c[role==std::string("parameter")?"number":role==std::string("property")?"accent":role==std::string("function")?"string":role==std::string("namespace")?"type":role==std::string("readonly")?"keyword":"text"];
    c["diagnostic.error"]=c["removed"];c["diagnostic.warning"]=c["number"];c["diagnostic.info"]=c["accent"];c["diagnostic.hint"]=c["muted"];
    c["terminal.background"]=c["background"];c["terminal.text"]=c["text"];c["selection.text"]=c["text"];
    const uint32_t ansi[]={0x000000,0xcd0000,0x00cd00,0xcdcd00,0x0000ee,0xcd00cd,0x00cdcd,0xe5e5e5,0x7f7f7f,0xff0000,0x00ff00,0xffff00,0x5c5cff,0xff00ff,0x00ffff,0xffffff};
    for(int i=0;i<16;++i) c["terminal.ansi"+std::to_string(i)]=ansi[i];
}
}
const std::vector<ColorTheme>& BuiltinColorThemes() {
    struct Palette { const char *id,*name;bool dark;std::vector<uint32_t> colors; };
    static const std::vector<ColorTheme> themes=[] {
        const Palette palettes[]={
        {"kiri.obsidian","Obsidian",true, {0x171b22,0x202630,0x282f3a,0xdce3ed,0x8c9bad,0x343e4b,0x78b7ff,0x334e70,0x202936,0x8195a6,0xc6a0f6,0xa6da95,0xf5a97f,0x8bd5ca,0xa6da95,0xed8796}},
        {"kiri.daylight","Daylight",false, {0xfafbfD,0xf0f2f6,0xe5e9f0,0x273448,0x68788d,0xcfd6e0,0x2567c6,0xc8ddf8,0xf0f4fa,0x718096,0x8047a8,0x3d7a40,0xa55028,0x187f86,0x24723b,0xc1384c}},
        {"kiri.nord","Nord",true, {0x2e3440,0x343c49,0x3b4252,0xeceff4,0x9caec6,0x4c566a,0x88c0d0,0x4c566a,0x353e4e,0x899db7,0xb48ead,0xa3be8c,0xd08770,0x8fbcbb,0xa3be8c,0xbf616a}},
        {"kiri.midnight","Midnight",true, {0x101827,0x162135,0x1c2a42,0xdde7ff,0x8fa2c2,0x30415f,0x91b4ff,0x2f4269,0x19253d,0x889ab5,0xc4a7ff,0x9cdaa0,0xffcc85,0x75cfe1,0x91ce9c,0xff9bac}},
        {"kiri.forest","Forest",true, {0x17231e,0x1e2d26,0x26392f,0xe0ece3,0x9bb1a2,0x3d5145,0xb4d594,0x355546,0x21362c,0x93aa97,0xceb0de,0xc6d88c,0xebba91,0x8acdbf,0xaad389,0xefadac}},
        {"kiri.ember","Ember",true, {0x241c19,0x2f2420,0x3a2c26,0xefe0d3,0xb5a092,0x564136,0xe9b47e,0x67452e,0x342820,0xaf9b8a,0xdcb2d5,0xbed293,0xedb497,0x85c9c0,0xbdd892,0xf6a29c}},
        {"kiri.linen","Linen",false, {0xfffaf0,0xf5eddf,0xeae0cf,0x40362b,0x7a6d5d,0xd9cbb6,0x986333,0xe9d4ad,0xf7eddc,0x81705e,0x864775,0x527038,0xa15131,0x316d79,0x417342,0xa1393f}},
        {"kiri.glacier","Glacier",false, {0xf5fbff,0xe9f2f8,0xdce8f1,0x233d50,0x5f7888,0xbfd0dd,0x2068a2,0xc1def3,0xe9f4fb,0x617b8b,0x6e4898,0x267257,0x9a5125,0x216d82,0x267141,0xbf3a52}},
        {"kiri.rose","Rose",false, {0xfff8fa,0xf6edf1,0xf0dfe7,0x47323e,0x856a79,0xdec9d3,0x985373,0xebc7d9,0xf9eaf0,0x8b7080,0x84499b,0x427849,0xa0572b,0x3a7586,0x327344,0xc13e56}},
        {"kiri.meadow","Meadow",false, {0xf7fcf5,0xebf3e6,0xdeecd9,0x2f4231,0x647c65,0xc5d8bd,0x477b35,0xcbe3c1,0xeaf5e5,0x657c64,0x7c519b,0x37743e,0xa25a2c,0x256f81,0x307739,0xa93f4d}},
        };
        std::vector<ColorTheme> result;
        for(auto& p:palettes) { ColorTheme t;t.id=p.id;t.name=p.name;t.base=p.id;t.dark=p.dark;for(size_t i=0;i<baseRoles.size();++i) t.colors[baseRoles[i]]=p.colors[i];Extend(t);result.push_back(std::move(t)); }
        return result;
    }();return themes;
}
const std::vector<std::string>& ColorRoles() {
    static const auto roles=[] { std::vector<std::string> result;for(auto& color:BuiltinColorThemes()[0].colors) result.push_back(color.first);return result; }();return roles;
}
bool ColorTheme::operator==(const ColorTheme& other) const { return id==other.id && name==other.name && base==other.base && dark==other.dark && colors==other.colors; }
std::string NewThemeID() {
    static std::atomic<uint64_t> sequence{0};
    return "custom."+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence);
}
std::string HexColor(uint32_t color) { std::ostringstream out;out<<'#'<<std::hex<<std::setfill('0')<<std::setw(6)<<(color&0xffffff);return out.str(); }
bool ParseColor(const std::string& text,uint32_t& color) {
    if(text.size()!=7 || text[0]!='#') return false;
    uint32_t result=0;for(size_t i=1;i<text.size();++i) { unsigned char c=text[i];int n=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;if(n<0) return false;result=(result<<4)|n; }color=result;return true;
}
ThemeResult ParseTheme(const std::string& bytes) {
    ThemeResult result;
    try {
        if(bytes.size()>1024*1024) throw std::runtime_error("Theme files are limited to 1 MiB.");
        auto value=Json::parse(bytes);
        if(value.at("format")!="kiri-theme" || value.at("version")!=1) throw std::runtime_error("Expected a Kiri theme with format version 1.");
        bool dark=value.value("dark",true);auto base=value.value("base",dark?std::string("kiri.obsidian"):std::string("kiri.daylight"));
        const auto& builtins=BuiltinColorThemes();auto found=std::find_if(builtins.begin(),builtins.end(),[&](const auto& t){return t.id==base;});
        result.theme=found==builtins.end()?builtins[dark?0:1]:*found;
        if(found==builtins.end()) result.warning="Unknown base theme; using "+result.theme.name+" defaults. ";
        auto& t=result.theme;t.base=result.theme.id;t.id=value.value("id",NewThemeID());t.name=value.at("name").get<std::string>();t.dark=dark;
        if(!ValidID(t.id) || t.name.empty() || t.name.size()>80 || !IsValidUTF8(t.name) || std::any_of(t.name.begin(),t.name.end(),[](unsigned char c){return c<32 || c==127;})) throw std::runtime_error("Theme IDs and names must be valid; names can contain up to 80 UTF-8 bytes.");
        auto colors=value.value("colors",Json::object());if(!colors.is_object()) throw std::runtime_error("Theme colors must be an object of named #RRGGBB values.");
        size_t applied=0,unknown=0;
        for(auto it=colors.begin();it!=colors.end();++it) {
            if(!t.colors.count(it.key())) { ++unknown;continue; }
            uint32_t color=0;if(!it.value().is_string() || !ParseColor(it.value().get<std::string>(),color)) throw std::runtime_error("Invalid color for "+it.key()+"; use #RRGGBB.");
            t.colors[it.key()]=color;++applied;
        }
        if(applied<t.colors.size()) result.warning+="Missing colors use the base palette. ";
        if(unknown) result.warning+="Unknown color names were ignored. ";
        result.warning+=ThemeContrastWarning(t);
    }catch(const std::exception& e) { result.error="Cannot read theme: "+std::string(e.what()); }
    return result;
}
std::string SerializeTheme(const ColorTheme& t) {
    Json colors=Json::object();for(auto& c:t.colors) colors[c.first]=HexColor(c.second);
    return Json{{"format","kiri-theme"},{"version",1},{"id",t.id},{"name",t.name},{"base",t.base},{"dark",t.dark},{"colors",colors}}.dump(2)+"\n";
}
ThemeResult ReadThemeFile(const std::string& path) {
    if(StatFile(path).size>1024*1024) {ThemeResult r;r.error="Theme files are limited to 1 MiB.";return r;}
    auto data=ReadFile(path,nullptr,1024*1024);if(!data.ok()) { ThemeResult r;r.error=data.error;return r; }return ParseTheme(data.bytes);
}
std::vector<ColorTheme> LoadColorThemes(const std::string& settings,std::string* warning) {
    auto result=BuiltinColorThemes();if(settings.empty()) return result;
    std::error_code error;std::filesystem::directory_iterator files(std::filesystem::path(settings)/"themes",error);
    size_t count=0;
    for(std::filesystem::directory_iterator end;files!=end;files.increment(error)) {
        if(error) {if(warning) *warning+="Cannot read the theme library: "+error.message();break;}
        const auto& entry=*files;
        if(++count>256) { if(warning) *warning+="Theme library limit: 256 files. ";break; }
        if(entry.path().extension()!=".json" || !entry.is_regular_file(error) || entry.is_symlink(error)) continue;
        auto read=ReadThemeFile(entry.path().string());
        if(!read.ok() || read.theme.id.rfind("custom.",0)!=0 || entry.path().filename()!=read.theme.id+".json") { if(warning) *warning+=entry.path().filename().string()+": invalid custom theme. ";continue; }
        result.push_back(std::move(read.theme));
    }
    std::sort(result.begin()+BuiltinColorThemes().size(),result.end(),[](const auto& a,const auto& b){return a.name==b.name?a.id<b.id:a.name<b.name;});return result;
}
std::string SaveColorTheme(const std::string& settings,const ColorTheme& theme) {
    if(!ValidID(theme.id) || theme.id.rfind("custom.",0)!=0) return "Duplicate a built-in theme before editing it.";
    auto bytes=SerializeTheme(theme);auto checked=ParseTheme(bytes);if(!checked.ok()) return checked.error;
    std::error_code error;auto folder=std::filesystem::path(settings)/"themes";std::filesystem::create_directories(folder,error);if(error) return error.message();
    auto path=(folder/(theme.id+".json")).string();return SaveFile(path,bytes,StatFile(path));
}
ThemeResult ResolveColorTheme(const std::string& settings,const std::string& id) {
    for(const auto& t:BuiltinColorThemes()) if(t.id==id) { ThemeResult r;r.theme=t;return r; }
    ThemeResult r;if(ValidID(id) && id.rfind("custom.",0)==0 && !settings.empty()) r=ReadThemeFile((std::filesystem::path(settings)/"themes"/(id+".json")).string());
    if(r.ok() && r.theme.id==id) return r;
    r.theme=BuiltinColorThemes()[0];r.error.clear();r.warning="Theme "+id+" is unavailable; using Obsidian.";return r;
}
std::string ThemeContrastWarning(const ColorTheme& theme) {
    auto luminance=[](uint32_t color) { auto channel=[](int value){double c=value/255.;return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};return .2126*channel(color>>16)+.7152*channel((color>>8)&255)+.0722*channel(color&255); };
    auto contrast=[&](const char* a,const char* b) { double x=luminance(theme.colors.at(a)),y=luminance(theme.colors.at(b));return (std::max(x,y)+.05)/(std::min(x,y)+.05); };
    for(auto background:{"background","panel","toolbar"}) if(contrast("text",background)<3) return "Low contrast between text and a surface; review the preview. ";
    if(contrast("terminal.text","terminal.background")<3 || contrast("selection.text","selection")<3) return "Low contrast in terminal or selected text; review the preview. ";
    return {};
}
}
