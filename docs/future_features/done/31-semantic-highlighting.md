# Language-server semantic highlighting

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Later** · size: **M**

## Reference behavior

CodeEdit includes a semantic-token highlighting provider and released language-server syntax highlighting in 0.3.5.
Sources: [CodeEdit 0.3.6: semantic highlighting](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/LSP/Features/SemanticTokens/SemanticTokenHighlightProvider.swift), [CodeEdit 0.3.5 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5).

## Baseline before implementation

[Editor::SetLanguage and ApplyTheme](../../../src/ui/Editor.cpp) use Lexilla token styles.
[LanguageServer.cpp](../../../src/core/LanguageServer.cpp) does not advertise or request semantic
tokens, so identifiers cannot be colored by server-resolved roles.

## Implemented scope

Layer supported LSP semantic token types and modifiers over lexical highlighting, with a preference
to disable it. Map server legends into stable theme roles and retain lexical fallback when analysis
is unavailable. Handle full responses first, adding deltas only with adequate invalidation coverage.

## Acceptance checks

- [x] Distinguish representative type, parameter and property roles with a real capable server.
- [x] Edit above a token, change tabs and restart the server without leaving colors on incorrect ranges.
- [x] Unknown token types/modifiers fall back cleanly and respect user themes.
- [x] Benchmark rendering and response processing near the 8 MiB language-tool limit; larger-file editing remains usable.

## Dependencies and review decisions

Coordinate style allocation with [diagnostics](26-diagnostics.md) and [custom themes](20-custom-themes.md). This does not replace Lexilla or imply that semantic features work in every language.

## Completed policy and verification

Full relative semantic tokens layer over Lexilla using eight stable color roles and a deprecated strike-through. The persisted preference disables the layer. Native and real-server checks cover types, parameters, properties, edits above tokens, tab changes and restart. An 8 MiB fixture with 91,180 tokens took about 120 ms to apply on Haiku; clearing and editing took about 8 ms. Invalid and unsupported responses fall back to lexical coloring.

See the [usage and limits guide](../../LANGUAGE_ANALYSIS.md),
[portable checks](../../../tests/AnalysisThemeTests.cpp),
[native workspace checks](../../../tests/WorkspaceTests.cpp) and
[verification record](../../STATUS.md).
