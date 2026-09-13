#include "ui/Workspace.h"
#include "ui/Editor.h"
#include "ui/Explorer.h"
#include "ui/GitView.h"
#include "ui/Messages.h"
#include "ui/PreviewView.h"
#include "ui/TabStrip.h"
#include "ui/TerminalView.h"
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <CardLayout.h>
#include <CheckBox.h>
#include <Clipboard.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <MessageFilter.h>
#include <Path.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <unistd.h>

namespace kiri {
namespace fs=std::filesystem;
namespace {
class SearchNavigation:public BMessageFilter {
public:
    explicit SearchNavigation(BListView* results):BMessageFilter(B_KEY_DOWN),fResults(results) {}
    filter_result Filter(BMessage* message,BHandler**) override {
        const char* bytes=nullptr;if(message->FindString("bytes",&bytes)!=B_OK || !bytes || !*bytes) return B_DISPATCH_MESSAGE;
        if(bytes[0]!=B_UP_ARROW && bytes[0]!=B_DOWN_ARROW) return B_DISPATCH_MESSAGE;
        auto index=fResults->CurrentSelection();index=index<0?0:index+(bytes[0]==B_DOWN_ARROW?1:-1);
        if(fResults->CountItems()) fResults->Select(std::clamp(index,int32(0),fResults->CountItems()-1));
        fResults->ScrollToSelection();return B_SKIP_MESSAGE;
    }
private:BListView* fResults;
};
class SearchItem:public BStringItem {
public:
    SearchItem(const char* text,Theme theme):BStringItem(text),fTheme(theme) {}
    void Update(BView* owner,const BFont* font) override { BStringItem::Update(owner,font);SetHeight(28); }
    void DrawItem(BView* owner,BRect frame,bool) override {
        owner->SetHighColor(IsSelected()?fTheme.selection:fTheme.panel);owner->FillRect(frame);owner->SetLowColor(owner->HighColor());owner->SetHighColor(fTheme.text);
        BString label(Text());owner->TruncateString(&label,B_TRUNCATE_END,frame.Width()-16);owner->DrawString(label.String(),BPoint(frame.left+8,frame.top+19));
    }
private:Theme fTheme;
};
class PromptWindow:public BWindow {
public:
    PromptWindow(BWindow* owner,const char* title,const char* label,uint32 command)
        :BWindow(BRect(0,0,400,100),title,B_TITLED_WINDOW_LOOK,B_FLOATING_APP_WINDOW_FEEL,B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS),fTarget(owner),fCommand(command) {
        fText=new BTextControl("value",label,"",new BMessage(kPromptAccept));
        auto* accept=new BButton("accept","Go",new BMessage(kPromptAccept));auto* cancel=new BButton("cancel","Cancel",new BMessage(kPromptCancel));
        BLayoutBuilder::Group<>(this,B_VERTICAL,10).SetInsets(15).Add(fText).AddGroup(B_HORIZONTAL).AddGlue().Add(cancel).Add(accept);
        fText->SetTarget(this);accept->SetTarget(this);cancel->SetTarget(this);SetDefaultButton(accept);AddShortcut(B_ESCAPE,0,new BMessage(kPromptCancel));CenterIn(owner->Frame());fText->MakeFocus();Show();
    }
    void MessageReceived(BMessage* msg) override {
        if(msg->what==kPromptAccept) { BMessage result(fCommand);result.AddString("value",fText->Text());fTarget.SendMessage(&result);PostMessage(B_QUIT_REQUESTED); }
        else if(msg->what==kPromptCancel) PostMessage(B_QUIT_REQUESTED);else BWindow::MessageReceived(msg);
    }
private:BTextControl* fText;BMessenger fTarget;uint32 fCommand;
};
class SearchWindow:public BWindow {
public:
    SearchWindow(BWindow* owner,std::string root,std::shared_ptr<ProjectIndex> index,bool search,const Theme& theme)
        :BWindow(BRect(0,0,700,420),search?"Search Project":"Open Quickly",B_TITLED_WINDOW_LOOK,B_FLOATING_APP_WINDOW_FEEL,B_AUTO_UPDATE_SIZE_LIMITS),
        fTarget(owner),fRoot(std::move(root)),fIndex(std::move(index)),fSearch(search),fTheme(theme) {
        fQuery=new BTextControl("query","", "",new BMessage(kQueryResult));fQuery->SetModificationMessage(new BMessage(kQueryChanged));
        fResults=new BListView("results");fResults->SetInvocationMessage(new BMessage(kQueryResult));
        fResults->SetExplicitMinSize(BSize(480,240));fQuery->TextView()->AddFilter(new SearchNavigation(fResults));
        fStatus=new BStringView("search status",search?"Search text in project files":"Type part of a file path");
        auto* panel=new BView("search panel",B_WILL_DRAW);
        BLayoutBuilder::Group<>(panel,B_VERTICAL,8).SetInsets(12).Add(fQuery).Add(new BScrollView("results scroll",fResults,0,false,true,B_NO_BORDER)).Add(fStatus);
        BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);
        fQuery->SetTarget(this);fResults->SetTarget(this);for(int32 i=0;i<CountChildren();++i) ThemeView(ChildAt(i),theme);fJobs=std::make_unique<AsyncQueue>(BMessenger(this));
        ResizeTo(700,420);AddShortcut(B_ESCAPE,0,new BMessage(kPromptCancel));CenterIn(owner->Frame());fQuery->MakeFocus();Query();Show();
    }
    ~SearchWindow() override { fTimer.reset();fJobs.reset();while(auto* item=fResults->RemoveItem(int32(0))) delete item; }
    void MessageReceived(BMessage* message) override {
        if(message->what==kWorkDone) fJobs->Drain();
        else if(message->what==kPromptCancel) PostMessage(B_QUIT_REQUESTED);
        else if(message->what==kQueryChanged) {
            fJobs->Cancel("query");
            ++fGeneration;BMessage request(kProjectSearch);fTimer=std::make_unique<BMessageRunner>(BMessenger(this),&request,120000,1);
        } else if(message->what==kProjectSearch) Query();
        else if(message->what==kQueryResult) {
            int32 selected=fResults->CurrentSelection();if(selected<0 && !fMatches.empty()) selected=0;
            if(selected>=0 && selected<static_cast<int32>(fMatches.size())) {
                auto& match=fMatches[selected];BMessage request(kOpenFile);request.AddString("path",(fs::path(fRoot)/match.path).c_str());
                request.AddInt64("line",match.line);request.AddInt64("column",match.column);fTarget.SendMessage(&request);PostMessage(B_QUIT_REQUESTED);
            }
        } else BWindow::MessageReceived(message);
    }
private:
    void Query() {
        auto query=std::string(fQuery->Text());auto generation=++fGeneration;auto root=fRoot;auto index=fIndex;bool search=fSearch;
        fJobs->Submit([this,query,generation,root,index,search](const auto& cancel) mutable {
            if(index->paths.empty()) index=std::make_shared<ProjectIndex>(IndexProject(root,&cancel));
            SearchResult result;
            if(search) result=SearchProject(root,*index,query,false,&cancel);
            else for(auto& path:QuickOpen(*index,query)) result.matches.push_back({path,"",1,1});
            return [this,result=std::move(result),generation,search,index] {
                if(generation!=fGeneration) return;
                while(auto* item=fResults->RemoveItem(int32(0))) delete item;
                fMatches=result.matches;
                fIndex=index;
                for(const auto& match:fMatches) {
                    std::string label=match.path;
                    if(search) label+=":"+std::to_string(match.line)+"    "+match.text;
                    fResults->AddItem(new SearchItem(label.c_str(),fTheme));
                }
                fResults->Select(0);
                std::string status=std::to_string(fMatches.size())+(fMatches.size()==1?" result":" results");
                if(result.truncated) status+=" · refine your search for more";
                if(index->truncated) status+=" · project index capped at 500,000 files";
                if(result.skipped) status+=" · "+std::to_string(result.skipped)+" binary, large or unavailable files skipped";
                fStatus->SetText(status.c_str());
            };
        },"query");
    }
    BMessenger fTarget;std::string fRoot;std::shared_ptr<ProjectIndex> fIndex;bool fSearch;Theme fTheme;
    BTextControl* fQuery;BListView* fResults;BStringView* fStatus;
    std::vector<SearchMatch> fMatches;std::unique_ptr<AsyncQueue> fJobs;std::unique_ptr<BMessageRunner> fTimer;int64 fGeneration=0;
};
}

Workspace::Workspace():BWindow(BRect(40,70,1235,755),"Kiri",B_TITLED_WINDOW,B_ASYNCHRONOUS_CONTROLS|B_AUTO_UPDATE_SIZE_LIMITS) {
    BPath settings;find_directory(B_USER_SETTINGS_DIRECTORY,&settings);settings.Append("Kiri");create_directory(settings.Path(),0755);fSettings=settings.Path();
    fJobs=std::make_unique<AsyncQueue>(BMessenger(this),2);
    fLoaderFactory=std::make_unique<Editor>();
    fRecoveryDirectory=fSettings+"/recovery";create_directory(fRecoveryDirectory.c_str(),0700);
    fSessionToken=std::to_string(time(nullptr))+"-"+std::to_string(getpid());
    fRecoveryJobs=std::make_unique<AsyncQueue>(BMessenger(this));
    fProjectTitle=new BStringView("project title","KIRI  /  No Project");fProjectTitle->SetFont(be_bold_font);
    auto* open=new BButton("open project","Open Folder…",new BMessage(kOpenProject));
    auto* files=new BButton("show files","Files",new BMessage(kToggleSidebar));
    auto* git=new BButton("show git","Git",new BMessage(kToggleGit));
    auto* terminal=new BButton("show terminal","Terminal",new BMessage(kToggleTerminal));
    auto* quick=new BButton("quick open","Open Quickly",new BMessage(kQuickOpen));
    auto* toolbar=new BView("toolbar",0);
    BLayoutBuilder::Group<>(toolbar,B_HORIZONTAL,8).SetInsets(12,8,12,8).Add(fProjectTitle).AddGlue().Add(open).Add(quick).Add(files).Add(git).Add(terminal);
    fExplorer=new Explorer();fExplorer->requestDirectory=[this](std::string path){LoadDirectory(path);};
    auto* sidebar=new BView("sidebar",0);
    BLayoutBuilder::Group<>(sidebar,B_VERTICAL,6).SetInsets(0,8,0,0)
        .AddGroup(B_HORIZONTAL,6).SetInsets(12,0,12,0).Add(new BStringView("files title","FILES"),1).Add(new BButton("refresh","Refresh",new BMessage(kRefresh)),0).End()
        .Add(new BScrollView("explorer scroll",fExplorer,0,false,true,B_NO_BORDER));
    fTabs=new TabStrip();
    auto* documentHost=new BView("document host",0);fDocumentsLayout=new BCardLayout();documentHost->SetLayout(fDocumentsLayout);
    auto* welcome=new BView("welcome",B_WILL_DRAW);
    auto* title=new BStringView("welcome title","Kiri");BFont large(be_bold_font);large.SetSize(32);title->SetFont(&large);
    BLayoutBuilder::Group<>(welcome,B_VERTICAL,12).SetInsets(40).AddGlue().Add(title)
        .Add(new BStringView("welcome detail","A native workspace for your code."))
        .Add(new BStringView("welcome help","Open a folder to explore files, review Git history, and work in the terminal."))
        .Add(new BStringView("welcome keys","Alt O  Open File     Alt P  Open Quickly     Alt F  Find     Alt G  Go to Line"))
        .AddGlue();
    fDocumentsLayout->AddView(welcome);
    fFindBar=new BView("find bar",0);fFindText=new BTextControl("find text","Find","",new BMessage(kFindNext));
    fReplaceText=new BTextControl("replace text","Replace","",new BMessage(kReplace));fMatchCase=new BCheckBox("case","Aa",nullptr);
    BLayoutBuilder::Group<>(fFindBar,B_HORIZONTAL,5).SetInsets(8,5,8,5).Add(fFindText).Add(fReplaceText).Add(fMatchCase)
        .Add(new BButton("previous","‹",new BMessage(kFindPrevious))).Add(new BButton("next","›",new BMessage(kFindNext)))
        .Add(new BButton("replace all","All",new BMessage(kReplaceAll))).Add(new BButton("hide find","×",new BMessage(kFind)));
    auto* editorPanel=new BView("editor panel",0);
    BLayoutBuilder::Group<>(editorPanel,B_VERTICAL,0).Add(fTabs).Add(fFindBar).Add(documentHost);
    fFindBar->Hide();
    fGit=new GitView();auto* modes=new BView("workspace modes",0);fModeLayout=new BCardLayout();modes->SetLayout(fModeLayout);fModeLayout->AddView(editorPanel);fModeLayout->AddView(fGit);
    fTerminal=new TerminalView();auto* terminalPanel=new BView("terminal panel",0);
    BLayoutBuilder::Group<>(terminalPanel,B_VERTICAL,0)
        .AddGroup(B_HORIZONTAL,8).SetInsets(12,4,10,3).Add(new BStringView("terminal title","TERMINAL")).AddGlue()
            .Add(new BButton("restart terminal","Restart",new BMessage(kTerminalRestart))).Add(new BButton("hide terminal","×",new BMessage(kToggleTerminal))).End()
        .Add(fTerminal);
    fTerminalSplit=new BSplitView(B_VERTICAL,1);fTerminalSplit->AddChild(modes);fTerminalSplit->AddChild(terminalPanel);
    fTerminalSplit->SetItemWeight(fTerminalSplit->GetLayout()->ItemAt(0),.74f);fTerminalSplit->SetItemWeight(fTerminalSplit->GetLayout()->ItemAt(1),.26f);
    fSidebarSplit=new BSplitView(B_HORIZONTAL,1);fSidebarSplit->AddChild(sidebar);fSidebarSplit->AddChild(fTerminalSplit);
    fSidebarSplit->SetItemWeight(fSidebarSplit->GetLayout()->ItemAt(0),.20f);fSidebarSplit->SetItemWeight(fSidebarSplit->GetLayout()->ItemAt(1),.80f);
    sidebar->SetExplicitMinSize(BSize(170,150));sidebar->SetExplicitMaxSize(BSize(600,B_SIZE_UNLIMITED));
    fStatus=new BStringView("status","Ready");fStatus->SetExplicitMinSize(BSize(200,25));
    auto* menu=BuildMenus();
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(menu).Add(toolbar).Add(fSidebarSplit).Add(fStatus);
    for(int32 i=0;i<CountChildren();++i) ThemeView(ChildAt(i),Theme::Builtins()[0]);
    ResizeTo(1195,685);
    RestoreSettings();ApplyTheme(fThemeIndex);
    BMessage pulse(kPulse);fPulse=std::make_unique<BMessageRunner>(BMessenger(this),&pulse,2000000);
    fTerminal->Start(fProject.empty()?"/boot/home":fProject);
}
Workspace::~Workspace() { fPulse.reset();fRecoveryTimer.reset();fRecoveryJobs.reset();fJobs.reset(); }

BMenuBar* Workspace::BuildMenus() {
    auto* bar=new BMenuBar("menu");
    auto add=[](BMenu* menu,const char* title,uint32 what,char key=0,uint32 modifiers=0) { menu->AddItem(new BMenuItem(title,new BMessage(what),key,modifiers)); };
    auto* file=new BMenu("File");add(file,"New File",kNewFile,'N');add(file,"Open File…",kOpenFile,'O');add(file,"Open Folder…",kOpenProject,'O',B_SHIFT_KEY);file->AddSeparatorItem();
    add(file,"Save",kSave,'S');add(file,"Save As…",kSaveAs,'S',B_SHIFT_KEY);add(file,"Save All",kSaveAll);add(file,"Close Tab",kCloseTab,'W');file->AddSeparatorItem();add(file,"Quit",B_QUIT_REQUESTED,'Q');bar->AddItem(file);
    auto* edit=new BMenu("Edit");add(edit,"Undo",B_UNDO,'Z');add(edit,"Redo",B_REDO,'Z',B_SHIFT_KEY);edit->AddSeparatorItem();add(edit,"Cut",B_CUT,'X');add(edit,"Copy",B_COPY,'C');add(edit,"Paste",B_PASTE,'V');add(edit,"Select All",B_SELECT_ALL,'A');bar->AddItem(edit);
    auto* view=new BMenu("View");add(view,"Show / Hide Files",kToggleSidebar,'B');add(view,"Show / Hide Terminal",kToggleTerminal,'`');add(view,"Source Control",kToggleGit,'G',B_SHIFT_KEY);view->AddSeparatorItem();
    auto* themes=new BMenu("Color Theme");themes->SetRadioMode(true);
    for(size_t i=0;i<Theme::Builtins().size();++i) { BMessage* message=new BMessage(kTheme);message->AddInt32("index",i);themes->AddItem(new BMenuItem(Theme::Builtins()[i].name.c_str(),message)); }
    view->AddItem(themes);add(view,"Zoom In",kZoomIn,'+');add(view,"Zoom Out",kZoomOut,'-');add(view,"Word Wrap",kWrap);add(view,"Show Whitespace",kShowWhitespace);bar->AddItem(view);
    auto* search=new BMenu("Search");add(search,"Find / Replace",kFind,'F');add(search,"Find Next",kFindNext);add(search,"Find Previous",kFindPrevious);add(search,"Open Quickly…",kQuickOpen,'P');add(search,"Search Project…",kProjectSearch,'F',B_SHIFT_KEY);add(search,"Go to Line…",kGoToLine,'G');bar->AddItem(search);
    auto* git=new BMenu("Git");add(git,"Show Source Control",kToggleGit);add(git,"File History",kFileHistory);add(git,"Copy GitHub Permalink",kCopyPermalink,'L',B_SHIFT_KEY);git->AddSeparatorItem();add(git,"Refresh",kGitRefresh);add(git,"Fetch",kGitFetch);add(git,"Pull (Fast-forward Only)",kGitPull);add(git,"Push",kGitPush);bar->AddItem(git);
    auto* term=new BMenu("Terminal");add(term,"Show / Hide",kToggleTerminal);add(term,"Restart Shell",kTerminalRestart);bar->AddItem(term);
    return bar;
}
Workspace::Document* Workspace::Current() { return fSelected>=0 && fSelected<static_cast<int>(fDocuments.size())?fDocuments[fSelected].get():nullptr; }
Workspace::Document* Workspace::ByID(int64 id) { for(auto& document:fDocuments) if(document->id==id) return document.get();return nullptr; }
void Workspace::Notice(const std::string& text) { fStatus->SetText(text.c_str());fStatus->SetToolTip(text.c_str()); }
void Workspace::UpdateTabs() {
    std::vector<TabLabel> labels;
    for(auto& d:fDocuments) labels.push_back({d->name+(d->external?" !":""),d->path,d->editor && d->editor->Dirty()});
    fTabs->SetTabs(std::move(labels),fSelected);UpdateStatus();
}
void Workspace::UpdateStatus() {
    auto* d=Current();
    if(!d) { Notice(fProject.empty()?"Open a folder to begin":fProject+" · "+std::to_string(fIndex->paths.size())+" indexed files");return; }
    std::string status=d->path.empty()?d->name:d->path;
    if(d->saving) status+="  ·  Saving…";
    else if(d->external) status+="  ·  Changed on disk — reopen to reload, or Save As to keep your edits";
    else if(d->editor) {
        auto position=d->editor->SendMessage(SCI_GETCURRENTPOS);
        status+="  ·  Ln "+std::to_string(d->editor->SendMessage(SCI_LINEFROMPOSITION,position)+1)+", Col "+std::to_string(d->editor->SendMessage(SCI_GETCOLUMN,position)+1);
        status+="  ·  "+d->editor->Language()+"  ·  UTF-8";
        status+=d->editor->SendMessage(SCI_GETEOLMODE)==0?"  CRLF":d->editor->SendMessage(SCI_GETEOLMODE)==1?"  CR":"  LF";
    }
    Notice(status);
}
void Workspace::SelectTab(int index) {
    if(index<0 || index>=static_cast<int>(fDocuments.size())) return;
    ++fFocusSerial;
    fSelected=index;fModeLayout->SetVisibleItem(int32(0));fDocumentsLayout->SetVisibleItem(index+1);
    if(fDocuments[index]->editor) fDocuments[index]->editor->MakeFocus();UpdateTabs();
}
void Workspace::NewFile() {
    auto document=std::make_unique<Document>();document->id=fNextID++;document->name="Untitled "+std::to_string(document->id);
    document->editor=new Editor();document->editor->SetText("");document->editor->ApplyTheme(Theme::Builtins()[fThemeIndex]);document->view=document->editor;
    fDocumentsLayout->AddView(document->view);fDocuments.push_back(std::move(document));SelectTab(fDocuments.size()-1);
}

void Workspace::OpenProject(const std::string& input) {
    auto path=CanonicalPath(input);std::error_code error;
    if(!fs::is_directory(path,error)) { Notice("Cannot open that folder.");return; }
    fProject=path;++fGeneration;fIndex=std::make_shared<ProjectIndex>();fGitRoot.clear();fExplorer->Clear();fGit->SetRepository("");
    fProjectTitle->SetText(("KIRI  /  "+fs::path(path).filename().string()).c_str());SetTitle((fs::path(path).filename().string()+" — Kiri").c_str());
    Notice("Opening project…");auto generation=fGeneration;
    LoadDirectory(path);
    fJobs->Submit([this,path,generation](const auto& cancel) {
        auto index=std::make_shared<ProjectIndex>(IndexProject(path,&cancel));auto result=GitRepository(path).Run({"rev-parse","--show-toplevel"},&cancel);
        auto root=result.ok()?result.output:std::string();while(!root.empty() && (root.back()=='\n' || root.back()=='\r')) root.pop_back();
        return [this,index,root,result,generation] {
            if(generation!=fGeneration) return;
            fIndex=index;fGitRoot=root;fGit->SetRepository(root,result.ok()?"":result.diagnostic());UpdateStatus();SaveSettings();
        };
    },"project");
    fTerminal->Start(path);
}
void Workspace::LoadDirectory(const std::string& path) {
    auto generation=fGeneration;bool root=path==fProject;
    fJobs->Submit([this,path,generation,root](const auto& cancel) {
        auto result=ListDirectory(path,&cancel);
        return [this,result=std::move(result),path,generation,root] {
            if(generation!=fGeneration) return;
            if(root) fExplorer->SetRoot(path,result);else fExplorer->AddListing(path,result);
        };
    });
}
void Workspace::RefreshIndex() {
    if(fProject.empty()) return;auto path=fProject;auto generation=fGeneration;
    fJobs->Submit([this,path,generation](const auto& cancel) {
        auto index=std::make_shared<ProjectIndex>(IndexProject(path,&cancel));
        return [this,index,generation] { if(generation==fGeneration) fIndex=index; };
    },"index refresh");
}
void Workspace::OpenFile(const std::string& input,size_t line,size_t column,bool activate) {
    auto path=CanonicalPath(input);
    if(fRestoring && fRestoringDrafts) { if(std::find(fRestorePaths.begin(),fRestorePaths.end(),path)==fRestorePaths.end()) fRestorePaths.push_back(path);return; }
    for(size_t i=0;i<fDocuments.size();++i) if(fDocuments[i]->path==path) {
        auto* d=fDocuments[i].get();
        if(d->external && d->editor) {
            int32 choice=(new BAlert("Reload File","This file changed on disk. Reloading will replace the contents in this tab.","Cancel","Reload",nullptr,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();
            if(choice==1) { d->editor->MarkSaved();CloseTab(i);break; }
        }
        if(activate) { SelectTab(i);if(d->editor) d->editor->GoTo(line,column); }return;
    }
    if(fPendingOpen.count(path)) return;fPendingOpen.insert(path);Notice("Opening "+path+"…");
    auto focus=activate?++fFocusSerial:0,originalID=Current()?Current()->id:0,originalRevision=Current() && Current()->editor?Current()->editor->Revision():0;
    auto loader=fLoaderFactory->CreateLoader(StatFile(path).size>8*1024*1024);
    fJobs->Submit([this,path,line,column,loader,activate,focus,originalID,originalRevision](const auto& cancel) {
        struct Loaded { FileData data;std::unique_ptr<BBitmap> image; };
        auto loaded=std::make_shared<Loaded>();
        std::string extension=fs::path(path).extension().string();for(auto& c:extension) c=std::tolower(static_cast<unsigned char>(c));
        const std::set<std::string> imageTypes={".png",".jpg",".jpeg",".gif",".bmp",".webp",".tga",".ico",".tif",".tiff",".icns"};
        if(imageTypes.count(extension)) {
            loaded->image.reset(BTranslationUtils::GetBitmap(path.c_str()));loaded->data.stamp=StatFile(path);
        }
        if(!loaded->image) {
            if(!loader->loader) loaded->data.error="Not enough memory to open this document.";
            else loaded->data=ReadFile(path,&cancel,1024ULL*1024*1024,[&](std::string_view chunk){return loader->loader->AddData(chunk.data(),chunk.size())==SC_STATUS_OK;});
        }
        return [this,path,line,column,loaded,loader,activate,focus,originalID,originalRevision] {
            fPendingOpen.erase(path);
            if(!loaded->data.ok()) { Notice(loaded->data.error);(new BAlert("Open File",loaded->data.error.c_str(),"OK"))->Go();FinishRestore();return; }
            auto d=std::make_unique<Document>();d->id=fNextID++;d->path=path;d->name=fs::path(path).filename().string();d->stamp=loaded->data.stamp;
            if(loaded->image) {
                d->view=new PreviewView(std::move(loaded->image),d->name,d->stamp.size);
                static_cast<PreviewView*>(d->view)->ApplyTheme(Theme::Builtins()[fThemeIndex]);
            } else {
                d->editor=new Editor();d->view=d->editor;
                bool binary=loaded->data.binary || !loaded->data.utf8;
                d->bom=loaded->data.bom;
                if(binary) {
                    d->editor->SetText(HexPreview(loaded->data.bytes,d->stamp.size),true);
                    if(!loaded->data.utf8 && !loaded->data.binary) d->name+=" · non-UTF-8";
                } else {
                    d->editor->Adopt(*loader,loaded->data.eol);
                }
                d->editor->SetLanguage(binary?"preview.txt":path,!binary && d->stamp.size>8*1024*1024);
                d->editor->ApplyTheme(Theme::Builtins()[fThemeIndex]);
            }
            auto* original=ByID(originalID);
            bool front=activate && focus==fFocusSerial && (!original || !original->editor || original->editor->Revision()==originalRevision);
            fDocumentsLayout->AddView(d->view);fDocuments.push_back(std::move(d));
            if(front) SelectTab(fDocuments.size()-1);else UpdateTabs();
            if(auto* editor=fDocuments.back()->editor) {
                auto restored=fRestoreDocuments.find(path);
                if(restored!=fRestoreDocuments.end()) {
                    int64 caret=0,anchor=0,first=0;int32 zoom=0,wrap=0;auto& state=restored->second;
                    state.FindInt64("caret",&caret);state.FindInt64("anchor",&anchor);state.FindInt64("first",&first);state.FindInt32("zoom",&zoom);state.FindInt32("wrap",&wrap);
                    editor->SendMessage(SCI_SETSEL,anchor,caret);editor->SendMessage(SCI_SETZOOM,zoom);editor->SendMessage(SCI_SETWRAPMODE,wrap);editor->SendMessage(SCI_SETFIRSTVISIBLELINE,first);
                    fRestoreDocuments.erase(restored);
                } else if(front) editor->GoTo(line,column);
            }
            FinishRestore();SaveSettings();
        };
    },{},[this,path](const std::string& error){fPendingOpen.erase(path);Notice("Cannot open "+path+": "+error);FinishRestore();});
}
void Workspace::Save(Document* d,bool saveAs) {
    if(!d || !d->editor || d->saving || d->editor->SendMessage(SCI_GETREADONLY)) return;
    if(saveAs || d->path.empty()) {
        if(fSavePanel && fSavePanel->IsShowing()) { fSavePanel->Window()->Activate();return; }
        fSavePanelID=d->id;fSavePanelAccepted=false;BMessage message(kSaveChosen);
        message.AddInt64("panel_token",++fSavePanelToken);message.AddInt64("document_id",d->id);
        BMessenger target(this);fSavePanel=std::make_unique<BFilePanel>(B_SAVE_PANEL,&target,nullptr,B_FILE_NODE,false,&message);
        if(!fProject.empty()) fSavePanel->SetPanelDirectory(fProject.c_str());fSavePanel->SetSaveText(d->name.c_str());fSavePanel->Show();return;
    }
    SaveTo(d->id,d->path);
}
void Workspace::SaveTo(int64 id,const std::string& input) {
    auto* d=ByID(id);if(!d || !d->editor || d->saving) return;
    std::string path=CanonicalPath(input);FileStamp expected=d->path==path?d->stamp:StatFile(path);
    for(const auto& other:fDocuments) if(other->id!=id && other->path==path) {
        d->closeAfterSave=false;fQuitWhenSaved=false;fSaveQueue.clear();
        Notice("That file is already open in another tab. Close that tab or choose another name.");return;
    }
    // Save As replacement was confirmed by the native BFilePanel.
    auto text=std::make_shared<std::string>(d->editor->Text());if(d->bom) text->insert(0,"\xef\xbb\xbf");
    int64 revision=d->editor->Revision();d->saving=true;UpdateStatus();
    fJobs->Submit([this,id,path,expected,text,revision](const auto&) {
        auto error=SaveFile(path,*text,expected);auto stamp=StatFile(path);
        return [this,id,path,error,stamp,revision,text] {
            auto* document=ByID(id);if(!document) return;
            document->saving=false;
            if(!error.empty()) {
                document->closeAfterSave=false;fQuitWhenSaved=false;fSaveQueue.clear();Notice(error);(new BAlert("Save File",error.c_str(),"OK"))->Go();return;
            }
            document->path=path;document->name=fs::path(path).filename().string();document->stamp=stamp;document->external=false;
            std::string_view saved(*text);if(document->bom) saved.remove_prefix(3);
            if(document->editor->Matches(saved)) { document->editor->MarkSaved();ClearRecovery(*document); }
            document->editor->SetLanguage(path,stamp.size>8*1024*1024);UpdateTabs();SaveSettings();
            if(document->closeAfterSave) {
                document->closeAfterSave=false;
                for(size_t i=0;i<fDocuments.size();++i) if(fDocuments[i]->id==id) { CloseTab(i);break; }
            }
            fGit->Refresh();
            RefreshIndex();
            if(fQuitWhenSaved) PostMessage(B_QUIT_REQUESTED);
            ContinueSaveAll();
        };
    },{},[this,id](const std::string& error) {
        if(auto* document=ByID(id)) { document->saving=false;document->closeAfterSave=false; }
        fQuitWhenSaved=false;fSaveQueue.clear();Notice("Save failed: "+error);
    });
}
void Workspace::ContinueSaveAll() {
    while(!fSaveQueue.empty()) {
        auto* document=ByID(fSaveQueue.front());
        if(document && document->saving) return;
        if(document && document->editor && document->editor->Dirty()) { Save(document);return; }
        fSaveQueue.pop_front();
    }
}
bool Workspace::CloseTab(int index) {
    if(index<0 || index>=static_cast<int>(fDocuments.size())) return false;
    auto* d=fDocuments[index].get();
    if(d->saving) { d->closeAfterSave=true;return false; }
    if(d->editor && d->editor->Dirty()) {
        int32 choice=(new BAlert("Unsaved Changes",("Save changes to "+d->name+"?").c_str(),"Cancel","Discard","Save",B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();
        if(choice==0) return false;
        if(choice==2) { d->closeAfterSave=true;Save(d);return false; }
    }
    ClearRecovery(*d);
    BView* view=d->view;fDocumentsLayout->RemoveView(view);view->RemoveSelf();delete view;fDocuments.erase(fDocuments.begin()+index);
    if(fDocuments.empty()) { fSelected=-1;fDocumentsLayout->SetVisibleItem(int32(0));UpdateTabs(); }
    else SelectTab(std::min(index,static_cast<int>(fDocuments.size())-1));
    SaveSettings();return true;
}
bool Workspace::QuitRequested() {
    if(!fQuitWhenSaved) fQuitDiscarded.clear();
    fQuitWhenSaved=true;
    for(auto& d:fDocuments) {
        if(d->saving) { fQuitWhenSaved=true;return false; }
        if(!d->editor || !d->editor->Dirty()) continue;
        auto discarded=fQuitDiscarded.find(d->id);
        if(discarded!=fQuitDiscarded.end() && discarded->second==d->editor->Revision()) continue;
        int32 choice=(new BAlert("Unsaved Changes",("Save changes to "+d->name+" before quitting?").c_str(),"Cancel","Discard","Save",B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();
        if(choice==0) { fQuitWhenSaved=false;fQuitDiscarded.clear();return false; }
        if(choice==2) { fQuitWhenSaved=true;Save(d.get());return false; }
        fQuitDiscarded[d->id]=d->editor->Revision();
    }
    fRecoveryTimer.reset();fSnapshot.reset();fRecoveryJobs.reset();
    for(const auto& d:fDocuments) if(!d->recoveryFile.empty()) fRetiredDrafts.insert(d->recoveryFile);
    for(const auto& file:fRetiredDrafts) unlink(file.c_str());
    SaveSettings();be_app->PostMessage(B_QUIT_REQUESTED);return true;
}
void Workspace::ApplyTheme(int index) {
    fThemeIndex=std::clamp(index,0,static_cast<int>(Theme::Builtins().size())-1);const auto& theme=Theme::Builtins()[fThemeIndex];
    for(int32 i=0;i<CountChildren();++i) ThemeView(ChildAt(i),theme);
    fExplorer->ApplyTheme(theme);fTabs->ApplyTheme(theme);fTerminal->ApplyTheme(theme);fGit->ApplyTheme(theme);
    for(auto& d:fDocuments) {
        if(d->editor) d->editor->ApplyTheme(theme);
        if(auto* preview=dynamic_cast<PreviewView*>(d->view)) preview->ApplyTheme(theme);
    }
    SaveSettings();
}
void Workspace::ShowFind() {
    if(!fFindBar->IsHidden()) { fFindBar->Hide();if(Current() && Current()->editor) Current()->editor->MakeFocus();return; }
    fFindBar->Show();fFindText->MakeFocus();fFindText->TextView()->SelectAll();
}
void Workspace::Search(bool search) {
    if(fProject.empty()) { Notice("Open a project folder first.");return; }
    new SearchWindow(this,fProject,fIndex,search,Theme::Builtins()[fThemeIndex]);
}
void Workspace::PromptLine() { new PromptWindow(this,"Go to Line","Line or line:column",kGoToResult); }
void Workspace::CopyPermalink() {
    auto* d=Current();
    if(!d || !d->editor || d->path.empty() || fGitRoot.empty()) { Notice("Open a saved file in a Git project first.");return; }
    if(d->editor->SendMessage(SCI_GETREADONLY)) { Notice("Permalinks are available for source text, not binary previews.");return; }
    if(d->editor->Dirty() || d->external) { Notice("Save or reload this file before copying a permalink.");return; }
    std::error_code error;auto relative=fs::relative(d->path,fGitRoot,error).string();
    if(error || relative.rfind("../",0)==0) { Notice("This file is outside the project repository.");return; }
    auto start=d->editor->SendMessage(SCI_GETSELECTIONSTART),end=d->editor->SendMessage(SCI_GETSELECTIONEND);
    size_t first=d->editor->SendMessage(SCI_LINEFROMPOSITION,start)+1,last=d->editor->SendMessage(SCI_LINEFROMPOSITION,end>start?end-1:end)+1;
    auto root=fGitRoot;Notice("Creating GitHub permalink…");
    fJobs->Submit([this,relative,root,first,last](const auto& cancel) {
        auto result=GitRepository(root).Permalink(relative,first,last,&cancel);
        return [this,result=std::move(result)] {
            if(!result.ok()) { Notice(result.diagnostic());return; }
            if(be_clipboard->Lock()) {
                be_clipboard->Clear();be_clipboard->Data()->AddData("text/plain",B_MIME_TYPE,result.output.data(),result.output.size());be_clipboard->Commit();be_clipboard->Unlock();Notice("Copied "+result.output);
            }
        };
    });
}

void Workspace::RestoreSettings() {
    BFile file((fSettings+"/settings").c_str(),B_READ_ONLY);BMessage settings;
    settings.Unflatten(&file);fRestoring=true;
    int32 theme=0;settings.FindInt32("theme",&theme);fThemeIndex=theme;
    BRect frame;if(settings.FindRect("frame",&frame)==B_OK && frame.Width()>600 && frame.Height()>400) { MoveTo(frame.LeftTop());ResizeTo(frame.Width(),frame.Height()); }
    const char* project=nullptr;if(settings.FindString("project",&project)==B_OK && project && *project) OpenProject(project);
    const char* path=nullptr;
    for(int32 i=0;settings.FindString("file",i,&path)==B_OK;++i) fRestorePaths.push_back(path);
    BMessage state;
    for(int32 i=0;settings.FindMessage("document",i,&state)==B_OK;++i)
        if(state.FindString("path",&path)==B_OK) fRestoreDocuments[path]=state;
    if(settings.FindString("selected",&path)==B_OK) fRestoreSelected=path;
    auto drafts=ListDrafts(fRecoveryDirectory);fRestoringDrafts=drafts.size();
    for(const auto& draft:drafts) RestoreDraft(draft);
    FinishRestore();
}
void Workspace::SaveSettings() {
    if(fRestoring) return;
    BMessage settings;settings.AddInt32("theme",fThemeIndex);settings.AddRect("frame",Frame());settings.AddString("project",fProject.c_str());
    if(auto* d=Current()) settings.AddString("selected",d->path.empty()?d->recoveryFile.c_str():d->path.c_str());
    for(const auto& d:fDocuments) if(!d->path.empty()) {
        settings.AddString("file",d->path.c_str());BMessage state;state.AddString("path",d->path.c_str());
        if(d->editor) {
            state.AddInt64("caret",d->editor->SendMessage(SCI_GETCURRENTPOS));state.AddInt64("anchor",d->editor->SendMessage(SCI_GETANCHOR));
            state.AddInt64("first",d->editor->SendMessage(SCI_GETFIRSTVISIBLELINE));state.AddInt32("zoom",d->editor->SendMessage(SCI_GETZOOM));state.AddInt32("wrap",d->editor->SendMessage(SCI_GETWRAPMODE));
        }
        settings.AddMessage("document",&state);
    }
    BFile file((fSettings+"/settings.tmp").c_str(),B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
    if(file.InitCheck()==B_OK && settings.Flatten(&file)==B_OK) { file.Sync();rename((fSettings+"/settings.tmp").c_str(),(fSettings+"/settings").c_str()); }
}
void Workspace::FinishRestore() {
    if(!fRestoring || fRestoringDrafts) return;
    if(!fRestoreFilesStarted) {
        fRestoreFilesStarted=true;
        for(const auto& path:fRestorePaths) {
            bool open=false;for(const auto& document:fDocuments) if(document->path==path) open=true;
            if(!open && StatFile(path).exists) OpenFile(path,1,1,false);
        }
    }
    if(!fPendingOpen.empty()) return;
    fRestoring=false;
    if(!fFocusSerial && !fDocuments.empty()) {
        size_t selected=0;for(size_t i=0;i<fDocuments.size();++i) if(fDocuments[i]->path==fRestoreSelected || fDocuments[i]->recoveryFile==fRestoreSelected) { selected=i;break; }
        SelectTab(selected);
    }
    SaveSettings();
}
void Workspace::RestoreDraft(const std::string& file) {
    auto loader=fLoaderFactory->CreateLoader(StatFile(file).size>8*1024*1024);
    fJobs->Submit([this,file,loader](const auto& cancel) {
        Draft draft;
        if(!loader->loader) draft.error="Not enough memory to recover this document.";
        else draft=ReadDraft(file,[&](auto chunk){return !cancel && loader->loader->AddData(chunk.data(),chunk.size())==SC_STATUS_OK;});
        return [this,file,loader,draft=std::move(draft)] {
            --fRestoringDrafts;
            if(!draft.ok()) { (new BAlert("Document Recovery",(file+"\n\n"+draft.error).c_str(),"OK"))->Go();FinishRestore();return; }
            auto d=std::make_unique<Document>();d->id=fNextID++;d->path=draft.path;d->name=draft.name;d->stamp=draft.base;d->bom=draft.bom;
            d->recoveryFile=file;d->external=!d->path.empty() && StatFile(d->path)!=d->stamp;
            d->editor=new Editor();d->view=d->editor;d->editor->Adopt(*loader,draft.eol);d->editor->MarkRecovered();
            d->editor->SetLanguage(d->path,d->editor->SendMessage(SCI_GETLENGTH)>8*1024*1024);d->editor->ApplyTheme(Theme::Builtins()[fThemeIndex]);
            d->recoveryRevision=d->editor->Revision();d->lastRecovery=system_time();
            fDocumentsLayout->AddView(d->view);fDocuments.push_back(std::move(d));
            fDocuments.back()->editor->SendMessage(SCI_SETSEL,draft.anchor,draft.caret);fDocuments.back()->editor->SendMessage(SCI_SETFIRSTVISIBLELINE,draft.firstLine);
            FinishRestore();Notice("Recovered unsaved changes. Save the document to keep them in its source file.");
        };
    },{},[this](const std::string& error){--fRestoringDrafts;Notice("Recovery failed: "+error);FinishRestore();});
}
void Workspace::StartRecovery() {
    if(fSnapshot || !fRecoveryJobs || fRestoring) return;
    for(auto& d:fDocuments) {
        if(!d->editor || !d->editor->Dirty() || d->saving || d->recovering || d->recoveryRevision==d->editor->Revision() || system_time()-d->lastRecovery<10000000) continue;
        if(d->recoveryFile.empty()) d->recoveryFile=fRecoveryDirectory+"/"+fSessionToken+"-"+std::to_string(d->id)+".draft";
        auto draft=std::make_shared<Draft>();draft->path=d->path;draft->name=d->name;draft->base=d->stamp;draft->bom=d->bom;draft->eol=d->editor->SendMessage(SCI_GETEOLMODE);
        draft->caret=d->editor->SendMessage(SCI_GETCURRENTPOS);draft->anchor=d->editor->SendMessage(SCI_GETANCHOR);draft->firstLine=d->editor->SendMessage(SCI_GETFIRSTVISIBLELINE);
        fSnapshot=std::make_unique<Snapshot>(Snapshot{d->id,d->editor->Revision(),size_t(d->editor->SendMessage(SCI_GETLENGTH)),0,draft});
        d->lastRecovery=system_time();
        BMessage tick(kRecoveryTick);fRecoveryTimer=std::make_unique<BMessageRunner>(BMessenger(this),&tick,16000);return;
    }
}
void Workspace::RecoveryTick() {
    if(!fSnapshot) { fRecoveryTimer.reset();return; }
    auto* d=ByID(fSnapshot->id);
    if(!d || d->saving || !d->editor->Dirty() || d->editor->Revision()!=fSnapshot->revision) { fSnapshot.reset();fRecoveryTimer.reset();return; }
    try {
        auto& s=*fSnapshot;auto count=std::min<size_t>(2*1024*1024,s.total-s.offset);
        if(s.offset==0) s.draft->text.reserve(s.total+1);
        s.draft->text.resize(s.offset+count+1);
        Sci_TextRangeFull range{{Sci_Position(s.offset),Sci_Position(s.offset+count)},s.draft->text.data()+s.offset};
        d->editor->SendMessage(SCI_GETTEXTRANGEFULL,0,reinterpret_cast<sptr_t>(&range));s.offset+=count;s.draft->text.resize(s.offset);
        if(s.offset<s.total) return;
        auto draft=s.draft;auto file=d->recoveryFile;auto revision=s.revision,id=s.id;d->recovering=true;
        fRecoveryJobs->Submit([this,draft,file,id,revision](const auto& cancel) {
            auto error=cancel?std::string("Recovery cancelled."):WriteDraft(file,*draft);
            return [this,id,revision,error] {
                if(auto* d=ByID(id)) { d->recovering=false;if(error.empty()) d->recoveryRevision=revision;else Notice("Recovery snapshot failed: "+error); }
            };
        },file,[this,id](const std::string& error){if(auto* d=ByID(id)) d->recovering=false;Notice("Recovery snapshot failed: "+error);});
        fSnapshot.reset();fRecoveryTimer.reset();SaveSettings();
    } catch(const std::exception& error) { fSnapshot.reset();fRecoveryTimer.reset();Notice("Recovery snapshot failed: "+std::string(error.what())); }
}
void Workspace::ClearRecovery(Document& d) {
    if(fSnapshot && fSnapshot->id==d.id) { fSnapshot.reset();fRecoveryTimer.reset(); }
    if(d.recoveryFile.empty() || !fRecoveryJobs) return;
    auto file=d.recoveryFile;fRetiredDrafts.insert(file);fRecoveryJobs->Cancel(file);
    fRecoveryJobs->Submit([file](const auto&) { unlink(file.c_str());return AsyncQueue::Callback{}; });
    d.recovering=false;d.recoveryRevision=-1;
}
void Workspace::Pulse() {
    StartRecovery();
    if(fDiskCheckPending || fDocuments.empty()) return;
    std::vector<std::pair<int64,std::string>> files;
    for(const auto& d:fDocuments) if(!d->path.empty() && !d->saving) files.emplace_back(d->id,d->path);
    fDiskCheckPending=true;
    fJobs->Submit([this,files](const auto&) {
        std::vector<std::pair<int64,FileStamp>> stamps;
        for(const auto& file:files) stamps.emplace_back(file.first,StatFile(file.second));
        return [this,stamps] {
            fDiskCheckPending=false;bool changed=false;
            for(const auto& item:stamps) {
                auto* d=ByID(item.first);if(!d || d->saving) continue;
                bool external=d->stamp!=item.second;if(external!=d->external) { d->external=external;changed=true; }
            }
            if(changed) UpdateTabs();
        };
    });
}
void Workspace::MessageReceived(BMessage* message) {
    auto* d=Current();auto* editor=d?d->editor:nullptr;
    switch(message->what) {
        case kWorkDone:fJobs->Drain();if(fRecoveryJobs) fRecoveryJobs->Drain();break;
        case kRecoveryTick:RecoveryTick();break;
        case kPulse:Pulse();break;
        case kOpenProject: {
            const char* path;if(message->FindString("path",&path)==B_OK) { OpenProject(path);break; }
            BMessage selected(kProjectChosen);BMessenger target(this);
            fFolderPanel=std::make_unique<BFilePanel>(B_OPEN_PANEL,&target,nullptr,B_DIRECTORY_NODE,false,&selected);fFolderPanel->Show();break;
        }
        case kProjectChosen:case kFileChosen: {
            entry_ref ref;
            for(int32 i=0;message->FindRef("refs",i,&ref)==B_OK;++i) { BPath path(&ref);if(message->what==kProjectChosen) OpenProject(path.Path());else OpenFile(path.Path()); }
            break;
        }
        case kOpenFile: {
            const char* path;
            if(message->FindString("path",&path)==B_OK) {
                int64 line=1,column=1;message->FindInt64("line",&line);message->FindInt64("column",&column);OpenFile(path,line,column);break;
            }
            int32 index;
            if(message->FindInt32("index",&index)==B_OK) {
                auto* item=static_cast<FileItem*>(fExplorer->ItemAt(index));if(item && !item->placeholder && !item->entry.directory) OpenFile(item->entry.path);break;
            }
            BMessage selected(kFileChosen);BMessenger target(this);
            fOpenPanel=std::make_unique<BFilePanel>(B_OPEN_PANEL,&target,nullptr,B_FILE_NODE,true,&selected);
            if(!fProject.empty()) fOpenPanel->SetPanelDirectory(fProject.c_str());fOpenPanel->Show();break;
        }
        case kNewFile:NewFile();break;
        case kSave:Save(d);break;
        case kSaveAs:Save(d,true);break;
        case kSaveAll:
            fSaveQueue.clear();for(auto& document:fDocuments) if(document->editor && document->editor->Dirty()) fSaveQueue.push_back(document->id);
            ContinueSaveAll();break;
        case kSaveChosen: {
            entry_ref directory;const char* name;
            int64 token=0,id=0;message->FindInt64("panel_token",&token);message->FindInt64("document_id",&id);
            if(token!=fSavePanelToken || fSavePanelAccepted) break;
            if(message->FindRef("directory",&directory)==B_OK && message->FindString("name",&name)==B_OK) {
                fSavePanelAccepted=true;
                if(fSavePanel) fSavePanel->Hide();
                BPath path(&directory);path.Append(name);SaveTo(id,path.Path());
            }
            break;
        }
        case B_CANCEL: {
            // BFilePanel also sends B_CANCEL when it hides after an accepted save.
            int64 token=0;if(message->FindInt64("panel_token",&token)!=B_OK || token!=fSavePanelToken || fSavePanelAccepted) break;
            fQuitWhenSaved=false;fQuitDiscarded.clear();fSaveQueue.clear();if(auto* document=ByID(fSavePanelID)) document->closeAfterSave=false;break;
        }
        case kCloseTab: { int32 index=fSelected;message->FindInt32("index",&index);CloseTab(index);break; }
        case kSelectTab: { int32 index;if(message->FindInt32("index",&index)==B_OK) SelectTab(index);break; }
        case kEditorChanged:case kEditorPosition: {
            void* source=nullptr;message->FindPointer("editor",&source);
            if(message->what==kEditorChanged) {
                for(auto& document:fDocuments) if(document->editor==source) ++document->revision;
                UpdateTabs();
            } else if(editor==source) UpdateStatus();
            break;
        }
        case B_UNDO:case B_REDO:case B_CUT:case B_COPY:case B_PASTE:case B_SELECT_ALL: {
            auto* focus=CurrentFocus();
            bool inEditor=false;for(BView* v=focus;v;v=v->Parent()) if(v==editor) inEditor=true;
            if(editor && inEditor) {
                uint32 command=message->what==B_UNDO?SCI_UNDO:message->what==B_REDO?SCI_REDO:message->what==B_CUT?SCI_CUT:message->what==B_COPY?SCI_COPY:message->what==B_PASTE?SCI_PASTE:SCI_SELECTALL;
                editor->SendMessage(command);
            } else if(focus) focus->MessageReceived(message);
            break;
        }
        case kTheme: { int32 index;if(message->FindInt32("index",&index)==B_OK) ApplyTheme(index);break; }
        case kToggleTerminal:fTerminalSplit->SetItemCollapsed(1,!fTerminalSplit->IsItemCollapsed(1));break;
        case kToggleSidebar:fSidebarSplit->SetItemCollapsed(0,!fSidebarSplit->IsItemCollapsed(0));break;
        case kToggleGit:++fFocusSerial;fModeLayout->SetVisibleItem(fModeLayout->VisibleIndex()==0?1:0);break;
        case kTerminalRestart:fTerminal->Start(fProject.empty()?"/boot/home":fProject);break;
        case kRefresh:if(!fProject.empty()) { LoadDirectory(fProject);fGit->Refresh();RefreshIndex(); }break;
        case kFind:ShowFind();break;
        case kFindNext:case kFindPrevious:
            if(editor) { if(!editor->Find(fFindText->Text(),message->what==kFindPrevious,fMatchCase->Value()==B_CONTROL_ON)) Notice("No match found."); }break;
        case kReplace:
            if(editor && !editor->ReplaceOne(fFindText->Text(),fReplaceText->Text(),fMatchCase->Value()==B_CONTROL_ON)) Notice("No match found.");break;
        case kReplaceAll:
            if(editor) Notice("Replaced "+std::to_string(editor->ReplaceAll(fFindText->Text(),fReplaceText->Text(),fMatchCase->Value()==B_CONTROL_ON))+" matches.");break;
        case kQuickOpen:Search(false);break;
        case kProjectSearch:Search(true);break;
        case kGoToLine:PromptLine();break;
        case kGoToResult: {
            const char* value;if(editor && message->FindString("value",&value)==B_OK) {
                size_t line=1,column=1;std::istringstream input(value);input>>line;if(input.peek()==':') { input.get();input>>column; }editor->GoTo(line,column);
            }
            break;
        }
        case kCopyPermalink:CopyPermalink();break;
        case kFileHistory:
            if(d && !d->path.empty() && !fGitRoot.empty()) { fModeLayout->SetVisibleItem(int32(1));std::error_code error;auto relative=fs::relative(d->path,fGitRoot,error).string();if(!error) fGit->ShowFileHistory(relative); }break;
        case kGitRefresh:case kGitFetch:case kGitPull:case kGitPush:fGit->MessageReceived(message);break;
        case kZoomIn:if(editor) editor->SendMessage(SCI_ZOOMIN);break;
        case kZoomOut:if(editor) editor->SendMessage(SCI_ZOOMOUT);break;
        case kWrap:if(editor) editor->SendMessage(SCI_SETWRAPMODE,editor->SendMessage(SCI_GETWRAPMODE)==SC_WRAP_NONE?SC_WRAP_WORD:SC_WRAP_NONE);break;
        case kShowWhitespace:if(editor) editor->SendMessage(SCI_SETVIEWWS,editor->SendMessage(SCI_GETVIEWWS)==SCWS_INVISIBLE?SCWS_VISIBLEALWAYS:SCWS_INVISIBLE);break;
        default:BWindow::MessageReceived(message);
    }
}
}
