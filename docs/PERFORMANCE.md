# Native measurements

Measured on Haiku R1/beta5 x86_64 (hrev57937+113), GCC 13.3 `-O2`,
Scintilla 5.3.4 and Lexilla 5.4.6, in QEMU/KVM with 4 vCPUs and 4 GiB RAM.
The host is an AMD Ryzen AI Max+ 395. These are warm-cache development
measurements from September 13, 2026, not a comparison with another editor.

| Operation | Observed result |
| --- | ---: |
| Load 200 MiB / 2,097,152 lines on a background thread | 503.7 ms |
| Attach the loaded document to Scintilla | 0.48 ms |
| Increase in resident mapped areas during that load | 473.3 MiB |
| Index 100,001 files on BFS | 824.5 ms |
| Fuzzy quick-open query across that index | 13.0 ms |
| 200 direct native window queries during opening: median | 0.269 ms |
| Same native window queries: maximum | 11.219 ms |

The resident-area measurement includes allocator capacity, line data and mapped
areas. It is not an exact private-heap measurement. The file fixture has short,
100-byte lines; extremely long lines, complex lexers, cold disks, slower CPUs,
network volumes and sustained memory pressure need separate measurements.

Reproduce on Haiku (the fixture directory must not already exist):

```sh
make check check-native
build-haiku/kiri_native_tests --fixtures /boot/home/KiriPerformance
build-haiku/kiri_native_tests --benchmark /boot/home/KiriPerformance
# With Kiri running and large.txt closed:
build-haiku/kiri_native_tests --probe-file /boot/home/KiriPerformance/large.txt
```

The loader streams 1 MiB blocks into a detached Scintilla document, validates
UTF-8 across block boundaries and keeps a 64 KiB binary-preview sample. Its
ownership follows [Scintilla's background-loading API](https://www.scintilla.org/ScintillaDoc.html#BackgroundLoadSave).
Files over 8 MiB use the null lexer and omit per-character style storage.
The current text limit is 1 GiB, subject to available memory. Explicit saves
take one complete text snapshot so editing can continue while disk I/O runs.
Recovery snapshots are copied in 2 MiB chunks and abandoned if editing changes
the document during capture.

Project indexing is capped at 500,000 paths; quick-open returns 100 candidates.
Project text search returns up to 2,000 matches and skips binary files, symlinks
and individual files over 32 MiB. Git command output is capped at 32 MiB.
Git history loads 200 commits per request and can continue to the beginning.

## Large and network folders

Measured September 30, 2026 on the X399 workstation (Threadripper 1950X,
Haiku x86_64 hrev60097 fork) with the workspace harness. `/Documents` is an SMB
share mounted through userlandfs, with about a hundred top-level entries and
many thousands of files below them. A local file was already open, as in
[issue #1](https://github.com/jmgasper/kiri/issues/1).

| Scenario | Before (develop `2b0c608`) | After |
| --- | ---: | ---: |
| Open `Readme.md` 10 s after opening the folder | not open after 45 s | about 1 s (1 s polling) |
| Open four files after 60 s of indexing | — | 70–176 ms each |
| Window round trip during indexing (40 samples) | — | under 150 ms |
| Quit while indexing | — | 44–50 ms |
| Close with a walk blocked in the kernel (test) | waits for the walk | 2.04 s (bounded) |

Before the change, a debugger report of the stuck harness showed both shared
workers in `IndexProject` → `ListDirectory` → `lstat`: the initial walk, and a
second full walk started because the change scan reported every directory it
discovered for the first time as changed. The file load queued behind them.
Every entry also cost two metadata reads during listing and a third during the
change scan, and watch targets were resolved with `stat` on the window thread.

Now document loading, tree listings and project-wide walks use separate workers;
the project index and change scan share one worker and never run twice at once.
First-time discovery is not a change, entries cost one read, and watch targets
come from the scan. Open Quickly uses partial index snapshots (after 0.5 s, then
every 2 s) instead of walking the project itself. Closing a window cancels all
workers and waits at most 2 s; a worker blocked in the kernel is left to finish
on its own, and killed child processes that do not exit within 1 s are reaped
on a detached thread. Reproduce the blocked-walk check with
`build-haiku/kiri_workspace_tests --slow-project`.

## Minimap

Measured September 15, 2026 in the same Haiku beta5 VM with a native editor
window and warm allocator. Each fixture is loaded before enabling its minimap.

| Document | Cell cache | Forced sampling | Insert at start | Refresh after edit |
| --- | ---: | ---: | ---: | ---: |
| 8 KiB | 13,600 bytes | 0.616 ms | 0.069 ms | 0.085 ms |
| 8 MiB | 94,560 bytes | 1.827 ms | 1.322 ms | 0.603 ms |
| 200 MiB | 0 bytes (paused) | 0.195 ms | 12.115 ms | 0.006 ms |

Sampling and edit timings measure synchronous native calls, excluding the timer
interval and display latency. The 200 MiB sample measures the pause check. No
additional resident mapped areas were observed while enabling these minimaps in
this warm run; allocator reuse means that does not imply zero memory cost.
The explicit cell cache is capped at 160 KiB per view, plus the palette and native
widget. It samples the existing text and styles without another document buffer.

Minimaps update visible views every 150 ms and pause above 32 MiB or 500,000
document lines. The pause message remains visible while editing continues.
Turning the minimap off frees its cells and restores editor width.
Reproduce with `build-haiku/kiri_workspace_tests --editor-options` after
`make -j4 build-haiku/kiri_workspace_tests`. See
[editor options](EDITOR_OPTIONS.md) for sampling and navigation behavior.

## Markdown preview

Measured September 16, 2026 in the Haiku beta5 VM. The native Markdown test
opens a real workspace and renders a small README before the long fixture, so
fonts and the translator service are warm. The long fixture contains 2,000
headings and paragraphs with emphasis and links (8,001 source lines).

| Operation | Observed elapsed time |
| --- | ---: |
| Parse a 134,890-byte README on the worker/core path | 4 ms |
| Replace the buffer and deliver the 8,001-line native preview | 385 ms |
| Resize and settle the same preview | 42 ms |

The render measurement includes the 100 ms change check, 150 ms debounce,
parsing, native layout and test polling; it is not a per-keystroke cost or a
worst-case guarantee. Native font measurements are cached and adjacent runs
share drawing operations. Source edits never wait on Markdown parsing or image
decoding; bounded native layout runs on the window thread when results arrive.

The preview pauses above 2 MiB, 30,000 source lines, 150,000 parser events, 128
nesting levels or 60,000 layout fragments. Local image input and decoded bitmap
headers have separate limits. See [Markdown preview](MARKDOWN_PREVIEW.md) for
resource policy and limits. Reproduce with `make check check-markdown-native`
inside the VM with the desktop awake.
