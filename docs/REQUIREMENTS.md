# Kiri: native Haiku project editor

The target is a practical replacement for the owner's VS Code workflow, built
in native C++ using Haiku's Interface Kit. No browser or web application runtime.

## Required capabilities and acceptance evidence

| Capability | Acceptance evidence required |
| --- | --- |
| Project explorer | Open a folder; expand a lazy directory tree; navigate and open files; stay responsive on a large project. |
| Source editing | Edit, save, reopen, undo/redo, select, clipboard, tabs, dirty-close protection, Unicode and line-ending preservation. |
| Syntax highlighting | Native lexers for popular languages and HTML, JSON, XML; rendered checks of representative files. |
| Line numbers | Visible, aligned gutter and accurate cursor/line navigation. |
| Prettier formatting | Right-click action formats the unsaved buffer using project settings; one Undo; safe rejection after edits or closing its tab. |
| LSP completion | Real language servers provide suggestions in the editor; keyboard acceptance applies their edit ranges correctly, including Unicode. |
| File symbol browsing | A bar above the active file lists variables, fields, methods and functions supplied by LSP and jumps to their declarations. |
| Themes and dark UI | Select light/dark color themes affecting all app-owned surfaces; persist preference. |
| Embedded terminal | Real PTY and terminal emulation; interactive shell, colors, resize, Ctrl-C, scrollback and full-screen programs inside the editor. |
| Git history and graph | Browse all reachable history with pagination, branch/merge graph, metadata, commit changes and file history. |
| Git operations | Status, staged/unstaged diffs, stage/unstage, commit with errors shown; refresh without blocking typing. |
| GitHub permalinks | Copy immutable commit URL for a file and line/range; handle SSH/HTTPS remotes, URL escaping, and unsaved/uncommitted lines correctly. |
| Diff viewing | Syntax-colored patch view for worktree, index and historical commits; filenames with spaces/Unicode supported. |
| Non-text previews | Native image decoding and fit/actual-size preview; useful bounded binary/hex preview for other formats. |
| Performance | Avoid recursive UI-thread indexing; bounded background work, cancellation, large-file measurements and responsiveness evidence on Haiku. |
| Delivery | Reproducible native build, test instructions, runnable Haiku artifact, dependency/license documentation. |

Reasonable additions: find/replace, quick open, project search, go to line,
session recovery, file-change detection, keyboard navigation and zoom.

## Design direction

Compact menus, a file symbol bar, resizable file sidebar, tabbed content, a docked
terminal and separate source-control workspace. Native controls and focused
custom drawing for document tabs, terminal cells and commit lanes.

References: https://nova.app/ and https://help.nova.app/projects/workspace/.
The workspace organization is inspiration; Kiri has its own UI and assets.

## Implementation plan

1. Establish a Haiku x86_64 VM, native build and portable core tests.
2. Build the workspace around Haiku Scintilla + Lexilla and safe file I/O.
3. Add lazy explorer, themes, previews, find/replace and navigation.
4. Add a real embedded terminal with libvterm and a PTY.
5. Add asynchronous Git, full paginated history, graph, diffs and permalinks.
6. Exercise the actual application in QEMU, measure large projects/files,
   fix functional/UI issues, and package with documentation.

Completion requires observed behavior across the whole table. A successful
compile or portable test run alone does not establish completion.
