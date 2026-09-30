#pragma once
#include "core/FileIO.h"
#include "core/Project.h"
#include "core/ProjectMonitor.h"
#include "core/Diff.h"
#include "core/Recovery.h"
#include "ui/Async.h"
#include "ui/Theme.h"
#include "ui/EditorSettings.h"
#include "core/LanguageTools.h"
#include "core/LanguageServer.h"
#include "core/LanguageAnalysis.h"
#include "core/EditTransaction.h"
#include <Window.h>
#include <memory>
#include <set>
#include <map>
#include <deque>
#include <vector>
class BFilePanel;class BCardLayout;class BSplitView;class BStringView;
class BTextControl;class BCheckBox;class BMessageRunner;
class BMenuBar;class BMenuField;class BButton;
namespace kiri {
class Editor;struct EditorState;class Explorer;class GitView;class TerminalPanel;class TabStrip;class RefreshButton;class SymbolBar;class ProblemsView;
class Workspace:public BWindow {
public:
    explicit Workspace(const std::string& settingsDirectory={},bool restoreSession=true,const std::string& sessionDirectory={});
    ~Workspace() override;
    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
    void WindowActivated(bool active) override;
    void OpenProject(const std::string& path);
    void OpenFile(const std::string& path,size_t line=1,size_t column=1,bool activate=true,bool preview=false,int64 pane=0,bool focusEditor=true);
private:
    friend struct WorkspaceTestAccess;
    struct ExternalChange { FileStamp diskStamp;int64 revision=0;std::string backupDirectory,error;std::shared_ptr<DiffModel> comparison; };
    struct Document {
        int64 id=0,revision=0;
        std::string path,name;
        BView* view=nullptr;
        Editor* editor=nullptr;
        // Non-owning representative view above; tabs own view lifetimes. Buffer
        // metadata, disk state, recovery and language-server state are per file.
        std::shared_ptr<EditorState> buffer;
        int64 configSerial=0;bool configPending=false;
        BMessenger settingsWindow;
        int64 closeView=0;
        FileStamp stamp;
        bool bom=false,saving=false,closeAfterSave=false,external=false;
        bool externalBusy=false;
        int64 externalSerial=0;
        FileStamp externalObserved;
        std::shared_ptr<ExternalChange> externalChange;
        BMessenger externalWindow;
        std::string recoveryFile;
        int64 recoveryRevision=-1,lastRecovery=0;
        bool recovering=false;
        std::string serverKey,serverURI,serverText,serverLanguage,languageStatus,symbolError;
        int serverVersion=0,symbolVersion=-1;
        int64 symbolRequest=0,completionRequest=0,textChangedAt=0,formatSerial=0,formatView=0;
        bool languageDirty=true;
        std::vector<DocumentSymbol> symbols;
        int64 serverRevision=-1,analysisRevision=-1,diagnosticSerial=0,semanticSerial=0,semanticRequest=0,diagnosticWaitAt=0;
        int semanticVersion=-1;
        std::vector<Diagnostic> diagnostics;
        std::vector<SemanticToken> semanticTokens;
        std::string diagnosticStatus,semanticStatus;
    };
    Document* Current();
    Document* ByID(int64 id);
    struct Tab { int64 id=0,document=0,previewChanges=0;BView* view=nullptr;Editor* editor=nullptr;bool preview=false; };
    struct Pane {
        int64 id=0,openSerial=0;int selected=-1;
        BView* panel=nullptr;TabStrip* strip=nullptr;BCardLayout* cards=nullptr;
        std::vector<std::unique_ptr<Tab>> tabs;
    };
    struct LayoutNode {
        Pane* pane=nullptr;BSplitView* split=nullptr;
        std::unique_ptr<LayoutNode> first,second;
        BView* View() const;
    };
    struct ClosedTab { std::string path;int64 pane=0;BMessage state; };
    Pane* CreatePane();
    Pane* FindPane(int64 id);
    Tab* FindTab(int64 id,Pane** pane=nullptr);
    Tab* CurrentTab();
    Document* ByEditor(const void* editor);
    void SyncFocus();
    void ActivatePane(Pane* pane,bool focus=false);
    int AddTab(Pane& pane,Document& document,bool preview=false,bool firstView=false);
    void ShowDocument(Pane& pane,Document& document,size_t line,size_t column,bool preview,bool activate,bool focusEditor,bool firstView=false);
    void PromotePreviews();
    void KeepTab(int index);
    void SetMarkdownPreview(Tab& tab,bool enabled);
    void OpenMarkdownLink(const BMessage& message);
    void SplitPane(orientation direction);
    void CyclePane(int step);
    void RemoveEmptyPane(Pane* pane);
    std::unique_ptr<Tab> TakeTab(Pane& pane,int index);
    bool MoveTab(int64 tab,Pane& destination,int slot);
    Workspace* DetachTab(int64 tab,BPoint screenPoint);
    void TrackTabDrag(BMessage& message);
    void RefreshRepresentative(Document& document);
    size_t ViewCount(int64 document) const;
    bool CloseView(int64 id,bool remember=true,bool collapse=true);
    void ReopenTab();
    BMessage ViewState(const Tab& tab) const;
    void RestoreView(Tab& tab,const BMessage& state);
    BMessage LayoutState(const LayoutNode& node) const;
    void RestoreLayout();
    std::unique_ptr<LayoutNode> ReadLayout(const BMessage& state,int depth=0);
    void CollectPanes(LayoutNode& node,std::vector<Pane*>& panes);
    BMenuBar* BuildMenus();
    void UpdateTabs();
    void UpdateStatus();
    void SelectTab(int index);
    bool CloseTab(int index);
    void CloseTabs(int keep=-1);
    void ContinueCloseTabs();
    void NewTerminal(bool focus=true);
    void FinishTerminalClose();
    void FocusWorkspace();
    void NewFile();
    void Save(Document* document,bool saveAs=false);
    void SaveTo(int64 id,const std::string& path);
    void ContinueSaveAll();
    void CancelQuit();
    void ApplyTheme(int index=-1);
    void ShowPreferences();
    void ShowDocumentSettings();
    void RefreshDocumentConfig(Document& document,bool force=false);
    void ApplyDocumentSettings(BMessage& message);
    void UpdateDocumentStyle(Document& document);
    void ShowFind();
    SearchOptions FindOptions() const;
    void RunFind(uint32 command);
    void RememberQuery();
    void Search(bool projectSearch);
    SnapshotMap OpenSnapshots();
    EditSnapshot CurrentSnapshot(const EditSnapshot& before);
    EditSnapshot BufferSnapshot(Document& document);
    bool VisitOpenDocument(const std::string& path,const std::function<void(Workspace&,Document&)>& action);
    std::string ApplyBufferEdit(FileEdit& file,bool restore);
    void RegisterEditWorkspace(bool add);
    bool ReserveEditPaths(bool reserve);
    bool EditPathBusy(const std::string& path) const;
    void PreviewReplacement(const BMessage& message);
    void ShowEditPreview(std::shared_ptr<EditPlan> plan,bool restore=false);
    void ApplyProjectEdit(bool restore=false);
    void ProjectEditStep();
    void UndoProjectEdit();
    void RenameSymbol(Editor* source);
    void SubmitRename(const BMessage& message);
    void ClearRenameBorrowed();
    void PromptLine();
    void CopyPermalink();
    void LoadDirectory(const std::string& path);
    void RefreshIndex();
    void SetIndex(std::shared_ptr<ProjectIndex> index,bool complete);
    void DrainJobs();
    void RestoreSettings(bool restoreSession);
    void RememberRecent(const std::string& path,bool folder);
    void SaveSettings();
    void FinishRestore();
    void RestoreDraft(const std::string& file);
    void StartRecovery();
    void RecoveryTick();
    void ClearRecovery(Document& document);
    void Pulse();
    void CheckExternalFiles();
    void CaptureExternal(Document& document,uint32 action=0);
    void ReloadExternal(Document& document,bool reviewed=false);
    void ReloadImage(Document& document);
    void ResolveExternal(BMessage& message);
    void UpdateExternalBar();
    void OpenExternalBackups(const std::string& directory);
    void PollProject();
    using WatchList=std::vector<std::pair<std::string,std::pair<uint64_t,uint64_t>>>;
    void UpdateNodeWatches(const WatchList& directories);
    void RefreshSearchWindows(bool indexOnly=false);
    void Notice(const std::string& text);
    void FormatDocument(Editor* source);
    void ShowLanguageTools();
    void ResetLanguages();
    void CloseLanguage(Document& document);
    void LanguageTick();
    LanguageServer* EnsureLanguage(Document& document);
    bool SyncLanguage(Document& document,LanguageServer& server);
    void RequestSymbols(Document& document,LanguageServer& server);
    void UpdateSymbolBar();
    void RequestSemantic(Document& document,LanguageServer& server);
    void LanguageNotification(const std::string& key,int64 generation,const std::string& method,const Json& params);
    void AcceptDiagnostics(const std::string& key,int64 generation,const Json& params,int64 snapshot=0);
    void StartDiagnosticSnapshot(const std::string& key);
    bool DiagnosticSnapshotCurrent(const std::string& key,int64 serial);
    void InvalidateAnalysis(Document& document);
    void AnalysisTick();
    void UpdateProblems();
    void JumpProblem(int64 document,int64 serial,size_t offset);
    void NavigateProblem(int direction);
    void Complete(Editor* source,bool manual=true,int character=0);
    void AcceptCompletion(Editor* source,const std::string& label);
    void ApplyCompletion(int64 document,int64 serial,CompletionItem item);
    void CancelCompletion();
    // Three lanes keep interactive work independent of slow volumes: fJobs
    // opens, saves and checks documents; fBrowseJobs lists tree folders; and
    // fProjectJobs runs one project-wide walk (index or change scan) at a time.
    std::unique_ptr<AsyncQueue> fJobs;
    std::unique_ptr<AsyncQueue> fBrowseJobs;
    std::unique_ptr<AsyncQueue> fProjectJobs;
    std::unique_ptr<Editor> fLoaderFactory;
    std::unique_ptr<AsyncQueue> fRecoveryJobs;
    std::unique_ptr<BFilePanel> fFolderPanel,fOpenPanel,fSavePanel;
    std::unique_ptr<BMessageRunner> fPulse;
    std::unique_ptr<BMessageRunner> fExternalTimer;
    std::shared_ptr<ProjectMonitor> fProjectMonitor;
    std::map<std::pair<uint64_t,uint64_t>,std::string> fWatchedNodes;
    std::vector<std::string> fMonitorPriority;
    bool fMonitorPending=false;
    int64 fIndexSerial=0;
    bool fIndexRunning=false,fIndexStale=false;
    std::unique_ptr<BMessageRunner> fRecoveryTimer;
    std::unique_ptr<BMessageRunner> fLanguageTimer;
    std::unique_ptr<BMessageRunner> fDetachTimer;
    int64 fDetachTab=0;
    BPoint fDetachPoint;
    struct DiagnosticSnapshot { int64 document=0,revision=0;int version=0;std::string uri,language,text; };
    struct ServerEntry {
        std::unique_ptr<LanguageServer> client;std::string status,root;
        std::vector<std::string> command;
        Json initialization,configuration;
        bool unversionedDiagnostics=false,snapshotDirty=false;
        int64 snapshotSerial=0,snapshotChangedAt=0;
        std::shared_ptr<LanguageServer> snapshotClient;
        std::vector<DiagnosticSnapshot> snapshots;
    };
    std::map<std::string,ServerEntry> fServers;
    LanguageTools fLanguageTools;
    BMessenger fLanguageToolsWindow;
    int64 fLanguageGeneration=0,fCompletionSerial=0,fCompletionDocument=0,fCompletionAt=0;
    int64 fCompletionCaret=0,fCompletionWordStart=0,fCompletionView=0;
    std::string fCompletionText;
    std::vector<CompletionItem> fCompletions;
    std::vector<std::string> fCompletionLabels;
    int64 fTypedDocument=0,fTypedAt=0;
    int fTypedCharacter=0;
    SymbolBar* fSymbolBar;
    ProblemsView* fProblems=nullptr;
    int fLanguageVersion=0;
    bool fProblemsDirty=true;
    struct Snapshot { int64 id,revision;size_t total,offset=0;std::shared_ptr<Draft> draft; };
    std::unique_ptr<Snapshot> fSnapshot;
    std::vector<std::unique_ptr<Document>> fDocuments;
    std::vector<std::unique_ptr<Pane>> fPanes;
    std::unique_ptr<LayoutNode> fLayout;
    Pane* fActivePane=nullptr;
    BView* fPaneHost=nullptr;
    int64 fNextPane=1,fNextView=1,fClosingPane=0;
    bool fPreviewTabs=true;
    std::deque<ClosedTab> fClosedTabs;
    BMessage fRestoreLayout;
    std::map<int64,BMessage> fRestoreViews;
    std::map<std::string,std::vector<std::function<void()>>> fPendingRequests;
    std::set<std::string> fPendingOpen;
    std::shared_ptr<ProjectIndex> fIndex=std::make_shared<ProjectIndex>();
    std::shared_ptr<ProjectIndexSlot> fIndexSlot=std::make_shared<ProjectIndexSlot>();
    std::string fProject,fGitRoot,fSettings;
    std::string fSessionDirectory;
    int64 fGeneration=0,fNextID=1,fSavePanelID=0;
    int64 fSavePanelToken=0;
    int64 fFocusSerial=0;
    bool fSavePanelAccepted=false;
    EditorSettings fEditorSettings;
    BMessenger fPreferencesWindow;
    BMenu* fThemes;
    bool fQuitWhenSaved=false,fDiskCheckPending=false;
    std::map<int64,int64> fQuitDiscarded;
    std::deque<int64> fSaveQueue;
    std::deque<int64> fCloseQueue;
    std::string fRecoveryDirectory,fSessionToken,fRestoreSelected;
    std::set<std::string> fRetiredDrafts;
    std::map<std::string,BMessage> fRestoreDocuments;
    std::vector<std::string> fRestorePaths;
    int fRestoringDrafts=0;
    bool fRestoring=false,fRestoreFilesStarted=false;
    Explorer* fExplorer;
    GitView* fGit;
    TerminalPanel* fTerminal;
    RefreshButton* fRefresh;
    BCardLayout* fModeLayout;
    BSplitView* fSidebarSplit;
    BSplitView* fTerminalSplit;
    BStringView* fStatus;
    BView* fFindBar;
    BView* fExternalBar;
    BStringView* fExternalStatus;
    BButton *fExternalCompare,*fExternalReload,*fExternalKeep;
    BTextControl* fFindText;
    BTextControl* fReplaceText;
    BCheckBox* fMatchCase;
    BCheckBox *fRegex,*fWholeWord,*fInSelection;
    BStringView* fFindStatus;
    BMenuField* fFindHistory;
    std::deque<std::string> fRecentQueries;
    std::unique_ptr<BMessageRunner> fFindTimer;
    int64 fFindScopeView=0;
    std::vector<BMessenger> fSearchWindows;
    BMessenger fEditWindow,fRenameWindow;
    std::shared_ptr<EditPlan> fEditPlan,fLastEdit;
    BMessage fReplaceRequest;
    size_t fEditFile=0;
    bool fApplyingEdit=false,fRestoringEdit=false,fCancelEdit=false;
    int64 fEditSerial=0,fRenameSerial=0,fRenameDocument=0;
    size_t fRenameCaret=0;
    std::string fRenameText;
    SnapshotMap fRenameOpen;
    std::map<std::string,FileStamp> fRenameStamps;
    std::string fRenameServerKey;
    std::vector<std::string> fRenameBorrowed;
};
}
