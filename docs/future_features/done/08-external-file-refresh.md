# Reload external edits and refresh the project automatically

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Soon** · size: **M**

## Reference behavior

CodeEdit 0.3.6 reflects externally changed files in open editors automatically.
Sources: [CodeEdit 0.3.6 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.6).

## Baseline before implementation

[Workspace::Pulse](../../../src/ui/Workspace.cpp) detects changed file stamps and marks tabs; safe
saves reject conflicts. The file tree and index have manual refresh. Detection is present, but
automatic reload and a dirty-buffer reconciliation flow are absent.

## Implemented scope

Reload clean documents after external writes while retaining view positions. For dirty documents,
show compare/reload/keep-editing choices with a recoverable copy. Watch or poll relevant directories
incrementally so external creation, rename and deletion update the tree and index.

## Acceptance checks

- [x] Change a clean file externally and see its new contents, symbols and line-ending metadata without reopening it.
- [x] Change a dirty file externally and verify both versions remain recoverable until the user decides.
- [x] Create, rename and remove files outside Kiri and verify tree, quick-open and search results refresh.
- [x] Atomic-save rename events, rapid writes and Kiri’s own saves do not cause repeated prompts or reload loops.

## Dependencies and review decisions

Reuse stamp checking and safe-save behavior; investigate Haiku node monitoring with a polling fallback. Dirty-file comparison can reuse [side-by-side diffs](40-side-by-side-diffs.md).

## Completed policy and verification

Clean documents reload through checked background reads; shared pane positions,
metadata and language symbols are updated together. Dirty-buffer and disk copies
are retained before Compare, Reload Disk or Keep Editing. Native notifications
and incremental polling refresh loaded tree branches, the index and live searches.
Tests cover atomic saves, rapid writes, typing during reload, deleted files,
recovery copies, own-save stability and image refresh.

See the [usage and limits guide](../../EXTERNAL_CHANGES_AND_DIFFS.md),
[portable checks](../../../tests/RefreshDiffTests.cpp),
[native workspace checks](../../../tests/WorkspaceTests.cpp) and
[verification record](../../STATUS.md).
