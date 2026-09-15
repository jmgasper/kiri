# Previewed symbol rename across files

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Next** · size: **L**

## Reference behavior

Fleet Smart Mode offers semantic Rename refactoring.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[LanguageServer.cpp](../../../src/core/LanguageServer.cpp) advertises applyEdit=false and rejects
workspace/applyEdit. [LanguageProtocol.cpp](../../../src/core/LanguageProtocol.cpp) handles edits
within one buffer, which is insufficient for multi-file rename.

## Implemented scope

Implement prepareRename where supported, request a new name and preview the returned workspace edit.
Validate all affected document versions and disk stamps before applying edits. Keep open unsaved
text authoritative and report unsupported file operations rather than applying only the easy part of
an edit.

## Acceptance checks

- [x] Rename a TypeScript or C++ symbol across declarations, imports and usages without changing unrelated text.
- [x] Reject invalid names and unsupported operations with useful messages.
- [x] Edit an affected file after preview and verify rename cannot overwrite newer work.
- [x] Recover from partial failure and provide the reviewed undo/restore behavior for all affected files.

## Dependencies and review decisions

Requires a reusable multi-document edit transaction, shared with [project replacement](06-project-replace.md) and [code actions](../../futures_features/30-code-actions.md). Decide save behavior for previously closed files before making this an implementation goal.

## Decision

For previously closed files, they should be saved after the change, but we should allow the undo action to reverse the entire rename, if needed.

## Completed policy and verification

Rename uses prepareRename where supported, a validated identifier prompt
and a complete workspace-edit preview. It synchronizes authoritative unsaved
buffers, validates document versions and UTF positions, and rejects unsupported
resource operations or annotations as a whole. Closed-file writes and open
buffer edits share replacement's journal and restore mechanism; normal Undo
also reverses the complete last rename. Native tests exercise the full request,
prompt, preview, apply and cross-window Undo flow. Real TypeScript tests rename
an exported declaration, import and call without touching a string literal or
a shadowed parameter, then restore the closed file.

See [search and refactoring](../../SEARCH_AND_REFACTORING.md),
[portable search/edit checks](../../../tests/SearchTests.cpp),
[native editor checks](../../../tests/NativeTests.cpp),
[workspace checks](../../../tests/WorkspaceTests.cpp),
[real language-server checks](../../../tests/LanguageTests.cpp) and
[the verification record](../../STATUS.md).
