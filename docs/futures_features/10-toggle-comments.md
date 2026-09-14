# Language-aware comment and uncomment commands

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **S** · Kiri gap: **Missing**

## Reference behavior

CotEditor and Fleet offer commands that insert or remove the comment delimiters for the current language.
Sources: [CotEditor 7.1.0: syntax selection and editing commands](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/syntax_overview.html), [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[Editor.cpp](../../src/ui/Editor.cpp) selects lexers and handles indentation, but defines no
language-aware comment action. [Workspace’s Edit menu](../../src/ui/Workspace.cpp) contains no
comment command.

## Candidate scope

Add Toggle Line Comment and, where the language supports it, Toggle Block Comment. Operate on the
current line or all selected lines, preserve indentation, and use language metadata for delimiters.
Make both commands discoverable in menus and the command palette.

## Acceptance checks

- [ ] Comment and uncomment C++, Python and shell selections, including blank lines and mixed indentation.
- [ ] Toggling twice restores the original text, and each invocation is one undoable action.
- [ ] Multiple selections are processed once per affected line without duplicated delimiters.
- [ ] Unknown languages and read-only previews expose an understandable unavailable state.

## Dependencies and review decisions

Share delimiter metadata with [syntax profiles](18-custom-syntax-profiles.md). Confirm default shortcuts against Haiku and Scintilla; existing generic line-edit shortcuts are not part of this gap.
