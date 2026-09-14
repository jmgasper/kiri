# Reload external edits and refresh the project automatically

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit 0.3.6 reflects externally changed files in open editors automatically.
Sources: [CodeEdit 0.3.6 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.6).

## Kiri today

[Workspace::Pulse](../../src/ui/Workspace.cpp) detects changed file stamps and marks tabs; safe
saves reject conflicts. The file tree and index have manual refresh. Detection is present, but
automatic reload and a dirty-buffer reconciliation flow are absent.

## Candidate scope

Reload clean documents after external writes while retaining view positions. For dirty documents,
show compare/reload/keep-editing choices with a recoverable copy. Watch or poll relevant directories
incrementally so external creation, rename and deletion update the tree and index.

## Acceptance checks

- [ ] Change a clean file externally and see its new contents, symbols and line-ending metadata without reopening it.
- [ ] Change a dirty file externally and verify both versions remain recoverable until the user decides.
- [ ] Create, rename and remove files outside Kiri and verify tree, quick-open and search results refresh.
- [ ] Atomic-save rename events, rapid writes and Kiri’s own saves do not cause repeated prompts or reload loops.

## Dependencies and review decisions

Reuse stamp checking and safe-save behavior; investigate Haiku node monitoring with a polling fallback. Dirty-file comparison can reuse [side-by-side diffs](40-side-by-side-diffs.md).
