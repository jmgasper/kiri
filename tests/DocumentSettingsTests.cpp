#include "core/DocumentSettings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace kiri;
namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x);}while(0)
static void Write(const fs::path& path,const std::string& text) {fs::create_directories(path.parent_path());std::ofstream(path)<<text;}
int main(int argc,char** argv) {
    if(argc==3 && std::string(argv[1])=="--resolve") {auto config=ReadDocumentConfig(argv[2]);for(auto& [key,value]:config.properties) std::cout<<key<<'='<<value.value<<'\n';return 0;}
    if(argc==4 && std::string(argv[1])=="--config") {std::ifstream input(argv[2]);std::string text((std::istreambuf_iterator<char>(input)),{});DocumentConfig config;bool root=false;ReadConfigText(config,text,argv[2],argv[3],root);for(auto& [key,value]:config.properties) std::cout<<key<<'='<<value.value<<'\n';return 0;}
    fs::path root=fs::temp_directory_path()/("kiri-document-settings-"+std::to_string(getpid()));
    try {
        struct Case {const char* glob;const char* path;bool match;};
        for(auto& test:{Case{"*.js","a/b/file.js",true},{"*.js","a.js/map",false},{"/Makefile","Makefile",true},{"/Makefile","sub/Makefile",false},{"lib/*.js","lib/deep/app.js",false},{"lib/**/test?.{js,ts}","lib/test1.js",true},{"lib/**/test?.{js,ts}","lib/a/b/test2.ts",true},{"file?.txt","file😀.txt",true},{"[ab].txt","c.txt",false},{"[!ab].txt","c.txt",true},{"[a-c].txt","b.txt",true},{"file\\?.txt","file?.txt",true},{"{src,{lib,test}}/*.cpp","test/main.cpp",true},{"file{-2..4}.cpp","file-1.cpp",true},{"file{-2..4}.cpp","file5.cpp",false},{"{one}.txt","{one}.txt",true},{"{one}.txt","one.txt",false},{"a{,b}.txt","a.txt",true},{"*",".hidden",true},{"src/","src/file",false},{"*.c","file.c\n",false},{"a[\\]b].txt","a].txt",true}}) {
            std::string error;auto matched=MatchConfigGlob(test.glob,test.path,error);CHECK(error.empty());CHECK(matched==test.match);
        }
        std::string error;CHECK(!MatchConfigGlob(std::string(4097,'*'),"a",error) && !error.empty());
        CHECK(MatchConfigGlob("file{{1..3},{8..10}}.cpp","file9.cpp",error));CHECK(MatchConfigGlob("file{{1..3},9}.cpp","file9.cpp",error));
        CHECK(!MatchConfigGlob("file{1..99}.cpp","file060.cpp",error));
        std::vector<int> columns{40};CHECK(ParseGuideColumns("100, 80, 80",columns,error));CHECK((columns==std::vector<int>{80,100}));CHECK(GuideColumnsText(columns)=="80, 100");
        CHECK(ParseGuideColumns(" OFF ",columns,error) && columns.empty());CHECK(!ParseGuideColumns("off 80",columns,error));
        for(const char* input:{"0","-1","1001","80x","1,2,3,4,5,6,7,8,9","80;100"}) CHECK(!ParseGuideColumns(input,columns,error));
        CHECK(ParseGuideColumns("",columns,error) && columns.empty());
        Write(root/".editorconfig","root=true\n[*]\nindent_size=7\n");
        Write(root/"project/.editorconfig","ROOT = TrUe\n[*]\nindent_style = space\nindent_size = 4\nmax_line_length=100\n[*.{js,ts}]\nindent_size=2\n[Makefile]\nindent_style=tab\nindent_size=tab\ntab_width=8\n");
        Write(root/"project/nested/.editorconfig","[*]\nindent_size=3\n[*.js]\nmax_line_length=80\n[app.js]\ntab_width=6\n");
        auto project=(root/"project").string();Indentation defaults;
        auto config=ReadDocumentConfig(project+"/src/app.js");auto style=ResolveDocumentStyle(defaults,{},config);
        CHECK(!style.indentation.tabs && style.indentation.indentWidth==2 && style.indentation.tabWidth==2);CHECK((style.guides==std::vector<int>{100}));CHECK(style.warning.empty());CHECK(config.files.size()==2);
        CHECK(style.origin.find(".editorconfig:7")!=std::string::npos);
        style=ResolveDocumentStyle(defaults,{},ReadDocumentConfig(project+"/Makefile"));CHECK(style.indentation.tabs && style.indentation.tabWidth==8 && style.indentation.indentWidth==8);
        config=ReadDocumentConfig(project+"/nested/app.js");style=ResolveDocumentStyle(defaults,{},config);CHECK(style.indentation.indentWidth==3 && style.indentation.tabWidth==6);CHECK((style.guides==std::vector<int>{80}));
        DocumentOverrides overrides;overrides.indentation=Indentation{true,4,4};overrides.guides=std::vector<int>{};style=ResolveDocumentStyle(defaults,{120},config,overrides);
        CHECK(style.indentation.tabs && style.indentation.indentWidth==4 && style.guides.empty());CHECK(style.origin.find("Document override")!=std::string::npos);
        Write(root/"project/nested/.editorconfig","[*]\nindent_style=unset\nindent_size=unset\ntab_width=unset\nmax_line_length=unset\n");
        style=ResolveDocumentStyle(defaults,{120},ReadDocumentConfig(project+"/nested/app.js"));CHECK(style.indentation==defaults);CHECK((style.guides==std::vector<int>{120}));
        Write(root/"project/nested/.editorconfig","[*]\nindent_style = tabs\nindent_size = 2 # not a comment\ntab_width=99\nmax_line_length=off\ninvalid line\n[broken\nindent_size=2\n");
        style=ResolveDocumentStyle(defaults,{120},ReadDocumentConfig(project+"/nested/app.js"));CHECK(style.indentation==defaults && style.guides.empty());CHECK(style.warning.find("expected key = value")!=std::string::npos);CHECK(style.warning.find("invalid section")!=std::string::npos);CHECK(style.warning.find("ignored invalid indent_size")!=std::string::npos);
        bool isRoot=false;config={};ReadConfigText(config,"# comment\nroot=TRUE\n[*.py]\nINDENT_STYLE=TAB\nindent_size=tab\ntab_width=3\n",project+"/.editorconfig","src/test.py",isRoot);CHECK(isRoot);
        style=ResolveDocumentStyle(defaults,{},config);CHECK(style.indentation.tabs && style.indentation.indentWidth==3 && style.indentation.tabWidth==3);
        config.properties["indent_size"].value="4";style=ResolveDocumentStyle(defaults,{},config);CHECK(style.indentation.indentWidth==4 && style.indentation.tabWidth==3);
        config.properties.erase("indent_size");style=ResolveDocumentStyle(defaults,{},config);CHECK(style.indentation.indentWidth==3);
        config.properties["trim_trailing_whitespace"]={"true","config",1};style=ResolveDocumentStyle(defaults,{},config);CHECK(style.warning.empty() && style.notes.find("trim_trailing_whitespace is not applied")!=std::string::npos);
        Write(root/"project/nested/.editorconfig",std::string(1024*1024+1,' '));config=ReadDocumentConfig(project+"/nested/app.js");CHECK(!config.warnings.empty());
        std::atomic<bool> cancel{true};config=ReadDocumentConfig(project+"/nested/app.js",&cancel);CHECK(config.properties.empty());
        fs::remove_all(root);std::cout<<checks<<" document settings checks passed\n";return 0;
    }catch(const std::exception& exception) {std::cerr<<exception.what()<<'\n';fs::remove_all(root);return 1;}
}
