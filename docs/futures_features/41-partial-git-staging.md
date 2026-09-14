# Stage or unstage selected Git hunks

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Partial**

## Reference behavior

Fleet 1.18 introduced partial commits by selecting individual changes in the diff viewer.
Sources: [Fleet 1.18 release notes](https://blog.jetbrains.com/fleet/2023/05/fleet-preview-update-1-18-is-out-with-prettier-on-save-net-unit-testing-in-tree-test-rerun-safe-quitting-updated-debug-console-git-integration-improvements-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[GitRepository::Stage and Unstage](../../src/core/Git.cpp) operate on file paths.
[GitView](../../src/ui/GitView.cpp) selects changed files, so users cannot separate unrelated edits
within one file through Kiri.

## Candidate scope

Add Stage Hunk and Unstage Hunk to a structured diff view. Build and validate a patch against the
index/worktree versions actually displayed. Refresh both sides after each operation. Selected-line
staging is a possible extension after whole-hunk staging works reliably.

## Acceptance checks

- [ ] Stage one of two distant changes and verify the index contains only that hunk.
- [ ] Unstage the hunk while preserving all working-tree text.
- [ ] An external edit or index update invalidates stale actions instead of applying them to the wrong content.
- [ ] Exercise new/deleted files, CRLF, Unicode paths and missing final newlines in real repositories.

## Dependencies and review decisions

Needs structured hunk data, shared with [diff viewing](40-side-by-side-diffs.md). The Kiri proposal uses its existing staging model to deliver the reference’s partial-commit outcome. Decide whether selected-line staging belongs in the first goal.
