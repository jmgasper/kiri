# External changes and diff viewing

## Automatic refresh

Clean source files reload after an external write. Reload updates all shared
views, UTF-8 BOM and line-ending metadata, syntax highlighting and language
server symbols. Each pane retains its selection by line and column, scroll
position, wrapping and zoom; positions beyond the new file are clamped. The
new disk contents become the saved version and replace the previous undo stack.
Image and bounded binary previews refresh as well. An undecodable image keeps
its previous preview and reports the external change.

If typing, saving, closing or a project edit intervenes while a read is in
flight, the stale read cannot replace the buffer. Atomic saves are checked at
the path after reading, so a replaced inode is handled as the new file. Kiri's
own saves update their expected disk stamps and do not trigger reload loops.

External file creation, rename and deletion refresh the file tree, project
index, Open Quickly and open project-search windows. Loaded tree branches keep
their expanded state, selection and scroll position. Deleted tracked files
are removed from the available-file index even while Git still records them.

Haiku directory notifications are debounced for 250 ms. Up to 512 directories
are watched, prioritizing open-file parents and loaded tree branches. A
two-second fallback polls open files and scans project directories incrementally:
up to 64 directories per batch, stopping after a directory when 100 ms has
elapsed. Large trees therefore refresh over successive batches. The monitor
tracks up to 50,000 directories, skips VCS internals and does not recurse through
symlink directories. Failed or unsupported watches still have polling coverage.
The **FILES** refresh button remains available.

Scanning uses one metadata read per entry, and the scan and project index share
one background worker, separate from document loading and tree listings. A
directory listed for the first time is not treated as changed unless something
in it is newer than the start of the latest index (or it was unreadable
before), so discovering a large tree never restarts the index. Refresh requests
made while an index is being built are combined into one walk afterwards.
Directory metadata for watches is read on that worker, never on the window
thread.

## When the buffer has unsaved changes

Kiri leaves your text in place and shows an external-change bar. It first
preserves the buffer and the observed disk version under:

`~/config/settings/Kiri/external-changes/<timestamp>/`

Each folder contains `buffer-<filename>`, `disk-<filename>` when the disk file
exists, and a `README.txt` identifying the original path. The buffer copy
preserves its BOM and line endings; the disk copy contains the complete bytes.
If copying fails, Kiri reports the failure and does not reload or authorize an
overwrite. A deleted file keeps its editor text and records that the disk side
is absent.

- **Compare…** shows read-only disk and unsaved-buffer snapshots in the shared
  diff viewer. Reopening Compare captures new typing or a newer disk version.
- **Reload Disk** replaces the buffer with the reviewed disk contents. Any text
  typed since the earlier comparison is backed up first. A later disk change
  requires a fresh comparison instead of applying stale contents.
- **Keep Editing** retains your buffer and accepts the reviewed disk version as
  the base for the next Save. That Save can replace this disk version; a later
  external write still fails the normal conflict check.
- **Open Backups…**, also available under **Search → Open External Backups…**,
  opens a native file panel at the retained copies. Open a copy to inspect it;
  use Save As if you want to restore it to another path.

Backups survive Save, Reload Disk, normal exit and restart. They are separate
from periodic crash-recovery drafts and are not automatically deleted. Remove
unneeded version folders with Tracker after you have recovered what you need.
Closing a comparison does not decide the conflict or change either version.

## Git comparisons

Select a changed file in the Git workspace, then choose **Working Tree** or
**Staged**. Select a commit in history and use **File** to choose one of its
changed files. The comparison labels identify both sources:

| Comparison | Original | Modified |
| --- | --- | --- |
| Working Tree | Index snapshot | File on disk at comparison time |
| Staged | Captured HEAD revision | Index snapshot |
| Commit history | First parent, or absent for an initial commit | Selected commit |
| External change | Captured disk version, or absent after deletion | Unsaved editor snapshot |

Added and untracked files have an absent original. Deleted files have an absent
modified side. Renames retain both paths. Historical merge comparisons use the
first parent. Git viewing does not include unsaved buffer text in the working
tree side, stage files, write the index or modify repository contents. A draft
commit message is submitted only with Enter or **Commit Staged**, not by moving
focus into the comparison.

**Side by side** shows syntax-colored source text, source line numbers, changed
line backgrounds and changed UTF-8 text ranges. Blank annotations align insertions,
deletions and unequal replacement blocks. **Wrap lines** also aligns different
wrapped heights. Vertical and horizontal scrolling are synchronized; **Previous**
and **Next** move through changed blocks. Turn off **Side by side** for a unified
patch with the same navigation. Switching modes retains the captured pair;
selecting a different file, commit or repository immediately clears the old pair
while its replacement loads. Late results cannot revive an earlier selection.

## Comparison limits

Text comparisons accept UTF-8, including CRLF, CR, LF and files without a final
newline. Revision labels state the line-ending style, and unified output marks
missing final newlines. Binary and non-UTF-8 data show an explanation and source
metadata instead of a misleading text alignment.

The limits are 8 MiB and 100,000 lines per side, 2,000 changed files per historical
commit, 8 million diff work steps and an edit-distance limit of 2,048 inserted or
deleted lines. Exceeding a limit reports it without displaying a partial pair.
External-change copies retain the full file even when its comparison is too
large to display. Memory exhaustion or file-system errors leave the current
buffer intact and keep any completed backup copies.

The reusable [diff model](../src/core/Diff.h) exposes source line spans, aligned
rows and changed-block ranges for future staging and merge features. Those
write operations are outside this read-only viewer.

Implementation references: [Haiku node monitoring](https://www.haiku-os.org/docs/api/NodeMonitor_8h.html),
[Scintilla wrapping and annotations](https://www.scintilla.org/ScintillaDoc.html#LineWrapping),
and [Git diff](https://git-scm.com/docs/git-diff). Verification is recorded in
[STATUS.md](STATUS.md), [portable checks](../tests/RefreshDiffTests.cpp) and
[native workspace checks](../tests/WorkspaceTests.cpp).
