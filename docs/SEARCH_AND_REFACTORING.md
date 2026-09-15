# Search, replacement and symbol rename

## Find in a document

**Search → Find / Replace** (Alt+F on the default Haiku keymap) opens the find
bar. Use **Case**, **Regex**, **Whole Word**, and **In Selection** to set the
matching rules. Next and Previous wrap at the end of the current scope. Matches
are highlighted and counted; pattern and replacement errors appear in the bar.
**Recent** keeps the last 20 submitted queries across restarts.

Select text in the editor before enabling **In Selection**. The bar captures
that range; navigating matches does not shrink it. Replacement adjusts the end
of that range as text grows or shrinks. An independent edit invalidates the
captured range: select the desired text and enable the option again. Switching
editor views resets the scope. Read-only buffers allow searching but reject edits.

**Replace** changes the selected match, or the next match if the selection is
not a match. Enter in the replacement field does the same. **All** computes the
edits against the original text and applies them as one Undo action. Invalid
patterns, invalid replacements and incomplete searches do not change text.

### Regex and replacement syntax

Both document and project search use **PCRE2**, with UTF-8 and Unicode properties.
Case-insensitive matching uses Unicode case rules. Whole words have boundaries
outside letters, numbers, combining marks and underscores. Document anchors
recognize LF, CR and CRLF line endings. Project search runs the same engine on
each individual line; patterns cannot span project-file line endings.

In Regex mode, replacement supports:

| Syntax | Meaning |
| --- | --- |
| `$0` | The full match |
| `$1` … `$99` | A numbered capture |
| `${name}` | A named capture, such as `(?<name>...)` |
| `$$` | A literal dollar sign |
| `\n`, `\r`, `\t`, `\\` | Newline, carriage return, tab, backslash |

For example, find `([\p{L}_]+)=(\d+)` and replace with `$1: $2` to change
`日本語=7` to `日本語: 7`. Unmatched optional captures insert empty text; unknown
capture references and unknown escapes are errors. With Regex off, both the
query and replacement are literal text, including dollar signs and backslashes.

Matches do not overlap. Empty matches advance by a complete UTF-8 character
(CRLF stays together), so replacing an empty match cannot loop indefinitely.
The byte-oriented `\C` regex escape is disabled. Pattern execution has time,
backtracking, depth and memory limits; an expensive pattern produces an error
instead of applying a partial replacement. Document counts stop at 100,000
matches and show `+`; All requires a complete match set within that limit.

PCRE2's [pattern reference](https://www.pcre.org/current/doc/html/pcre2pattern.html)
describes the dialect; Kiri's replacement syntax is the table above.

## Search project files

**Search → Search Project…** (Alt+Shift+F) searches **disk contents**, as stated
in the result status. Unsaved edits are used when building a replacement preview.

- **Folders:** project-relative folders separated by semicolons, such as `src;tests`.
- **Include / Exclude:** semicolon-separated globs, such as `*.ts;*.tsx` and
  `generated/*;*.min.js`. A pattern without `/` also matches file basenames.
  `*` spans directories; `**/` can also match zero directories.
- **Include ignored files:** explicitly includes Git-ignored files. In projects
  without Git, it includes normally omitted build/vendor/cache directories.
  Repository metadata directories remain excluded.
- **Case / Regex / Whole Word:** the same query rules as document search.

Results are grouped by file and include every match on a line. Line and column
positions count Unicode characters, so opening a result after an emoji or
non-ASCII identifier selects the right location. Changing the query cancels
older work and clears old results. Changing projects closes the previous search.

Search returns up to 2,000 matches from an index of up to 500,000 paths. Status
reports result/index limits and separate counts for binary/unsupported encoding,
files over 32 MiB, unreadable files, symlinks and path filters. Symlinks are not
followed by project search. Search and indexing run on cancellable workers.

## Preview and apply project replacements

Enter a replacement in Search Project and choose **Preview Replace…**. The
preview groups changes by file and labels whether each file is an open buffer
or a disk write. Select a row to inspect its before/after text. Double-click a
file or match, or use **Include / Exclude**, to omit it. Nothing changes until
**Apply Reviewed Changes**.

Open buffers, including buffers in other Kiri windows, supply their current
unsaved text. If a file has independent copies in multiple windows, close one
before previewing; there is no unambiguous authoritative buffer in that case.
The search filters and ignored-file choice still determine which files qualify.

Apply first checks every affected snapshot, then checks each file again before
editing. A buffer revision, document identity or disk change stops the operation
without overwriting the newer text. **Refresh Preview** rebuilds the entries
from current text for another review. An edit made during Apply can stop the
remaining files; the report identifies completed writes and the failed/stale file.

Open files receive one Undo group and remain unsaved. Closed files are saved
with the existing atomic-save helper, preserving the UTF-8 BOM, unchanged line
ending bytes, file permissions and Haiku attributes. Other encodings stay
unsupported for editing. Save, close, detach and reopen actions for affected
files wait until the current operation finishes.

**Cancel remaining** stops before the next file, after the current file has
finished. It reports which files already changed. Preview is limited to 20,000
matches, 32 MiB per resulting file and 128 MiB of combined before/after text.
A truncated preview cannot be applied; narrow the folders or filters instead.

## Rename a symbol

Put the caret on a source identifier and choose **Edit → Rename Symbol…** or
the editor context-menu command. The configured language server must be ready
and support rename. Kiri calls `prepareRename` when the server supports it,
then asks for a new identifier and requests a workspace edit.

The result uses the same before/after preview, snapshot checks, safe saves and
recovery as project replacement. All returned symbol edits stay selected
together. Unsaved text, including text in another Kiri window, is synchronized
for the request. Invalid identifier syntax, reserved TypeScript/JavaScript/C++
keywords, server errors, stale document versions, invalid Unicode ranges,
overlapping edits and unsupported file operations produce useful errors.
File creation, file deletion, file renaming and annotated workspace edits are
rejected as a whole. Unsolicited `workspace/applyEdit` requests remain disabled.

Closed files must be in the project snapshot captured for the request; an edit
outside that snapshot is rejected as a whole. Open source buffers can also
participate when they are outside the project folder. The language server's
[rename protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_rename)
defines the semantic edits.

## Undo and recovery

**Edit → Undo Last Project Edit…** opens a restore preview for the most recent
operation. **Restore Changed Files** checks the current text and disk stamps
and restores only files changed by that operation. Newer text is retained and
reported. An open-buffer change already undone locally is recognized and kept.
For rename, the normal **Edit → Undo** also restores the whole rename when the
active affected buffer still contains its renamed text. Later unrelated edits
keep their ordinary buffer Undo behavior.

Before each change, Kiri writes a private recovery journal containing the original
text and expected replacement. Closed-file recovery is available after restarting
Kiri through the same menu. The final report includes the journal path, under
the initiating window's settings/session directory in `project-edits/`. The
`latest` file points to the last journal; earlier journals remain available as
explicit before/after text records. No restore blindly overwrites changed text.

Ordinary dirty-buffer crash recovery remains active alongside these journals.
If an originally open file was later saved and closed, restoring can write its
original text back to disk after verifying its contents still equal the applied
text. A closed file opened after Apply is restored on disk and in its buffer.
