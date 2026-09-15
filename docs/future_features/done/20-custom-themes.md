# Create, edit, import and export color themes

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Next** · size: **M**

## Reference behavior

CotEditor and CodeEdit provide theme editing and import/export workflows.
Sources: [CotEditor 7.1.0: customize theme](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_customize_theme.html), [CodeEdit 0.3.6: theme management](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Settings/Pages/ThemeSettings/Models/ThemeModel+CRUD.swift).

## Baseline before implementation

Kiri has ten built-in themes and a live preferences preview. [Theme.cpp](../../../src/ui/Theme.cpp)
compiles their values, while [EditorSettings](../../../src/ui/EditorSettings.h) persists a numeric
built-in index; there is no editable theme format.

## Implemented scope

Allow duplicating a theme, editing named colors with the existing preview, and importing/exporting a
versioned theme file. Introduce stable theme IDs so upgrades and removed custom themes do not
silently select the wrong palette. Include all Kiri-owned surfaces, editor syntax and Git diff
colors.

## Acceptance checks

- [x] Create a custom light and dark theme, apply each, restart, and retain the selection.
- [x] Preview and Cancel preserve the previously applied theme and document state.
- [x] Import malformed or incomplete themes with clear validation and safe defaults.
- [x] Inspect editor, launcher, terminal, search, dialogs and Git views for readable foreground/background combinations.

## Dependencies and review decisions

Build on existing Theme and PreferencesWindow code. Decide which colors are editable and whether automatic light/dark switching is desired; platform appearance detection requires separate Haiku verification.

## Completed policy and verification

Preferences support duplicating, naming, editing, importing and exporting versioned themes with live preview. Native tests create and restart custom light and dark palettes, preserve editor state on Preview/Cancel, round-trip exports and reject malformed imports. Incomplete files inherit a named built-in base. Stable IDs survive menu sorting and missing definitions safely fall back to Obsidian. Switching is manual; native OS chrome retains the desktop palette.

See the [usage and limits guide](../../CUSTOM_THEMES.md),
[portable checks](../../../tests/AnalysisThemeTests.cpp),
[native workspace checks](../../../tests/WorkspaceTests.cpp) and
[verification record](../../STATUS.md).
