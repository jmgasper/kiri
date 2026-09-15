# Inline diagnostics and a Problems panel

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Soon** · size: **L**

## Reference behavior

Fleet Smart Mode surfaces linter/code-analysis problems in the editor.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[LanguageServer.cpp](../../../src/core/LanguageServer.cpp) and
[WorkspaceLanguage.cpp](../../../src/ui/WorkspaceLanguage.cpp) implement completion and document
symbols, but do not publish a diagnostics UI. The [language tools guide](../../LANGUAGE_TOOLS.md)
explicitly identifies diagnostics as a separate feature.

## Implemented scope

Consume supported LSP diagnostic notifications, underline ranges and list problems by file and
severity in a native panel. Show the diagnostic message at the location, support next/previous
problem, and clear stale reports after edits, closure or server replacement.

## Acceptance checks

- [x] Use real TypeScript and clangd servers to show a deliberate error, jump to it, fix it and see it clear.
- [x] Map UTF-16/UTF-8 positions correctly around emoji and non-ASCII identifiers.
- [x] Out-of-order or obsolete diagnostics cannot decorate the wrong revision or reopened tab.
- [x] Missing servers, unsupported capabilities and the existing 8 MiB language limit produce understandable states without blocking editing.

## Dependencies and review decisions

Choose push diagnostics first; pull diagnostics can follow after capability review. Reserve indicator styles so [semantic highlighting](31-semantic-highlighting.md), find matches and [code actions](../../futures_features/30-code-actions.md) can coexist.

## Completed policy and verification

Push diagnostics decorate open source files and populate a native Problems panel. Navigation, Unicode ranges, shared panes, edits, undo, closure/reopening and server replacement are covered by native tests. Real TypeScript and clangd errors were shown, selected, fixed and cleared. Servers without diagnostic versions use immutable analysis sessions; their extra startup and memory cost is documented.

See the [usage and limits guide](../../LANGUAGE_ANALYSIS.md),
[portable checks](../../../tests/AnalysisThemeTests.cpp),
[native workspace checks](../../../tests/WorkspaceTests.cpp) and
[verification record](../../STATUS.md).
