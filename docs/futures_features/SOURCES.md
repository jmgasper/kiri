# Research evidence and current Kiri baseline

Research date: **2026-09-14**. This is a documentation and source comparison,
with a read-only observation of the running Kiri VM. CodeEdit and CotEditor were
not executed on macOS, and historical Fleet releases were not installed. The
reference claims describe documented/released behavior rather than newly tested
cross-editor parity. Proposed Kiri scopes and priorities are planning judgments.

## Source snapshots

| Reference | Snapshot and evidence |
| --- | --- |
| Kiri | Local source commit `5ab4ba5517cf04f9de8ab05aad596f9cb9933215`, 0.0.1 alpha. The worktree was clean before this documentation work. |
| CodeEdit | [Homepage](https://www.codeedit.app/), [0.3.6 release](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.6), [0.3.5 release](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5), and released source at [`05704cb7d2b19892ffcb9742b0ab3cc6116af165`](https://github.com/CodeEditApp/CodeEdit/tree/05704cb7d2b19892ffcb9742b0ab3cc6116af165). GitHub's latest-release API and the homepage identified 0.3.6 at research time. |
| CotEditor | [Homepage](https://coteditor.com/), [7.1.0 release](https://github.com/coteditor/CotEditor/releases/tag/7.1.0), and English help bundled at [`545031e89383b7a20b1b2be4a846fca02dbf9c0f`](https://github.com/coteditor/CotEditor/tree/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs). GitHub's latest-release API identified 7.1.0. Individual drafts link directly to the applicable pinned help pages. |
| Fleet | The requested [product page](https://www.jetbrains.com/fleet/) provided little extractable detail. Dated Fleet announcements, primarily 2023–2024 release notes, establish the specific features. Each draft links to its supporting article. |

CodeEdit is still described as in development. Its language-server installer is
explicitly experimental in the [0.3.6 release
notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.6). A menu label
or wrapper API alone was insufficient evidence of an implemented workflow:
for example, the released [Source Control
commands](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/WindowCommands/SourceControlCommands.swift)
contain a disabled Cherry-Pick placeholder, and the [tab context
menu](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Editor/TabBar/Views/EditorTabBarContextMenu.swift)
contains disabled Open in New Window. These were not used to establish feature
availability. Source implementations and their calling UI were inspected together.

JetBrains [announced Fleet's
retirement](https://blog.jetbrains.com/fleet/2025/12/the-future-of-fleet/) with no
further updates and distribution ending on December 22, 2025. The same page was updated
in May 2026 to identify Air as the successor. Existing Fleet installations may
continue running, while service-dependent capabilities can stop working. Some
Fleet help URLs were unavailable or yielded Air documentation during research,
so the catalog uses dated Fleet-specific material. An IntelliJ IDEA, Rider,
CLion or Air feature was not treated as Fleet evidence merely because it came
from JetBrains.

## How gaps were established

The README, requirements, release/status records, language-tool documentation
and the actual implementation were compared. The code was the deciding evidence
when broad feature labels hid differences. A live QMP query confirmed the existing
QEMU instance was running, and a screenshot of its VNC-visible display confirmed
the current explorer, Git patch/history workspace and terminal layout. This
observation did not establish interactive behavior for proposed features.

The code comparison covered workspace menus and message dispatch, document/view
ownership, preferences, editor configuration and notifications, file I/O and
recovery, project indexing/search, explorer actions, tabs, previews, terminal
launching, Git APIs/UI, and LSP capabilities/request handling. Each draft points
to the relevant current Kiri file and often the specific function or class.
Recheck those links before future implementation because Kiri will evolve.

## Existing capabilities that should be reused

| Kiri capability | Current evidence and planning consequence |
| --- | --- |
| Native editing, multiple selections, folding, indentation guides, matching-brace highlighting, wrapping and zoom | [Editor.cpp](../../src/ui/Editor.cpp) configures Scintilla directly. Ordinary multiple-caret editing, folding and brace matching are already present; pairing and configurable indentation are narrower gaps. |
| Tabs, close protections and session restoration | [TabStrip](../../src/ui/TabStrip.cpp) and [Workspace](../../src/ui/Workspace.cpp) implement open/close/close-all/close-others, saved positions and session handling. Split views, preview reuse and reopening closed tabs extend this baseline. |
| Recovery and safe saves | [Recovery](../../src/core/Recovery.cpp), [FileIO](../../src/core/FileIO.cpp) and Workspace preserve drafts, file stamps, permissions and Haiku attributes. Disk autosave and legacy-encoding conversion are separate additions. |
| Literal document find/replace and quick-open/project search | [Editor](../../src/ui/Editor.cpp), [Project](../../src/core/Project.cpp) and Workspace implement these. A regex argument exists in Editor::Find but is not exposed by the find bar or implemented by the replacement methods. |
| Ignore-aware indexing | [IndexProject](../../src/core/Project.cpp) uses Git's tracked/nonignored paths and has a fallback exclusion set. Search filters extend existing ignore behavior. |
| Syntax highlighting, themes and font preferences | [Editor](../../src/ui/Editor.cpp), [Theme](../../src/ui/Theme.cpp) and [PreferencesWindow](../../src/ui/PreferencesWindow.cpp) provide these already. Custom profiles/themes and semantic tokens extend their configuration. |
| Prettier, completion and current-file symbols | [Language tools guide](../LANGUAGE_TOOLS.md), [WorkspaceLanguage](../../src/ui/WorkspaceLanguage.cpp) and [LanguageServer](../../src/core/LanguageServer.cpp). Snippets are explicitly disabled; workspace edits are rejected. Configuration and completion-attached commands already have partial protocol handling. |
| Git integration | [GitRepository](../../src/core/Git.h) and [GitView](../../src/ui/GitView.cpp) cover status, whole-file staging, commits, graph/history, unified patches, file history, fetch/pull/push and GitHub permalinks. The catalog proposes specific missing workflows rather than a replacement Git integration. |
| Real multiple terminals | [TerminalPanel](../../src/ui/TerminalPanel.cpp), [TerminalView](../../src/ui/TerminalView.cpp) and [Terminal](../../src/core/Terminal.cpp) provide independent PTYs, scrollback, clipboard and alternate-screen support. Named tasks/profiles add configuration and lifecycle UI. |
| Images and binary previews | [PreviewView](../../src/ui/PreviewView.cpp) and [Workspace::OpenFile](../../src/ui/Workspace.cpp) provide translated images and bounded hex previews. Markdown rendering and source printing are separate capabilities. |
| Launcher and recent items | [LauncherWindow](../../src/ui/LauncherWindow.cpp), [RecentItems](../../src/ui/RecentItems.cpp) and [Application](../../src/ui/Application.cpp) already support opening files/folders and recent sessions, including Tracker/command-line entry. |

The existing [verification record](../STATUS.md), [language-tool
limits](../LANGUAGE_TOOLS.md) and [performance measurements](../PERFORMANCE.md)
are baseline evidence from earlier work, not new test results from this comparison.
Future proposals should retain the asynchronous and bounded behavior described
there: 8 MiB language/highlighting limits, larger-file editing, cancellation and
explicit result/output caps.

## Scope and interpretation

- Only a supporting reference is asserted in the index. The catalog is not a
  complete yes/no matrix of every capability in every editor.
- Draft scope and acceptance criteria describe an adaptation for Kiri. For
  example, Fleet's partial-commit workflow motivates hunk staging within Kiri's
  existing index-based Git UI; its implementation is not copied or assumed.
- Native macOS facilities need Haiku equivalents. UNIX text-transform scripts
  are relevant inspiration from CotEditor; AppleScript integration is specific
  to macOS. CotEditor's printing/PDF workflow does not establish a working Haiku
  PDF backend. Its broader typography and OS integrations were not exhaustively
  cataloged.
- A generic CodeEdit extension ecosystem was not counted as shipped on the
  strength of marketing or roadmap language. This catalog instead identifies
  concrete customization mechanisms: themes, syntax profiles, keybindings,
  snippets and script actions.
- Haiku availability of new parsers, Unicode libraries, debugger adapters,
  remote helpers and AI clients remains to be verified by the relevant future
  goals. The current native C++/Interface Kit/Scintilla direction and lack of a
  browser runtime remain the design context from [REQUIREMENTS.md](../REQUIREMENTS.md).

The research deliverable is the linked proposal catalog. Application code,
installed packages and the existing VM workspace were not changed for it.
