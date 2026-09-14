# Custom syntax profiles and server-free outlines

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Partial**

## Reference behavior

CotEditor permits manual syntax selection and custom regex-based definitions for file mapping, highlighting, comments, completion words and outlines. Its built-in tree-sitter grammars have different customization limits.
Sources: [CotEditor 7.1.0: syntax selection and editing commands](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/syntax_overview.html), [CotEditor 7.1.0: custom syntax definitions](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/syntax_definition.html), [CotEditor 7.1.0: local outline rules](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/syntax_outline_settings.html).

## Kiri today

[Editor::SetLanguage](../../src/ui/Editor.cpp) uses compiled filename/extension mappings and
Lexilla. [WorkspaceLanguage.cpp](../../src/ui/WorkspaceLanguage.cpp) supplies outlines exclusively
from LSP document symbols. There is no UI override or local outline-rule definition.

## Candidate scope

Add a syntax selector for untitled and misidentified files, user file associations, and declarative
profiles that reuse installed Lexilla lexers. Add bounded outline patterns for formats without a
server, such as Markdown headings and custom configuration sections. Custom highlighting rules can
be a subsequent delivery after the profile model is reviewed.

## Acceptance checks

- [ ] Override an extensionless file’s language and persist a reviewed filename association.
- [ ] List and jump to headings in an unsaved Markdown buffer with no language server installed.
- [ ] Reject malformed or costly patterns without blocking typing or damaging built-in profiles.
- [ ] When an LSP outline is available, apply a documented precedence without duplicate entries.

## Dependencies and review decisions

Decide how much custom highlighting belongs in the first goal and whether profile import/export is required. Share metadata with [comments](10-toggle-comments.md) and [paired delimiters](11-paired-delimiters.md); no new parser engine is assumed.
