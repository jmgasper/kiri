# Per-document indentation and EditorConfig support

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CotEditor exposes indentation style and width, including mode overrides. Fleet documents EditorConfig-based code style.
Sources: [CotEditor 7.1.0: settings edit](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/settings_edit.html), [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[Editor’s constructor](../../src/ui/Editor.cpp) fixes tab width and indentation at four spaces.
[EditorSettings](../../src/ui/EditorSettings.h) only persists font, size and theme. Prettier may
honor project settings during formatting, but typing does not use them.

## Candidate scope

Expose tabs/spaces, tab width and indent width per document and as defaults. Resolve .editorconfig
rules for supported properties, show their origin, and allow an explicit document override. Start
with indentation, then connect line endings and save-time whitespace rules to their dedicated
implementations.

## Acceptance checks

- [ ] Use tabs in a Makefile and two spaces in JavaScript in the same workspace.
- [ ] Nested .editorconfig files and root boundaries produce the documented precedence.
- [ ] Tab, Shift+Tab, newline auto-indent and pasted multiline text respect the effective settings.
- [ ] Changing indentation preferences alone leaves existing bytes and undo history unchanged; malformed configuration is explained.

## Dependencies and review decisions

Verify the EditorConfig specification and Haiku library options during implementation. Coordinate with [line-ending conversion](13-line-ending-conversion.md); decide whether style detection runs only when no explicit rule exists.
