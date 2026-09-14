# Regular expressions and scoped document replacement

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CotEditor supports regex replacement, selection-only searches, whole-word matching, search histories and highlighted results.
Sources: [CotEditor 7.1.0: advanced find and replace](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_find.html).

## Kiri today

[Editor::Find](../../src/ui/Editor.cpp) already accepts a regex flag, but [the find
bar](../../src/ui/Workspace.cpp) only exposes case sensitivity. ReplaceOne and ReplaceAll are
literal and do not substitute capture groups.

## Candidate scope

Extend the existing bar with Regex, Whole Word and In Selection options, match counts and visible
result highlighting. Support capture-group replacement and literal escapes with documented syntax.
Retain a bounded recent-query history. Keep regex behavior consistent across forward search,
backward search and replacement.

## Acceptance checks

- [ ] Replace captured identifiers in a UTF-8 fixture and restore the exact buffer with one Undo.
- [ ] An invalid pattern shows an inline explanation without editing the document.
- [ ] Zero-length matches terminate; selection-only replacement never changes text outside its initial scope.
- [ ] Next/Previous, wrapping, case sensitivity, CRLF and read-only buffers behave consistently.

## Dependencies and review decisions

Choose the regex dialect before coding; a Scintilla regex flag alone does not establish replacement compatibility. Share query semantics with [project search](05-scoped-project-search.md).
