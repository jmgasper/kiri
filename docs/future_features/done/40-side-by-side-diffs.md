# Side-by-side diff viewing

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Next** · size: **L**

## Reference behavior

Fleet 1.18 describes synchronized scrolling while comparing code changes side by side.
Sources: [Fleet 1.18 release notes](https://blog.jetbrains.com/fleet/2023/05/fleet-preview-update-1-18-is-out-with-prettier-on-save-net-unit-testing-in-tree-test-rerun-safe-quitting-updated-debug-console-git-integration-improvements-and-more/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[GitView](../../../src/ui/GitView.cpp) sends patch text to one read-only Editor. Kiri has
syntax-colored unified diffs for worktree, index and history, but no aligned original/modified
document views.

## Implemented scope

Offer a side-by-side mode alongside the unified patch view. Align changed blocks, synchronize
scrolling, highlight changed text and navigate hunks. Label both revisions and indicate whether the
working side reflects disk or an unsaved buffer. Keep viewing read-only initially.

## Acceptance checks

- [x] Inspect added, deleted and edited files in worktree, staged and historical comparisons.
- [x] Keep aligned hunks usable with unequal line counts, wrapping, long lines and Unicode.
- [x] Handle binary files, missing final newlines, CRLF and large diffs with explicit limits.
- [x] Switch diff modes and files without showing a stale revision pair or modifying repository state.

## Dependencies and review decisions

General source [splits](01-split-editors.md) can share view infrastructure, but diff alignment has separate requirements. Support [partial staging](../../futures_features/41-partial-git-staging.md) and [merge resolution](../../futures_features/38-merge-conflicts.md) through reusable hunk models.

## Completed policy and verification

Git working-tree, index and historical file pairs use the shared read-only diff
viewer. It provides aligned rows, UTF-8 changes, synchronized scrolling, wrapping,
source labels and changed-block navigation in both modes. Portable and native
checks cover added/deleted/renamed files, missing newlines, CRLF, binary and large
inputs, asynchronous file/repository switches and unchanged repository state.

See the [usage and limits guide](../../EXTERNAL_CHANGES_AND_DIFFS.md),
[portable checks](../../../tests/RefreshDiffTests.cpp),
[native workspace checks](../../../tests/WorkspaceTests.cpp) and
[verification record](../../STATUS.md).
