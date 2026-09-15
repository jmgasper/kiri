#include "core/EditTransaction.h"
#include "core/Process.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __HAIKU__
#include <fs_attr.h>
#include <TypeConstants.h>
#include <fcntl.h>
#endif
using namespace kiri;
namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> static bool Throws(F f) { try { f();return false; }catch(const std::exception&) { return true; } }
struct Temp { std::string path;Temp() { char name[]="/tmp/kiri-search-XXXXXX";path=mkdtemp(name);path=CanonicalPath(path); }~Temp(){fs::remove_all(path);} };
static void Write(const std::string& path,const std::string& text) { std::ofstream f(path,std::ios::binary);f<<text; }
static Json Range(size_t line,size_t start,size_t end) { return {{"start",PositionJSON({line,start})},{"end",PositionJSON({line,end})}}; }
static void Queries() {
    SearchOptions options;options.regex=true;options.matchCase=true;options.query="(?<id>[\\p{L}_]+)=(\\d+)";
    TextQuery query(options);CHECK(query.Error().empty());std::string replacement="${id}:$2 $$ \\t",text="é=12 日本語=7\r\n";
    auto matches=query.Find(text,0,SIZE_MAX,100,nullptr,&replacement);CHECK(matches.error.empty());CHECK(matches.matches.size()==2);
    CHECK(ApplyTextEdits(text,matches.matches)=="é:12 $ \t 日本語:7 $ \t\r\n");CHECK(matches.matches[1].start==6);
    CHECK(!query.ValidateReplacement("$9").empty());CHECK(!query.ValidateReplacement("${missing}").empty());CHECK(!query.ValidateReplacement("\\q").empty());
    options.query="[";CHECK(!TextQuery(options).Error().empty());options.query="\\C";CHECK(!TextQuery(options).Error().empty());
    options.query="é";options.wholeWord=true;options.matchCase=false;
    matches=TextQuery(options).Find("É é élan aé é_ é😀");CHECK(matches.matches.size()==3);
    options.wholeWord=false;options.query=".";matches=TextQuery(options).Find("😀é\r\nx");CHECK(matches.matches.size()==3);CHECK(matches.matches[0].end==4);
    options.query="^|$";matches=TextQuery(options).Find("é\r\n😀\n");CHECK(matches.matches.size()==5);
    options.query="(?=.)";replacement="x";matches=TextQuery(options).Find("😀é",0,SIZE_MAX,100,nullptr,&replacement);CHECK(matches.matches.size()==2);CHECK(ApplyTextEdits("😀é",matches.matches)=="x😀xé");
    options.query="(é)";replacement="$1!";text="é é é";matches=TextQuery(options).Find(text,3,5,100,nullptr,&replacement);CHECK(ApplyTextEdits(text,matches.matches)=="é é! é");
    options.query="a";matches=TextQuery(options).Find("aaaa",0,SIZE_MAX,2);CHECK(matches.truncated);CHECK(matches.matches.size()==2);
    std::atomic<bool> cancel{true};CHECK(TextQuery(options).Find("aaaa",0,SIZE_MAX,20,&cancel).cancelled);
    options.regex=false;options.query="a$";replacement="$1\\n";matches=TextQuery(options).Find("a$",0,SIZE_MAX,20,nullptr,&replacement);CHECK(ApplyTextEdits("a$",matches.matches)==replacement);
    options.regex=true;options.query="(a+)+$";matches=TextQuery(options).Find(std::string(10000,'a')+"!");CHECK(!matches.error.empty());CHECK(matches.matches.empty());
}
static void ProjectQueries() {
    Temp temp;fs::create_directories(temp.path+"/src/generated");fs::create_directories(temp.path+"/tests");
    ProcessOptions process;process.directory=temp.path;CHECK(RunProcess({"git","init","-q"},process).ok());
    Write(temp.path+"/.gitignore","ignored.txt\n");Write(temp.path+"/ignored.txt","name\n");
    Write(temp.path+"/src/a.ts","😀 name name\r\nName names\n");Write(temp.path+"/src/generated/g.ts","name\n");Write(temp.path+"/tests/a.ts","name\n");
    Write(temp.path+"/src/binary",std::string("name\0name",9));Write(temp.path+"/src/encoding",std::string("name\xff",5));
    Write(temp.path+"/src/large","");fs::resize_file(temp.path+"/src/large",33*1024*1024);
    fs::create_symlink(temp.path+"/src/a.ts",temp.path+"/src/link");
    auto index=IndexProject(temp.path);CHECK(std::find(index.paths.begin(),index.paths.end(),"ignored.txt")==index.paths.end());
    auto all=IndexProject(temp.path,nullptr,500000,true);CHECK(std::find(all.paths.begin(),all.paths.end(),"ignored.txt")!=all.paths.end());
    ProjectSearchOptions options;options.query="name";options.folders="src";options.exclude="src/generated/*";options.wholeWord=true;
    auto result=SearchProject(temp.path,all,options);CHECK(result.matches.size()==3);CHECK(result.matches[0].column==3);CHECK(result.matches[1].column==8);
    CHECK(result.binary==2);CHECK(result.large==1);CHECK(result.symlinks==1);CHECK(result.filtered==4);
    options.include="**/*.ts";options.matchCase=true;options.regex=true;options.query="n(a)me";
    result=SearchProject(temp.path,all,options);CHECK(result.matches.size()==2);CHECK(result.matches[0].start==5);CHECK(result.matches[1].start==10);
    options.query="name.*Name";CHECK(SearchProject(temp.path,all,options).matches.empty());
    options.query="name";result=SearchProject(temp.path,all,options,nullptr,1);CHECK(result.truncated);CHECK(result.matches.size()==1);
    options.query="[";CHECK(!SearchProject(temp.path,all,options).error.empty());
    options.include="src/**/*.ts";options.query="name";CHECK(SearchProject(temp.path,all,options).matches.size()==2);options.include="**/*.ts";
    options.query="name";options.folders="src;tests";CHECK(SearchProject(temp.path,all,options).matches.size()==3);
    index.paths.push_back("src/unavailable");options.include="";result=SearchProject(temp.path,index,options);CHECK(result.unreadable==1);
}
static void Transactions() {
    Temp temp;auto a=temp.path+"/a.ts",b=temp.path+"/b.ts";Write(a,"\xef\xbb\xbf" "old old\r\n");Write(b,"old\n");chmod(a.c_str(),0640);
#ifdef __HAIKU__
    int fd=open(a.c_str(),O_RDWR);CHECK(fs_write_attr(fd,"Kiri:test",B_STRING_TYPE,0,"metadata",8)==8);close(fd);
#endif
    SnapshotMap open;auto snapshot=DiskSnapshot(b);snapshot.text="old dirty old\n";snapshot.open=true;snapshot.document=1;snapshot.revision=8;open.emplace(b,snapshot);
    ProjectIndex index;index.paths={"a.ts","b.ts"};ProjectSearchOptions options;options.query="old";
    auto plan=ReplacementPlan(temp.path,index,options,"new",open);CHECK(plan.files.size()==2);
    auto& disk=plan.files[0];auto& buffer=plan.files[1];CHECK(disk.before.bom);CHECK(buffer.before.text=="old dirty old\n");
    disk.edits[1].selected=false;disk.Prepare();CHECK(disk.after=="new old\r\n");CHECK(buffer.after=="new dirty new\n");
    plan.journal=temp.path+"/journal.json";CHECK(WriteEditJournal(plan).empty());disk.pending=true;CHECK(WriteEditJournal(plan).empty());
    CHECK(ApplyDiskEdit(disk).empty());CHECK(ReadFile(a).bytes=="\xef\xbb\xbf" "new old\r\n");CHECK(ReadFile(a).bom);CHECK(ReadFile(b).bytes=="old\n");
    struct stat st{};stat(a.c_str(),&st);CHECK((st.st_mode&0777)==0640);
#ifdef __HAIKU__
    char metadata[8];fd=::open(a.c_str(),O_RDONLY);CHECK(fs_read_attr(fd,"Kiri:test",B_STRING_TYPE,0,metadata,8)==8);CHECK(std::string(metadata,8)=="metadata");close(fd);
#endif
    auto recovered=ReadEditJournal(plan.journal);CHECK(recovered.files[0].applied);CHECK(recovered.files[0].writtenStamp==StatFile(a));
    CHECK(ApplyDiskEdit(recovered.files[0],true).empty());CHECK(ReadFile(a).bytes=="\xef\xbb\xbf" "old old\r\n");CHECK(ReadFile(a).bom);
    auto current=buffer.before;current.revision++;CHECK(!ValidateEdit(buffer,current).empty());current=buffer.before;current.text+="newer";CHECK(!ValidateEdit(buffer,current).empty());
    auto fresh=ReplacementPlan(temp.path,index,options,"new",open);Write(a,"external\n");CHECK(!ApplyDiskEdit(fresh.files[0]).empty());CHECK(ReadFile(a).bytes=="external\n");
    Write(a,"old old\r\n");fresh=ReplacementPlan(temp.path,index,options,"new",open);CHECK(ApplyDiskEdit(fresh.files[0]).empty());Write(a,"newer\n");CHECK(!ApplyDiskEdit(fresh.files[0],true).empty());CHECK(ReadFile(a).bytes=="newer\n");
}
static void WorkspaceEdits() {
    Temp temp;auto path=temp.path+"/日本語 #%.ts";Write(path,"😀 old\n");auto snapshot=DiskSnapshot(path);snapshot.open=true;snapshot.version=3;
    CHECK(PathFromURI(FileURI(path))==path);CHECK(Throws([]{PathFromURI("https://example.com/a.ts");}));CHECK(Throws([]{PathFromURI("file:///tmp/%00x");}));
    CHECK(!ValidateRenameName("class","typescript").empty());CHECK(!ValidateRenameName("namespace","cpp").empty());
    CHECK(ValidateRenameName("newName").empty());CHECK(ValidateRenameName("日本語").empty());CHECK(!ValidateRenameName("x y").empty());CHECK(!ValidateRenameName("1x").empty());
    Json change={{"textDocument",{{"uri",FileURI(path)},{"version",3}}},{"edits",Json::array({{{"range",Range(0,3,6)},{"newText","fresh"}}})}};
    Json edit={{"documentChanges",Json::array({change})}};
    auto resolver=[&](const std::string& requested){CHECK(requested==path);return snapshot;};
    auto plan=WorkspaceEditPlan(edit,PositionEncoding::UTF16,resolver);CHECK(plan.files.size()==1);CHECK(plan.files[0].after=="😀 fresh\n");
    edit["documentChanges"][0]["textDocument"]["version"]=2;CHECK(Throws([&]{WorkspaceEditPlan(edit,PositionEncoding::UTF16,resolver);}));
    edit["documentChanges"][0]=change;edit["documentChanges"].push_back({{"kind","rename"},{"oldUri",FileURI(path)},{"newUri",FileURI(path+"x")}});
    int resolved=0;CHECK(Throws([&]{WorkspaceEditPlan(edit,PositionEncoding::UTF16,[&](const std::string&){++resolved;return snapshot;});}));CHECK(resolved==0);
    edit={{"changes",{{FileURI(path),change["edits"]}}}};CHECK(WorkspaceEditPlan(edit,PositionEncoding::UTF16,resolver).files[0].after=="😀 fresh\n");
    edit["changes"][FileURI(path)][0]["range"]=Range(0,1,6);CHECK(Throws([&]{WorkspaceEditPlan(edit,PositionEncoding::UTF16,resolver);}));
    edit["changes"][FileURI(path)][0]["range"]=Range(0,3,99);CHECK(Throws([&]{WorkspaceEditPlan(edit,PositionEncoding::UTF16,resolver);}));
    edit["changes"][FileURI(path)]=Json::array({change["edits"][0],change["edits"][0]});CHECK(Throws([&]{WorkspaceEditPlan(edit,PositionEncoding::UTF16,resolver);}));
}
int main() { try { Queries();ProjectQueries();Transactions();WorkspaceEdits();std::cout<<"Passed "<<checks<<" search, replacement and rename checks\n";return 0; }catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
