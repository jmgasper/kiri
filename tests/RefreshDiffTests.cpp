#include "core/Diff.h"
#include "core/Git.h"
#include "core/ProjectMonitor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace kiri;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x); } while(0)
static DiffSource Text(std::string text) { DiffSource source;source.text=std::move(text);source.size=source.text.size();source.path="example.cpp";source.label="Test";return source; }
static void Put(const std::string& path,const std::string& text) { std::ofstream file(path,std::ios::binary);file<<text;CHECK(bool(file)); }
static void ModelTests() {
    auto empty=CompareText(Text(""),Text(""));CHECK(empty.error.empty() && empty.hunks.empty());
    auto added=CompareText(Text(""),Text("one\ntwo\n"));CHECK(added.hunks.size()==1 && added.hunks[0].added==2 && added.hunks[0].removed==0);
    auto removed=CompareText(Text("one\ntwo\n"),Text(""));CHECK(removed.hunks.size()==1 && removed.hunks[0].removed==2);
    auto changed=CompareText(Text("same\r\nold\r\nend\r\n"),Text("same\r\nnew 😀\r\nextra\r\nend\r\n"));
    CHECK(changed.hunks.size()==1 && changed.hunks[0].before==1 && changed.hunks[0].added==2);
    CHECK(changed.rows.size()==4 && changed.rows[2].before==-1 && changed.rows[3].after==3);
    CHECK(changed.Unified().find("-old\n+new 😀\n+extra\n")!=std::string::npos);CHECK(DiffDescription(changed.before).find("CRLF")!=std::string::npos);
    auto final=CompareText(Text("same\n"),Text("same"));CHECK(final.hunks.size()==1);CHECK(final.Unified().find("\\ No newline at end of file")!=std::string::npos);
    CHECK(CompareText(Text(std::string("a\0b",3)),Text("text")).binary);CHECK(CompareText(Text("\xff"),Text("text")).binary);
    CHECK(!CompareText(Text(std::string(kDiffBytes+1,'x')),Text("")).error.empty());
    std::string lines;for(size_t i=0;i<=kDiffLines;++i) lines+="x\n";CHECK(!CompareText(Text(lines),Text("")).error.empty());
    std::atomic<bool> cancel{true};CHECK(CompareText(Text("one"),Text("two"),&cancel).error=="Comparison cancelled.");
    CHECK(!CompareText(Text("a\nb\nc\n"),Text("d\ne\nf\n"),nullptr,1).error.empty());
    auto middle=ChangedMiddle("x😀z","x😁z");CHECK(middle.prefix==1 && middle.beforeEnd==5 && middle.afterEnd==5);
    std::mt19937 random(72);
    for(int attempt=0;attempt<150;++attempt) {
        std::vector<int> a(random()%18),b(random()%18);std::string left,right;
        for(auto& item:a) { item=random()%5;left+=std::to_string(item)+"\n"; }for(auto& item:b) { item=random()%5;right+=std::to_string(item)+"\n"; }
        auto diff=CompareText(Text(left),Text(right));CHECK(diff.error.empty());
        size_t x=0,y=0;for(auto row:diff.rows) { if(row.before>=0) CHECK(size_t(row.before)==x++);if(row.after>=0) CHECK(size_t(row.after)==y++);if(row.hunk<0) CHECK(a[row.before]==b[row.after]); }
        CHECK(x==a.size() && y==b.size());
        std::vector<int> previous(b.size()+1),current(b.size()+1);for(size_t j=0;j<=b.size();++j) previous[j]=j;
        for(size_t i=0;i<a.size();++i) { current[0]=i+1;for(size_t j=0;j<b.size();++j) current[j+1]=a[i]==b[j]?previous[j]:std::min(previous[j+1],current[j])+1;previous=current; }
        size_t edits=0;for(auto h:diff.hunks) edits+=h.added+h.removed;CHECK(edits==size_t(previous.back()));
    }
}
static void GitTests(const std::string& root) {
    fs::create_directories(root);GitRepository git(root);CHECK(git.Run({"init","-q"}).ok());CHECK(git.Run({"config","user.name","Kiri Test"}).ok());CHECK(git.Run({"config","user.email","kiri@example.invalid"}).ok());
    auto run=[&](std::vector<std::string> command) { auto result=git.Run(command);if(!result.ok()) throw std::runtime_error(result.diagnostic());return result.output; };
    Put(root+"/example.txt","first\nold\n");run({"add","--","example.txt"});
    auto unborn=git.CompareFile({'A',' ',"example.txt",""},true);CHECK(!unborn.before.exists && unborn.after.text=="first\nold\n");
    run({"commit","-qm","Initial"});auto first=run({"rev-parse","HEAD"});first.pop_back();
    auto initialFiles=git.CommitFiles(first);CHECK(initialFiles.error.empty() && initialFiles.files.size()==1);
    auto initial=git.CompareCommitFile(initialFiles.files[0],first);CHECK(!initial.before.exists && initial.after.text=="first\nold\n");
    Put(root+"/example.txt","first\nchanged\nextra\n");auto status=run({"status","--porcelain=v1","-z"});
    auto work=git.CompareFile({' ','M',"example.txt",""},false);CHECK(work.before.text=="first\nold\n" && work.after.text=="first\nchanged\nextra\n");CHECK(work.after.label.find("disk")!=std::string::npos);
    CHECK(status==run({"status","--porcelain=v1","-z"}));run({"add","--","example.txt"});
    Put(root+"/example.txt","unsaved to index\n");auto staged=git.CompareFile({'M','M',"example.txt",""},true);CHECK(staged.before.text=="first\nold\n" && staged.after.text=="first\nchanged\nextra\n");
    run({"commit","-qm","Edit"});auto second=run({"rev-parse","HEAD"});second.pop_back();auto edited=git.CommitFiles(second);CHECK(edited.files.size()==1);
    auto historical=git.CompareCommitFile(edited.files[0],second);CHECK(historical.before.text=="first\nold\n" && historical.after.text=="first\nchanged\nextra\n");
    run({"checkout","--","example.txt"});run({"mv","--","example.txt","renamed file.txt"});
    auto rename=git.CompareFile({'R',' ',"renamed file.txt","example.txt"},true);CHECK(rename.error.empty() && rename.hunks.empty());
    run({"commit","-qm","Rename"});auto third=run({"rev-parse","HEAD"});third.pop_back();auto renamed=git.CommitFiles(third);CHECK(renamed.files.size()==1 && renamed.files[0].originalPath=="example.txt");
    CHECK(git.CompareCommitFile(renamed.files[0],third).hunks.empty());
    fs::remove(root+"/renamed file.txt");auto deleted=git.CompareFile({' ','D',"renamed file.txt",""},false);CHECK(!deleted.after.exists && deleted.hunks.size()==1);CHECK(IndexProject(root).paths.empty());
    run({"add","-u"});auto stagedDelete=git.CompareFile({'D',' ',"renamed file.txt",""},true);CHECK(!stagedDelete.after.exists && stagedDelete.before.exists);
    run({"commit","-qm","Delete"});auto fourth=run({"rev-parse","HEAD"});fourth.pop_back();auto deletion=git.CommitFiles(fourth);CHECK(deletion.files.size()==1 && !git.CompareCommitFile(deletion.files[0],fourth).after.exists);
    Put(root+"/untracked.txt","a new file\n");auto untracked=git.CompareFile({'?','?',"untracked.txt",""},false);CHECK(!untracked.before.exists && untracked.after.text=="a new file\n");
    Put(root+"/binary.bin",std::string("a\0b",3));CHECK(git.CompareFile({'?','?',"binary.bin",""},false).binary);
    CHECK(!git.CommitFiles("--output=bad").error.empty());CHECK(!fs::exists(root+"/bad"));
}
static void MonitorTests(const std::string& root) {
    fs::create_directories(root+"/nested");Put(root+"/nested/one.txt","first");ProjectMonitor monitor(root);
    for(int i=0;i<4;++i) monitor.Poll();CHECK(!monitor.Poll().contents);
    Put(root+"/nested/one.txt","second!");auto modified=monitor.Poll();CHECK(modified.contents && modified.changedDirectories.empty());
    Put(root+"/nested/two.txt","new");CHECK(!monitor.Poll().changedDirectories.empty());
    fs::rename(root+"/nested/two.txt",root+"/nested/renamed.txt");CHECK(!monitor.Poll().changedDirectories.empty());
    fs::remove(root+"/nested/renamed.txt");CHECK(!monitor.Poll().changedDirectories.empty());
    fs::create_directories(root+"/created/deep");Put(root+"/created/deep/inside.txt","inside");
    auto change=monitor.Poll();CHECK(!change.changedDirectories.empty());auto next=monitor.Poll();CHECK(std::find(next.directories.begin(),next.directories.end(),root+"/created/deep")!=next.directories.end());
    fs::create_directory_symlink(root,root+"/cycle");auto symlinks=monitor.Poll();CHECK(std::find(symlinks.directories.begin(),symlinks.directories.end(),root+"/cycle")==symlinks.directories.end());
    std::atomic<bool> cancel{true};CHECK(monitor.Poll({},&cancel).changedDirectories.empty());
}
int main() {
    char temp[]="/tmp/kiri-refresh-diff-XXXXXX";auto* root=mkdtemp(temp);if(!root) return 1;
    try { ModelTests();GitTests(std::string(root)+"/git");MonitorTests(std::string(root)+"/monitor");fs::remove_all(root);std::cout<<"Passed "<<checks<<" refresh and diff checks.\n"; }
    catch(const std::exception& error) { std::cerr<<error.what()<<" (files: "<<root<<")\n";return 1; }
}
