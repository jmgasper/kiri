# Configurable column guides

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **S** · Kiri gap: **Missing**

## Reference behavior

CodeEdit exposes a reformatting guide at a configurable column.
Sources: [CodeEdit 0.3.5 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5), [CodeEdit 0.3.6: column-guide settings](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Settings/Pages/TextEditingSettings/Models/TextEditingSettings.swift).

## Kiri today

[Editor.cpp](../../src/ui/Editor.cpp) enables indentation guides but has no configured right-margin
ruler. The status bar reports a cursor column, and Word Wrap follows the editor width.

## Candidate scope

Add one or more optional guide columns, such as 80 and 100, with a theme-aware color and a
per-document override. Explain that a guide is visual and does not reflow or reformat the file.
Allow project settings to supply a preferred width once that mechanism exists.

## Acceptance checks

- [ ] Show guides at configured columns with several fonts, sizes and zoom levels.
- [ ] Tabs and long lines use the documented column measurement.
- [ ] Wrapping and horizontal scrolling retain useful guide behavior.
- [ ] Preferences persist; changing guides leaves the text, undo history and dirty state untouched.

## Dependencies and review decisions

Can be delivered independently using the installed Scintilla API after verification. Coordinate defaults with [EditorConfig](09-editorconfig-indentation.md); proportional-font behavior needs an explicit decision.
