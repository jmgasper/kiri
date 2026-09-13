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
