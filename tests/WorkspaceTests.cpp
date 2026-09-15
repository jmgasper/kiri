// Real Haiku windows and Scintilla buffers, with isolated files and settings.
#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/TabStrip.h"
#include "ui/TerminalPanel.h"
#include "ui/DiffView.h"
#include "ui/GitView.h"
#include "ui/Explorer.h"
#include "ui/ProblemsView.h"
#include "ui/PreferencesWindow.h"
#include <MenuField.h>
#include <MenuItem.h>
#include <CheckBox.h>
#include <Entry.h>
#include <Application.h>
#include <Button.h>
#include <CardLayout.h>
#include <File.h>
#include <ListView.h>
#include <LayoutBuilder.h>
#include <OS.h>
#include <SplitView.h>
#include <TextControl.h>
#include <TextView.h>
#include <StringView.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs=std::filesystem;
static int checks=0;
#define CHECK(x) do { ++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x); } while(0)
namespace kiri {
static int LanguageFixture() {
    RpcFramer framer;std::map<std::string,std::string> documents;char bytes[4096];
    while(auto count=read(STDIN_FILENO,bytes,sizeof(bytes))) {
        if(count<0) return 1;
        for(auto& message:framer.Feed(std::string_view(bytes,count))) {
            auto method=message.value("method",std::string());auto params=message.value("params",Json::object());Json result=nullptr;
            if(method=="exit") return 0;
            if(method=="initialize") result={{"capabilities",{{"textDocumentSync",1},{"documentSymbolProvider",true},{"completionProvider",Json::object()},{"renameProvider",{{"prepareProvider",true}}}}}};
            else if(method=="textDocument/didOpen") documents[params["textDocument"]["uri"]]=params["textDocument"]["text"];
            else if(method=="textDocument/didChange") documents[params["textDocument"]["uri"]]=params["contentChanges"][0]["text"];
            else if(method=="textDocument/didClose") documents.erase(params["textDocument"]["uri"]);
            else if(method=="textDocument/documentSymbol") {
                auto uri=params["textDocument"]["uri"].get<std::string>();auto text=documents.at(uri);
                Json range={{"start",PositionJSON({0,0})},{"end",PositionJSON(PositionAt(text,text.size()))}};
                auto name=text.find("externalSymbol")!=std::string::npos?"externalSymbol":fs::path(uri).filename().string();
                result=Json::array({{{"name",name},{"kind",13},{"range",range},{"selectionRange",range}}});
            } else if(method=="textDocument/prepareRename") {
                auto text=documents.at(params["textDocument"]["uri"]);auto start=text.find("old");
                if(start!=std::string::npos) result={{"range",{{"start",PositionJSON(PositionAt(text,start))},{"end",PositionJSON(PositionAt(text,start+3))}}},{"placeholder","old"}};
            } else if(method=="textDocument/rename") {
                auto file=PathFromURI(params["textDocument"]["uri"]);result={{"changes",Json::object()}};
                for(auto path:{file,(fs::path(file).parent_path()/"closed.ts").string()}) {
                    auto uri=FileURI(path);auto text=documents.count(uri)?documents[uri]:DiskSnapshot(path).text;
                    SearchOptions options;options.query="old";options.wholeWord=true;auto matches=TextQuery(options).Find(text);
                    auto edits=Json::array();for(auto& match:matches.matches) edits.push_back({{"range",{{"start",PositionJSON(PositionAt(text,match.start))},{"end",PositionJSON(PositionAt(text,match.end))}}},{"newText",params["newName"]}});
                    result["changes"][uri]=edits;
                }
            } else if(method=="textDocument/completion") {
                snooze(120000);auto uri=params["textDocument"]["uri"].get<std::string>();
                result=Json::array({{{"label","name"},{"insertText","name"},{"detail",fs::path(uri).filename().string()}}});
            }
            if(message.contains("id")) {
                auto wire=RpcFramer::Frame({{"jsonrpc","2.0"},{"id",message["id"]},{"result",result}});
                size_t at=0;while(at<wire.size()) { auto written=write(STDOUT_FILENO,wire.data()+at,wire.size()-at);if(written<=0) return 1;at+=written; }
            }
        }
    }return 0;
}
static int AnalysisFixture(const std::string& mode) {
    RpcFramer framer;std::map<std::string,Json> documents;char bytes[8192];
    auto send=[](const Json& message) {auto wire=RpcFramer::Frame(message);size_t at=0;while(at<wire.size()) {auto count=write(STDOUT_FILENO,wire.data()+at,wire.size()-at);if(count<=0) _exit(2);at+=count;}};
    auto publish=[&](const Json& document) {
        auto text=document["text"].get<std::string>();Json rows=Json::array();auto at=text.find("bad");
        if(at!=std::string::npos) rows.push_back({{"range",{{"start",PositionJSON(PositionAt(text,at))},{"end",PositionJSON(PositionAt(text,at+3))}}},{"severity",1},{"message","Deliberate bad value 日本語"},{"source","fixture"},{"code",26}});
        Json params={{"uri",document["uri"]},{"diagnostics",rows}};if(mode!="unversioned") params["version"]=document["version"];
        send({{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},{"params",params}});
    };
    while(auto count=read(STDIN_FILENO,bytes,sizeof(bytes))) {
        if(count<0) return 1;
        for(auto& message:framer.Feed(std::string_view(bytes,count))) {
            auto method=message.value("method",std::string());auto params=message.value("params",Json::object());Json result=nullptr;
            if(method=="exit") return 0;
            if(method=="initialize") {
                result={{"capabilities",{{"textDocumentSync",mode=="no-changes"?0:1},{"documentSymbolProvider",true},{"semanticTokensProvider",{{"full",true},{"legend",{{"tokenTypes",Json::array({"type","parameter","property"})},{"tokenModifiers",Json::array()}}}}}}}};
                auto caps=params["capabilities"]["textDocument"];
                if(caps["publishDiagnostics"]["versionSupport"]!=true || caps["semanticTokens"]["requests"]["full"]!=true) return 3;
            }else if(method=="textDocument/didOpen") {auto doc=params["textDocument"];documents[doc["uri"]]=doc;publish(doc);}
            else if(method=="textDocument/didChange") {auto& doc=documents[params["textDocument"]["uri"]];doc["text"]=params["contentChanges"][0]["text"];doc["version"]=params["textDocument"]["version"];publish(doc);}
            else if(method=="textDocument/didClose") documents.erase(params["textDocument"]["uri"]);
            else if(method=="textDocument/documentSymbol") result=Json::array();
            else if(method=="textDocument/semanticTokens/full") {
                auto text=documents.at(params["textDocument"]["uri"]).at("text").get<std::string>();std::vector<std::pair<size_t,int>> spans;
                for(auto word:{std::pair<const char*,int>{"Widget",0},{"parameter",1},{"property",2}}) for(size_t at=text.find(word.first);at!=std::string::npos;at=text.find(word.first,at+1)) spans.push_back({at,word.second});
                std::sort(spans.begin(),spans.end());Json data=Json::array();TextPosition previous{0,0};
                for(auto span:spans) {auto position=PositionAt(text,span.first);size_t length=span.second==0?6:span.second==1?9:8;for(auto value:{position.line-previous.line,position.line==previous.line?position.character-previous.character:position.character,length,size_t(span.second),size_t(0)}) data.push_back(value);previous=position;}
                snooze(100000);result={{"data",data}};
            }
            if(message.contains("id")) send({{"jsonrpc","2.0"},{"id",message["id"]},{"result",result}});
        }
    }return 0;
}
struct WorkspaceTestAccess {
    static void Wait(Workspace& w,const std::function<bool()>& ready) {
        auto until=system_time()+15000000;
        do { w.Unlock();snooze(30000);if(!w.Lock()) throw std::runtime_error("Window closed while waiting");if(ready()) return; } while(system_time()<until);
        auto* d=w.Current();throw std::runtime_error("Timed out after check "+std::to_string(checks)+"; "+(d?d->path+" · "+d->languageStatus+" · "+d->diagnosticStatus+" · "+d->semanticStatus+"; version "+std::to_string(d->serverVersion)+" revision "+std::to_string(d->serverRevision)+"/"+std::to_string(d->editor?d->editor->InputRevision():-1):"no document")+"; Problems collapsed="+std::to_string(w.fTerminalSplit->IsItemCollapsed(2))+" hidden="+std::to_string(w.fProblems->IsHidden()));
    }
    static void Open(Workspace& w,const std::string& path,bool preview=false) {
        w.OpenFile(path,1,1,true,preview);Wait(w,[&]{return w.fPendingOpen.empty();});
        if(!w.Current() || w.Current()->path!=path) throw std::runtime_error("Open expected "+path+", got "+(w.Current()?w.Current()->path:"no document"));
        CHECK(w.Current() && w.Current()->path==path);
    }
    static void Send(Workspace& w,uint32 command) { BMessage message(command);w.MessageReceived(&message); }
    static void Save(Workspace& w) {
        auto* d=w.Current();w.Save(d);Wait(w,[&]{return !d->saving;});CHECK(!d->editor->Dirty());
    }
    static void CheckCards(Workspace& w) {
        for(auto& pane:w.fPanes) {
            CHECK(pane->cards->CountItems()==int(pane->tabs.size())+1);
            CHECK(pane->cards->VisibleIndex()==pane->selected+1);
            for(size_t i=0;i<pane->tabs.size();++i) {
                CHECK(pane->cards->ItemAt(i+1)->View()==pane->tabs[i]->view);
                CHECK(pane->tabs[i]->view->Window()==&w);
            }
        }
    }
    static void Analysis(const std::string& base,const std::string& executable) {
        for(const std::string mode:{"versioned","unversioned","no-changes"}) {
            auto root=base+"/analysis-"+mode;fs::create_directories(root);auto file=root+"/sample.ts",other=root+"/other.ts";
            const std::string original="// 😀 日本語\nWidget parameter property; bad\n";std::ofstream(file)<<original;std::ofstream(other)<<"Widget property;\n";
            auto* w=new Workspace(root+"/settings",false);w->Show();CHECK(w->Lock());
            w->fLanguageTools.profiles={{"Analysis fixture","typescript","'"+executable+"' --analysis-fixture "+mode,{".ts"}}};w->fLanguageTools.completion=false;
            w->OpenProject(root);Open(*w,file);auto id=w->Current()->id;auto* d=w->ByID(id);auto* editor=d->editor;
            Wait(*w,[&]{return d->diagnostics.size()==1 && d->semanticTokens.size()==3;});auto at=original.find("bad");auto key=d->serverKey;auto generation=w->fLanguageGeneration;
            CHECK(d->diagnostics[0].start==at && d->diagnostics[0].end==at+3);CHECK(editor->SendMessage(SCI_INDICATORVALUEAT,21,at));
            CHECK(d->semanticTokens[0].role==SemanticRole::Type && d->semanticTokens[1].role==SemanticRole::Parameter && d->semanticTokens[2].role==SemanticRole::Property);
            CHECK(!editor->Dirty());Send(*w,kToggleProblems);Wait(*w,[&]{return !w->fTerminalSplit->IsItemCollapsed(2);});CHECK(w->fProblems->Entries().size()==1);
            Send(*w,kNextProblem);CHECK(editor->SendMessage(SCI_GETSELECTIONSTART)==int(at));CHECK(editor->SendMessage(SCI_CALLTIPACTIVE));
            auto input=editor->InputRevision();editor->SendMessage(SCI_CHARRIGHT);CHECK(editor->InputRevision()==input);CHECK(editor->SendMessage(SCI_INDICATORVALUEAT,21,at));
            w->SplitPane(B_HORIZONTAL);auto* mirror=w->CurrentTab()->editor;CHECK(mirror->SendMessage(SCI_INDICATORVALUEAT,21,at));
            auto version=d->serverVersion;Json report={{"uri",d->serverURI},{"version",version},{"diagnostics",Json::array({{{"range",{{"start",PositionJSON(PositionAt(original,at))},{"end",PositionJSON(PositionAt(original,at+3))}}},{"message","Obsolete report"}}})}};
            w->RequestSemantic(*d,*w->fServers.at(key).client);editor->SendMessage(SCI_INSERTTEXT,0,reinterpret_cast<sptr_t>("// inserted 😀\n"));
            CHECK(!editor->SendMessage(SCI_INDICATORVALUEAT,21,at));CHECK(!mirror->SendMessage(SCI_INDICATORVALUEAT,9,original.find("Widget")));
            w->AnalysisTick();auto serial=d->diagnosticSerial;w->LanguageNotification(key,generation,"textDocument/publishDiagnostics",report);CHECK(d->diagnosticSerial==serial && d->diagnostics.empty());
            if(mode=="no-changes") {
                Wait(*w,[&]{return d->diagnosticStatus.find("does not accept")!=std::string::npos;});CHECK(d->semanticTokens.empty());CHECK(editor->Dirty());w->Quit();continue;
            }
            Wait(*w,[&]{return d->diagnostics.size()==1 && d->semanticTokens.size()==3 && d->serverVersion>version;});
            CHECK(d->diagnostics[0].start==editor->Text().find("bad"));CHECK(d->semanticTokens[0].start==editor->Text().find("Widget"));
            report["version"]=d->serverVersion;editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));editor->SendMessage(SCI_UNDO);
            w->AnalysisTick();serial=d->diagnosticSerial;w->LanguageNotification(key,generation,"textDocument/publishDiagnostics",report);CHECK(d->diagnosticSerial==serial && d->diagnostics.empty());
            Wait(*w,[&]{return d->diagnostics.size()==1 && d->semanticVersion==d->serverVersion;});
            at=editor->Text().find("bad");editor->ApplyEdits({{at,at+3,"good"}});
            Wait(*w,[&]{return d->diagnosticStatus=="Diagnostics current" && d->diagnostics.empty() && d->semanticVersion==d->serverVersion;});CHECK(w->fProblems->Entries().empty());
            BMessage preferences(kApplyPreferences);auto settings=w->fEditorSettings;settings.semanticHighlighting=false;settings.WriteTo(preferences);w->MessageReceived(&preferences);w->AnalysisTick();CHECK(d->semanticTokens.empty());CHECK(!editor->SendMessage(SCI_INDICATORVALUEAT,9,editor->Text().find("Widget")));
            preferences.MakeEmpty();preferences.what=kApplyPreferences;settings.semanticHighlighting=true;settings.WriteTo(preferences);w->MessageReceived(&preferences);Wait(*w,[&]{return d->semanticTokens.size()==3;});
            editor->SendMessage(SCI_UNDO);Wait(*w,[&]{return d->diagnostics.size()==1;});
            Open(*w,other);auto otherID=w->Current()->id;Wait(*w,[&]{return w->ByID(otherID)->semanticTokens.size()==2 && d->diagnostics.size()==1;});
            if(mode=="unversioned") {
                auto snapshot=w->fServers.at(key).snapshotSerial;CHECK(w->DiagnosticSnapshotCurrent(key,snapshot));
                auto* sibling=w->ByID(otherID)->editor;sibling->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>(" "));
                serial=d->diagnosticSerial;report.erase("version");w->AcceptDiagnostics(key,generation,report,snapshot);CHECK(d->diagnosticSerial==serial);CHECK(!w->DiagnosticSnapshotCurrent(key,snapshot));
                w->AnalysisTick();CHECK(d->diagnostics.empty());Wait(*w,[&]{return d->diagnostics.size()==1;});
            }
            version=d->serverVersion;auto oldID=id;auto staleGeneration=w->fLanguageGeneration;w->ResetLanguages();CHECK(d->diagnostics.empty() && d->semanticTokens.empty());
            w->LanguageNotification(key,staleGeneration,"textDocument/publishDiagnostics",report);CHECK(d->diagnostics.empty());Wait(*w,[&]{return d->diagnostics.size()==1 && d->semanticTokens.size()==3;});CHECK(d->serverVersion>version);
            version=d->serverVersion;std::vector<int64> views;for(auto& pane:w->fPanes) for(auto& tab:pane->tabs) if(tab->document==id) views.push_back(tab->id);editor->MarkSaved();for(auto view:views) CHECK(w->CloseView(view));CHECK(!w->ByID(oldID));
            Open(*w,file);d=w->Current();Wait(*w,[&]{return d->diagnostics.size()==1 && d->semanticTokens.size()==3;});CHECK(d->id!=oldID && d->serverVersion>version);
            report["version"]=version;serial=d->diagnosticSerial;w->LanguageNotification(d->serverKey,w->fLanguageGeneration,"textDocument/publishDiagnostics",report);CHECK(d->diagnosticSerial==serial);
            // Crossing the language limit clears overlays while keeping editable text.
            auto large=root+"/large.ts";std::ofstream(large)<<std::string(kLanguageBytes+1,' ');Open(*w,large);CHECK(w->Current()->languageStatus.find("8 MiB")!=std::string::npos);CHECK(w->Current()->serverKey.empty());
            w->Current()->editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));CHECK(w->Current()->editor->Dirty());w->Quit();
        }
        auto root=base+"/analysis-unavailable";fs::create_directories(root);auto file=root+"/sample.ts";std::ofstream(file)<<"const value = 1;\n";
        auto* w=new Workspace(root+"/settings",false);w->Show();CHECK(w->Lock());w->fLanguageTools.profiles={{"Missing","typescript",root+"/missing-server",{".ts"}}};w->OpenProject(root);Open(*w,file);
        Wait(*w,[&]{return w->Current()->languageStatus.find("Install")!=std::string::npos;});CHECK(w->Current()->diagnostics.empty());auto* editor=w->Current()->editor;editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));CHECK(editor->Dirty());
        w->fLanguageTools.profiles={{"No analysis","typescript","'"+executable+"' --language-fixture",{".ts"}}};w->ResetLanguages();
        Wait(*w,[&]{return w->Current()->semanticStatus.find("no full semantic")!=std::string::npos && w->Current()->diagnosticStatus.find("No diagnostic report")!=std::string::npos;});CHECK(editor->Dirty() && w->Current()->diagnostics.empty());w->Quit();
        std::cout<<"Versioned, unversioned and unsupported-change analysis lifecycles passed.\n";
    }
    static void RealAnalysis(const std::string& base,const std::string& installed) {
        auto root=base+"/real-analysis",settings=root+"/settings";fs::create_directories(settings);fs::create_directory_symlink(installed+"/tools",settings+"/tools");
        std::ofstream(root+"/tsconfig.json")<<R"({"compilerOptions":{"target":"ES2022","strict":true},"include":["*.ts"]})";
        for(const std::string extension:{".ts",".cpp"}) {
            auto file=root+"/example"+extension;
            std::string source=extension==".ts"?"const face = '😀';\nclass Widget { property = 1; }\nfunction render(value: Widget, parameter: number) { return value.property + parameter; }\nlet broken: number = 'wrong';\n":
                "const char* face = \"😀\";\nstruct Widget { int property; };\nint render(Widget value, int parameter) { return value.property + parameter; }\nint broken = \"wrong\";\n";
            std::ofstream(file)<<source;auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());w->fLanguageTools.completion=false;w->OpenProject(root);Open(*w,file);auto* d=w->Current();auto* editor=d->editor;
            auto ready=[&] {return std::any_of(d->diagnostics.begin(),d->diagnostics.end(),[](const auto& item){return item.severity==1;}) && !d->semanticTokens.empty();};
            auto deadline=system_time()+45000000;while(!ready() && system_time()<deadline) {w->Unlock();snooze(100000);CHECK(w->Lock());}
            if(!ready()) throw std::runtime_error(extension+" real analysis: "+d->languageStatus+" / "+d->diagnosticStatus+" / "+d->semanticStatus);
            for(auto role:{SemanticRole::Type,SemanticRole::Parameter,SemanticRole::Property}) CHECK(std::any_of(d->semanticTokens.begin(),d->semanticTokens.end(),[&](const auto& token){return token.role==role;}));
            auto error=*std::find_if(d->diagnostics.begin(),d->diagnostics.end(),[](const auto& item){return item.severity==1;});CHECK(error.line==3);CHECK(editor->SendMessage(SCI_INDICATORVALUEAT,21,error.start));
            Send(*w,kToggleProblems);w->JumpProblem(d->id,d->diagnosticSerial,error.start);CHECK(editor->SendMessage(SCI_GETSELECTIONSTART)==int(error.start));CHECK(editor->SendMessage(SCI_CALLTIPACTIVE));
            auto key=d->serverKey;auto& entry=w->fServers.at(key);std::cout<<extension<<": "<<d->diagnostics.size()<<" problems, "<<d->semanticTokens.size()<<" semantic tokens, "<<(entry.unversionedDiagnostics?"immutable snapshots":"versioned reports")<<"; "<<error.message<<"\n";
            auto quoted=extension==".ts"?"'wrong'":"\"wrong\"";auto at=source.find(quoted);editor->ApplyEdits({{at,at+7,"1"}});
            Wait(*w,[&]{return d->diagnosticStatus=="Diagnostics current" && std::none_of(d->diagnostics.begin(),d->diagnostics.end(),[](const auto& item){return item.severity==1;}) && d->semanticVersion==d->serverVersion;});
            CHECK(!editor->SendMessage(SCI_INDICATORVALUEAT,21,error.start));CHECK(editor->Dirty());auto fixed=editor->Text();
            editor->SendMessage(SCI_INSERTTEXT,0,reinterpret_cast<sptr_t>("// inserted 日本語 😀\n"));CHECK(editor->State()->semanticTokens.empty());
            Wait(*w,[&]{return !d->semanticTokens.empty() && d->semanticVersion==d->serverVersion;});CHECK(d->semanticTokens.front().start>=std::string("// inserted 日本語 😀\n").size());
            auto other=root+"/notes.txt";std::ofstream(other)<<"Other tab\n";Open(*w,other);CHECK(editor->State()->semanticTokens.size()==d->semanticTokens.size());
            w->ResetLanguages();CHECK(d->semanticTokens.empty() && d->diagnostics.empty());Wait(*w,[&]{return !d->semanticTokens.empty() && d->diagnosticStatus=="Diagnostics current";});CHECK(std::none_of(d->diagnostics.begin(),d->diagnostics.end(),[](const auto& item){return item.severity==1;}));
            CHECK(editor->Text()=="// inserted 日本語 😀\n"+fixed);w->Quit();std::cout<<extension<<" error → jump → fix → clear; edit above, tab change and restart passed.\n";
        }
    }
    static void Themes(const std::string& base) {
        auto root=base+"/theme-project",settings=root+"/settings";fs::create_directories(root);auto file=root+"/sample.cpp";std::ofstream(file)<<"int original = 1;\n";
        auto visit=[](BMessenger target,const std::function<void(BWindow&)>& action) {
            CHECK(target.LockTargetWithTimeout(1000000)==B_OK);BLooper* looper=nullptr;target.Target(&looper);auto* window=dynamic_cast<BWindow*>(looper);CHECK(window);action(*window);looper->Unlock();
        };
        auto command=[](BWindow& window,uint32 what) {BMessage message(what);window.MessageReceived(&message);};
        std::vector<std::string> ids;
        for(int builtin:{0,1}) {
            auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());w->OpenProject(root);Open(*w,file);w->ApplyTheme(builtin);
            auto* editor=w->Current()->editor;editor->SendMessage(SCI_APPENDTEXT,9,reinterpret_cast<sptr_t>("// draft\n"));editor->SendMessage(SCI_SETSEL,4,12);auto text=editor->Text(),beforeID=w->fEditorSettings.themeID;
            auto revision=editor->InputRevision();auto previousBackground=editor->SendMessage(SCI_STYLEGETBACK,STYLE_DEFAULT);
            w->ShowPreferences();auto preferences=w->fPreferencesWindow;
            visit(preferences,[&](BWindow& window) {
                command(window,'thdu');auto* color=static_cast<BTextControl*>(window.FindView("theme color"));color->SetText(builtin?"#fafafa":"#101820");command(window,'thcl');
                auto* preview=static_cast<Editor*>(window.FindView("preferences preview"));CHECK(preview->SendMessage(SCI_STYLEGETBACK,STYLE_DEFAULT)==SciColor(builtin?rgb_color{250,250,250,255}:rgb_color{16,24,32,255}));
                CHECK(preview->State()->semanticTokens.size()>3);CHECK(!preview->State()->diagnostics.empty());
            });
            CHECK(editor->SendMessage(SCI_STYLEGETBACK,STYLE_DEFAULT)==previousBackground);preferences.SendMessage(B_QUIT_REQUESTED);Wait(*w,[&]{return !preferences.IsValid();});
            CHECK(w->fEditorSettings.themeID==beforeID && editor->Matches(text) && editor->InputRevision()==revision && editor->Dirty());CHECK(editor->SendMessage(SCI_GETANCHOR)==4 && editor->SendMessage(SCI_GETCURRENTPOS)==12);CHECK(LoadColorThemes(settings).size()==10+ids.size());
            w->Search(true);auto search=w->fSearchWindows.back();visit(search,[&](BWindow& window){static_cast<BTextControl*>(window.FindView("query"))->SetText("original");});
            w->ShowPreferences();preferences=w->fPreferencesWindow;
            visit(preferences,[&](BWindow& window) {
                command(window,'thdu');static_cast<BTextControl*>(window.FindView("theme name"))->SetText(builtin?"Morning custom":"Night custom");command(window,'thnm');
                static_cast<BTextControl*>(window.FindView("theme color"))->SetText(builtin?"#fafafa":"#101820");command(window,'thcl');command(window,'papl');
            });
            Wait(*w,[&]{return bool(w->fEditorSettings.customTheme);});ids.push_back(w->fEditorSettings.themeID);CHECK(w->fEditorSettings.Colors().dark==!builtin);CHECK(ResolveColorTheme(settings,ids.back()).theme==*w->fEditorSettings.customTheme);
            CHECK(editor->Matches(text) && editor->InputRevision()==revision && editor->Dirty());CHECK(editor->SendMessage(SCI_GETANCHOR)==4 && editor->SendMessage(SCI_GETCURRENTPOS)==12);
            Wait(*w,[&]{bool colored=false;visit(search,[&](BWindow& window){colored=window.FindView("search panel")->ViewColor()==w->fEditorSettings.Colors().panel;CHECK(std::string(static_cast<BTextControl*>(window.FindView("query"))->Text())=="original");});return colored;});
            visit(preferences,[&](BWindow& window) {
                entry_ref directory;CHECK(get_ref_for_path(root.c_str(),&directory)==B_OK);BMessage exportTheme('thwr');exportTheme.AddRef("directory",&directory);exportTheme.AddString("name","export.kiri-theme.json");window.MessageReceived(&exportTheme);
            });
            CHECK(ReadThemeFile(root+"/export.kiri-theme.json").theme==*w->fEditorSettings.customTheme);
            // Validation must preserve the applied palette on malformed import.
            std::ofstream(root+"/broken.json")<<"{broken";
            visit(preferences,[&](BWindow& window) {entry_ref ref;CHECK(get_ref_for_path((root+"/broken.json").c_str(),&ref)==B_OK);BMessage imported('thrd');imported.AddRef("refs",&ref);window.MessageReceived(&imported);CHECK(std::string(static_cast<BStringView*>(window.FindView("preferences validation"))->Text()).find("Cannot read theme")!=std::string::npos);});
            CHECK(w->fEditorSettings.themeID==ids.back());preferences.SendMessage(B_QUIT_REQUESTED);Wait(*w,[&]{return !preferences.IsValid();});search.SendMessage(B_QUIT_REQUESTED);
            editor->SendMessage(SCI_UNDO);CHECK(editor->Matches("int original = 1;\n") && !editor->Dirty());w->SaveSettings();CHECK(w->QuitRequested());w->Quit();
            w=new Workspace(settings,true);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring;});CHECK(w->fEditorSettings.themeID==ids.back());CHECK(w->fEditorSettings.customTheme && w->fEditorSettings.Colors().dark==!builtin);CHECK(w->Current()->editor->Matches("int original = 1;\n"));w->Quit();
        }
        // Incomplete import uses the specified base and does not alter disk until Apply.
        auto value=Json::parse(SerializeTheme(BuiltinColorThemes()[1]));value["id"]=NewThemeID();value["name"]="Incomplete";value["colors"]={{"accent","#a00060"}};std::ofstream(root+"/incomplete.json")<<value.dump();
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());w->ShowPreferences();auto preferences=w->fPreferencesWindow;
        visit(preferences,[&](BWindow& window){entry_ref ref;CHECK(get_ref_for_path((root+"/incomplete.json").c_str(),&ref)==B_OK);BMessage imported('thrd');imported.AddRef("refs",&ref);window.MessageReceived(&imported);CHECK(std::string(static_cast<BStringView*>(window.FindView("preferences validation"))->Text()).find("Missing colors")!=std::string::npos);command(window,'papl');});
        Wait(*w,[&]{return w->fEditorSettings.themeID==value["id"];});CHECK(w->fEditorSettings.customTheme->colors.at("background")==BuiltinColorThemes()[1].colors.at("background"));
        auto removed=w->fEditorSettings.themeID;preferences.SendMessage(B_QUIT_REQUESTED);Wait(*w,[&]{return !preferences.IsValid();});w->SaveSettings();w->Quit();fs::remove(settings+"/themes/"+removed+".json");
        w=new Workspace(settings,true);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring;});CHECK(w->fEditorSettings.themeID==removed && !w->fEditorSettings.customTheme);CHECK(w->fEditorSettings.Colors().definition.id=="kiri.obsidian" && !w->fEditorSettings.themeStatus.empty());w->Quit();
        std::cout<<"Custom dark/light theme preview, Cancel, Apply, export, import and restart passed.\n";
    }
    static void AnalysisRendering() {
        auto* window=new BWindow(BRect(40,40,1000,600),"Analysis rendering benchmark",B_TITLED_WINDOW,0);auto* editor=new Editor();BLayoutBuilder::Group<>(window,B_VERTICAL,0).Add(editor);window->Show();CHECK(window->Lock());
        std::string text;std::vector<SemanticToken> tokens;const std::string line="Type parameter property; // 😀 benchmark padding.........................................\n";
        while(text.size()+line.size()<=kLanguageBytes) {tokens.push_back({text.size(),text.size()+4,SemanticRole::Type,false});text+=line;}text.resize(kLanguageBytes,' ');
        editor->SetText(text);editor->SetLanguage("sample.cpp");auto start=system_time();editor->SetSemanticTokens(tokens);auto painted=system_time();
        CHECK(editor->SendMessage(SCI_INDICATORVALUEAT,9,tokens.back().start));CHECK(!editor->Dirty());window->Unlock();snooze(100000);CHECK(window->Lock());window->UpdateIfNeeded();auto realized=system_time();
        CHECK(editor->SendMessage(SCI_GETSTYLEAT,0)>=0);start=painted-start;
        auto cleared=system_time();editor->SendMessage(SCI_INSERTTEXT,0,reinterpret_cast<sptr_t>("x"));auto editTime=system_time()-cleared;
        CHECK(!editor->SendMessage(SCI_INDICATORVALUEAT,9,tokens.back().start));CHECK(editor->Dirty());CHECK(editor->SendMessage(SCI_GETLENGTH)==int(kLanguageBytes+1));CHECK(start<3000000 && editTime<1000000);
        std::cout<<"8 MiB native semantic indicators: "<<tokens.size()<<" tokens, apply="<<start/1000.0<<" ms, visible paint (including 100 ms settle)="<<(realized-painted)/1000.0<<" ms, clear/edit="<<editTime/1000.0<<" ms\n";window->Quit();
    }
    static void DragTabs(const std::string& root) {
        auto settings=root+"/drag-settings",a=root+"/one.txt",b=root+"/two.txt",c=root+"/four.txt";
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());
        Open(*w,a);auto first=w->CurrentTab()->id;auto* editor=w->CurrentTab()->editor;
        Open(*w,b);auto second=w->CurrentTab()->id;Open(*w,c,true);auto third=w->CurrentTab()->id;
        auto* left=w->fActivePane;auto history=w->fClosedTabs.size();
        CHECK(w->MoveTab(third,*left,0));CHECK(!w->CurrentTab()->preview);
        CHECK(left->tabs[0]->id==third && left->tabs[1]->id==first && left->tabs[2]->id==second);
        CHECK(w->MoveTab(third,*left,3));CHECK(left->tabs[2]->id==third);
        CHECK(w->MoveTab(first,*left,1));CHECK(left->tabs[0]->id==first); // Adjacent slot is a no-op.
        CHECK(!w->MoveTab(first,*left,-1) && !w->MoveTab(99999,*left,0));CheckCards(*w);
        editor->SendMessage(SCI_APPENDTEXT,5,reinterpret_cast<sptr_t>("drag!"));auto contents=editor->Text();
        editor->SendMessage(SCI_SETSELECTION,20,10);editor->SendMessage(SCI_ADDSELECTION,80,70);
        editor->SendMessage(SCI_SETMAINSELECTION,1);editor->SendMessage(SCI_SETZOOM,2);editor->SendMessage(SCI_SETFIRSTVISIBLELINE,35);
        CHECK(w->MoveTab(first,*left,3));CHECK(w->CurrentTab()->editor==editor && editor->Dirty());
        CHECK(editor->SendMessage(SCI_GETSELECTIONS)==2 && editor->SendMessage(SCI_GETSELECTIONNANCHOR,0)==10);
        CHECK(editor->SendMessage(SCI_GETMAINSELECTION)==1 && editor->SendMessage(SCI_GETCURRENTPOS)==80);
        CHECK(editor->SendMessage(SCI_GETFIRSTVISIBLELINE)==35);CHECK(editor->SendMessage(SCI_GETZOOM)==2);CheckCards(*w);
        w->SplitPane(B_HORIZONTAL);auto* right=w->fActivePane;auto duplicate=w->CurrentTab()->id;
        w->SplitPane(B_VERTICAL);auto* bottom=w->fActivePane;auto nested=bottom->id;
        CHECK(w->MoveTab(second,*bottom,0));CHECK(w->Current()->path==b && bottom->tabs.size()==2);CheckCards(*w);
        // A duplicate destination keeps the dragged view and never prompts for shared dirty text.
        CHECK(w->MoveTab(first,*right,1));CHECK(!w->FindTab(duplicate));CHECK(w->CurrentTab()->editor==editor);
        CHECK(w->ViewCount(w->Current()->id)==2 && editor->Matches(contents));
        CHECK(w->fClosedTabs.size()==history);CheckCards(*w);
        CHECK(w->MoveTab(bottom->tabs[1]->id,*right,0));CHECK(bottom->tabs.size()==1);
        CHECK(w->MoveTab(second,*left,1));CHECK(!w->FindPane(nested) && w->fPanes.size()==2);CheckCards(*w);
        // Moving the final tab out of a nested group retains its sibling and order on disk.
        w->SaveSettings();BFile saved((settings+"/settings").c_str(),B_READ_ONLY);BMessage session,layout,leaf,view;
        CHECK(session.Unflatten(&saved)==B_OK && session.FindMessage("editor_layout",&layout)==B_OK);
        CHECK(layout.FindMessage("first",&leaf)==B_OK && leaf.FindMessage("tab",0,&view)==B_OK);
        const char* path=nullptr;CHECK(view.FindString("path",&path)==B_OK && std::string(path)==c);

        // Tear off a unique dirty view without copying or losing its undo history.
        auto movedID=right->tabs[0]->id;auto* movedEditor=right->tabs[0]->editor;
        w->ActivatePane(right,true);w->SelectTab(0);movedEditor->SendMessage(SCI_SETSEL,10,20);
        auto pointer=movedEditor->SendMessage(SCI_GETDOCPOINTER);auto docID=w->Current()->id;
        w->StartRecovery();Wait(*w,[&]{auto* d=w->ByID(docID);return !w->fSnapshot && !d->recovering && !d->recoveryFile.empty() && fs::exists(d->recoveryFile);});
        auto oldDraft=w->Current()->recoveryFile;
        auto* detached=w->DetachTab(movedID,BPoint(180,190));CHECK(detached && !w->FindTab(movedID));
        CHECK(w->fPanes.size()==1 && !w->ByID(docID));CheckCards(*w);
        CHECK(detached->Lock());CHECK(detached->CurrentTab()->editor==movedEditor);
        CHECK(movedEditor->SendMessage(SCI_GETDOCPOINTER)==pointer && movedEditor->Matches(contents) && movedEditor->Dirty());
        CHECK(movedEditor->SendMessage(SCI_GETCURRENTPOS)==20 && movedEditor->SendMessage(SCI_GETANCHOR)==10);
        CHECK(detached->fSettings==settings && detached->fSessionDirectory!=settings);
        CHECK(!fs::exists(oldDraft) && fs::exists(detached->Current()->recoveryFile));CheckCards(*detached);
        CHECK(detached->Current()->serverKey.empty());auto detachedSession=detached->fSessionDirectory;
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        movedEditor->SendMessage(SCI_UNDO);CHECK(!movedEditor->Dirty());movedEditor->SendMessage(SCI_REDO);CHECK(movedEditor->Matches(contents));
        Save(*detached);CHECK(ReadFile(a).bytes==contents);CHECK(detached->QuitRequested());detached->Quit();
        CHECK(!fs::exists(detachedSession+"/settings"));

        // Shared source views become independent buffers in different loopers.
        w->ActivatePane(left,true);w->SelectTab(0);w->SplitPane(B_VERTICAL);auto* shared=w->CurrentTab()->editor;
        shared->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("shared"));auto sharedText=shared->Text();
        auto sharedPointer=shared->SendMessage(SCI_GETDOCPOINTER);auto sharedID=w->CurrentTab()->id;
        detached=w->DetachTab(sharedID,BPoint(200,200));CHECK(detached && !w->FindTab(sharedID));
        CHECK(detached->Lock());CHECK(detached->CurrentTab()->editor->SendMessage(SCI_GETDOCPOINTER)!=sharedPointer);
        CHECK(detached->CurrentTab()->editor->Matches(sharedText) && detached->CurrentTab()->editor->Dirty());
        auto* original=left->tabs[0]->editor;CHECK(original->Matches(sharedText));
        detached->CurrentTab()->editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));CHECK(original->Matches(sharedText));
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        auto recoverySession=detached->fSessionDirectory;detached->Current()->lastRecovery=0;detached->StartRecovery();
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        auto recoveredText=detached->CurrentTab()->editor->Text();detached->SaveSettings();detached->Quit();
        detached=new Workspace(settings,true,recoverySession);detached->Show();CHECK(detached->Lock());Wait(*detached,[&]{return !detached->fRestoring;});
        CHECK(detached->fDocuments.size()==1 && detached->CurrentTab()->editor->Matches(recoveredText));
        CHECK(detached->CurrentTab()->editor->Dirty());Save(*detached);CHECK(detached->QuitRequested());detached->Quit();
        original->SendMessage(SCI_UNDO);CHECK(!original->Dirty());

        // Unsaved, unnamed documents can be detached, saved and undone too.
        w->NewFile();auto untitled=w->CurrentTab()->id;auto* draft=w->CurrentTab()->editor;
        draft->SendMessage(SCI_APPENDTEXT,5,reinterpret_cast<sptr_t>("draft"));
        detached=w->DetachTab(untitled,BPoint(220,210));CHECK(detached && !w->FindTab(untitled));CHECK(detached->Lock());
        CHECK(detached->Current()->path.empty() && detached->CurrentTab()->editor==draft && draft->Dirty());
        Wait(*detached,[&]{return !detached->fSnapshot && !detached->Current()->recovering;});
        detached->SaveTo(detached->Current()->id,root+"/detached-draft.txt");Wait(*detached,[&]{return !detached->Current()->saving;});
        CHECK(ReadFile(root+"/detached-draft.txt").bytes=="draft");CHECK(detached->QuitRequested());detached->Quit();
        // A mouse release during an asynchronous save is fulfilled when the
        // callback has updated the disk stamp, without requiring another drag.
        Open(*w,b);auto deferred=w->CurrentTab()->id;auto* saving=w->Current();
        saving->editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));w->Save(saving);
        CHECK(saving->saving && !w->DetachTab(deferred,BPoint(200,200)) && w->fDetachTab==deferred);
        Wait(*w,[&]{return !w->FindTab(deferred);});detached=nullptr;
        for(int32 i=0;i<be_app->CountWindows();++i) {
            auto* candidate=dynamic_cast<Workspace*>(be_app->WindowAt(i));if(!candidate || candidate==w) continue;
            if(candidate->Lock()) {
                if(candidate->Current() && candidate->Current()->path==b) { detached=candidate;break; }
                candidate->Unlock();
            }
        }
        CHECK(detached && !detached->Current()->saving && !detached->CurrentTab()->editor->Dirty());
        CHECK(detached->Current()->stamp==StatFile(b));CHECK(detached->QuitRequested());detached->Quit();
        // Image views and bounded binary previews use the same move path.
        auto binary=root+"/binary.bin";{std::ofstream out(binary,std::ios::binary);out.write("a\0b",3);}
        Open(*w,binary);auto binaryID=w->CurrentTab()->id;auto* hex=w->CurrentTab()->editor;
        CHECK(hex && hex->SendMessage(SCI_GETREADONLY));detached=w->DetachTab(binaryID,BPoint(210,210));CHECK(detached && detached->Lock());
        CHECK(detached->CurrentTab()->editor==hex && hex->SendMessage(SCI_GETREADONLY));CHECK(detached->QuitRequested());detached->Quit();
        auto image=CanonicalPath("resources/branding/kiri-icon-256.png");Open(*w,image);auto imageID=w->CurrentTab()->id;auto* imageView=w->CurrentTab()->view;
        CHECK(!w->CurrentTab()->editor);detached=w->DetachTab(imageID,BPoint(210,210));CHECK(detached && detached->Lock());
        CHECK(detached->CurrentTab()->view==imageView && !detached->CurrentTab()->editor);CHECK(detached->QuitRequested());detached->Quit();
        CHECK(w->QuitRequested());w->Quit();
    }
    static void RefreshAndDiff(const std::string& base,const std::string& executable) {
        auto root=base+"/refresh-project",settings=base+"/refresh-settings";fs::create_directories(root+"/src");auto file=root+"/src/example.ts";
        std::string original="const original = 1;\n";for(int i=0;i<120;++i) original+="// line "+std::to_string(i)+" 😀\n";
        {std::ofstream(file)<<original;}
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());
        w->fLanguageTools.profiles={{"Fixture","typescript","'"+executable+"' --language-fixture",{".ts"}}};w->OpenProject(root);Open(*w,file);
        auto* left=w->Current()->editor;auto id=w->Current()->id;w->SplitPane(B_HORIZONTAL);auto* right=w->Current()->editor;
        left->SendMessage(SCI_SETSEL,left->SendMessage(SCI_FINDCOLUMN,3,2),left->SendMessage(SCI_FINDCOLUMN,3,6));left->SendMessage(SCI_SETFIRSTVISIBLELINE,2);
        right->SendMessage(SCI_SETSEL,right->SendMessage(SCI_FINDCOLUMN,50,1),right->SendMessage(SCI_FINDCOLUMN,50,4));right->SendMessage(SCI_SETFIRSTVISIBLELINE,40);
        auto changed=std::string("const externalSymbol = 2;\r\n");for(int i=0;i<120;++i) changed+="// line "+std::to_string(i)+" 😀\r\n";
        {std::ofstream(file+".tmp")<<"\xef\xbb\xbf"<<changed;}fs::rename(file+".tmp",file);w->Pulse();
        Wait(*w,[&]{return left->Matches(changed) && !w->ByID(id)->externalBusy;});CHECK(right->Matches(changed));CHECK(!left->Dirty());CHECK(w->ByID(id)->bom);
        CHECK(left->SendMessage(SCI_GETEOLMODE)==0 && right->SendMessage(SCI_GETEOLMODE)==0);
        CHECK(left->SendMessage(SCI_LINEFROMPOSITION,left->SendMessage(SCI_GETCURRENTPOS))==3);CHECK(left->SendMessage(SCI_GETCOLUMN,left->SendMessage(SCI_GETCURRENTPOS))==6);
        CHECK(right->SendMessage(SCI_LINEFROMPOSITION,right->SendMessage(SCI_GETCURRENTPOS))==50);CHECK(right->SendMessage(SCI_GETFIRSTVISIBLELINE)==40);
        CHECK(left->SendMessage(SCI_GETDOCPOINTER)==right->SendMessage(SCI_GETDOCPOINTER));
        Wait(*w,[&]{auto* d=w->ByID(id);return d->serverText==changed && d->symbols.size()==1 && d->symbols[0].name=="externalSymbol";});
        auto revision=left->InputRevision();for(int i=0;i<3;++i) {w->Pulse();Wait(*w,[&]{return !w->fDiskCheckPending;});}CHECK(left->InputRevision()==revision);
        // Preserve both versions, leave the dirty buffer unchanged, then resolve.
        left->SendMessage(SCI_APPENDTEXT,7,reinterpret_cast<sptr_t>("// mine"));auto mine=left->Text();{std::ofstream(file)<<"disk version\n";}w->Pulse();
        Wait(*w,[&]{auto* d=w->ByID(id);return d->external && !d->externalBusy && d->externalChange;});
        auto snapshot=w->ByID(id)->externalChange;CHECK(snapshot->error.empty());CHECK(left->Matches(mine));CHECK(ReadFile(file).bytes=="disk version\n");
        CHECK(ReadFile(snapshot->backupDirectory+"/buffer-example.ts").bytes=="\xef\xbb\xbf"+mine);CHECK(ReadFile(snapshot->backupDirectory+"/disk-example.ts").bytes=="disk version\n");
        auto snapshotSerial=w->ByID(id)->externalSerial;w->Pulse();Wait(*w,[&]{return !w->fDiskCheckPending;});CHECK(w->ByID(id)->externalSerial==snapshotSerial);
        BMessage compare(kExternalCompare);compare.AddInt64("document",id);w->ResolveExternal(compare);CHECK(w->ByID(id)->externalWindow.IsValid());
        BMessage keep(kExternalKeep);keep.AddInt64("document",id);keep.AddInt64("external_serial",snapshotSerial);w->ResolveExternal(keep);CHECK(!w->ByID(id)->external);CHECK(left->Dirty());Save(*w);
        CHECK(ReadFile(file).bytes=="\xef\xbb\xbf"+mine);CHECK(fs::exists(snapshot->backupDirectory+"/disk-example.ts"));
        revision=left->InputRevision();w->Pulse();Wait(*w,[&]{return !w->fDiskCheckPending;});CHECK(left->InputRevision()==revision && !w->ByID(id)->external);
        left->SendMessage(SCI_APPENDTEXT,7,reinterpret_cast<sptr_t>("// more"));{std::ofstream(file)<<"reload this\n";}w->Pulse();
        Wait(*w,[&]{return w->ByID(id)->externalChange!=snapshot && !w->ByID(id)->externalBusy;});
        BMessage stale(kExternalKeep);stale.AddInt64("document",id);stale.AddInt64("external_serial",snapshotSerial);w->ResolveExternal(stale);CHECK(w->ByID(id)->external);
        // Typing after the comparison must be backed up before an explicit reload.
        left->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("// new"));auto latest=left->Text();BMessage reload(kExternalReload);reload.AddInt64("document",id);w->ResolveExternal(reload);
        Wait(*w,[&]{return left->Matches("reload this\n") && !w->ByID(id)->externalBusy;});CHECK(!left->Dirty());CHECK(!w->ByID(id)->bom);CHECK(left->SendMessage(SCI_GETEOLMODE)==SC_EOL_LF);
        auto reloaded=w->ByID(id)->externalChange;CHECK(ReadFile(reloaded->backupDirectory+"/buffer-example.ts").bytes=="\xef\xbb\xbf"+latest);
        fs::remove(file);w->Pulse();Wait(*w,[&]{return w->ByID(id)->external && w->ByID(id)->externalChange!=reloaded && !w->ByID(id)->externalBusy;});CHECK(left->Matches("reload this\n"));CHECK(left->Dirty());
        BMessage recreate(kExternalKeep);recreate.AddInt64("document",id);w->ResolveExternal(recreate);Save(*w);CHECK(ReadFile(file).bytes=="reload this\n");
        // An edit typed while an automatic read is in flight must never be lost.
        {std::ofstream(file)<<"racing disk\n";}w->ReloadExternal(*w->ByID(id));left->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("typed!"));
        Wait(*w,[&]{return !w->ByID(id)->externalBusy;});CHECK(left->Matches("reload this\ntyped!"));CHECK(left->Dirty());
        w->Pulse();Wait(*w,[&]{auto d=w->ByID(id);return d->externalChange && d->externalChange->revision==left->InputRevision() && !d->externalBusy;});
        w->ResolveExternal(reload);Wait(*w,[&]{return left->Matches("racing disk\n") && !w->ByID(id)->externalBusy;});
        for(int i=0;i<6;++i) { std::ofstream(file+".tmp")<<"rapid "<<i<<'\n';fs::rename(file+".tmp",file);w->Pulse(); }
        Wait(*w,[&]{return left->Matches("rapid 5\n") && !w->ByID(id)->externalBusy;});CHECK(!left->Dirty());
        // Loaded tree branches, the cached index and already-open quick-open refresh.
        w->LoadDirectory(root+"/src");Wait(*w,[&]{return w->fExplorer->LoadedDirectories().size()>=2 && !w->fMonitorPending;});
        w->Search(false);auto search=w->fSearchWindows.back();
        auto rows=[&](BMessenger target) { if(target.LockTargetWithTimeout(100000)!=B_OK) return -1;BLooper* looper=nullptr;target.Target(&looper);auto* window=dynamic_cast<BWindow*>(looper);auto* list=window?dynamic_cast<BListView*>(window->FindView("results")):nullptr;auto count=list?list->CountItems():-1;looper->Unlock();return count; };
        w->Search(true);auto projectSearch=w->fSearchWindows.back();CHECK(projectSearch.LockTargetWithTimeout(100000)==B_OK);
        BLooper* searchLooper=nullptr;projectSearch.Target(&searchLooper);auto* searchWindow=dynamic_cast<BWindow*>(searchLooper);dynamic_cast<BTextControl*>(searchWindow->FindView("query"))->SetText("created");searchWindow->PostMessage(kQueryChanged);searchLooper->Unlock();
        {std::ofstream(root+"/src/created.txt")<<"created\n";}w->Pulse();
        Wait(*w,[&]{return std::find(w->fIndex->paths.begin(),w->fIndex->paths.end(),"src/created.txt")!=w->fIndex->paths.end();});
        Wait(*w,[&]{return rows(search)==2 && rows(projectSearch)==2;});
        auto inTree=[&](const std::string& path) {for(int32 i=0;i<w->fExplorer->FullListCountItems();++i) if(static_cast<FileItem*>(w->fExplorer->FullListItemAt(i))->entry.path==path) return true;return false;};
        Wait(*w,[&]{return inTree(root+"/src/created.txt");});
        fs::rename(root+"/src/created.txt",root+"/src/renamed.txt");w->Pulse();Wait(*w,[&]{return inTree(root+"/src/renamed.txt") && !inTree(root+"/src/created.txt");});
        fs::remove(root+"/src/renamed.txt");w->Pulse();Wait(*w,[&]{return !inTree(root+"/src/renamed.txt") && w->fIndex->paths.size()==1 && rows(search)==1 && rows(projectSearch)==0;});CHECK(search.IsValid());search.SendMessage(B_QUIT_REQUESTED);projectSearch.SendMessage(B_QUIT_REQUESTED);
        // Manually opening a symlink directory still displays its cached children
        // after a refresh. Automatic monitoring does not recurse through links.
        fs::create_directory_symlink(root+"/src",root+"/linked");w->LoadDirectory(root);w->LoadDirectory(root+"/linked");
        Wait(*w,[&]{return inTree(root+"/linked/example.ts");});CHECK(inTree(root+"/linked/example.ts"));
        w->fExplorer->SetRoot(root,ListDirectory(root));CHECK(inTree(root+"/linked/example.ts"));
        fs::remove(root+"/linked");w->LoadDirectory(root);
        auto image=root+"/image.png";fs::copy_file(CanonicalPath("resources/branding/kiri-icon-256.png"),image);Open(*w,image);auto imageID=w->Current()->id;auto* imageView=w->CurrentTab()->view;
        fs::copy_file(image,image+".tmp");fs::rename(image+".tmp",image);w->Pulse();Wait(*w,[&]{auto* d=w->ByID(imageID);return d->stamp==StatFile(image) && !d->externalBusy;});CHECK(w->ByID(imageID)->view==imageView && !w->ByID(imageID)->external);
        auto backup=reloaded->backupDirectory;w->Quit();CHECK(ReadFile(backup+"/buffer-example.ts").bytes=="\xef\xbb\xbf"+latest);

        // Exercise annotation alignment, wrapping and synchronization in real Scintilla views.
        auto* window=new BWindow(BRect(50,50,1050,650),"Diff tests",B_TITLED_WINDOW,B_AUTO_UPDATE_SIZE_LIMITS);
        auto* diff=new DiffView();BLayoutBuilder::Group<>(window,B_VERTICAL,0).Add(diff);window->Show();CHECK(window->Lock());
        DiffSource before,after;before.path=after.path="sample.cpp";before.label="Original";after.label="Modified";
        before.text="same\nold 😀\ntail\n";after.text="intro\nsame\nnew 😁\nadded\ntail\n";
        auto model=std::make_shared<DiffModel>(CompareText(before,after));diff->SetModel(model);diff->Align();
        CHECK(diff->Left()->SendMessage(SCI_GETREADONLY) && diff->Right()->SendMessage(SCI_GETREADONLY));
        CHECK(diff->Left()->SendMessage(SCI_ANNOTATIONGETLINES,0)==1);CHECK(diff->Left()->SendMessage(SCI_ANNOTATIONGETLINES,2)==1);
        CHECK(diff->Left()->SendMessage(SCI_VISIBLEFROMDOCLINE,3)==diff->Right()->SendMessage(SCI_VISIBLEFROMDOCLINE,5));
        auto text=diff->Right()->Text();diff->Right()->SendMessage(SCI_APPENDTEXT,4,reinterpret_cast<sptr_t>("oops"));CHECK(diff->Right()->Matches(text));
        diff->SetSideBySide(false);CHECK(diff->Model()==model);diff->SetSideBySide(true);diff->Navigate(1);CHECK(diff->Model()==model);
        before.text="short\n";after.text=std::string(700,'x')+"\n";for(int i=0;i<120;++i) { before.text+="equal line\n";after.text+="equal line\n"; }
        diff->SetModel(std::make_shared<DiffModel>(CompareText(before,after)));diff->SetWrap(true);
        auto wrappingDeadline=system_time()+5000000;
        do { window->Unlock();snooze(50000);if(!window->Lock()) throw std::runtime_error("Diff window closed");diff->Align(); } while(diff->Right()->SendMessage(SCI_WRAPCOUNT,1)<=1 && system_time()<wrappingDeadline);
        if(diff->Right()->SendMessage(SCI_WRAPCOUNT,1)<=1) throw std::runtime_error("Wrap not realized: mode="+std::to_string(diff->Right()->SendMessage(SCI_GETWRAPMODE))+" width="+std::to_string(diff->Right()->Bounds().Width())+" length="+std::to_string(diff->Right()->SendMessage(SCI_GETLENGTH))+" hidden="+std::to_string(diff->Right()->IsHidden()));
        CHECK(diff->Right()->SendMessage(SCI_WRAPCOUNT,1)>1);CHECK(diff->Left()->SendMessage(SCI_VISIBLEFROMDOCLINE,2)==diff->Right()->SendMessage(SCI_VISIBLEFROMDOCLINE,2));
        auto waitForScroll=[&](int line) {
            auto deadline=system_time()+5000000;
            do { window->Unlock();snooze(50000);if(!window->Lock()) throw std::runtime_error("Diff window closed"); }
            while((diff->Left()->SendMessage(SCI_GETFIRSTVISIBLELINE)!=line || diff->Right()->SendMessage(SCI_GETFIRSTVISIBLELINE)!=line) && system_time()<deadline);
            CHECK(diff->Left()->SendMessage(SCI_GETFIRSTVISIBLELINE)==line);CHECK(diff->Right()->SendMessage(SCI_GETFIRSTVISIBLELINE)==line);
        };
        diff->Right()->SendMessage(SCI_SETFIRSTVISIBLELINE,50);waitForScroll(50);
        diff->Left()->SendMessage(SCI_SETFIRSTVISIBLELINE,20);waitForScroll(20);
        diff->Clear("Waiting for another revision");CHECK(!diff->Model());CHECK(diff->Left()->Text().empty() && diff->Right()->Text().empty());window->Quit();

        auto gitRoot=base+"/git-diff-project";fs::create_directories(gitRoot);GitRepository git(gitRoot);
        auto run=[&](std::vector<std::string> args) { auto result=git.Run(args);if(!result.ok()) throw std::runtime_error(result.diagnostic());return result.output; };
        run({"init","-q"});run({"config","user.name","Kiri Test"});run({"config","user.email","kiri@example.invalid"});
        {std::ofstream(gitRoot+"/a.txt")<<"original a\n";std::ofstream(gitRoot+"/b.txt")<<"original b\n";}
        run({"add","."});run({"commit","-qm","First"});{std::ofstream(gitRoot+"/a.txt")<<"committed a\n";}run({"add","a.txt"});run({"commit","-qm","Second"});
        {std::ofstream(gitRoot+"/a.txt")<<"disk a\n";std::ofstream(gitRoot+"/b.txt")<<"index b\n";}run({"add","b.txt"});{std::ofstream(gitRoot+"/b.txt")<<"disk b\n";}
        auto status=run({"status","--porcelain=v1","-z"});w=new Workspace(base+"/git-diff-settings",false);w->Show();CHECK(w->Lock());w->OpenProject(gitRoot);
        auto* view=w->fGit;Wait(*w,[&]{return view->fFiles.size()==2 && view->fCommits.size()==2;});
        w->fModeLayout->SetVisibleItem(int32(1));view->fCommitMessage->MakeFocus();view->fCommitMessage->TextView()->SetText("An unfinished commit message");view->fChanges->MakeFocus();
        view->fChanges->Select(0);view->fChanges->Select(1);BMessage staged(kGitStagedDiff);view->MessageReceived(&staged);
        Wait(*w,[&]{auto m=view->fDiff->Model();return m && m->after.path=="b.txt" && m->after.text=="index b\n";});
        CHECK(view->fDiff->Model()->before.text=="original b\n");auto pair=view->fDiff->Model();view->fDiff->SetSideBySide(false);CHECK(view->fDiff->Model()==pair);
        BMessage working(kGitWorktreeDiff);view->MessageReceived(&working);CHECK(!view->fDiff->Model());
        Wait(*w,[&]{auto m=view->fDiff->Model();return m && m->after.text=="disk b\n";});CHECK(view->fDiff->Model()->before.text=="index b\n");
        view->fHistory->Select(0);Wait(*w,[&]{auto m=view->fDiff->Model();return m && m->after.label.find("Commit ")==0;});
        CHECK(view->fDiff->Model()->before.text=="original a\n" && view->fDiff->Model()->after.text=="committed a\n");
        auto other=base+"/different-repository";fs::create_directories(other);CHECK(GitRepository(other).Run({"init","-q"}).ok());
        view->LoadCommitFile(0);view->SetRepository(other);CHECK(!view->fDiff->Model());Wait(*w,[&]{return !view->fHistoryBusy;});CHECK(!view->fDiff->Model());
        CHECK(status==run({"status","--porcelain=v1","-z"}));w->Quit();
    }
    static void SearchInput(const std::string& base) {
        auto root=base+"/search-input-project";fs::create_directories(root);
        auto file=root+"/example.txt";{std::ofstream(file)<<"old here\nold there\n";}
        auto* w=new Workspace(base+"/search-input-settings",false);w->Show();CHECK(w->Lock());w->OpenProject(root);
        w->Search(true);auto search=w->fSearchWindows.back();
        auto inspect=[&](const std::function<bool(BWindow&)>& action) {
            if(search.LockTargetWithTimeout(100000)!=B_OK) return false;
            BLooper* looper=nullptr;search.Target(&looper);auto* window=dynamic_cast<BWindow*>(looper);
            bool result=window && action(*window);looper->Unlock();return result;
        };
        auto type=[&](const char* name,const char* text) {
            CHECK(inspect([&](BWindow& window) {
                auto* field=dynamic_cast<BTextControl*>(window.FindView(name));if(!field) return false;
                field->MakeFocus();
                for(auto* c=text;*c;++c) { BMessage key(B_KEY_DOWN);key.AddString("bytes",std::string(1,*c).c_str());key.AddInt32("modifiers",0);if(window.PostMessage(&key,field->TextView())!=B_OK) return false; }
                return true;
            }));
        };
        type("query","old");
        Wait(*w,[&]{return inspect([](BWindow& window) { auto* results=dynamic_cast<BListView*>(window.FindView("results"));return results && results->CountItems()==3; });});
        CHECK(inspect([](BWindow& window) { window.FindView("project replacement")->MakeFocus();return true; }));
        Wait(*w,[&]{return true;});CHECK(search.IsValid());
        type("project replacement","new");
        Wait(*w,[&]{return inspect([](BWindow& window) { return std::string(dynamic_cast<BTextControl*>(window.FindView("project replacement"))->Text())=="new"; });});
        auto previewSerial=w->fEditSerial;
        CHECK(inspect([](BWindow& window) { window.FindView("include")->MakeFocus();return true; }));
        Wait(*w,[&]{return true;});CHECK(search.IsValid());CHECK(!w->fEditPlan);CHECK(w->fEditSerial==previewSerial);
        // Enter previews the replacement even after the unchanged field regains focus.
        type("project replacement","\n");Wait(*w,[&]{return bool(w->fEditPlan);});
        CHECK(w->fEditPlan->files.size()==1 && w->fEditPlan->files[0].edits.size()==2);
        CHECK(w->fEditPlan->files[0].edits[0].edit.text=="new");CHECK(ReadFile(file).bytes=="old here\nold there\n");
        w->fEditWindow.SendMessage(B_QUIT_REQUESTED);Wait(*w,[&]{return !w->fEditWindow.IsValid();});
        auto previous=w->fEditPlan;
        CHECK(inspect([](BWindow& window) { auto* button=dynamic_cast<BButton*>(window.FindView("preview replacement"));return button && button->Invoke()==B_OK; }));
        Wait(*w,[&]{return w->fEditPlan!=previous;});CHECK(search.IsValid());
        w->fEditWindow.SendMessage(B_QUIT_REQUESTED);Wait(*w,[&]{return !w->fEditWindow.IsValid();});
        // Enter in the query still opens the selected result and dismisses search.
        type("query",std::string(1,B_DOWN_ARROW).c_str());
        Wait(*w,[&]{return inspect([](BWindow& window) { return dynamic_cast<BListView*>(window.FindView("results"))->CurrentSelection()==2; });});
        type("query","\n");Wait(*w,[&]{return !search.IsValid() && w->fPendingOpen.empty();});CHECK(w->Current()->path==file);
        CHECK(w->Current()->editor->SendMessage(SCI_LINEFROMPOSITION,w->Current()->editor->SendMessage(SCI_GETCURRENTPOS))==1);
        // Open Quickly shares query controls: moving to its list must not accept it.
        w->Search(false);search=w->fSearchWindows.back();type("query","example");
        Wait(*w,[&]{return inspect([](BWindow& window) { auto* results=dynamic_cast<BListView*>(window.FindView("results"));return results && results->CountItems()==1; });});
        CHECK(inspect([](BWindow& window) { window.FindView("results")->MakeFocus();return true; }));
        Wait(*w,[&]{return true;});CHECK(search.IsValid());
        CHECK(inspect([](BWindow& window) { return dynamic_cast<BListView*>(window.FindView("results"))->Invoke()==B_OK; }));
        Wait(*w,[&]{return !search.IsValid();});w->Quit();
    }
    static void SearchAndEdits(const std::string& base,const std::string& executable) {
        auto root=base+"/edit-project",settings=base+"/edit-settings";fs::create_directories(root);
        auto a=root+"/open.ts",b=root+"/closed.ts";
        { std::ofstream(a)<<"const old = '😀';\r\nold;\r\n";std::ofstream(b)<<"old; old;\r\n"; }
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());w->OpenProject(root);Open(*w,a);
        auto* editor=w->Current()->editor;editor->SendMessage(SCI_APPENDTEXT,10,reinterpret_cast<sptr_t>("// dirty\r\n"));
        Wait(*w,[&]{return w->fIndex->paths.size()==2;});auto original=editor->Text();
        BMessage request(kProjectReplace);request.AddString("root",root.c_str());request.AddString("query","old");request.AddString("replacement","new");request.AddBool("case",true);
        auto preview=[&] { auto previous=w->fEditPlan;w->PreviewReplacement(request);Wait(*w,[&]{return w->fEditPlan && w->fEditPlan!=previous;});CHECK(w->fEditPlan->files.size()==2); };
        preview();CHECK(w->fEditPlan->files[0].before.path==b);CHECK(w->fEditPlan->files[1].before.text==original);
        auto oldPreview=w->fEditSerial;preview();BMessage oldApply(kEditApply);oldApply.AddInt64("preview",oldPreview);w->MessageReceived(&oldApply);
        CHECK(!w->fApplyingEdit);CHECK(ReadFile(b).bytes=="old; old;\r\n");CHECK(editor->Text()==original);
        BMessage oldRefresh(kEditRefresh);oldRefresh.AddInt64("preview",oldPreview);auto currentPreview=w->fEditSerial;w->MessageReceived(&oldRefresh);CHECK(w->fEditSerial==currentPreview);
        auto inputRevision=editor->InputRevision();SCNotification delayed{};delayed.nmhdr.code=SCN_MODIFIED;delayed.modificationType=SC_MOD_INSERTTEXT;editor->NotificationReceived(&delayed);CHECK(editor->InputRevision()==inputRevision);
        w->fEditPlan->files[0].edits[1].selected=false;BMessage reviewedApply(kEditApply);reviewedApply.AddInt64("preview",w->fEditSerial);w->MessageReceived(&reviewedApply);CHECK(w->fApplyingEdit);Wait(*w,[&]{return !w->fApplyingEdit;});
        CHECK(ReadFile(b).bytes=="new; old;\r\n");CHECK(editor->Text()=="const new = '😀';\r\nnew;\r\n// dirty\r\n");CHECK(editor->Dirty());CHECK(ReadFile(a).bytes=="const old = '😀';\r\nold;\r\n");
        CHECK(w->fLastEdit->files[0].applied);CHECK(w->fLastEdit->files[1].applied);CHECK(StatFile(w->fLastEdit->journal).exists);
        editor->SendMessage(SCI_UNDO);CHECK(editor->Text()==original);
        w->UndoProjectEdit();w->ApplyProjectEdit(true);Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(ReadFile(b).bytes=="old; old;\r\n");CHECK(editor->Text()==original);
        preview();editor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));w->ApplyProjectEdit();CHECK(!w->fApplyingEdit);CHECK(ReadFile(b).bytes=="old; old;\r\n");CHECK(editor->Text()==original+"x");
        editor->SendMessage(SCI_UNDO);preview();{ std::ofstream(b)<<"external\n"; }w->ApplyProjectEdit();Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(editor->Text()==original);CHECK(ReadFile(b).bytes=="external\n");
        { std::ofstream(b)<<"old; old;\r\n"; }preview();w->ApplyProjectEdit();w->fCancelEdit=true;Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(editor->Text()==original);CHECK(ReadFile(b).bytes=="old; old;\r\n");
        // A failed later disk write preserves the completed file and its restore path.
        auto c=root+"/z-readonly.ts";{std::ofstream(c)<<"old;\n";}chmod(c.c_str(),0444);
        ProjectSearchOptions replaceOptions;replaceOptions.query="old";replaceOptions.matchCase=true;replaceOptions.wholeWord=true;auto index=IndexProject(root);auto plan=std::make_shared<EditPlan>(ReplacementPlan(root,index,replaceOptions,"new",w->OpenSnapshots()));
        w->ShowEditPreview(plan);w->ApplyProjectEdit();Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(ReadFile(b).bytes=="new; new;\r\n");CHECK(!plan->files.back().error.empty());CHECK(!plan->files.back().applied);
        w->UndoProjectEdit();w->ApplyProjectEdit(true);Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(ReadFile(b).bytes=="old; old;\r\n");CHECK(editor->Text()==original);chmod(c.c_str(),0644);fs::remove(c);
        // Real Workspace -> LSP -> native prompt -> edit preview -> whole-operation Undo.
        w->fLanguageTools.ForFile(a);for(auto& profile:w->fLanguageTools.profiles) if(profile.language=="typescript") profile.command="'"+executable+"' --language-fixture";
        w->ResetLanguages();auto* server=w->EnsureLanguage(*w->Current());CHECK(server);Wait(*w,[&]{return server->Ready();});w->SyncLanguage(*w->Current(),*server);
        editor->SendMessage(SCI_GOTOPOS,6);w->RenameSymbol(editor);Wait(*w,[&]{return w->fRenameWindow.IsValid();});
        auto beforePlan=w->fEditPlan;BMessage rename(kRenameSubmit);rename.AddInt64("serial",w->fRenameSerial);rename.AddString("name","renamed");w->SubmitRename(rename);
        Wait(*w,[&]{return w->fEditPlan!=beforePlan;});CHECK(w->fEditPlan->rename);CHECK(w->fEditPlan->files.size()==2);
        w->ApplyProjectEdit();Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(editor->Text().find("const renamed")!=std::string::npos);CHECK(ReadFile(b).bytes=="renamed; renamed;\r\n");
        editor->MakeFocus();Send(*w,B_UNDO);Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(editor->Text()==original);CHECK(ReadFile(b).bytes=="old; old;\r\n");
        auto* other=new Workspace(base+"/other-edit-settings",false);other->Show();CHECK(other->Lock());Open(*other,b);
        other->Current()->editor->SendMessage(SCI_APPENDTEXT,9,reinterpret_cast<sptr_t>("// dirty\n"));auto otherOriginal=other->Current()->editor->Text();other->Unlock();
        Wait(*w,[&]{return true;});preview();CHECK(w->fEditPlan->files[0].before.open);w->ApplyProjectEdit();Wait(*w,[&]{return !w->fApplyingEdit;});
        CHECK(other->Lock());CHECK(other->Current()->editor->Text()=="new; new;\r\n// dirty\n");CHECK(other->Current()->editor->Dirty());other->Unlock();CHECK(ReadFile(b).bytes=="old; old;\r\n");
        w->UndoProjectEdit();w->ApplyProjectEdit(true);Wait(*w,[&]{return !w->fApplyingEdit;});CHECK(other->Lock());CHECK(other->Current()->editor->Text()==otherOriginal);other->Unlock();
        editor->SendMessage(SCI_GOTOPOS,6);w->RenameSymbol(editor);Wait(*w,[&]{return w->fRenameWindow.IsValid();});
        beforePlan=w->fEditPlan;BMessage crossRename(kRenameSubmit);crossRename.AddInt64("serial",w->fRenameSerial);crossRename.AddString("name","everywhere");w->SubmitRename(crossRename);
        Wait(*w,[&]{return w->fEditPlan!=beforePlan;});w->ApplyProjectEdit();Wait(*w,[&]{return !w->fApplyingEdit;});
        CHECK(other->Lock());CHECK(other->Current()->editor->Text()=="everywhere; everywhere;\r\n// dirty\n");CHECK(ReadFile(b).bytes=="old; old;\r\n");
        other->Current()->editor->MakeFocus();w->Unlock();Send(*other,B_UNDO);Wait(*other,[&]{return !other->fApplyingEdit;});CHECK(other->Current()->editor->Text()==otherOriginal);other->Quit();CHECK(w->Lock());CHECK(editor->Text()==original);
        // Query generations and project changes cannot revive stale disk results.
        for(int i=0;i<23;++i) { w->fFindText->SetText(("recent "+std::to_string(i)).c_str());w->RememberQuery(); }
        CHECK(w->fRecentQueries.size()==20);CHECK(Json::parse(ReadFile(settings+"/search-history.json").bytes).size()==20);
        w->Search(true);auto search=w->fSearchWindows.back();
        auto inspectSearch=[&](const std::function<bool(BWindow&)>& action) {
            if(search.LockTargetWithTimeout(100000)!=B_OK) return false;
            BLooper* looper=nullptr;search.Target(&looper);auto* window=dynamic_cast<BWindow*>(looper);bool result=window && action(*window);looper->Unlock();return result;
        };
        CHECK(inspectSearch([](BWindow& search) { auto* query=dynamic_cast<BTextControl*>(search.FindView("query"));if(!query) return false;query->SetText("obsolete query");query->SetText("old");search.PostMessage(kQueryChanged);return true; }));
        Wait(*w,[&]{return inspectSearch([](BWindow& search) { auto* status=dynamic_cast<BStringView*>(search.FindView("search status"));return status && std::string(status->Text()).find("Disk only · 4 matches")!=std::string::npos; });});
        auto nextRoot=base+"/next-project";fs::create_directory(nextRoot);w->OpenProject(nextRoot);Wait(*w,[&]{return !search.IsValid();});
        auto documents=w->fDocuments.size();BMessage stale(kOpenFile);stale.AddString("path",b.c_str());stale.AddString("search_project",root.c_str());w->MessageReceived(&stale);CHECK(w->fDocuments.size()==documents);
        w->Quit();
    }
    static void Run(const std::string& root,const std::string& executable) {
        auto settings=root+"/settings",a=root+"/one.txt",b=root+"/two.txt",c=root+"/three.txt",e=root+"/four.txt";
        std::string lines;for(int i=0;i<400;++i) lines+="line "+std::to_string(i)+" needle\n";
        for(auto& path:{a,b,c,e}) { std::ofstream file(path);file<<path<<'\n'<<lines; }
        auto* w=new Workspace(settings,false);w->Show();CHECK(w->Lock());
        Open(*w,a);auto* left=w->fActivePane;auto* first=w->CurrentTab()->editor;
        first->SendMessage(SCI_SETSEL,10,20);first->SendMessage(SCI_SETFIRSTVISIBLELINE,40);
        w->SplitPane(B_HORIZONTAL);auto* right=w->fActivePane;auto* second=w->CurrentTab()->editor;
        CHECK(w->fPanes.size()==2 && w->fDocuments.size()==1);CHECK(first!=second);
        CHECK(first->SendMessage(SCI_GETDOCPOINTER)==second->SendMessage(SCI_GETDOCPOINTER));CHECK(first->State()==second->State());
        second->SendMessage(SCI_SETSEL,70,80);CHECK(first->SendMessage(SCI_GETCURRENTPOS)==20);
        second->SendMessage(SCI_SETFIRSTVISIBLELINE,90);CHECK(first->SendMessage(SCI_GETFIRSTVISIBLELINE)==40);
        auto revision=first->Revision();second->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("shared"));
        CHECK(first->Dirty() && second->Dirty() && first->Revision()>revision);CHECK(first->Matches(second->Text()));
        first->SendMessage(SCI_UNDO);CHECK(!first->Dirty() && !second->Dirty());
        first->MarkRecovered();CHECK(second->Dirty());second->MarkSaved();CHECK(!first->Dirty());
        second->SendMessage(SCI_APPENDTEXT,6,reinterpret_cast<sptr_t>("saved!"));Save(*w);CHECK(!first->Dirty());CHECK(ReadFile(a).bytes==first->Text());

        Open(*w,b);CHECK(right->tabs.size()==2 && left->tabs.size()==1 && w->fDocuments.size()==2);
        w->SplitPane(B_VERTICAL);auto* bottom=w->fActivePane;CHECK(w->fPanes.size()==3);CHECK(w->fLayout->second->split->Orientation()==B_VERTICAL);
        auto* third=w->CurrentTab()->editor;auto bottomView=w->CurrentTab()->id;
        // Mouse focus and menu commands must choose the view containing input.
        first->MakeFocus();Send(*w,kZoomIn);CHECK(w->fActivePane==left);CHECK(first->SendMessage(SCI_GETZOOM)==1);CHECK(third->SendMessage(SCI_GETZOOM)==0);
        w->fFindText->SetText("needle");Send(*w,kFindNext);CHECK(first->SendMessage(SCI_GETSELECTIONEND)>first->SendMessage(SCI_GETSELECTIONSTART));
        auto otherPosition=first->SendMessage(SCI_GETCURRENTPOS);
        third->MakeFocus();Send(*w,kFindNext);CHECK(w->fActivePane==bottom);CHECK(w->Current()->path==b);
        CHECK(otherPosition==first->SendMessage(SCI_GETCURRENTPOS));CHECK(third->SendMessage(SCI_GETSELECTIONEND)>third->SendMessage(SCI_GETSELECTIONSTART));
        third->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));
        CHECK(w->CloseView(bottomView));CHECK(w->fPanes.size()==2 && w->fDocuments.size()==2); // No dirty prompt while another view exists.
        w->ActivatePane(right,true);w->SelectTab(1);w->CurrentTab()->editor->SendMessage(SCI_UNDO);
        CHECK(!w->CurrentTab()->editor->Dirty());

        Open(*w,c,true);CHECK(w->CurrentTab()->preview);auto previewID=w->CurrentTab()->id;
        Open(*w,e,true);CHECK(!w->FindTab(previewID));CHECK(right->tabs.size()==3);CHECK(w->fClosedTabs.size()==1);
        auto* previewEditor=w->CurrentTab()->editor;previewEditor->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("!"));
        // Open before SCN_MODIFIED is delivered: synchronous dirty state protects edits.
        w->OpenFile(c,1,1,true,true);Wait(*w,[&]{return w->fPendingOpen.empty();});
        CHECK(right->tabs.size()==4);CHECK(!right->tabs[2]->preview && right->tabs[2]->editor->Dirty());
        CHECK(w->CurrentTab()->preview);BMessage keep(kKeepTab);keep.AddBool("current_tab",true);keep.AddInt32("index",23);
        auto keepID=w->CurrentTab()->id;w->MessageReceived(&keep);CHECK(w->CurrentTab()->id==keepID && !w->CurrentTab()->preview);
        w->SelectTab(2);Save(*w);
        w->CurrentTab()->editor->SendMessage(SCI_SETSEL,150,160);w->CurrentTab()->editor->SendMessage(SCI_SETFIRSTVISIBLELINE,85);
        CHECK(w->CloseTab(right->selected));w->ReopenTab();Wait(*w,[&]{return w->fPendingOpen.empty();});
        CHECK(w->Current()->path==e);CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETCURRENTPOS)==160);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE)==85);
        CHECK(w->CloseTab(right->selected));Open(*w,e);auto count=right->tabs.size();w->ReopenTab();CHECK(right->tabs.size()==count);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETCURRENTPOS)==160);
        auto closeID=w->CurrentTab()->id,retainID=right->tabs.front()->id;
        BMessage close(kCloseTab);close.AddBool("current_tab",true);close.AddInt32("index",23);w->MessageReceived(&close);
        CHECK(!w->FindTab(closeID) && w->FindTab(retainID));w->ReopenTab();Wait(*w,[&]{return w->fPendingOpen.empty();});CHECK(w->Current()->path==e);
        // An edit followed immediately by Undo still keeps its preview open.
        Open(*w,c);CHECK(w->CloseTab(right->selected));Open(*w,c,true);CHECK(w->CurrentTab()->preview);
        auto* undone=w->CurrentTab()->editor;undone->SendMessage(SCI_APPENDTEXT,1,reinterpret_cast<sptr_t>("x"));undone->SendMessage(SCI_UNDO);
        w->UpdateTabs();CHECK(!w->CurrentTab()->preview && !undone->Dirty());
        CHECK(w->CloseTab(right->selected));Open(*w,c,true);Save(*w);CHECK(!w->CurrentTab()->preview);
        Open(*w,c);CHECK(w->CloseTab(right->selected));fs::remove(c);w->ReopenTab();CHECK(w->fPendingOpen.empty());CHECK(!fs::exists(c));
        w->NewFile();auto history=w->fClosedTabs.size();CHECK(w->CloseTab(w->fActivePane->selected));CHECK(w->fClosedTabs.size()==history);
        Send(*w,kPreviewTabs);CHECK(!w->fPreviewTabs);

        // Restore a nested layout with the same document in three independent views.
        w->ActivatePane(left,true);w->SplitPane(B_VERTICAL);auto middle=w->fActivePane->id;
        w->CurrentTab()->editor->SendMessage(SCI_SETSEL,250,270);w->CurrentTab()->editor->SendMessage(SCI_SETZOOM,2);
        w->CurrentTab()->editor->SendMessage(SCI_SETWRAPMODE,SC_WRAP_WORD);
        w->CurrentTab()->editor->SendMessage(SCI_SETFIRSTVISIBLELINE,70);
        w->fLayout->split->SetItemWeight(int32(0),.65f,true);w->fLayout->split->SetItemWeight(int32(1),.35f,true);
        Wait(*w,[&]{return true;});w->SaveSettings();auto expected=w->LayoutState(*w->fLayout);auto oldCount=w->fDocuments.size();
        w->Quit();w=new Workspace(settings);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring;});
        CHECK(w->fPanes.size()==3 && w->fDocuments.size()==oldCount);CHECK(w->fActivePane->id==middle);
        CHECK(w->fLayout->split->Orientation()==B_HORIZONTAL && w->fLayout->first->split->Orientation()==B_VERTICAL);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETCURRENTPOS)==270);CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETANCHOR)==250);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETZOOM)==2);CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETWRAPMODE)==SC_WRAP_WORD);
        if(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE)!=70) throw std::runtime_error("Restored first line "+std::to_string(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE))+", expected 70");
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETFIRSTVISIBLELINE)==70);CHECK(!w->fPreviewTabs);
        auto restored=w->LayoutState(*w->fLayout);float before=.5,after=.5;expected.FindFloat("ratio",&before);restored.FindFloat("ratio",&after);
        if(std::abs(before-after)>=.06) throw std::runtime_error("Restored split ratio "+std::to_string(after)+", expected "+std::to_string(before));CHECK(std::abs(before-after)<.06);
        auto* restoredEditor=w->CurrentTab()->editor;auto restoredID=w->Current()->id;
        for(auto& pane:w->fPanes) for(auto& tab:pane->tabs) if(tab->document==restoredID) CHECK(tab->editor->SendMessage(SCI_GETDOCPOINTER)==restoredEditor->SendMessage(SCI_GETDOCPOINTER));

        // Recovery writes one snapshot per buffer and recreates all its views.
        restoredEditor->SendMessage(SCI_APPENDTEXT,9,reinterpret_cast<sptr_t>("recovered"));auto text=restoredEditor->Text();
        w->StartRecovery();Wait(*w,[&]{auto* d=w->ByID(restoredID);return !w->fSnapshot && !d->recovering && !d->recoveryFile.empty() && fs::exists(d->recoveryFile);});
        CHECK(ListDrafts(w->fRecoveryDirectory).size()==1);w->SaveSettings();w->Quit();
        w=new Workspace(settings);w->Show();CHECK(w->Lock());Wait(*w,[&]{return !w->fRestoring;});
        CHECK(w->fPanes.size()==3);CHECK(w->CurrentTab()->editor->Dirty());CHECK(w->CurrentTab()->editor->Matches(text));
        CHECK(w->ViewCount(w->Current()->id)==3);Save(*w);

        // Concurrent requests for the same path create one buffer in both panes.
        auto concurrent=root+"/concurrent.txt";{std::ofstream out(concurrent);out<<lines;}
        w->OpenFile(concurrent,1,1,true,false,w->fPanes[0]->id);
        w->OpenFile(concurrent,1,1,true,false,w->fPanes[1]->id);
        Wait(*w,[&]{return w->fPendingOpen.empty();});size_t documents=0;int64 concurrentID=0;
        for(auto& d:w->fDocuments) if(d->path==concurrent) { ++documents;concurrentID=d->id; }
        CHECK(documents==1 && w->ViewCount(concurrentID)==2);

        // Exercise actual RPC and formatting processes against the focused view.
        auto tsA=root+"/first.ts",tsB=root+"/second.ts";
        std::string sourceA="const first = obj.na;\n",sourceB="const second = obj.na;\n";
        {std::ofstream out(tsA);out<<sourceA;}{std::ofstream out(tsB);out<<sourceB;}
        w->fLanguageTools.profiles={{"Fixture","typescript","\""+executable+"\" --language-fixture",{".ts"}}};
        w->fLanguageTools.prettier="\""+executable+"\" --format-fixture";w->fLanguageTools.completion=false;w->ResetLanguages();
        Open(*w,tsA);auto* paneA=w->fActivePane;auto* editorA=w->CurrentTab()->editor;auto idA=w->Current()->id;
        w->SplitPane(B_HORIZONTAL);auto* paneB=w->fActivePane;auto* mirror=w->CurrentTab()->editor;
        Open(*w,tsB);auto* editorB=w->CurrentTab()->editor;auto idB=w->Current()->id;
        Wait(*w,[&]{return w->ByID(idA)->symbols.size()==1 && w->ByID(idB)->symbols.size()==1;});
        CHECK(w->ByID(idA)->symbols[0].name=="first.ts" && w->ByID(idB)->symbols[0].name=="second.ts");
        auto chooseSymbol=[&](int64 id) { auto* d=w->ByID(id);BMessage message(kSymbolChosen);message.AddInt64("document",id);message.AddInt64("version",d->symbolVersion);message.AddInt32("symbol",0);w->MessageReceived(&message); };
        editorB->SendMessage(SCI_GOTOPOS,10);chooseSymbol(idA);CHECK(editorB->SendMessage(SCI_GETCURRENTPOS)==10);
        chooseSymbol(idB);CHECK(editorB->SendMessage(SCI_GETCURRENTPOS)==0);
        editorB->SendMessage(SCI_GOTOPOS,sourceB.find("na;")+2);w->Complete(editorB);
        Wait(*w,[&]{return !w->fCompletionLabels.empty();});CHECK(w->fCompletionLabels[0].find("second.ts")!=std::string::npos);
        w->AcceptCompletion(editorB,w->fCompletionLabels[0]);CHECK(editorB->Text().find("obj.name")!=std::string::npos);CHECK(editorA->Matches(sourceA));
        editorB->SendMessage(SCI_UNDO);CHECK(editorB->Matches(sourceB));
        w->ActivatePane(paneA,true);editorA->SendMessage(SCI_GOTOPOS,sourceA.find("na;")+2);w->Complete(editorA);
        CHECK(w->fCompletionDocument==idA);w->ActivatePane(paneB,true);w->SelectTab(0);
        Wait(*w,[&]{return w->fCompletionDocument==0;});CHECK(!mirror->SendMessage(SCI_AUTOCACTIVE));CHECK(editorA->Matches(sourceA));
        // A late formatter applies to its original view even after focus changes.
        w->ActivatePane(paneA,true);w->FormatDocument(editorA);w->ActivatePane(paneB,true);w->SelectTab(1);
        Wait(*w,[&]{return editorA->Text().find("CONST FIRST")!=std::string::npos;});
        CHECK(mirror->Matches(editorA->Text()));CHECK(editorB->Matches(sourceB));
        editorA->SendMessage(SCI_UNDO);CHECK(mirror->Matches(sourceA));
        // Closing the requested view cancels its formatter; the other view remains.
        w->ActivatePane(paneA,true);auto formatView=w->CurrentTab()->id;w->FormatDocument(editorA);CHECK(w->CloseView(formatView));
        Wait(*w,[&]{return true;});w->Unlock();snooze(250000);CHECK(w->Lock());CHECK(mirror->Matches(sourceA));
        w->ActivatePane(paneB,true);w->SelectTab(1);Save(*w);

        // Large-file splits share storage without copying the text or reloading it.
        auto large=root+"/large.txt";{std::ofstream out(large);std::string chunk(1023,'x');chunk+='\n';for(int i=0;i<32768;++i) out<<chunk;}
        Open(*w,large);auto pointer=w->CurrentTab()->editor->SendMessage(SCI_GETDOCPOINTER);auto start=system_time();w->SplitPane(B_HORIZONTAL);
        auto elapsed=system_time()-start;CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETDOCPOINTER)==pointer);
        CHECK(w->CurrentTab()->editor->SendMessage(SCI_GETLENGTH)==32*1024*1024);CHECK(elapsed<2000000);
        std::cout<<"32 MiB shared split: "<<elapsed/1000.0<<" ms\n";
        // Closing an entire group collapses just that leaf and preserves others.
        auto panes=w->fPanes.size();Send(*w,kClosePane);CHECK(w->fPanes.size()==panes-1);
        CHECK(w->ViewCount(concurrentID)==2);
        // The shared Close Tab shortcut must retain terminal selection as well.
        w->fTerminal->CloseTerminals();w->NewTerminal();w->NewTerminal();
        BMessage currentTerminal(kCloseTerminal),firstTerminal(kCloseTerminal);firstTerminal.AddInt32("index",0);
        CHECK(w->fTerminal->IndexForMessage(currentTerminal)==1);CHECK(w->fTerminal->IndexForMessage(firstTerminal)==0);
        w->fTerminal->CloseTerminal(w->fTerminal->IndexForMessage(currentTerminal));
        CHECK(!w->fTerminal->Empty() && w->fTerminal->IndexForMessage(currentTerminal)==0);w->Quit();
    }
};
}
int main(int argc,char** argv) {
    signal(SIGPIPE,SIG_IGN);
    if(argc>2 && std::string(argv[1])=="--analysis-fixture") return kiri::AnalysisFixture(argv[2]);
    if(argc>1 && std::string(argv[1])=="--language-fixture") return kiri::LanguageFixture();
    if(argc>1 && std::string(argv[1])=="--format-fixture") {
        std::string text((std::istreambuf_iterator<char>(std::cin)),{});snooze(180000);
        std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::toupper(c);});std::cout<<text;return 0;
    }
    BApplication application("application/x-vnd.Kiri-workspace-unit-tests");
    char folder[]="/tmp/kiri-workspace-XXXXXX";auto* root=mkdtemp(folder);if(!root) return 1;
    try { if(argc==2 && std::string(argv[1])=="--analysis") {kiri::WorkspaceTestAccess::Analysis(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));fs::remove_all(root);std::cout<<"Passed "<<checks<<" analysis workspace checks.\n";return 0;}if(argc==3 && std::string(argv[1])=="--real-analysis") {kiri::WorkspaceTestAccess::RealAnalysis(kiri::CanonicalPath(root),argv[2]);fs::remove_all(root);std::cout<<"Passed "<<checks<<" real analysis checks.\n";return 0;}kiri::WorkspaceTestAccess::Themes(kiri::CanonicalPath(root));kiri::WorkspaceTestAccess::AnalysisRendering();kiri::WorkspaceTestAccess::Analysis(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));kiri::WorkspaceTestAccess::RefreshAndDiff(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));kiri::WorkspaceTestAccess::SearchInput(kiri::CanonicalPath(root));kiri::WorkspaceTestAccess::Run(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));kiri::WorkspaceTestAccess::DragTabs(kiri::CanonicalPath(root));kiri::WorkspaceTestAccess::SearchAndEdits(kiri::CanonicalPath(root),kiri::CanonicalPath(argv[0]));fs::remove_all(root);std::cout<<"Passed "<<checks<<" workspace checks.\n"; }
    catch(const std::exception& error) { std::cerr<<error.what()<<" (test files: "<<root<<")\n";return 1; }
}
