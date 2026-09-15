# Previewed replacement across project files

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Soon** · size: **L**

## Reference behavior

CodeEdit’s Find navigator includes Replace All across matching files.
Sources: [CodeEdit 0.3.6: project replacement](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/FindNavigator/FindNavigatorForm.swift), [CodeEdit 0.3.6: replacement results](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/FindNavigator/FindNavigatorView.swift).

## Baseline before implementation

[Project.h](../../../src/core/Project.h) exposes search results only. Kiri’s replacement commands
operate on the current [Editor](../../../src/ui/Editor.cpp); there is no multi-file edit plan or
replacement preview.

## Implemented scope

Build a preview of replacements grouped by file, with per-file and per-match selection before Apply.
Use current buffers for open documents and checked snapshots for closed files. Report exactly which
files changed, failed or became stale. Design recovery for a partly applied operation.

## Acceptance checks

- [x] Preview a replacement across saved and dirty files and exclude selected matches before applying.
- [x] Detect edits or disk changes since preview and refresh affected entries without overwriting newer text.
- [x] Preserve encoding metadata, line endings, attributes and permissions through existing safe-save helpers.
- [x] Undo open-buffer edits and provide an explicit restore path for disk writes; cancellation reports any completed writes.

## Dependencies and review decisions

Build on [scoped search](05-scoped-project-search.md). Decide whether closed files are opened as dirty buffers or written after approval; specify that choice before an implementation goal. Reuse transaction work with [rename](29-symbol-rename.md).

## Decision

Closed files should be written after approval

## Completed policy and verification

Replacement previews use current indexed open buffers across Kiri windows
and checked closed-file snapshots. File and match rows can be excluded before
Apply. Open buffers receive one undo group; closed files use the existing
safe-save helper after approval. Stale plans require refreshed review. Per-file
reports and write-ahead journals retain completed writes through cancellation
or a later failure. Undo Last Project Edit restores checked buffers and disk
writes, including recovery after restarting. Native tests verify dirty buffers,
match exclusion, stale edits, partial failure, cancellation, cross-window edits
and restore; Haiku file tests verify BOM, CRLF, attributes and permissions.

See [search and refactoring](../../SEARCH_AND_REFACTORING.md),
[portable search/edit checks](../../../tests/SearchTests.cpp),
[native editor checks](../../../tests/NativeTests.cpp),
[workspace checks](../../../tests/WorkspaceTests.cpp),
[real language-server checks](../../../tests/LanguageTests.cpp) and
[the verification record](../../STATUS.md).
