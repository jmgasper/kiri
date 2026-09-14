#pragma once
#include "core/FileIO.h"
#include "core/Project.h"
#include "core/Recovery.h"
#include "ui/Async.h"
#include "ui/Theme.h"
#include "ui/EditorSettings.h"
#include "core/LanguageTools.h"
#include "core/LanguageServer.h"
#include <Window.h>
#include <memory>
#include <set>
#include <map>
#include <deque>
#include <vector>
class BFilePanel;class BCardLayout;class BSplitView;class BStringView;
class BTextControl;class BCheckBox;class BMessageRunner;
class BMenuBar;
namespace kiri {
class Editor;class Explorer;class GitView;class TerminalPanel;class TabStrip;class RefreshButton;class SymbolBar;
class Workspace:public BWindow {
public:
    explicit Workspace(const std::string& settingsDirectory={},bool restoreSession=true);
    ~Workspace() override;
    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
    void OpenProject(const std::string& path);
    void OpenFile(const std::string& path,size_t line=1,size_t column=1,bool activate=true);
private:
    struct Document {
        int64 id=0,revision=0;
        std::string path,name;
        BView* view=nullptr;
        Editor* editor=nullptr;
        FileStamp stamp;
        bool bom=false,saving=false,closeAfterSave=false,external=false;
        std::string recoveryFile;
        int64 recoveryRevision=-1,lastRecovery=0;
        bool recovering=false;
        std::string serverKey,serverURI,serverText,serverLanguage,languageStatus,symbolError;
        int serverVersion=0,symbolVersion=-1;
        int64 symbolRequest=0,completionRequest=0,textChangedAt=0,formatSerial=0;
        bool languageDirty=true;
        std::vector<DocumentSymbol> symbols;
    };
    Document* Current();
    Document* ByID(int64 id);
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
    void ApplyTheme(int index);
    void ShowPreferences();
    void ShowFind();
    void Search(bool projectSearch);
    void PromptLine();
    void CopyPermalink();
    void LoadDirectory(const std::string& path);
    void RefreshIndex();
    void RestoreSettings(bool restoreSession);
    void RememberRecent(const std::string& path,bool folder);
    void SaveSettings();
    void FinishRestore();
    void RestoreDraft(const std::string& file);
    void StartRecovery();
    void RecoveryTick();
    void ClearRecovery(Document& document);
    void Pulse();
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
    void Complete(Editor* source,bool manual=true,int character=0);
    void AcceptCompletion(Editor* source,const std::string& label);
    void ApplyCompletion(int64 document,int64 serial,CompletionItem item);
    void CancelCompletion();
    std::unique_ptr<AsyncQueue> fJobs;
    std::unique_ptr<Editor> fLoaderFactory;
    std::unique_ptr<AsyncQueue> fRecoveryJobs;
    std::unique_ptr<BFilePanel> fFolderPanel,fOpenPanel,fSavePanel;
    std::unique_ptr<BMessageRunner> fPulse;
    std::unique_ptr<BMessageRunner> fRecoveryTimer;
    std::unique_ptr<BMessageRunner> fLanguageTimer;
    struct ServerEntry { std::unique_ptr<LanguageServer> client;std::string status; };
    std::map<std::string,ServerEntry> fServers;
    LanguageTools fLanguageTools;
    BMessenger fLanguageToolsWindow;
    int64 fLanguageGeneration=0,fCompletionSerial=0,fCompletionDocument=0,fCompletionAt=0;
    int64 fCompletionCaret=0,fCompletionWordStart=0;
    std::string fCompletionText;
    std::vector<CompletionItem> fCompletions;
    std::vector<std::string> fCompletionLabels;
    int64 fTypedDocument=0,fTypedAt=0;
    int fTypedCharacter=0;
    SymbolBar* fSymbolBar;
    struct Snapshot { int64 id,revision;size_t total,offset=0;std::shared_ptr<Draft> draft; };
    std::unique_ptr<Snapshot> fSnapshot;
    std::vector<std::unique_ptr<Document>> fDocuments;
    std::set<std::string> fPendingOpen;
    std::shared_ptr<ProjectIndex> fIndex=std::make_shared<ProjectIndex>();
    std::string fProject,fGitRoot,fSettings;
    int64 fGeneration=0,fNextID=1,fSavePanelID=0;
    int64 fSavePanelToken=0;
    int64 fFocusSerial=0;
    bool fSavePanelAccepted=false;
    int fSelected=-1;
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
    TabStrip* fTabs;
    BCardLayout* fDocumentsLayout;
    BCardLayout* fModeLayout;
    BSplitView* fSidebarSplit;
    BSplitView* fTerminalSplit;
    BStringView* fStatus;
    BView* fFindBar;
    BTextControl* fFindText;
    BTextControl* fReplaceText;
    BCheckBox* fMatchCase;
};
}
