#pragma once
#include <SupportDefs.h>
namespace kiri {
constexpr uint32 kMarkdownPreview='mdpv',kMarkdownFocus='mdfc',kMarkdownLink='mdln';
constexpr uint32 kDocumentSettings='dcst',kApplyDocumentSettings='dcap',kEditingDefaults='eddf',kApplyEditingDefaults='edad',kToggleMinimap='mmap';
constexpr uint32 kWindowTheme='wthm',kThemeLibraryChanged='thlb';
constexpr uint32 kToggleProblems='prob',kNextProblem='pbnx',kPreviousProblem='pbpr',kJumpProblem='pbgo',kRestartLanguages='lgrs';
constexpr uint32 kExternalTick='exck',kExternalCompare='excp',kExternalReload='exrl',kExternalKeep='exkp',kExternalBackups='exbk',kProjectUpdated='prup';
constexpr uint32 kFindChanged='fsch',kFindScope='fscp',kFindHistory='fhst',kFindRefresh='frsh';
constexpr uint32 kProjectReplace='prrp',kEditPreview='edpv',kEditApply='edap',kEditToggle='edtg',kEditSelect='edsl',kEditCancel='edca',kEditUndo='edun',kEditRefresh='edrf';
constexpr uint32 kRenameSymbol='rnsy',kRenameSubmit='rnsm';
constexpr uint32 kRenameDismiss='rnds';
enum : uint32 {
    kTabDragUpdate='tbdm',kTabDragEnd='tbde',kTabDragCancel='tbdc',kTabDragTick='tbdt',kDetachTabReady='tbdr',
    kWorkspaceOpened='wsop',kWorkspaceClosed='wscl',kWorkspaceActivated='wsac',
    kQuitApplication='aqut',kWorkspaceQuitCancelled='wsqc',
    kSplitRight='sprt',kSplitDown='spdn',kNextPane='pnxt',kPreviousPane='pprv',kClosePane='pcls',kEditorFocus='edfc',
    kKeepTab='tbkp',kReopenTab='tbro',kPreviewTabs='tbpv',kQueryPreview='qypv',
    kFormatPrettier='prtf',kComplete='lcpl',kCompletionChosen='lcch',kCancelCompletion='lcca',kEditorText='edtx',kEditorTyped='edty',
    kLanguageTick='lgtk',kBrowseSymbols='lsym',kSymbolChosen='lsgo',kShowLanguageTools='lgtl',kApplyLanguageTools='lgap',
    kShowLauncher='lnsh',kHideLauncher='lnhd',kLauncherClosed='lncl',kRecentsChanged='rccg',kActivateWorkspace='actw',
    kOpenProject='oppr', kProjectChosen='prch', kOpenFile='opfl', kFileChosen='flch',
    kNewFile='newf', kSave='save', kSaveAs='svas', kSaveChosen='svch', kSaveAll='sval',
    kCloseTab='cltb', kSelectTab='sltb', kEditorChanged='edch', kEditorPosition='edps',
    kCloseAllTabs='clat', kCloseOtherTabs='clot',
    kShowPreferences='pref', kApplyPreferences='eprf', kSyncPreferences='epsy',
    kTheme='them', kToggleTerminal='tote', kToggleSidebar='tosi', kToggleGit='togi',
    kRefresh='refr', kFind='find', kFindNext='fnxt', kFindPrevious='fprv', kReplace='repl',
    kReplaceAll='rpal', kGoToLine='gtln', kGoToResult='gtrs', kQuickOpen='qkop',
    kProjectSearch='prsc', kQueryChanged='qych', kQueryResult='qyrs',
    kCopyPermalink='cplk', kFileHistory='flhi', kZoomIn='zmin', kZoomOut='zmot',
    kWrap='wrap', kShowWhitespace='shws', kWorkDone='wkdn', kPulse='puls',
    kGitRefresh='gref', kGitSelection='gsel', kGitHistorySelection='ghse',
    kGitStage='gsta', kGitUnstage='guns', kGitCommit='gcmt', kGitMore='gmor',
    kGitStagedDiff='gsdf', kGitWorktreeDiff='gwdf', kGitFetch='gftc',
    kGitPull='gpul', kGitPush='gpsh', kGitCommandDone='gcmd',
    kTreeExpand='trex', kTerminalTick='tetk', kTerminalState='test', kTerminalFocus='tefo',
    kNewTerminal='tent', kSelectTerminal='tesl', kCloseTerminal='tect',
    kCloseAllTerminals='teca', kCloseOtherTerminals='teco',
    kPreviewZoom='przm', kPromptAccept='prac', kPromptCancel='prca', kRecoveryTick='rctk'
};
}
