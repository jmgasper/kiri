# Beta 1 verification record

Verified September 13, 2026 on Haiku R1/beta5 x86_64, hrev57937+113, in
QEMU/KVM with 4 vCPUs and 4 GiB RAM. Native build: GCC 13.3, Scintilla 5.3.4,
Lexilla 5.4.6. Portable core also built and tested with Linux GCC 13.3.

## Automated checks

- **155 core checks pass on Linux and Haiku.** Coverage includes literal
  subprocess arguments, timeout/cancellation/output limits; UTF-8 boundaries,
  streamed loading and BOM handling; safe-save conflicts, modes and symlinks;
  checksummed draft recovery and corrupt/truncated records;
  Git staging, unstaging, commits, history pagination, rename status, patches
  and graph lanes; GitHub immutable line/range mapping and boundaries; ignore-aware indexing
  and search; terminal escape handling and a real PTY shell including Ctrl+D.
- **34 native editor, file and worker checks pass.** C++/JSON token styles,
  Unicode navigation, dirty state, undo, current-match and all-match replacement,
  three themes, streamed document adoption, text/binary Haiku attribute
  preservation during saves and cancellation of superseded work.
- The native performance fixture adds checks for the 200 MiB document's byte
  and line counts and a 100,001-file index. See [measurements](PERFORMANCE.md).

## Observed native interaction

| Area | Evidence |
| --- | --- |
| Workspace | Native toolbar, file tree, resizable panels, scrolling tabs, keyboard navigation, line gutter and three color themes. |
| Source files | C++, HTML, JSON, XML and Python render with highlighting; go to line and project-search navigation reach the requested location. |
| Editing and saving | Native Save As, existing-file save, consecutive untitled files through Save All, clipboard and undo/redo. Cancelling quit after an earlier Discard leaves both remaining documents dirty. |
| Recovery | Force-killed Kiri with an unsaved two-line draft, restarted it, verified the text and dirty state, then saved it and confirmed draft cleanup. |
| Navigation | Quick-open query and Enter open JSON; project search returns matching files and opens XML at line 2, column 2. |
| Terminal | Interactive shell, commands, ANSI colors, Ctrl+C, Ctrl+D shell exit, restart and full-screen Haiku `top`. |
| Git | Selected worktree and index diffs; staged/unstaged and committed through the UI, confirmed by Git. Loaded all 231 test commits to the root, including a merge graph; file history shows the corresponding patch. |
| Permalinks | Copied a real clipboard URL with the selected line at an immutable commit; automated tests cover ranges, escaped paths, remote formats and changed-line rejection. |
| Previews | Native transparent PNG preview, fit/actual-size display and bounded binary/hex preview. |
| Package | Installed the HPKG with `pkgman`, verified its Deskbar shortcut, launched `/boot/system/apps/Kiri`, restored the light theme and open files, and copied a selected two-line GitHub permalink whose commit matched `git rev-parse HEAD`. |
| Responsiveness | Opened a 200 MiB document while measuring direct native window-message response; indexed 100,001 files on BFS. See [PERFORMANCE.md](PERFORMANCE.md). |

The GitHub URL used in tests points to a disposable example remote. Remote
fetch/pull/push use real Git commands, but no credentials or live GitHub writes
were used for testing.

## Scope and remaining limits

This is the first usable beta. It has the requested native editor, terminal,
Git and preview workflow, with bounded large-file/project support. It is not
backed by a long-duration daily-use study. The [README](../README.md) records
the file-size, indexing, search, encoding and terminal limits. Cold-disk and
very-long-line performance, giant flat directories and low-memory operation
remain separate measurement work. Native CMake builds are supplied but the
tested release build uses the Makefile.

Screenshots show the [workspace](screenshots/workspace.png),
[Git history and diff](screenshots/git.png), [image preview](screenshots/image.png)
and [Daylight theme](screenshots/daylight.png) in the running Haiku application.
VM state, test repositories, recovery crash diagnostics and test
SSH keys remain in ignored development directories.
