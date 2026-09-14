# Inline diagnostics and a Problems panel

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **L** · Kiri gap: **Missing**

## Reference behavior

Fleet Smart Mode surfaces linter/code-analysis problems in the editor.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[LanguageServer.cpp](../../src/core/LanguageServer.cpp) and
[WorkspaceLanguage.cpp](../../src/ui/WorkspaceLanguage.cpp) implement completion and document
symbols, but do not publish a diagnostics UI. The [language tools guide](../LANGUAGE_TOOLS.md)
explicitly identifies diagnostics as a separate feature.

## Candidate scope

Consume supported LSP diagnostic notifications, underline ranges and list problems by file and
severity in a native panel. Show the diagnostic message at the location, support next/previous
problem, and clear stale reports after edits, closure or server replacement.

## Acceptance checks

- [ ] Use real TypeScript and clangd servers to show a deliberate error, jump to it, fix it and see it clear.
- [ ] Map UTF-16/UTF-8 positions correctly around emoji and non-ASCII identifiers.
- [ ] Out-of-order or obsolete diagnostics cannot decorate the wrong revision or reopened tab.
- [ ] Missing servers, unsupported capabilities and the existing 8 MiB language limit produce understandable states without blocking editing.

## Dependencies and review decisions

Choose push diagnostics first; pull diagnostics can follow after capability review. Reserve indicator styles so [semantic highlighting](31-semantic-highlighting.md), find matches and [code actions](30-code-actions.md) can coexist.
