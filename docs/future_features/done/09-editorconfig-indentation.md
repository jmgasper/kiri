# Per-document indentation and EditorConfig support

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Soon** · size: **M**

## Reference behavior

CotEditor exposes indentation style and width, including mode overrides. Fleet documents EditorConfig-based code style.
Sources: [CotEditor 7.1.0: settings edit](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/settings_edit.html), [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[Editor’s constructor](../../../src/ui/Editor.cpp) fixes tab width and indentation at four spaces.
[EditorSettings](../../../src/ui/EditorSettings.h) only persists font, size and theme. Prettier may
honor project settings during formatting, but typing does not use them.

## Implemented scope

Expose tabs/spaces, tab width and indent width per document and as defaults. Resolve .editorconfig
rules for supported properties, show their origin, and allow an explicit document override. Start
with indentation, then connect line endings and save-time whitespace rules to their dedicated
implementations.

## Acceptance checks

- [x] Use tabs in a Makefile and two spaces in JavaScript in the same workspace.
- [x] Nested .editorconfig files and root boundaries produce the documented precedence.
- [x] Tab, Shift+Tab, newline auto-indent and pasted multiline text respect the effective settings.
- [x] Changing indentation preferences alone leaves existing bytes and undo history unchanged; malformed configuration is explained.

## Dependencies and review decisions

Verify the EditorConfig specification and Haiku library options during implementation. Coordinate with [line-ending conversion](../../futures_features/13-line-ending-conversion.md); decide whether style detection runs only when no explicit rule exists.

## Completed policy and verification

Defaults and explicit per-document overrides cover tabs/spaces, indent width and tab
width (1–16). Configuration is resolved on a worker before opening, recovery and
Save As, with parent/child precedence, root boundaries, `unset`, origins and
background change detection. No automatic style detection runs. Only indentation
and the visual `max_line_length` extension are applied; encoding, line-ending and
save-time whitespace properties are informational for this feature.

Tab/Shift+Tab, synchronous newline auto-indent and bounded multiline paste follow
the effective settings. Changes to settings preserve existing bytes and undo.
The bounded resolver uses the existing PCRE2 library without a new dependency;
Haiku's EditorConfig C library option and the specification were investigated.
Portable tests and 130 independent upstream glob cases pass. Native checks cover
Makefiles alongside JavaScript, invalid rules, shared panes, overrides, Save As
and session restoration; real keyboard input confirms tabs/spaces and undo.

See [editor options](../../EDITOR_OPTIONS.md),
[native workspace checks](../../../tests/WorkspaceTests.cpp),
[native measurements](../../PERFORMANCE.md#minimap) and the
[verification record](../../STATUS.md).
