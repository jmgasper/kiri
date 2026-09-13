# Beta 3 verification record

Verified September 13, 2026 on Haiku R1/beta5 x86_64, hrev57937+113, in
QEMU/KVM with 4 vCPUs and 4 GiB RAM. Native build: GCC 13.3, Scintilla 5.3.4,
Lexilla 5.4.6. Portable core also built and tested with Linux GCC 13.3.

## Automated checks

- **163 core checks pass on Linux and Haiku.** Coverage includes literal
  subprocess arguments, timeout/cancellation/output limits; UTF-8 boundaries,
  streamed loading and BOM handling; safe-save conflicts, modes and symlinks;
  checksummed draft recovery and corrupt/truncated records;
  Git staging, unstaging, commits, history pagination, rename status, patches
  and graph lanes; GitHub immutable line/range mapping and boundaries; ignore-aware indexing
  and search; terminal escape handling and real PTY shells including Ctrl+D,
  independent directories and variables, and closing one shell while another runs.
- **190 native editor, file and worker checks pass.** C++/JSON token styles,
  Unicode navigation, dirty state, undo, current-match and all-match replacement,
  ten themes (five dark and five light), text/syntax contrast, installed font
  selection and size, settings round trips and legacy defaults, invalid settings
  fallback, unchanged text/selection/dirty state during appearance changes,
  native MIME icons, streamed document adoption, text/binary Haiku attribute
  preservation during saves and cancellation of superseded work.
- The native performance fixture adds checks for the 200 MiB document's byte
  and line counts and a 100,001-file index. See [measurements](PERFORMANCE.md).

## Observed native interaction

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
| Workspace | Native toolbar, file tree, resizable panels, scrolling tabs, keyboard navigation, line gutter, file-type icons and ten color themes. |
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

Screenshots show [editor preferences](screenshots/preferences.png), the
[ten-theme menu](screenshots/preferences-themes.png), the [workspace](screenshots/workspace.png),
[Git history and diff](screenshots/git.png), [image preview](screenshots/image.png)
and [Daylight theme](screenshots/daylight.png) in the running Haiku application.
VM state, test repositories, recovery crash diagnostics and test
SSH keys remain in ignored development directories.
