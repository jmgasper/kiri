# Regular expressions and scoped document replacement

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Soon** · size: **M**

## Reference behavior

CotEditor supports regex replacement, selection-only searches, whole-word matching, search histories and highlighted results.
Sources: [CotEditor 7.1.0: advanced find and replace](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_find.html).

## Baseline before implementation

[Editor::Find](../../../src/ui/Editor.cpp) already accepts a regex flag, but [the find
bar](../../../src/ui/Workspace.cpp) only exposes case sensitivity. ReplaceOne and ReplaceAll are
literal and do not substitute capture groups.

## Implemented scope

Extend the existing bar with Regex, Whole Word and In Selection options, match counts and visible
result highlighting. Support capture-group replacement and literal escapes with documented syntax.
Retain a bounded recent-query history. Keep regex behavior consistent across forward search,
backward search and replacement.

## Acceptance checks

- [x] Replace captured identifiers in a UTF-8 fixture and restore the exact buffer with one Undo.
- [x] An invalid pattern shows an inline explanation without editing the document.
- [x] Zero-length matches terminate; selection-only replacement never changes text outside its initial scope.
- [x] Next/Previous, wrapping, case sensitivity, CRLF and read-only buffers behave consistently.

## Dependencies and review decisions

Choose the regex dialect before coding; a Scintilla regex flag alone does not establish replacement compatibility. Share query semantics with [project search](05-scoped-project-search.md).

## Completed policy and verification

The find bar now exposes PCRE2 UTF-8 regex, whole words, captured selection
scope, result indicators/counts and a persisted 20-query history. Capture and
escape syntax is documented in the [usage guide](../../SEARCH_AND_REFACTORING.md).
Portable and native tests exercise Unicode captures, invalid patterns and
replacements, one-step Undo, zero-width progress, wrapping, case, CRLF,
selection confinement and read-only buffers.

See [search and refactoring](../../SEARCH_AND_REFACTORING.md),
[portable search/edit checks](../../../tests/SearchTests.cpp),
[native editor checks](../../../tests/NativeTests.cpp),
[workspace checks](../../../tests/WorkspaceTests.cpp),
[real language-server checks](../../../tests/LanguageTests.cpp) and
[the verification record](../../STATUS.md).
