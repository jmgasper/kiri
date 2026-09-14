# Side-by-side diff viewing

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Partial**

## Reference behavior

Fleet 1.18 describes synchronized scrolling while comparing code changes side by side.
Sources: [Fleet 1.18 release notes](https://blog.jetbrains.com/fleet/2023/05/fleet-preview-update-1-18-is-out-with-prettier-on-save-net-unit-testing-in-tree-test-rerun-safe-quitting-updated-debug-console-git-integration-improvements-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[GitView](../../src/ui/GitView.cpp) sends patch text to one read-only Editor. Kiri has
syntax-colored unified diffs for worktree, index and history, but no aligned original/modified
document views.

## Candidate scope

Offer a side-by-side mode alongside the unified patch view. Align changed blocks, synchronize
scrolling, highlight changed text and navigate hunks. Label both revisions and indicate whether the
working side reflects disk or an unsaved buffer. Keep viewing read-only initially.

## Acceptance checks

- [ ] Inspect added, deleted and edited files in worktree, staged and historical comparisons.
- [ ] Keep aligned hunks usable with unequal line counts, wrapping, long lines and Unicode.
- [ ] Handle binary files, missing final newlines, CRLF and large diffs with explicit limits.
- [ ] Switch diff modes and files without showing a stale revision pair or modifying repository state.

## Dependencies and review decisions

General source [splits](../future_features/done/01-split-editors.md) can share view infrastructure, but diff alignment has separate requirements. Support [partial staging](41-partial-git-staging.md) and [merge resolution](38-merge-conflicts.md) through reusable hunk models.
