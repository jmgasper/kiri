# Line sorting with numeric and pattern keys

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Missing**

## Reference behavior

CotEditor sorts lines by entire text, a delimited column or a regex capture, with numeric sorting and an option to preserve a header.
Sources: [CotEditor 7.1.0: pattern sort](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_pattern_sort.html).

## Kiri today

[Editor.cpp](../../src/ui/Editor.cpp) implements search/replacement and applies text edits, but Kiri
has no sorting command or sort-options UI. This gap does not imply that Scintilla lacks ordinary
line movement or duplication.

## Candidate scope

Provide Sort Lines for the selection with ascending/descending, case sensitivity, natural numeric
order, a column delimiter or regex key, and Keep First Line. Preview how the key is extracted from a
sample. Leave duplicate removal and broader text transformations for separately reviewed extensions.

## Acceptance checks

- [ ] Sort names containing 2 and 10 numerically and lexically with different expected results.
- [ ] Sort tab-separated rows by a selected column while retaining the header.
- [ ] Invalid patterns or missing keys follow an explained policy without dropping lines.
- [ ] One Undo restores bytes exactly, including mixed line endings and the final unterminated line.

## Dependencies and review decisions

Reuse regex semantics from [advanced find](04-advanced-find-replace.md). Decide stable ordering for equal/missing keys and whether locale-sensitive collation is required.
