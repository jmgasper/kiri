# Create, edit, import and export color themes

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CotEditor and CodeEdit provide theme editing and import/export workflows.
Sources: [CotEditor 7.1.0: customize theme](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_customize_theme.html), [CodeEdit 0.3.6: theme management](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Settings/Pages/ThemeSettings/Models/ThemeModel+CRUD.swift).

## Kiri today

Kiri has ten built-in themes and a live preferences preview. [Theme.cpp](../../src/ui/Theme.cpp)
compiles their values, while [EditorSettings](../../src/ui/EditorSettings.h) persists a numeric
built-in index; there is no editable theme format.

## Candidate scope

Allow duplicating a theme, editing named colors with the existing preview, and importing/exporting a
versioned theme file. Introduce stable theme IDs so upgrades and removed custom themes do not
silently select the wrong palette. Include all Kiri-owned surfaces, editor syntax and Git diff
colors.

## Acceptance checks

- [ ] Create a custom light and dark theme, apply each, restart, and retain the selection.
- [ ] Preview and Cancel preserve the previously applied theme and document state.
- [ ] Import malformed or incomplete themes with clear validation and safe defaults.
- [ ] Inspect editor, launcher, terminal, search, dialogs and Git views for readable foreground/background combinations.

## Dependencies and review decisions

Build on existing Theme and PreferencesWindow code. Decide which colors are editable and whether automatic light/dark switching is desired; platform appearance detection requires separate Haiku verification.
