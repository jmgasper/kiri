# Quick fixes and refactoring code actions

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Missing**

## Reference behavior

Fleet exposes quick fixes and intention actions for the current code context.
Sources: [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/), [Kotlin support in Fleet, October 2023](https://blog.jetbrains.com/kotlin/2023/10/kotlin-support-in-jetbrains-fleet/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[WorkspaceLanguage.cpp](../../src/ui/WorkspaceLanguage.cpp) can execute commands attached to
completion items, but it has no codeAction request or picker. General workspace edits remain
explicitly disabled in [LanguageServer.cpp](../../src/core/LanguageServer.cpp).

## Candidate scope

Request supported actions for a selection or diagnostic, list clear action titles and resolve lazy
actions when needed. Preview multi-file changes and handle edit-bearing and command-bearing actions.
Begin with quick fixes and organize imports; advertise only the kinds actually implemented.

## Acceptance checks

- [ ] Apply an import fix and a server-provided refactoring against unsaved text.
- [ ] Handle edits returned directly and through a subsequent workspace/applyEdit request.
- [ ] Preserve one coherent undo/restore operation and reject stale versions or unsupported resource operations.
- [ ] A command failure, cancelled request or unsupported capability leaves the document recoverable and explains the outcome.

## Dependencies and review decisions

Depends on [diagnostics](26-diagnostics.md) for problem-driven entry points and the transaction layer described in [rename](../future_features/done/29-symbol-rename.md). Server commands may execute project tooling; define the user-triggered execution boundary explicitly.
