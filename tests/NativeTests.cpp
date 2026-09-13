#include "ui/Editor.h"
#include "ui/Async.h"
#include "core/FileIO.h"
#include "core/Project.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <Window.h>
#include <OS.h>
#include <SciLexer.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <fs_attr.h>
using namespace kiri;
namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
static uint64_t Resident() { ssize_t cookie=0;area_info area{};uint64_t total=0;while(get_next_area_info(getpid(),&cookie,&area)==B_OK) total+=area.ram_size;return total; }
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
        for(const auto& theme:Theme::Builtins()) { editor->ApplyTheme(theme);CHECK(editor->SendMessage(SCI_STYLEGETBACK,STYLE_DEFAULT)==SciColor(theme.background)); }
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
