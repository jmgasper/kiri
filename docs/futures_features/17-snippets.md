# Reusable snippets and completion placeholders

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Partial**

## Reference behavior

CotEditor has language-scoped insertion snippets with selection wrapping and caret placement. Fleet’s Kotlin support documents live templates.
Sources: [CotEditor 7.1.0: insert snippet](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_insert_snippet.html), [Kotlin support in Fleet, October 2023](https://blog.jetbrains.com/kotlin/2023/10/kotlin-support-in-jetbrains-fleet/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[LanguageServer.cpp](../../src/core/LanguageServer.cpp) explicitly advertises snippetSupport=false.
Completion inserts plain text through [WorkspaceLanguage.cpp](../../src/ui/WorkspaceLanguage.cpp);
there is no snippet library or placeholder session.

## Candidate scope

Create a snippet picker and editable language-scoped library. Support a reviewed placeholder syntax,
repeated placeholders, selection wrapping and Tab/Shift+Tab navigation. Advertise LSP snippet
support only once server-provided snippet strings can be handled correctly.

## Acceptance checks

- [ ] Insert a function template, edit a repeated placeholder, navigate fields and leave the session.
- [ ] Undo insertion and edits predictably; Escape exits placeholder navigation without losing text.
- [ ] Exercise Unicode, indentation, additional completion edits and unsupported snippet constructs.
- [ ] Ordinary Tab indentation and completion acceptance continue to work outside a snippet session.

## Dependencies and review decisions

Decide whether local snippets and LSP snippets are one goal or two deliveries. [Keybindings](03-custom-keybindings.md) must account for placeholder navigation. File-drop templates are an optional later extension.
