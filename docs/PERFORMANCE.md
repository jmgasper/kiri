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
