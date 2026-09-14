#include "ui/Workspace.h"
#include "ui/SymbolBar.h"
#include "ui/Editor.h"
#include "ui/Explorer.h"
#include "ui/GitView.h"
#include "ui/Messages.h"
#include "ui/PreviewView.h"
#include "ui/TabStrip.h"
#include "ui/TerminalPanel.h"
#include "ui/RefreshButton.h"
#include "ui/PreferencesWindow.h"
#include "ui/FileIcons.h"
#include "ui/RecentItems.h"
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
    SearchWindow(BWindow* owner,std::string root,std::shared_ptr<ProjectIndex> index,bool search,const Theme& theme,bool preview)
        :BWindow(BRect(0,0,700,420),search?"Search Project":"Open Quickly",B_TITLED_WINDOW_LOOK,B_FLOATING_APP_WINDOW_FEEL,B_AUTO_UPDATE_SIZE_LIMITS),
        fTarget(owner),fRoot(std::move(root)),fIndex(std::move(index)),fSearch(search),fTheme(theme) {
        fQuery=new BTextControl("query","", "",new BMessage(kQueryResult));fQuery->SetModificationMessage(new BMessage(kQueryChanged));
        fResults=new BListView("results");fResults->SetInvocationMessage(new BMessage(kQueryResult));
        if(preview) fResults->SetSelectionMessage(new BMessage(kQueryPreview));
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
        else if(message->what==kQueryResult || message->what==kQueryPreview) {
            int32 selected=fResults->CurrentSelection();if(selected<0 && !fMatches.empty()) selected=0;
            if(selected>=0 && selected<static_cast<int32>(fMatches.size())) {
                auto& match=fMatches[selected];BMessage request(kOpenFile);request.AddString("path",(fs::path(fRoot)/match.path).c_str());
                request.AddInt64("line",match.line);request.AddInt64("column",match.column);request.AddBool("preview",message->what==kQueryPreview);fTarget.SendMessage(&request);if(message->what==kQueryResult) PostMessage(B_QUIT_REQUESTED);
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

Workspace::Workspace(const std::string& settingsDirectory,bool restoreSession,const std::string& sessionDirectory):BWindow(BRect(40,70,1235,755),"Kiri",B_TITLED_WINDOW,B_ASYNCHRONOUS_CONTROLS|B_AUTO_UPDATE_SIZE_LIMITS) {
    BPath settings;find_directory(B_USER_SETTINGS_DIRECTORY,&settings);settings.Append("Kiri");
    fSettings=settingsDirectory.empty()?settings.Path():settingsDirectory;create_directory(fSettings.c_str(),0755);
    fSessionDirectory=sessionDirectory.empty()?fSettings:sessionDirectory;create_directory(fSessionDirectory.c_str(),0700);
    RecentItems(fSettings).Load();
    fLanguageTools.Load(fSettings);
    fJobs=std::make_unique<AsyncQueue>(BMessenger(this),2);
    fLoaderFactory=std::make_unique<Editor>();
    fRecoveryDirectory=fSessionDirectory+"/recovery";create_directory(fRecoveryDirectory.c_str(),0700);
    fSessionToken=std::to_string(time(nullptr))+"-"+std::to_string(getpid())+"-"+std::to_string(system_time());
    fRecoveryJobs=std::make_unique<AsyncQueue>(BMessenger(this));
    fExplorer=new Explorer();fExplorer->requestDirectory=[this](std::string path){LoadDirectory(path);};
    fRefresh=new RefreshButton(new BMessage(kRefresh));
    auto* sidebar=new BView("sidebar",0);
    BLayoutBuilder::Group<>(sidebar,B_VERTICAL,6).SetInsets(0,8,0,0)
        .AddGroup(B_HORIZONTAL,6).SetInsets(12,0,12,0).Add(new BStringView("files title","FILES"),0).Add(fRefresh,0).AddGlue().End()
        .Add(new BScrollView("explorer scroll",fExplorer,0,false,true,B_NO_BORDER));
    fLayout=std::make_unique<LayoutNode>();fLayout->pane=CreatePane();
    fActivePane=fLayout->pane;
    fPaneHost=new BView("editor panes",0);
    BLayoutBuilder::Group<>(fPaneHost,B_VERTICAL,0).Add(fLayout->View());
    fFindBar=new BView("find bar",0);fFindText=new BTextControl("find text","Find","",new BMessage(kFindNext));
    fReplaceText=new BTextControl("replace text","Replace","",new BMessage(kReplace));fMatchCase=new BCheckBox("case","Aa",nullptr);
    BLayoutBuilder::Group<>(fFindBar,B_HORIZONTAL,5).SetInsets(8,5,8,5).Add(fFindText).Add(fReplaceText).Add(fMatchCase)
        .Add(new BButton("previous","‹",new BMessage(kFindPrevious))).Add(new BButton("next","›",new BMessage(kFindNext)))
        .Add(new BButton("replace all","All",new BMessage(kReplaceAll))).Add(new BButton("hide find","×",new BMessage(kFind)));
    auto* editorPanel=new BView("editor panel",0);
    fSymbolBar=new SymbolBar();
    BLayoutBuilder::Group<>(editorPanel,B_VERTICAL,0).Add(fSymbolBar).Add(fFindBar).Add(fPaneHost);
    fFindBar->Hide();
    fGit=new GitView();auto* modes=new BView("workspace modes",0);fModeLayout=new BCardLayout();modes->SetLayout(fModeLayout);fModeLayout->AddView(editorPanel);fModeLayout->AddView(fGit);
    fTerminal=new TerminalPanel();
    fTerminalSplit=new BSplitView(B_VERTICAL,1);fTerminalSplit->AddChild(modes);fTerminalSplit->AddChild(fTerminal);
    fTerminalSplit->SetItemWeight(fTerminalSplit->GetLayout()->ItemAt(0),.74f);fTerminalSplit->SetItemWeight(fTerminalSplit->GetLayout()->ItemAt(1),.26f);
    fSidebarSplit=new BSplitView(B_HORIZONTAL,1);fSidebarSplit->AddChild(sidebar);fSidebarSplit->AddChild(fTerminalSplit);
    fSidebarSplit->SetItemWeight(fSidebarSplit->GetLayout()->ItemAt(0),.20f);fSidebarSplit->SetItemWeight(fSidebarSplit->GetLayout()->ItemAt(1),.80f);
    sidebar->SetExplicitMinSize(BSize(170,150));sidebar->SetExplicitMaxSize(BSize(600,B_SIZE_UNLIMITED));
    fStatus=new BStringView("status","Ready");fStatus->SetExplicitMinSize(BSize(200,25));
    auto* menu=BuildMenus();
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(menu).Add(fSidebarSplit).Add(fStatus);
    for(int32 i=0;i<CountChildren();++i) ThemeView(ChildAt(i),Theme::Builtins()[0]);
    ResizeTo(1195,685);
    RestoreSettings(restoreSession);ApplyTheme(fEditorSettings.theme);
    menu->FindItem(kPreviewTabs)->SetMarked(fPreviewTabs);
    BMessage pulse(kPulse);fPulse=std::make_unique<BMessageRunner>(BMessenger(this),&pulse,2000000);
    BMessage languages(kLanguageTick);fLanguageTimer=std::make_unique<BMessageRunner>(BMessenger(this),&languages,150000);
    if(fTerminal->Empty()) NewTerminal(false);
    BMessage opened(kWorkspaceOpened);opened.AddMessenger("workspace",BMessenger(this));be_app->PostMessage(&opened);
}
Workspace::~Workspace() {
    BMessage closed(kWorkspaceClosed);closed.AddMessenger("workspace",BMessenger(this));be_app->PostMessage(&closed);
    if(fPreferencesWindow.IsValid()) fPreferencesWindow.SendMessage(B_QUIT_REQUESTED);
    if(fLanguageToolsWindow.IsValid()) fLanguageToolsWindow.SendMessage(B_QUIT_REQUESTED);
    fDetachTimer.reset();fLanguageTimer.reset();for(auto& entry:fServers) if(entry.second.client) entry.second.client->Stop();fServers.clear();
    fPulse.reset();fRecoveryTimer.reset();fRecoveryJobs.reset();fJobs.reset();
}
void Workspace::WindowActivated(bool active) {
    BWindow::WindowActivated(active);
    if(active) { BMessage message(kWorkspaceActivated);message.AddMessenger("workspace",BMessenger(this));be_app->PostMessage(&message); }
}

BMenuBar* Workspace::BuildMenus() {
    auto* bar=new BMenuBar("menu");
    auto add=[](BMenu* menu,const char* title,uint32 what,char key=0,uint32 modifiers=0) {
        auto* message=new BMessage(what);
        if(what==kCloseTab || what==kCloseTerminal || what==kToggleTerminal || what==kKeepTab) message->AddBool("current_tab",true);
        if(what==kOpenFile) message->AddBool("file_picker",true);
        menu->AddItem(new BMenuItem(title,message,key,modifiers));
    };
    auto* file=new BMenu("File");add(file,"New File",kNewFile,'N');add(file,"Open File…",kOpenFile,'O');add(file,"Open Folder…",kOpenProject,'O',B_SHIFT_KEY);add(file,"Show Launcher…",kShowLauncher);file->AddSeparatorItem();
    add(file,"Save",kSave,'S');add(file,"Save As…",kSaveAs,'S',B_SHIFT_KEY);add(file,"Save All",kSaveAll);add(file,"Close Tab",kCloseTab,'W');add(file,"Keep Open",kKeepTab,'K');add(file,"Reopen Closed Tab",kReopenTab,'W',B_SHIFT_KEY);file->AddSeparatorItem();add(file,"Quit",kQuitApplication,'Q');bar->AddItem(file);
    auto* edit=new BMenu("Edit");add(edit,"Undo",B_UNDO,'Z');add(edit,"Redo",B_REDO,'Z',B_SHIFT_KEY);edit->AddSeparatorItem();add(edit,"Cut",B_CUT,'X');add(edit,"Copy",B_COPY,'C');add(edit,"Paste",B_PASTE,'V');add(edit,"Select All",B_SELECT_ALL,'A');edit->AddSeparatorItem();
    add(edit,"Format with Prettier",kFormatPrettier);add(edit,"Complete Code",kComplete);add(edit,"Language Tools…",kShowLanguageTools);
    edit->AddSeparatorItem();add(edit,"Preferences…",kShowPreferences,',');bar->AddItem(edit);
    auto* view=new BMenu("View");add(view,"Show / Hide Files",kToggleSidebar,'B');add(view,"Show / Hide Terminal",kToggleTerminal,'`');add(view,"Source Control",kToggleGit,'G',B_SHIFT_KEY);view->AddSeparatorItem();
    add(view,"Split Right",kSplitRight,'\\');add(view,"Split Down",kSplitDown,'\\',B_SHIFT_KEY);
    add(view,"Focus Next Pane",kNextPane,']',B_CONTROL_KEY);add(view,"Focus Previous Pane",kPreviousPane,'[',B_CONTROL_KEY);add(view,"Close Pane",kClosePane);
    add(view,"Preview Tabs",kPreviewTabs);view->FindItem(kPreviewTabs)->SetMarked(fPreviewTabs);view->AddSeparatorItem();
    fThemes=ThemeMenu("Color Theme",kTheme,0);fThemes->SetLabelFromMarked(false);
    view->AddItem(fThemes);add(view,"Zoom In",kZoomIn,'+');add(view,"Zoom Out",kZoomOut,'-');add(view,"Word Wrap",kWrap);add(view,"Show Whitespace",kShowWhitespace);bar->AddItem(view);
    auto* search=new BMenu("Search");add(search,"Find / Replace",kFind,'F');add(search,"Find Next",kFindNext);add(search,"Find Previous",kFindPrevious);add(search,"Open Quickly…",kQuickOpen,'P');add(search,"Search Project…",kProjectSearch,'F',B_SHIFT_KEY);add(search,"Go to Line…",kGoToLine,'G');add(search,"Go to Symbol…",kBrowseSymbols,'R',B_SHIFT_KEY);bar->AddItem(search);
    auto* git=new BMenu("Git");add(git,"Show Source Control",kToggleGit);add(git,"File History",kFileHistory);add(git,"Copy GitHub Permalink",kCopyPermalink,'L',B_SHIFT_KEY);git->AddSeparatorItem();add(git,"Refresh",kGitRefresh);add(git,"Fetch",kGitFetch);add(git,"Pull (Fast-forward Only)",kGitPull);add(git,"Push",kGitPush);bar->AddItem(git);
    auto* term=new BMenu("Terminal");add(term,"New Terminal",kNewTerminal,'T',B_SHIFT_KEY);add(term,"Close Terminal",kCloseTerminal);term->AddSeparatorItem();add(term,"Show / Hide",kToggleTerminal);bar->AddItem(term);
    return bar;
}
Workspace::Document* Workspace::ByID(int64 id) { for(auto& document:fDocuments) if(document->id==id) return document.get();return nullptr; }
void Workspace::Notice(const std::string& text) { fStatus->SetText(text.c_str());fStatus->SetToolTip(text.c_str()); }
void Workspace::UpdateStatus() {
    auto* d=Current();
    if(fSessionDirectory!=fSettings) {
        auto title=d?d->name+" — Kiri":std::string("Kiri");if(title!=Title()) SetTitle(title.c_str());
    }
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
void Workspace::NewFile() {
    auto document=std::make_unique<Document>();document->id=fNextID++;document->name="Untitled "+std::to_string(document->id);
    document->editor=new Editor();document->editor->SetText("");document->editor->ApplySettings(fEditorSettings);document->view=document->editor;
    int index=AddTab(*fActivePane,*document,false,true);fDocuments.push_back(std::move(document));SelectTab(index);SaveSettings();
}
void Workspace::NewTerminal(bool focus) {
    if(focus) ++fFocusSerial;
    fTerminalSplit->SetItemCollapsed(1,false);
    fTerminal->NewTerminal(fProject.empty()?"/boot/home":fProject,focus);
}
void Workspace::FinishTerminalClose() {
    if(!fTerminal->Empty()) return;
    fTerminalSplit->SetItemCollapsed(1,true);
    FocusWorkspace();
}
void Workspace::FocusWorkspace() {
    if(fModeLayout->VisibleIndex()==1) fGit->FindView("changes")->MakeFocus();
    else if(auto* document=Current();document && document->editor) document->editor->MakeFocus();
    else if(auto* focus=CurrentFocus()) focus->MakeFocus(false);
}

void Workspace::OpenProject(const std::string& input) {
    auto path=CanonicalPath(input);std::error_code error;
    if(!fs::is_directory(path,error)) { Notice("Cannot open that folder.");return; }
    if(!fRestoring) RememberRecent(path,true);
    bool changed=fProject!=path;
    if(changed) ResetLanguages();
    fProject=path;++fGeneration;fIndex=std::make_shared<ProjectIndex>();fGitRoot.clear();fExplorer->Clear();fGit->SetRepository("");
    SetTitle((fs::path(path).filename().string()+" — Kiri").c_str());
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
    // A project change gets a fresh shell without interrupting existing tabs.
    if(changed && !fTerminal->Empty()) NewTerminal(fTerminal->OwnsFocus());
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
void Workspace::OpenFile(const std::string& input,size_t line,size_t column,bool activate,bool preview,int64 paneID,bool focusEditor) {
    auto path=CanonicalPath(input);auto* pane=paneID?FindPane(paneID):fActivePane;if(!pane) return;paneID=pane->id;
    if(fRestoring && fRestoringDrafts) { if(std::find(fRestorePaths.begin(),fRestorePaths.end(),path)==fRestorePaths.end()) fRestorePaths.push_back(path);return; }
    preview=preview && fPreviewTabs;PromotePreviews();
    auto paneSerial=++pane->openSerial;auto focus=activate?++fFocusSerial:0;
    int64 reloadID=0,reloadRevision=0;
    for(auto& document:fDocuments) if(document->path==path) {
        auto* d=document.get();
        if(activate) RememberRecent(path,false);
        if(activate && !preview && d->external && d->editor) {
            int32 choice=(new BAlert("Reload File","This file changed on disk. Reloading replaces its contents in every pane and clears its undo history.","Cancel","Reload",nullptr,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();
            if(choice!=1) return;reloadID=d->id;reloadRevision=d->editor->InputRevision();break;
        }
        ShowDocument(*pane,*d,line,column,preview,activate,focusEditor);SaveSettings();return;
    }
    if(fPendingOpen.count(path)) {
        fPendingRequests[path].push_back([this,path,line,column,activate,preview,paneID,focusEditor,paneSerial] {
            auto* pane=FindPane(paneID);if(pane && (!preview || pane->openSerial==paneSerial)) OpenFile(path,line,column,activate,preview,paneID,focusEditor);
        });return;
    }
    fPendingOpen.insert(path);Notice("Opening "+path+"…");
    auto* original=Current();auto originalID=original?original->id:0,originalRevision=original && original->editor?original->editor->InputRevision():0;
    auto loader=fLoaderFactory->CreateLoader(StatFile(path).size>8*1024*1024);
    fJobs->Submit([this,path,line,column,loader,activate,preview,paneID,paneSerial,focusEditor,focus,originalID,originalRevision,reloadID,reloadRevision](const auto& cancel) {
        struct Loaded { FileData data;std::shared_ptr<BBitmap> image; };
        auto loaded=std::make_shared<Loaded>();
        std::string extension=fs::path(path).extension().string();for(auto& c:extension) c=std::tolower(static_cast<unsigned char>(c));
        const std::set<std::string> imageTypes={".png",".jpg",".jpeg",".gif",".bmp",".webp",".tga",".ico",".tif",".tiff",".icns"};
        if(imageTypes.count(extension)) { loaded->image.reset(BTranslationUtils::GetBitmap(path.c_str()));loaded->data.stamp=StatFile(path); }
        if(!loaded->image) {
            if(!loader->loader) loaded->data.error="Not enough memory to open this document.";
            else loaded->data=ReadFile(path,&cancel,1024ULL*1024*1024,[&](std::string_view chunk){return loader->loader->AddData(chunk.data(),chunk.size())==SC_STATUS_OK;});
        }
        return [this,path,line,column,loaded,loader,activate,preview,paneID,paneSerial,focusEditor,focus,originalID,originalRevision,reloadID,reloadRevision] {
            fPendingOpen.erase(path);
            auto requests=std::move(fPendingRequests[path]);fPendingRequests.erase(path);
            auto finish=[&] { for(auto& request:requests) request();FinishRestore();SaveSettings(); };
            auto* pane=FindPane(paneID);
            if(!loaded->data.ok()) {
                Notice(loaded->data.error);if(!preview) (new BAlert("Open File",loaded->data.error.c_str(),"OK"))->Go();FinishRestore();return;
            }
            if(!pane || (preview && pane->openSerial!=paneSerial)) { finish();return; }
            auto d=std::make_unique<Document>();d->id=fNextID++;d->path=path;d->name=fs::path(path).filename().string();d->stamp=loaded->data.stamp;
            if(loaded->image) {
                d->view=new PreviewView(loaded->image,d->name,d->stamp.size);
                static_cast<PreviewView*>(d->view)->ApplyTheme(Theme::Builtins()[fEditorSettings.theme]);
            } else {
                d->editor=new Editor();d->view=d->editor;bool binary=loaded->data.binary || !loaded->data.utf8;d->bom=loaded->data.bom;
                if(binary) {
                    d->editor->SetText(HexPreview(loaded->data.bytes,d->stamp.size),true);
                    if(!loaded->data.utf8 && !loaded->data.binary) d->name+=" · non-UTF-8";
                } else d->editor->Adopt(*loader,loaded->data.eol);
                d->editor->SetLanguage(binary?"preview.txt":path,!binary && d->stamp.size>8*1024*1024);d->editor->ApplySettings(fEditorSettings);
            }
            if(reloadID) {
                auto* existing=ByID(reloadID);
                if(!existing || !existing->editor || existing->editor->InputRevision()!=reloadRevision || !d->editor) {
                    delete d->view;Notice("Reload was not applied because the document changed or closed.");finish();return;
                }
                CancelCompletion();CloseLanguage(*existing);ClearRecovery(*existing);++existing->formatSerial;
                d->editor->State()->revision=reloadRevision+1;
                for(auto& group:fPanes) for(auto& tab:group->tabs) if(tab->document==reloadID) {
                    auto state=ViewState(*tab);tab->editor->ShareDocument(*d->editor);RestoreView(*tab,state);
                }
                existing->buffer=d->editor->State();existing->stamp=d->stamp;existing->bom=d->bom;existing->name=d->name;existing->external=false;
                delete d->view;UpdateTabs();finish();return;
            }
            auto* original=ByID(originalID);
            bool front=activate && focus==fFocusSerial && (!original || !original->editor || original->editor->InputRevision()==originalRevision);
            // An explicit file open may finish in the background; a stale
            // preview must never replace a newer choice or take input focus.
            if(preview && !front) { delete d->view;finish();return; }
            auto* document=d.get();fDocuments.push_back(std::move(d));
            ShowDocument(*pane,*document,line,column,preview,front,focusEditor,true);
            if(activate) RememberRecent(path,false);
            auto restored=fRestoreDocuments.find(path);
            if(restored!=fRestoreDocuments.end()) for(auto& tab:pane->tabs) if(tab->document==document->id) RestoreView(*tab,restored->second);
            finish();
        };
    },{},[this,path,preview](const std::string& error) {
        fPendingOpen.erase(path);fPendingRequests.erase(path);Notice("Open failed: "+error);FinishRestore();
    });
}
void Workspace::Save(Document* d,bool saveAs) {
    if(!d || !d->editor || d->saving || d->editor->SendMessage(SCI_GETREADONLY)) return;
    for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==d->id) tab->preview=false;
    UpdateTabs();
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
        d->closeAfterSave=false;CancelQuit();fSaveQueue.clear();fCloseQueue.clear();fClosingPane=0;
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
                document->closeAfterSave=false;CancelQuit();fSaveQueue.clear();fCloseQueue.clear();fClosingPane=0;Notice(error);(new BAlert("Save File",error.c_str(),"OK"))->Go();return;
            }
            if(document->path!=path) CloseLanguage(*document);
            document->path=path;document->name=fs::path(path).filename().string();document->stamp=stamp;document->external=false;
            RememberRecent(path,false);
            std::string_view saved(*text);if(document->bom) saved.remove_prefix(3);
            if(document->editor->Matches(saved)) { document->editor->MarkSaved();ClearRecovery(*document); }
            for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==id && tab->editor) tab->editor->SetLanguage(path,stamp.size>8*1024*1024);UpdateTabs();SaveSettings();
            if(auto* server=EnsureLanguage(*document);server && server->Ready()) { SyncLanguage(*document,*server);server->Save(document->serverURI,std::string(saved)); }
            if(document->closeAfterSave) {
                document->closeAfterSave=false;
                CloseView(document->closeView);
            }
            fGit->Refresh();
            RefreshIndex();
            if(fQuitWhenSaved) PostMessage(B_QUIT_REQUESTED);
            ContinueSaveAll();
            ContinueCloseTabs();
        };
    },{},[this,id](const std::string& error) {
        if(auto* document=ByID(id)) { document->saving=false;document->closeAfterSave=false; }
        CancelQuit();fSaveQueue.clear();fCloseQueue.clear();fClosingPane=0;Notice("Save failed: "+error);
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
void Workspace::CancelQuit() {
    if(fQuitWhenSaved) { BMessage message(kWorkspaceQuitCancelled);message.AddMessenger("workspace",BMessenger(this));be_app->PostMessage(&message); }
    fQuitWhenSaved=false;fQuitDiscarded.clear();
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
        if(choice==0) { CancelQuit();return false; }
        if(choice==2) { fQuitWhenSaved=true;Save(d.get());return false; }
        fQuitDiscarded[d->id]=d->editor->Revision();
    }
    fRecoveryTimer.reset();fSnapshot.reset();fRecoveryJobs.reset();
    for(const auto& d:fDocuments) if(!d->recoveryFile.empty()) fRetiredDrafts.insert(d->recoveryFile);
    for(const auto& file:fRetiredDrafts) unlink(file.c_str());
    SaveSettings();
    // A deliberately closed detached window must not reappear at next launch.
    // Its private session is retained on abnormal exit for recovery instead.
    if(fSessionDirectory!=fSettings) {
        unlink((fSessionDirectory+"/settings").c_str());rmdir(fRecoveryDirectory.c_str());rmdir(fSessionDirectory.c_str());
    }
    return true;
}
void Workspace::ApplyTheme(int index) {
    fEditorSettings.theme=std::clamp(index,0,static_cast<int>(Theme::Builtins().size())-1);const auto& theme=Theme::Builtins()[fEditorSettings.theme];
    for(int32 i=0;i<CountChildren();++i) ThemeView(ChildAt(i),theme);
    auto* filesTitle=static_cast<BStringView*>(FindView("files title"));
    BSize titleSize(filesTitle->StringWidth(filesTitle->Text())+2,B_SIZE_UNSET);
    filesTitle->SetExplicitMinSize(titleSize);filesTitle->SetExplicitMaxSize(titleSize);
    fExplorer->ApplyTheme(theme);fRefresh->ApplyTheme(theme);fTerminal->ApplyTheme(theme);fGit->ApplySettings(fEditorSettings);fSymbolBar->ApplyTheme(theme);
    for(auto& pane:fPanes) {
        pane->strip->ApplyTheme(theme);
        for(auto& tab:pane->tabs) {
            if(tab->editor) tab->editor->ApplySettings(fEditorSettings);
            if(auto* preview=dynamic_cast<PreviewView*>(tab->view)) preview->ApplyTheme(theme);
        }
    }
    MarkTheme(fThemes,fEditorSettings.theme);
    if(fPreferencesWindow.IsValid()) { BMessage settings(kSyncPreferences);fEditorSettings.WriteTo(settings);fPreferencesWindow.SendMessage(&settings); }
    SaveSettings();
    be_app->PostMessage(kRecentsChanged);
}
void Workspace::ShowPreferences() {
    if(fPreferencesWindow.IsValid()) { fPreferencesWindow.SendMessage(kShowPreferences);return; }
    auto* window=new PreferencesWindow(BMessenger(this),fEditorSettings,Frame());
    fPreferencesWindow=BMessenger(window);window->Show();
}
void Workspace::ShowFind() {
    if(!fFindBar->IsHidden()) { fFindBar->Hide();if(Current() && Current()->editor) Current()->editor->MakeFocus();return; }
    fFindBar->Show();fFindText->MakeFocus();fFindText->TextView()->SelectAll();
}
void Workspace::Search(bool search) {
    if(fProject.empty()) { Notice("Open a project folder first.");return; }
    new SearchWindow(this,fProject,fIndex,search,Theme::Builtins()[fEditorSettings.theme],fPreviewTabs);
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

void Workspace::RememberRecent(const std::string& path,bool folder) {
    if(RecentItems(fSettings).Remember(path,folder)) be_app->PostMessage(kRecentsChanged);
}
void Workspace::RestoreSettings(bool restoreSession) {
    BFile file((fSessionDirectory+"/settings").c_str(),B_READ_ONLY);BMessage settings;
    bool hasSession=file.InitCheck()==B_OK;
    if(file.InitCheck()!=B_OK && fSessionDirectory!=fSettings) file.SetTo((fSettings+"/settings").c_str(),B_READ_ONLY);
    settings.Unflatten(&file);fRestoring=true;
    fEditorSettings.ReadFrom(settings);bool previewTabs=true;if(settings.FindBool("preview_tabs",&previewTabs)==B_OK) fPreviewTabs=previewTabs;
    BRect frame;if(settings.FindRect("frame",&frame)==B_OK && frame.Width()>600 && frame.Height()>400) { MoveTo(frame.LeftTop());ResizeTo(frame.Width(),frame.Height()); }
    if(restoreSession && hasSession) {
        settings.FindMessage("editor_layout",&fRestoreLayout);
        const char* project=nullptr;if(settings.FindString("project",&project)==B_OK && project && *project) OpenProject(project);
        const char* path=nullptr;
        for(int32 i=0;settings.FindString("file",i,&path)==B_OK;++i) fRestorePaths.push_back(path);
        BMessage state;
        for(int32 i=0;settings.FindMessage("document",i,&state)==B_OK;++i)
            if(state.FindString("path",&path)==B_OK) fRestoreDocuments[path]=state;
        if(settings.FindString("selected",&path)==B_OK) fRestoreSelected=path;
    }
    auto drafts=ListDrafts(fRecoveryDirectory);fRestoringDrafts=drafts.size();
    for(const auto& draft:drafts) RestoreDraft(draft);
    FinishRestore();
}
void Workspace::SaveSettings() {
    if(fRestoring) return;
    BMessage settings;fEditorSettings.WriteTo(settings);settings.AddRect("frame",Frame());settings.AddString("project",fProject.c_str());
    settings.AddBool("preview_tabs",fPreviewTabs);auto layout=LayoutState(*fLayout);settings.AddMessage("editor_layout",&layout);
    if(auto* d=Current()) settings.AddString("selected",d->path.empty()?d->recoveryFile.c_str():d->path.c_str());
    for(const auto& d:fDocuments) if(!d->path.empty()) {
        settings.AddString("file",d->path.c_str());BMessage state;state.AddString("path",d->path.c_str());
        if(d->editor) {
            state.AddInt64("caret",d->editor->SendMessage(SCI_GETCURRENTPOS));state.AddInt64("anchor",d->editor->SendMessage(SCI_GETANCHOR));
            state.AddInt64("first",d->editor->SendMessage(SCI_GETFIRSTVISIBLELINE));state.AddInt32("zoom",d->editor->SendMessage(SCI_GETZOOM));state.AddInt32("wrap",d->editor->SendMessage(SCI_GETWRAPMODE));
        }
        settings.AddMessage("document",&state);
    }
    BFile file((fSessionDirectory+"/settings.tmp").c_str(),B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
    if(file.InitCheck()==B_OK && settings.Flatten(&file)==B_OK) { file.Sync();rename((fSessionDirectory+"/settings.tmp").c_str(),(fSessionDirectory+"/settings").c_str()); }
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
    if(!fFocusSerial && !fRestoreLayout.IsEmpty()) RestoreLayout();
    else if(!fFocusSerial && !fDocuments.empty()) {
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
            d->editor->SetLanguage(d->path,d->editor->SendMessage(SCI_GETLENGTH)>8*1024*1024);d->editor->ApplySettings(fEditorSettings);
            d->recoveryRevision=d->editor->Revision();d->lastRecovery=system_time();
            AddTab(*fActivePane,*d,false,true);fDocuments.push_back(std::move(d));
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
    if(message->what==kTabDragUpdate || message->what==kTabDragEnd || message->what==kTabDragCancel) { TrackTabDrag(*message);return; }
    if(message->what==kDetachTabReady) {
        auto id=fDetachTab;auto point=fDetachPoint;fDetachTimer.reset();fDetachTab=0;
        if(id) DetachTab(id,point);return;
    }
    // BMenuItem appends its menu position as "index". These menu commands
    // operate on the current tab; that position is not a document/session index.
    bool currentTab=false;
    if(message->FindBool("current_tab",&currentTab)==B_OK && currentTab) message->RemoveName("index");
    SyncFocus();
    int64 paneID=0;if(message->FindInt64("pane",&paneID)==B_OK) {
        auto* pane=FindPane(paneID);if(!pane) return;ActivatePane(pane);
    }
    auto* d=Current();auto* editor=d?d->editor:nullptr;
    auto tabIndex=[&] {
        int64 id;
        if(message->FindInt64("tab_id",&id)==B_OK) {
            for(size_t i=0;i<fActivePane->tabs.size();++i) if(fActivePane->tabs[i]->id==id) return static_cast<int>(i);
            return -1;
        }
        int32 index;
        return message->FindInt32("index",&index)==B_OK?static_cast<int>(index):fActivePane->selected;
    };
    switch(message->what) {
        case kLanguageTick:LanguageTick();break;
        case kShowLanguageTools:ShowLanguageTools();break;
        case kApplyLanguageTools: {
            auto error=fLanguageTools.Load(fSettings);
            if(error.empty()) { ResetLanguages();UpdateSymbolBar(); }else Notice(error);
            break;
        }
        case kFormatPrettier:case kComplete:case kCompletionChosen: {
            void* source=nullptr;if(message->FindPointer("editor",&source)!=B_OK) source=editor;
            Editor* target=ByEditor(source)?static_cast<Editor*>(source):nullptr;
            if(message->what==kFormatPrettier) FormatDocument(target);
            else if(message->what==kComplete) Complete(target);
            else { const char* label;if(message->FindString("label",&label)==B_OK) AcceptCompletion(target,label); }
            break;
        }
        case kCancelCompletion:CancelCompletion();break;
        case kEditorText:case kEditorTyped: {
            void* source=nullptr;message->FindPointer("editor",&source);
            if(auto* document=ByEditor(source)) {
                if(message->what==kEditorText) {
                    UpdateTabs();
                    if(fCompletionDocument==document->id) CancelCompletion();
                    document->languageDirty=true;document->textChangedAt=system_time();
                    if(document==Current()) UpdateSymbolBar();
                } else if(document==Current() && fLanguageTools.completion) {
                    int32 character=0;message->FindInt32("character",&character);
                    fTypedDocument=document->id;fTypedAt=system_time();fTypedCharacter=character;
                }
            }
            break;
        }
        case kEditorFocus:SyncFocus();break;
        case kBrowseSymbols:fModeLayout->SetVisibleItem(int32(0));fSymbolBar->Browse();break;
        case kSymbolChosen: {
            int64 document=0,version=0;int32 symbol=-1;
            message->FindInt64("document",&document);message->FindInt64("version",&version);message->FindInt32("symbol",&symbol);
            auto* selected=ByID(document);
            if(selected && selected==d && selected->editor && selected->symbolVersion==version && !selected->languageDirty
                    && symbol>=0 && symbol<static_cast<int32>(selected->symbols.size()) && selected->editor->Matches(selected->serverText)) {
                auto offset=selected->symbols[symbol].selection;CancelCompletion();
                ++fFocusSerial;fModeLayout->SetVisibleItem(int32(0));
                auto line=editor->SendMessage(SCI_LINEFROMPOSITION,offset);editor->SendMessage(SCI_ENSUREVISIBLEENFORCEPOLICY,line);
                editor->SendMessage(SCI_GOTOPOS,offset);editor->SendMessage(SCI_SCROLLCARET);editor->MakeFocus();
            }
            break;
        }
        case kQuitApplication:be_app->PostMessage(B_QUIT_REQUESTED);break;
        case kShowLauncher:be_app->PostMessage(kShowLauncher);break;
        case kActivateWorkspace:Activate();break;
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
                int64 line=1,column=1;message->FindInt64("line",&line);message->FindInt64("column",&column);bool preview=false;message->FindBool("preview",&preview);
                if(preview && !fPreviewTabs) break;
                OpenFile(path,line,column,true,preview,0,!preview);break;
            }
            int32 index;
            if(!message->HasBool("file_picker") && message->FindInt32("index",&index)==B_OK) {
                auto* item=static_cast<FileItem*>(fExplorer->ItemAt(index));if(item && !item->placeholder && !item->entry.directory) OpenFile(item->entry.path);break;
            }
            BMessage selected(kFileChosen);BMessenger target(this);
            fOpenPanel=std::make_unique<BFilePanel>(B_OPEN_PANEL,&target,nullptr,B_FILE_NODE,true,&selected);
            if(!fProject.empty()) fOpenPanel->SetPanelDirectory(fProject.c_str());fOpenPanel->Show();break;
        }
        case kNewFile:NewFile();break;
        case kKeepTab:KeepTab(tabIndex());break;
        case kReopenTab:ReopenTab();break;
        case kSplitRight:case kSplitDown:
            if(message->HasInt64("tab_id")) SelectTab(tabIndex());
            SplitPane(message->what==kSplitRight?B_HORIZONTAL:B_VERTICAL);break;
        case kNextPane:CyclePane(1);break;
        case kPreviousPane:CyclePane(-1);break;
        case kClosePane:fClosingPane=fActivePane->id;CloseTabs();break;
        case kPreviewTabs:
            fPreviewTabs=!fPreviewTabs;
            if(!fPreviewTabs) for(auto& pane:fPanes) for(auto& tab:pane->tabs) tab->preview=false;
            if(auto* menu=dynamic_cast<BMenuBar*>(FindView("menu"))) if(auto* item=menu->FindItem(kPreviewTabs)) item->SetMarked(fPreviewTabs);
            UpdateTabs();SaveSettings();break;
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
            CancelQuit();fSaveQueue.clear();fCloseQueue.clear();fClosingPane=0;if(auto* document=ByID(fSavePanelID)) document->closeAfterSave=false;break;
        }
        case kCloseTab: {
            if(!message->HasInt64("tab_id") && !message->HasInt32("index") && fTerminal->OwnsFocus()) {
                fTerminal->CloseTerminal(fTerminal->IndexForMessage(*message));FinishTerminalClose();
            } else CloseTab(tabIndex());
            break;
        }
        case kSelectTab:SelectTab(tabIndex());break;
        case kCloseAllTabs:CloseTabs();break;
        case kCloseOtherTabs:if(auto index=tabIndex();index>=0) CloseTabs(index);break;
        case kEditorChanged:case kEditorPosition: {
            void* source=nullptr;message->FindPointer("editor",&source);
            if(message->what==kEditorChanged) {
                if(auto* document=ByEditor(source)) ++document->revision;
                UpdateTabs();
            } else if(editor==source) {
                UpdateStatus();if(editor) fSymbolBar->SetPosition(editor->SendMessage(SCI_GETCURRENTPOS));
                if(editor && fCompletionDocument==d->id && editor->SendMessage(SCI_GETCURRENTPOS)!=fCompletionCaret) CancelCompletion();
            }
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
        case kShowPreferences:ShowPreferences();break;
        case kApplyPreferences:fEditorSettings.ReadFrom(*message);ApplyTheme(fEditorSettings.theme);break;
        case kTheme: {
            const char* name;int32 index;
            if(message->FindString("theme_name",&name)==B_OK) ApplyTheme(ThemeIndex(name));
            else if(message->FindInt32("index",&index)==B_OK) ApplyTheme(index);
            break;
        }
        case kToggleTerminal:
            if(fTerminal->Empty()) NewTerminal();
            else {
                bool show=fTerminalSplit->IsItemCollapsed(1);fTerminalSplit->SetItemCollapsed(1,!show);
                if(show) { ++fFocusSerial;fTerminal->SelectTerminal(fTerminal->IndexForMessage(*message)); }
                else FocusWorkspace();
            }
            break;
        case kToggleSidebar:fSidebarSplit->SetItemCollapsed(0,!fSidebarSplit->IsItemCollapsed(0));break;
        case kToggleGit:++fFocusSerial;fModeLayout->SetVisibleItem(fModeLayout->VisibleIndex()==0?1:0);break;
        case kNewTerminal:NewTerminal();break;
        case kSelectTerminal:++fFocusSerial;fTerminal->SelectTerminal(fTerminal->IndexForMessage(*message));break;
        case kCloseTerminal:fTerminal->CloseTerminal(fTerminal->IndexForMessage(*message));FinishTerminalClose();break;
        case kCloseAllTerminals:fTerminal->CloseTerminals();FinishTerminalClose();break;
        case kCloseOtherTerminals:
            if(auto index=fTerminal->IndexForMessage(*message);index>=0) fTerminal->CloseTerminals(index);
            break;
        case kTerminalState:fTerminal->UpdateTabs();break;
        case kTerminalFocus:++fFocusSerial;break;
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
