# Line-level Git blame and history navigation

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

Fleet 1.37 documents Git Blame and context actions for closing blame or showing a diff.
Sources: [Fleet 1.37 release notes](https://blog.jetbrains.com/fleet/2024/07/fleet-1-37-is-out-introducing-ai-code-completion-java-folding-imports-by-default-soft-wraps-markdown-asciidoc-other-enhancements/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri already has file history and immutable GitHub permalinks. [GitRepository](../../src/core/Git.h)
has no blame operation, and the [Editor](../../src/ui/Editor.cpp) has no line-author annotations.

## Candidate scope

Add an optional blame gutter or panel showing author, commit and date for the visible file. Let
users open the associated commit in the existing history/diff view. Distinguish uncommitted lines
and map unchanged lines correctly when the buffer differs from HEAD.

## Acceptance checks

- [ ] Inspect a file edited by several commits and open the correct revision from an annotation.
- [ ] Test renames, Unicode paths, untracked files, empty repositories and shallow history.
- [ ] Show unsaved/modified lines as such instead of attributing them to an unrelated commit.
- [ ] Run blame asynchronously with cancellation and bounded output; toggling it does not interrupt typing.

## Dependencies and review decisions

Reuse existing line-mapping logic where appropriate, but verify its suitability for blame. Decide between current-buffer attribution and a clearly labeled HEAD-only first version; the latter must not look like live-line blame.
