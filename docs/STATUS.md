# Verification record

## External refresh and side-by-side diffs — develop

Verified September 15, 2026 on Linux and in the existing Haiku R1/beta5 QEMU VM.
All four CMake/CTest suites passed. Haiku passed 163 core checks, 94 search/edit
checks (91 on Linux), 3,675 refresh/diff checks, 266 native editor/worker checks
and 421 workspace checks. Real Prettier, TypeScript and clangd runs passed 121
language checks on Haiku.

Portable checks compare randomized diff output with an independent edit-distance
calculation, and cover line alignment, UTF-8 changes, CRLF, missing final newlines,
binary/invalid text, cancellation and comparison limits. Real temporary Git
repositories exercise working-tree, index, initial-commit and historical pairs,
including added, deleted, renamed and untracked files. Viewing leaves repository
status unchanged. Incremental polling checks file creation, rename, deletion,
atomic replacement, nested directories and symlink boundaries.

Native workspace checks verify clean reload in shared panes, independent view
positions, refreshed symbols and BOM/EOL metadata, dirty-buffer and disk backups,
Compare/Reload/Keep Editing, typing during reload, stale comparison actions,
rapid atomic writes, deletion/recreation, image refresh and retained copies after
closing. Already-open quick-open and project-search windows refresh their results;
loaded tree branches update without losing manually opened symlink contents.
Real Scintilla views verify read-only comparisons, aligned annotations, unequal
wrapping, synchronized scrolling and changed-block navigation. Delayed Git results
cannot replace a newer file, mode or repository selection. Moving focus away from
an unfinished commit message cannot submit it.

Live checks in `/boot/home/KiriRefreshDemo` reloaded an external TypeScript edit
without reopening the file, updated the symbol bar from `original` to `refreshed`,
and recognized BOM/CRLF metadata. A later external write preserved unsaved typing,
created byte-checked copies of both versions and opened the side-by-side comparison.
Wrapping remained aligned, and Reload Disk restored a clean buffer while retaining
the copies. The Git viewer displayed index and disk snapshots with aligned added
and replaced lines. Native navigation buttons retain readable desktop colors
inside dark comparisons.

Usage, recovery policy and explicit limits are documented in
[external changes and diffs](EXTERNAL_CHANGES_AND_DIFFS.md). Requirements
[08](future_features/done/08-external-file-refresh.md) and
[40](future_features/done/40-side-by-side-diffs.md) are archived in the done folder.

## Project search field focus — develop

Verified September 15, 2026 in the existing Haiku R1/beta5 QEMU VM. The native
workspace suite passes 351 checks. A regression test failed on the previous
build when moving focus from the edited query to the replacement field.

Search and replacement fields now use explicit Enter key actions. Moving focus
keeps the search window open and does not start a replacement preview. Native
checks cover typing, changing fields, Enter after refocusing unchanged text,
the Preview Replace button, arrow navigation, opening the selected result, and
moving to the results list in Open Quickly. A real mouse click reproduced the
original closure; the fixed build accepts replacement text with results visible.

## Advanced search, project replacement and symbol rename — develop

Verified September 15, 2026 on Linux and in the existing Haiku R1/beta5 QEMU VM.
All three CMake/CTest suites passed. Haiku passed 163 core checks, 94 search/edit
checks (91 on Linux), 266 native editor/worker checks and 328 workspace checks.
Real-tool runs passed 111 language checks on Linux and 121 on Haiku, including
Prettier, TypeScript prepareRename/rename, and clangd completion/symbols.

The search suite covers Unicode captures and whole words, invalid patterns and
replacement syntax, CR/LF/CRLF anchors, zero-width progress, regex work limits,
folder/glob and ignored-file scope, multiple results on a line, Unicode columns,
binary/encoding/size/unreadable/symlink exclusions and result caps. Native editor
checks verify highlighted ranges, wrapping, selection-only replacement,
read-only buffers and exact one-step Undo.

Workspace checks run the production preview/apply/restore flow against saved
and dirty files, including per-match exclusion, edits after preview, changed
disk files, early cancellation, a failed later write and restoration of already
completed writes. Tests also cover buffers in another window, cross-window
rename and whole-operation Undo from either window, query history persistence,
rapid query changes, project switches and stale result messages. Delayed
Scintilla notifications are explicitly exercised: they must not invalidate an
otherwise unchanged preview. Apply, Refresh and Cancel messages are tied to
the reviewed preview generation, and native tests reject delayed actions from
a replaced preview. Protocol tests reject stale LSP versions, malformed
UTF positions, overlapping edits and unsupported resource operations as a whole.

The real TypeScript integration renames an exported function, its import and
its call in another file; an identical string literal and a shadowed parameter
stay unchanged. The source buffer includes unsaved text, the closed file uses a
checked safe save, and restore recovers its original bytes. Haiku transaction
tests verify UTF-8 BOM, CRLF, mode bits and native attributes. A persisted
write-ahead journal test simulates interruption between a disk write and its
completion record, then restores the file from that journal.

VNC checks used `/boot/home/KiriSearchDemo`: project results were grouped by
file, a whole generated file was excluded from a replacement, source disk
writes were applied and restored, and a real TypeScript rename preview showed
only the declaration, import and call. Its Apply preserved the module path and
string literal; Undo Last Project Edit restored the disk text. The native
package was rebuilt with its PCRE2 runtime dependency.

Usage and limits are documented in [search and refactoring](SEARCH_AND_REFACTORING.md).
Feature specs 04, 05, 06 and 29 are archived in [the done folder](future_features/done/).

## Editor tab dragging — develop

Verified September 14, 2026 on Haiku R1/beta5 in the isolated QEMU VM.
The native build passed 264 workspace checks, 233 editor/worker checks,
163 core checks and 80 language checks. Both Linux CMake/CTest suites passed.

The workspace suite covers moves in both directions, insertion at either end,
duplicate files in the destination, preview promotion, nested-pane collapse,
unchanged close history, card order, multiple selections, scroll and zoom,
dirty buffers, undo/redo, detached saves, deferred drops during a save,
untitled drafts, images, binary previews and independent recovery directories.
Shared views that remain in another window use independent text buffers.

Real pointer input verified reordering, dropping into a nested pane's editor,
scrolling a crowded 14-tab strip while holding a drag at its edge, Escape
cancellation, invalid sidebar drops and detaching an unsaved tab. Closing the
source window left the detached window editable and its Save wrote the expected
file. Closing the final workspace exited the application. File → Quit stopped
when cancelled, then on a fresh request saved two dirty windows in sequence
and exited after their asynchronous saves completed.

A separate production-startup harness was terminated with an unsaved detached
window. Restarting without command-line files restored that window and its dirty
state; saving produced an exact byte-for-byte match with the 1,010-byte recovery
snapshot captured before termination. Test files, snapshots and logs remain in
ignored VM directories. See [tab workflows](EDITOR_PANES.md) for behavior and
[the VM guide](VM.md) for the pointer driver and inspection harness.

## Nested editor panes and tab workflows — develop

Verified September 14, 2026 in the existing Haiku R1/beta5 QEMU VM. The native
build passed 163 core checks, 233 editor/worker checks, 80 language protocol
checks and 118 workspace checks. Both Linux CMake/CTest suites passed.

The new workspace suite uses real Haiku windows and shared Scintilla documents.
It covers nested splits, independent selections/scrolling, shared edits and undo,
save state, pane focus and find, preview reuse and edit/undo promotion, Keep Open,
Close shortcuts, reopen positions, duplicate prevention, missing files, layout
proportions, session recovery and concurrent opens. Deterministic formatting and
LSP subprocesses check symbols, completion and delayed formatting across panes.
A 32 MiB split shares the document pointer and took about 20 ms in this VM.

Live keyboard/mouse checks in an isolated workspace verified Split Right/Down,
three simultaneous shared views, independent line navigation, italic previews,
double-click Keep Open and edit/undo promotion. Closing additional dirty views
kept the document alive; the last dirty view offered Cancel/Discard/Save.
Cancel preserved it and its close history, Save wrote the file and closed the
view, and Reopen restored its cursor. Explicitly discarding an untitled draft
left no reopen entry. The Close/Keep shortcut checks include a non-first tab,
and terminal tab selection retains the same default-index behavior.

See [editor panes and tab workflows](EDITOR_PANES.md) and the
[native screenshot](screenshots/editor-panes.png). `make check-workspace` runs
the new suite with disposable files, settings and application identity.

## 0.0.1 alpha public release

The first public release uses version `0.0.1-alpha` and Haiku package version
`0.0.1~alpha-1`. Earlier beta labels below refer to local development builds.
The release is built from a clean source copy on Haiku R1/beta5 x86_64, with
matching CMake, native application and language-server client versions.

Fresh builds passed **163 core checks on Linux and Haiku**, **233 native
editor/file/worker checks**, and **98 language checks on Linux / 108 on Haiku**.
The language checks exercised real Prettier and TypeScript servers, plus clangd
on Haiku. CMake's two Linux test suites also passed. An isolated instance showed
the launcher and accepted a real folder reference, opening `KiriDemo`.

The native resource reports `0 0 1 a 0` (0.0.1 alpha), and the release package
uses the corresponding version in its filename, metadata and provided packages.
Packaging verifies both executable and MIME icon attributes against the HVIF
source. Release assets include the HPKG, stripped and debug executables, a source
archive from the release tag, and SHA-256 checksums.

## Development build history

Verified September 14, 2026 on Haiku R1/beta5 x86_64, hrev57937+113, in
QEMU/KVM with 4 vCPUs and 4 GiB RAM. Native build: GCC 13.3, Scintilla 5.3.4,
Lexilla 5.4.6. Portable core also built and tested with Linux GCC 13.3.

## Native application icon (package revision 7)

The approved blue and mint folded-paper logo now has a native HVIF icon,
with editable SVG and Icon-O-Matic sources. The 1,512-byte vector was exported
by Haiku's Icon-O-Matic and rendered through `BIconUtils` at 16, 32, 64 and
256 pixels. Light and dark previews were inspected; all four renders contained
visible artwork without opaque pixels clipped at the canvas edges.

The revision 7 package was built and installed on Haiku. Its embedded `VICN`
resource, executable `BEOS:ICON` attribute and both the system and user MIME
database `META:ICON` attributes matched the source HVIF byte for byte. Native
Tracker icon lookups at 16 and 32 pixels matched direct HVIF renders pixel for
pixel. [Tracker's Applications window](screenshots/app-icon.png) also displayed
the installed icon alongside the system applications. Packaging forces a complete
staged MIME record even when Kiri is already registered in the build machine's
fallback database. It verifies the executable and MIME icon attributes against
the source before creating the package; rebuilding with the icon already
installed also passed these checks.

Makefile linking, package creation, shell syntax and host CMake configuration
passed. Compiling the resource from a different working directory also passed,
covering the absolute resource path used by CMake. Both build systems track
the external HVIF dependency. This revision changes branding and packaging;
the runtime test results below are the existing beta 6 baseline.

## Automated checks

Beta 6 rebuilt the native application and reran the portable and expanded native
suites. Language integration uses Node.js 20.15.1 on Haiku, Prettier 3.9.6,
TypeScript 5.9.3, typescript-language-server 4.3.4 and clangd 16.0.6.

- **163 core checks pass on Linux and Haiku.** Coverage includes literal
  subprocess arguments, timeout/cancellation/output limits; UTF-8 boundaries,
  streamed loading and BOM handling; safe-save conflicts, modes and symlinks;
  checksummed draft recovery and corrupt/truncated records;
  Git staging, unstaging, commits, history pagination, rename status, patches
  and graph lanes; GitHub immutable line/range mapping and boundaries; ignore-aware indexing
  and search; terminal escape handling and real PTY shells including Ctrl+D,
  independent directories and variables, and closing one shell while another runs.
- **233 native editor, file and worker checks pass.** C++/JSON token styles,
  Unicode navigation, dirty state, undo, current-match and all-match replacement,
  ten themes (five dark and five light), text/syntax contrast, installed font
  selection and size, settings round trips and legacy defaults, invalid settings
  fallback, unchanged text/selection/dirty state during appearance changes,
  native MIME icons, streamed document adoption, text/binary Haiku attribute
  preservation during saves and cancellation of superseded work. Recent-history
  checks cover migration from the previous session, persistence, canonical-path
  deduplication, Unicode paths, most-recent ordering, the 24-item limit, malformed
  records, removal without deleting source files, and excluding unsaved drafts.
  Formatting and completion edits preserve undo/redo and dirty state, apply
  additional edits atomically, reject overlapping ranges before changing text,
  preserve read-only buffers and intercept Haiku's Ctrl+Space without inserting NUL.
- **108 language checks pass on Haiku; 98 on Linux.** These exercise byte-fragmented
  and combined JSON-RPC frames, invalid headers, Unicode positions and file URIs,
  literal command arguments, project-local tool selection, configuration round
  trips, nested and flat symbols, completion edit ranges, server requests,
  errors, timeouts, cancellation, shutdown and full/incremental synchronization.
  Real Prettier checks cover unsaved input, project style/ignore rules, syntax
  errors and unchanged disk contents. Real TypeScript checks cover symbols,
  completion resolution and changed unsaved text. Haiku also exercises C++
  completion and symbols through clangd, using the same client.
- The native performance fixture adds checks for the 200 MiB document's byte
  and line counts and a 100,001-file index. See [measurements](PERFORMANCE.md).

## Observed native interaction

Beta 6 used an isolated workspace and settings directory beside the existing
user sessions. **Format with Prettier** in the editor's actual right-click menu
formatted JavaScript using the project's single-quote/no-semicolon settings.
The editor became dirty while the disk file remained unchanged; one Undo
restored the exact original text and clean state.

A delayed Prettier process completed after new text was typed. Its result was
rejected and the new text remained intact. Closing its target tab while a result
was pending retained the other file without reopening or replacing either tab.

The TypeScript symbol bar displayed ten symbols including a class, constructor,
method, function, constants and properties. Selecting `greet` moved the caret
to byte 84, its declaration. **Ctrl+Space** after `user.na` displayed `name`, and
Tab inserted `user.name` with zero NUL bytes. Enter acceptance also worked.
Typing `user.` displayed `age` and `name` automatically; Down and Tab selected
and inserted `name`. Undo restored the incomplete text in one step.
The native Language Tools dialog switched profiles and persisted the Prettier
command with automatic suggestions enabled.
In the final build, invoking Complete Code from the Edit menu while the terminal
had focus returned focus to the editor and inserted the selected property.
Go to Symbol from the Git workspace revealed the file; selecting `greet` again
reached byte 84 without changing its text.

Beta 5's isolated application instance used the production startup flow. A
normal launch showed only the launcher. Native folder and file pickers opened
the selected items, and **File → Show Launcher** returned to the recent list.
Selecting a recent file activated it; choosing the previous project after a
restart restored its three saved tabs and selected document. A missing recent
file displayed Cancel and Remove from Recent, and removal retained the other
four entries. Starting with New File created a single untitled document;
passing a file on the command line opened only that file without a launcher.
Passing a project folder also bypassed the launcher. Changing the workspace
theme updated the visible launcher in both dark and light appearances.

After the test instance was terminated with an unsaved draft, restarting
recovered its exact text and dirty state directly in the workspace. Discarding
that test draft removed its recovery file. The existing user session remained
open throughout these checks.

Beta 4 removes the project breadcrumb and the entire top button row. The
file tree and editor sit directly below the menu bar, with the project folder
in the window title. In a fresh isolated application instance, Alt+Shift+O
opened the folder picker and Alt+P opened Quick Open. Alt+B, Alt+Shift+G and
Alt+` hid and restored the file tree, source control and terminal respectively.
The View menu retained all three controls, and Alt+Shift+T opened a third
terminal whose `pwd` output confirmed the project directory. Existing sessions
remained open throughout the checks.

Beta 3 used another isolated application signature and settings directory so
the existing Kiri sessions could remain open. **Edit → Preferences** / Alt+,
opened the native preferences window with installed fonts, numeric size input,
five dark and five light choices, and a syntax-highlighted code preview.
An invalid size disabled Apply/OK and showed an explanation. Applying Bitstream
Charter at 18 points with Linen updated existing files, a new document and Git
diffs. Cancel retained the applied values, and a fresh application process
restored all three choices. Restore Defaults previewed the system fixed font
at 13 points with Obsidian and applied those defaults when requested.
The file tree and document tabs displayed native folder, source, text and HTML
icons; automated checks also covered images, archives, PDF, audio and unknown files.

Beta 2 was exercised in a separate application instance with its own settings
and disposable project, alongside an existing session with unsaved edits.
The refresh icon picked up an externally created file. Three terminal tabs kept
their directories, variables and background output when switching; the plus
control, menu shortcuts, shell-exit label, last-tab close and reopening worked.
Both tab strips' context menus closed all tabs or kept the right-clicked tab,
including when that tab was inactive. Closing terminal tabs left documents open.

Two untitled documents saved consecutively through **Close all**. Cancelling
either the unsaved prompt or Save As retained the remaining dirty tabs, and
saving afterwards did not close them. Closing the last terminal while Git was
visible returned keyboard focus to Git without editing a hidden document.
The broader Git, recovery and performance observations below were established
in beta 1. Beta 2 reran both automated suites and the tab interactions above;
beta 3 reran the portable suite and expanded native checks for preferences and icons.

| Area | Evidence |
| --- | --- |
| Workspace | Native menus, file tree, resizable panels, scrolling tabs, keyboard navigation, line gutter, file-type icons and ten color themes. |
| Source files | C++, HTML, JSON, XML and Python render with highlighting; go to line and project-search navigation reach the requested location. |
| Editing and saving | Native Save As, existing-file save, consecutive untitled files through Save All, clipboard and undo/redo. Cancelling quit after an earlier Discard leaves both remaining documents dirty. |
| Recovery | Force-killed Kiri with an unsaved two-line draft, restarted it, verified the text and dirty state, then saved it and confirmed draft cleanup. |
| Navigation | Quick-open query and Enter open JSON; project search returns matching files and opens XML at line 2, column 2. |
| Terminal | Independent shell tabs, commands, ANSI colors, background output, Ctrl+C, Ctrl+D shell exit, tab creation/closing and full-screen Haiku `top`. |
| Git | Selected worktree and index diffs; staged/unstaged and committed through the UI, confirmed by Git. Loaded all 231 test commits to the root, including a merge graph; file history shows the corresponding patch. |
| Permalinks | Copied a real clipboard URL with the selected line at an immutable commit; automated tests cover ranges, escaped paths, remote formats and changed-line rejection. |
| Previews | Native transparent PNG preview, fit/actual-size display and bounded binary/hex preview. |
| Package | Installed the HPKG with `pkgman`, verified its Deskbar shortcut, launched `/boot/system/apps/Kiri`, restored the light theme and open files, and copied a selected two-line GitHub permalink whose commit matched `git rev-parse HEAD`. |
| Responsiveness | Opened a 200 MiB document while measuring direct native window-message response; indexed 100,001 files on BFS. See [PERFORMANCE.md](PERFORMANCE.md). |

The GitHub URL used in tests points to a disposable example remote. Remote
fetch/pull/push use real Git commands, but no credentials or live GitHub writes
were used for testing.

## Scope and remaining limits

This beta has the requested native editor, terminal,
Git and preview workflow, with bounded large-file/project support. It is not
backed by a long-duration daily-use study. The [README](../README.md) records
the file-size, indexing, search, encoding and terminal limits. Cold-disk and
very-long-line performance, giant flat directories and low-memory operation
remain separate measurement work. Native CMake builds are supplied but the
tested release build uses the Makefile.

Screenshots show the [launcher](screenshots/launcher.png) in dark and
[light](screenshots/launcher-light.png) themes, [Prettier](screenshots/prettier.png),
[code completion](screenshots/code-completion.png), [file symbols](screenshots/file-symbols.png),
[language tools](screenshots/language-tools.png), [editor preferences](screenshots/preferences.png), the
[ten-theme menu](screenshots/preferences-themes.png), the [workspace](screenshots/workspace.png),
[Git history and diff](screenshots/git.png), [image preview](screenshots/image.png)
and [Daylight theme](screenshots/daylight.png) in the running Haiku application.
VM state, test repositories, recovery crash diagnostics and test
SSH keys remain in ignored development directories.
