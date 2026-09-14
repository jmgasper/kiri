# Previewed symbol rename across files

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Missing**

## Reference behavior

Fleet Smart Mode offers semantic Rename refactoring.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[LanguageServer.cpp](../../src/core/LanguageServer.cpp) advertises applyEdit=false and rejects
workspace/applyEdit. [LanguageProtocol.cpp](../../src/core/LanguageProtocol.cpp) handles edits
within one buffer, which is insufficient for multi-file rename.

## Candidate scope

Implement prepareRename where supported, request a new name and preview the returned workspace edit.
Validate all affected document versions and disk stamps before applying edits. Keep open unsaved
text authoritative and report unsupported file operations rather than applying only the easy part of
an edit.

## Acceptance checks

- [ ] Rename a TypeScript or C++ symbol across declarations, imports and usages without changing unrelated text.
- [ ] Reject invalid names and unsupported operations with useful messages.
- [ ] Edit an affected file after preview and verify rename cannot overwrite newer work.
- [ ] Recover from partial failure and provide the reviewed undo/restore behavior for all affected files.

## Dependencies and review decisions

Requires a reusable multi-document edit transaction, shared with [project replacement](06-project-replace.md) and [code actions](30-code-actions.md). Decide save behavior for previously closed files before making this an implementation goal.
