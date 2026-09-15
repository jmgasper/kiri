#include "ui/Editor.h"
#include "ui/Async.h"
#include "ui/FileIcons.h"
#include "ui/RecentItems.h"
#include "core/FileIO.h"
#include "core/Project.h"
#include <Application.h>
#include <Bitmap.h>
#include <Font.h>
#include <File.h>
#include <LayoutBuilder.h>
#include <Window.h>
#include <OS.h>
#include <SciLexer.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <fs_attr.h>
using namespace kiri;
namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
static uint64_t Resident() { ssize_t cookie=0;area_info area{};uint64_t total=0;while(get_next_area_info(getpid(),&cookie,&area)==B_OK) total+=area.ram_size;return total; }
static void Recents() {
    char temporary[]="/tmp/kiri-recents-XXXXXX";CHECK(mkdtemp(temporary));std::string root=CanonicalPath(temporary);
    auto write=[](const std::string& path,const BMessage& message) {
        BFile file(path.c_str(),B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);CHECK(file.InitCheck()==B_OK);CHECK(message.Flatten(&file)==B_OK);
    };
    std::string folder=root+"/project",first=root+"/first.cpp",second=root+"/日本語 file.txt";
    fs::create_directory(folder);{std::ofstream file(first);file<<"preserve this file\n";}
    BMessage legacy;legacy.AddString("project",folder.c_str());legacy.AddString("file",first.c_str());legacy.AddString("file",second.c_str());legacy.AddString("selected",second.c_str());
    write(root+"/settings",legacy);auto saved=ReadFile(root+"/settings").bytes;
    RecentItems history(root);auto items=history.Load();
    CHECK(items.size()==3);CHECK(items[0].folder && items[0].path==folder);CHECK(!items[1].folder && items[1].path==second);
    CHECK(history.Remember(first,false));items=RecentItems(root).Load();
    CHECK(items.size()==3 && items[0].path==first && items[1].path==folder);
    CHECK(history.Remember(folder+"/../project",true));items=history.Load();
    CHECK(items.size()==3 && items[0].path==folder && items[0].folder);
    CHECK(!history.Remember("",false));CHECK(history.Load().size()==3);
    CHECK(history.Remove(first));CHECK(ReadFile(first).bytes=="preserve this file\n");CHECK(history.Load().size()==2);
    CHECK(ReadFile(root+"/settings").bytes==saved);
    for(size_t i=0;i<RecentItems::Limit+5;++i) if(!history.Remember(root+"/file-"+std::to_string(i),false)) throw std::runtime_error("Cannot save recent items.");
    items=RecentItems(root).Load();CHECK(items.size()==RecentItems::Limit);
    CHECK(items.front().path==root+"/file-"+std::to_string(RecentItems::Limit+4));CHECK(items.back().path==root+"/file-5");
    for(const auto& item:items) history.Remove(item.path);
    CHECK(RecentItems(root).Load().empty()); // An empty list must not re-import the old session.
    BMessage malformed,row;row.AddString("path","relative.cpp");row.AddBool("folder",false);malformed.AddMessage("item",&row);
    row.MakeEmpty();row.AddString("path",first.c_str());malformed.AddMessage("item",&row);
    row.AddBool("folder",false);malformed.AddMessage("item",&row);malformed.AddMessage("item",&row);
    write(root+"/recent",malformed);items=history.Load();CHECK(items.size()==1 && items[0].path==first);
    // Unsaved recovery snapshots are not recent source files.
    std::string oldRoot=root+"/legacy";fs::create_directory(oldRoot);legacy.MakeEmpty();legacy.AddString("selected",(root+"/recovery/draft").c_str());
    write(oldRoot+"/settings",legacy);CHECK(RecentItems(oldRoot).Load().empty());
    fs::remove_all(root);
}
static double Luminance(rgb_color color) {
    auto channel=[](uint8 value) { double c=value/255.0;return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4); };
    return .2126*channel(color.red)+.7152*channel(color.green)+.0722*channel(color.blue);
}
static double Contrast(rgb_color a,rgb_color b) {
    double first=Luminance(a),second=Luminance(b);
    return (std::max(first,second)+.05)/(std::min(first,second)+.05);
}
static void Preferences(Editor* editor) {
    EditorSettings defaults,settings;
    font_family family;be_plain_font->GetFamilyAndStyle(&family,nullptr);
    settings.fontFamily=family;settings.fontSize=18;settings.theme=ThemeIndex("Linen");settings.Normalize();
    BMessage stored;settings.WriteTo(stored);EditorSettings restored;restored.ReadFrom(stored);CHECK(restored==settings);
    BMessage legacy;legacy.AddInt32("theme",1);restored=defaults;restored.ReadFrom(legacy);
    CHECK(Theme::Builtins()[restored.theme].name=="Daylight" && restored.fontFamily==defaults.fontFamily && restored.fontSize==defaults.fontSize);
    BMessage invalid;invalid.AddString("editor_font_family","No such Kiri test font");invalid.AddInt32("editor_font_size",-1);invalid.AddInt32("theme",1000);
    restored.ReadFrom(invalid);CHECK(restored.fontFamily==defaults.fontFamily && restored.fontSize==8 && restored.theme==0);
    invalid.MakeEmpty();invalid.AddInt32("editor_font_size",1000);restored.ReadFrom(invalid);CHECK(restored.fontSize==48);
    int dark=0,light=0;std::set<std::string> names;
    auto text=editor->Text();auto revision=editor->Revision();auto dirty=editor->Dirty();
    editor->SendMessage(SCI_SETSEL,2,7);
    for(size_t i=0;i<Theme::Builtins().size();++i) {
        const auto& theme=Theme::Builtins()[i];theme.dark?++dark:++light;names.insert(theme.name);
        CHECK(Contrast(theme.text,theme.background)>=7);
        for(auto color:{theme.comment,theme.keyword,theme.string,theme.number,theme.type,theme.added,theme.removed}) CHECK(Contrast(color,theme.background)>=3);
        settings.theme=i;editor->ApplySettings(settings);
        CHECK(editor->SendMessage(SCI_STYLEGETBACK,STYLE_DEFAULT)==SciColor(theme.background));
        CHECK(editor->SendMessage(SCI_STYLEGETFORE,STYLE_DEFAULT)==SciColor(theme.text));
        CHECK(editor->SendMessage(SCI_STYLEGETSIZE,STYLE_DEFAULT)==18);
        char actual[B_FONT_FAMILY_LENGTH+1]{};editor->SendMessage(SCI_STYLEGETFONT,STYLE_DEFAULT,reinterpret_cast<sptr_t>(actual));
        CHECK(std::string(actual)==family);
        CHECK(editor->Text()==text && editor->Revision()==revision && editor->Dirty()==dirty);
        CHECK(editor->SendMessage(SCI_GETANCHOR)==2 && editor->SendMessage(SCI_GETCURRENTPOS)==7);
    }
    CHECK(dark==5 && light==5 && names.size()==10);
    editor->SetLanguage("appearance.cpp");
    CHECK(editor->SendMessage(SCI_STYLEGETSIZE,SCE_C_WORD)==18);
    editor->ApplySettings(defaults);
    CHECK(editor->SendMessage(SCI_STYLEGETSIZE,STYLE_DEFAULT)==13);
}
static void Icons() {
    std::set<std::string> bitmaps;
    for(const char* file:{"main.cpp","index.html","notes.txt","photo.png","archive.zip","report.pdf","track.mp3","data.bin"}) {
        auto icon=FileIcon(file);CHECK(icon && icon->InitCheck()==B_OK && icon->Bounds()==BRect(0,0,15,15));
        bitmaps.emplace(static_cast<const char*>(icon->Bits()),icon->BitsLength());
    }
    CHECK(bitmaps.size()>=4);
    CHECK(FileIcon("one.cpp")==FileIcon("two.CPP"));
    CHECK(FileIcon("one",true) && FileIcon("one",true)==FileIcon("two",true));
    CHECK(FileIcon("unknown.kiri-test-extension"));
}
static void LanguageEdits(Editor* editor) {
    std::string before="const user={name:\"Kiri\"};\n😀\n",formatted="const user = { name: 'Kiri' }\n😀\n";
    editor->SetText(before);editor->SendMessage(SCI_SETSEL,6,10);
    editor->ApplyEdits({{0,before.size(),formatted}},true);
    CHECK(editor->Text()==formatted);CHECK(editor->Dirty());CHECK(editor->SendMessage(SCI_GETANCHOR)==6);CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==10);
    editor->SendMessage(SCI_UNDO);CHECK(editor->Text()==before);CHECK(!editor->Dirty());
    editor->SendMessage(SCI_REDO);CHECK(editor->Text()==formatted);
    before="const result = user.na;\n";editor->SetText(before);editor->SendMessage(SCI_SETSEL,22,22);
    std::vector<TextEdit> edits={{20,22,"name"},{0,0,"// imported user\n"}};
    editor->ApplyEdits(edits);CHECK(editor->Text()=="// imported user\nconst result = user.name;\n");CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==41);
    editor->SendMessage(SCI_UNDO);CHECK(editor->Text()==before);CHECK(!editor->Dirty());
    bool rejected=false;try { editor->ApplyEdits({{0,6,"x"},{3,10,"y"}}); }catch(const std::exception&) { rejected=true; }
    CHECK(rejected);CHECK(editor->Text()==before);CHECK(!editor->Dirty());
    editor->SetText(before,true);editor->ApplyEdits({{0,before.size(),"changed"}});CHECK(editor->Text()==before);editor->SetText("");
    BMessage complete(B_KEY_DOWN);complete.AddInt32("modifiers",B_CONTROL_KEY);complete.AddInt32("raw_char",32);complete.AddString("bytes","");
    CHECK(editor->FilterLanguageKey(&complete));CHECK(editor->Text().empty());
}
static void IndentationAndGuides(Editor* editor) {
    EditorSettings settings;settings.indentation={false,2,2};settings.guideColumns={80,100};settings.Normalize();
    BMessage stored;settings.WriteTo(stored);EditorSettings restored;restored.ReadFrom(stored);CHECK(restored==settings);
    editor->State()->config={};editor->State()->overrides={};editor->SetText("value\n");editor->ApplySettings(settings);
    CHECK(editor->SendMessage(SCI_GETUSETABS)==0 && editor->SendMessage(SCI_GETTABWIDTH)==2 && editor->SendMessage(SCI_GETINDENT)==2);
    CHECK(editor->SendMessage(SCI_GETEDGEMODE)==EDGE_MULTILINE);CHECK(editor->SendMessage(SCI_GETMULTIEDGECOLUMN,0)==80);CHECK(editor->SendMessage(SCI_GETMULTIEDGECOLUMN,1)==100);
    editor->SendMessage(SCI_SETSEL,0,0);editor->SendMessage(SCI_TAB);CHECK(editor->Text()=="  value\n");editor->SendMessage(SCI_BACKTAB);CHECK(editor->Text()=="value\n");
    editor->SetText("  value");editor->SendMessage(SCI_SETSEL,7,7);editor->InsertNewline();CHECK(editor->Text()=="  value\n  ");CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==10);
    editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="  value" && !editor->Dirty());editor->SendMessage(SCI_REDO);CHECK(editor->Text()=="  value\n  ");
    editor->SetText("  ");editor->SendMessage(SCI_SETSEL,2,2);editor->PasteText("    one\n      two\n    three");CHECK(editor->Text()=="  one\n    two\n  three");editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="  " && !editor->Dirty());
    settings.indentation={true,3,4};editor->ApplySettings(settings);editor->SetText("value");editor->SendMessage(SCI_SETSEL,0,0);editor->SendMessage(SCI_TAB);CHECK(editor->Text()=="\t value");
    editor->SendMessage(SCI_SETSEL,7,7);editor->InsertNewline();CHECK(editor->Text()=="\t value\n\t ");
    editor->SetText("\t");editor->SendMessage(SCI_SETSEL,1,1);editor->PasteText("one\n   two");CHECK(editor->Text()=="\tone\n\t\ttwo");editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="\t" && !editor->Dirty());
    // Multiple carets receive their own indentation in a single undo group.
    settings.indentation={false,2,2};editor->ApplySettings(settings);editor->SetText("  one\n    two");editor->SendMessage(SCI_SETSEL,5,5);editor->SendMessage(SCI_ADDSELECTION,13,13);editor->InsertNewline();
    CHECK(editor->Text()=="  one\n  \n    two\n    ");CHECK(editor->SendMessage(SCI_GETSELECTIONS)==2);editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="  one\n    two" && !editor->Dirty());
    editor->SetText("  one",false,SC_EOL_CRLF);editor->SendMessage(SCI_SETSEL,5,5);editor->InsertNewline();CHECK(editor->Text()=="  one\r\n  ");editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="  one");
    editor->SetText("unchanged");editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));editor->SendMessage(SCI_SETSEL,2,6);auto before=editor->Text();auto revision=editor->InputRevision();
    std::set<std::string> families;for(auto* font:{be_plain_font,be_fixed_font}) {font_family family;font->GetFamilyAndStyle(&family,nullptr);families.insert(family);}for(int32 i=0;i<count_font_families() && families.size()<3;++i) {font_family family;if(get_font_family(i,&family)==B_OK) families.insert(family);}CHECK(families.size()==3);
    for(const auto& family:families) for(int size:{10,18,26}) {settings.fontFamily=family;settings.fontSize=size;settings.indentation={true,8,8};settings.guideColumns={72,120};editor->ApplySettings(settings);for(int zoom:{-2,0,3}) {editor->SendMessage(SCI_SETZOOM,zoom);CHECK(editor->SendMessage(SCI_GETMULTIEDGECOLUMN,1)==120);CHECK(editor->Text()==before && editor->InputRevision()==revision && editor->Dirty());CHECK(editor->SendMessage(SCI_GETANCHOR)==2 && editor->SendMessage(SCI_GETCURRENTPOS)==6);}}
    editor->State()->overrides.indentation=Indentation{false,3,3};editor->State()->overrides.guides=std::vector<int>{};editor->ApplyDocumentStyle();CHECK(editor->SendMessage(SCI_GETINDENT)==3 && editor->SendMessage(SCI_GETEDGEMODE)==EDGE_NONE);
    BMessage document;WriteDocumentOverrides(document,editor->State()->overrides);auto overrides=ReadDocumentOverrides(document);CHECK(overrides.indentation==editor->State()->overrides.indentation && overrides.guides==editor->State()->overrides.guides);
    editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="unchanged" && !editor->Dirty());
    editor->State()->overrides={};editor->ApplySettings(EditorSettings());editor->SendMessage(SCI_SETZOOM,0);editor->SetText("");
}
static void Fixtures(const std::string& root) {
    if(fs::exists(root)) throw std::runtime_error("Fixture directory must be new.");
    fs::create_directories(root);
    { std::ofstream large(root+"/large.txt",std::ios::binary);std::string line="0123456789 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";line.resize(99,' ');line+='\n';
      for(int i=0;i<2097152;++i) large<<line; }
    for(int directory=0;directory<100;++directory) {
        auto path=root+"/module-"+std::to_string(directory);fs::create_directory(path);
        for(int file=0;file<1000;++file) { std::ofstream out(path+"/source-"+std::to_string(file)+".cpp");out<<"// indexed fixture\nint value = "<<file<<";\n"; }
    }
    std::cout<<"Created a 200 MiB file and 100,000 source files.\n";
}

static void AdvancedSearch(kiri::Editor* editor) {
    using namespace kiri;
    const std::string original="é=12 日本語=7\r\nkeep=9\r\n";
    editor->SetText(original);SearchOptions options;options.regex=true;options.matchCase=true;options.query="([\\p{L}]+)=(\\d+)";
    CHECK(editor->SearchMatches(options).matches.size()==3);
    CHECK(editor->SendMessage(SCI_INDICATORVALUEAT,8,0)!=0);
    CHECK(editor->Replace(options,"$1:$2",true)==3);CHECK(editor->Text()=="é:12 日本語:7\r\nkeep:9\r\n");
    editor->SendMessage(SCI_UNDO);CHECK(editor->Text()==original);CHECK(!editor->Dirty());
    editor->SendMessage(SCI_SETSEL,0,17);editor->SetSearchSelection(true);
    CHECK(editor->Replace(options,"${missing}",true)==0);CHECK(!editor->SearchError().empty());CHECK(editor->Text()==original);
    CHECK(editor->Replace(options,"$1=$2!",true)==2);CHECK(editor->Text()=="é=12! 日本語=7!\r\nkeep=9\r\n");
    editor->SendMessage(SCI_UNDO);CHECK(editor->Text()==original);editor->SetSearchSelection(false);
    options.query="[";CHECK(editor->Replace(options,"oops",true)==0);CHECK(!editor->SearchError().empty());CHECK(editor->Text()==original);
    options.query="(?=.)";editor->SetText("😀é");CHECK(editor->Replace(options,"x",true)==2);CHECK(editor->Text()=="x😀xé");editor->SendMessage(SCI_UNDO);CHECK(editor->Text()=="😀é");
    editor->SendMessage(SCI_SETSEL,0,0);CHECK(editor->Find(options));CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==0);
    CHECK(editor->Find(options));CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==4);CHECK(editor->Find(options));CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==0);
    CHECK(editor->Find(options,true));CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==4);
    options.query="name";options.regex=false;options.wholeWord=true;options.matchCase=false;editor->SetText("name names NAME\r\n");
    CHECK(editor->SearchMatches(options).matches.size()==2);options.matchCase=true;CHECK(editor->SearchMatches(options).matches.size()==1);
    editor->SendMessage(SCI_SETREADONLY,1);CHECK(editor->Find(options));CHECK(editor->Replace(options,"new",true)==0);CHECK(!editor->SearchError().empty());
    CHECK(editor->Text()=="name names NAME\r\n");editor->SendMessage(SCI_SETREADONLY,0);editor->ClearSearchHighlights();CHECK(editor->SendMessage(SCI_INDICATORVALUEAT,8,0)==0);
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--fixtures") { Fixtures(argv[2]);return 0; }
        BApplication application("application/x-vnd.Kiri-native-tests");
        if(argc==3 && std::string(argv[1])=="--probe-file") {
            BMessenger app("application/x-vnd.Kiri-editor");BMessage request(B_GET_PROPERTY),reply;
            request.AddSpecifier("Messenger");request.AddSpecifier("Window",int32(0));
            CHECK(app.SendMessage(&request,&reply)==B_OK);BMessenger workspace;CHECK(reply.FindMessenger("result",&workspace)==B_OK);
            BMessage open(kOpenFile);open.AddString("path",argv[2]);CHECK(workspace.SendMessage(&open)==B_OK);
            std::vector<double> samples;
            for(int i=0;i<200;++i) {
                BMessage ping(B_GET_PROPERTY),answer;ping.AddSpecifier("Frame");auto start=system_time();
                CHECK(workspace.SendMessage(&ping,&answer,1000000,1000000)==B_OK);samples.push_back((system_time()-start)/1000.0);snooze(10000);
            }
            std::sort(samples.begin(),samples.end());
            std::cout<<"native_window_pings="<<samples.size()<<" median_ms="<<samples[samples.size()/2]<<" max_ms="<<samples.back()<<'\n';return 0;
        }
        {
            char path[]="/tmp/kiri-attributes-XXXXXX";int descriptor=mkstemp(path);CHECK(descriptor>=0);
            CHECK(fs_write_attr(descriptor,"kiri:value",B_STRING_TYPE,0,"Kiri",5)==5);
            CHECK(fs_write_attr(descriptor,"kiri:binary",B_RAW_TYPE,0,"a\0b",3)==3);close(descriptor);
            CHECK(SaveFile(path,"saved text\n",StatFile(path)).empty());
            descriptor=open(path,O_RDONLY);char value[5]{};attr_info info{};
            CHECK(fs_read_attr(descriptor,"kiri:value",B_STRING_TYPE,0,value,sizeof(value))==5 && std::string(value)=="Kiri");
            CHECK(fs_read_attr(descriptor,"kiri:binary",B_RAW_TYPE,0,value,sizeof(value))==3 && value[0]=='a' && value[1]==0 && value[2]=='b'
                && fs_stat_attr(descriptor,"kiri:binary",&info)==0 && info.size==3 && info.type==B_RAW_TYPE);
            close(descriptor);unlink(path);
        }
        auto* window=new BWindow(BRect(0,0,700,500),"Kiri Native Tests",B_TITLED_WINDOW,0);
        auto* editor=new Editor();BLayoutBuilder::Group<>(window,B_VERTICAL,0).Add(editor);
        editor->SetLanguage("test.cpp");editor->SetText("int main() { return 0; }\n日本語\n");
        CHECK(!editor->Dirty());editor->SendMessage(SCI_COLOURISE,0,-1);
        CHECK(editor->SendMessage(SCI_GETSTYLEAT,0)==SCE_C_WORD);
        auto revision=editor->Revision();editor->SendMessage(SCI_APPENDTEXT,2,reinterpret_cast<sptr_t>("x\n"));
        CHECK(editor->Dirty());CHECK(editor->Revision()>revision);editor->SendMessage(SCI_UNDO);CHECK(!editor->Dirty());
        editor->GoTo(2,2);CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==28);
        editor->GoTo(1,1000000);CHECK(editor->SendMessage(SCI_GETCURRENTPOS)==24);
        CHECK(editor->Find("日本語"));CHECK(editor->ReplaceAll("return","co_return")==1);editor->SendMessage(SCI_UNDO);CHECK(editor->Text().find("co_return")==std::string::npos);
        editor->SetText("one ONE one");editor->SendMessage(SCI_SETSEL,0,3);
        CHECK(editor->ReplaceOne("one","first") && editor->Text()=="first ONE one");
        CHECK(editor->ReplaceOne("one","second") && editor->Text()=="first second one");
        CHECK(!editor->ReplaceOne("missing","value") && editor->Text()=="first second one");
        editor->SetText("{\"key\": true, \"n\": 42}\n");editor->SetLanguage("test.json");editor->SendMessage(SCI_COLOURISE,0,-1);
        CHECK(editor->SendMessage(SCI_GETSTYLEAT,3)==SCE_JSON_PROPERTYNAME);CHECK(editor->SendMessage(SCI_GETSTYLEAT,8)==SCE_JSON_KEYWORD);
        editor->MarkRecovered();CHECK(editor->Dirty());editor->MarkSaved();CHECK(!editor->Dirty());
        Preferences(editor);Icons();Recents();LanguageEdits(editor);AdvancedSearch(editor);IndentationAndGuides(editor);
        auto loader=editor->CreateLoader(true);CHECK(loader->loader);
        std::thread loading([&]{CHECK(loader->loader->AddData("first\r\nsecond\r\n",15)==SC_STATUS_OK);});loading.join();editor->Adopt(*loader,0);
        CHECK(editor->Text()=="first\r\nsecond\r\n");CHECK(editor->SendMessage(SCI_GETLINECOUNT)==3);CHECK(!editor->Dirty());CHECK(editor->SendMessage(SCI_GETDOCUMENTOPTIONS)&SC_DOCUMENTOPTION_STYLES_NONE);
        {
            AsyncQueue jobs{BMessenger(window)};std::atomic<bool> started{false},finished{false};bool old=false,current=false;
            jobs.Submit([&](const auto& cancel){started=true;while(!cancel) snooze(1000);return [&]{old=true;};},"search");
            while(!started) snooze(1000);
            jobs.Submit([&](const auto&){finished=true;return [&]{current=true;};},"search");
            while(!finished) snooze(1000);snooze(10000);jobs.Drain();CHECK(current);CHECK(!old);
        }
        if(argc==3 && std::string(argv[1])=="--benchmark") {
            std::string root=argv[2];auto before=Resident();auto start=system_time();auto large=editor->CreateLoader(true);FileData data;
            std::thread read([&]{data=ReadFile(root+"/large.txt",nullptr,1024ULL*1024*1024,[&](auto bytes){return large->loader->AddData(bytes.data(),bytes.size())==SC_STATUS_OK;});});read.join();
            auto loaded=system_time();CHECK(data.ok());editor->Adopt(*large);auto adopted=system_time();
            CHECK(editor->SendMessage(SCI_GETLENGTH)==209715200);CHECK(editor->SendMessage(SCI_GETLINECOUNT)==2097153);
            std::cout<<"file_bytes="<<data.stamp.size<<" background_load_ms="<<(loaded-start)/1000.0<<" attach_ms="<<(adopted-loaded)/1000.0<<" resident_delta_mib="<<(Resident()-before)/1048576.0<<'\n';
            start=system_time();auto index=IndexProject(root);loaded=system_time();auto matches=QuickOpen(index,"m42s999");adopted=system_time();
            CHECK(index.paths.size()==100001);CHECK(!matches.empty());
            std::cout<<"indexed_files="<<index.paths.size()<<" index_ms="<<(loaded-start)/1000.0<<" quick_open_ms="<<(adopted-loaded)/1000.0<<'\n';
        }
        window->Quit();std::cout<<"Passed "<<checks<<" native editor and background-worker checks.\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
