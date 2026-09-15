# Configurable column guides

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Next** · size: **S**

## Reference behavior

CodeEdit exposes a reformatting guide at a configurable column.
Sources: [CodeEdit 0.3.5 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5), [CodeEdit 0.3.6: column-guide settings](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Settings/Pages/TextEditingSettings/Models/TextEditingSettings.swift).

## Baseline before implementation

[Editor.cpp](../../../src/ui/Editor.cpp) enables indentation guides but has no configured right-margin
ruler. The status bar reports a cursor column, and Word Wrap follows the editor width.

## Implemented scope

Add one or more optional guide columns, such as 80 and 100, with a theme-aware color and a
per-document override. Explain that a guide is visual and does not reflow or reformat the file.
Allow project settings to supply a preferred width once that mechanism exists.

## Acceptance checks

- [x] Show guides at configured columns with several fonts, sizes and zoom levels.
- [x] Tabs and long lines use the documented column measurement.
- [x] Wrapping and horizontal scrolling retain useful guide behavior.
- [x] Preferences persist; changing guides leaves the text, undo history and dirty state untouched.

## Dependencies and review decisions

Can be delivered independently using the installed Scintilla API after verification. Coordinate defaults with [EditorConfig](09-editorconfig-indentation.md); proportional-font behavior needs an explicit decision.

## Completed policy and verification

Editing Defaults and Document Settings accept up to eight guide columns from
1 to 1000; empty input or `off` disables guides. EditorConfig's nonstandard
`max_line_length` property can supply one preferred guide. Explicit document
overrides take precedence and persist with the session.

Scintilla's multiple-edge API draws the guides using the theme's `border` color.
A column is one space-character width in the default editor font; proportional
fonts therefore use a visual ruler, not arbitrary character counts. Tabs use
the effective tab stops. Guides follow zoom and horizontal scrolling and remain
visual under wrapping, without reflow or edits. Native checks span three fonts,
three sizes and three zoom levels, and verify preserved text, undo, dirty state
and selection. Live inspection confirms multiple guides in light and dark themes.

See [editor options](../../EDITOR_OPTIONS.md),
[native workspace checks](../../../tests/WorkspaceTests.cpp),
[native measurements](../../PERFORMANCE.md#minimap) and the
[verification record](../../STATUS.md).
