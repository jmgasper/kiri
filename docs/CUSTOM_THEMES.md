# Custom color themes

Open **Edit → Preferences…**. Choose any built-in or saved theme, then select
**Duplicate Theme** to make an editable copy. Give it a name, choose a named color,
and enter a `#RRGGBB` value. The preview updates as valid values are entered.
The preview includes lexical syntax, semantic identifiers and an example warning.

**Apply** saves the theme and updates the workspace. **OK** applies and closes.
**Cancel** discards changes made since the last Apply. Previewing colors does not
change the workspace palette, editor text, selection, undo history, or dirty state.
Built-in definitions remain available as starting palettes. The Dark theme
checkbox controls menu grouping; colors are edited individually. Theme switching
is manual and does not depend on the desktop's light/dark appearance.

## Import and export

**Import…** reads a versioned JSON file into the preview. Apply saves it to the
library. Missing colors inherit the selected built-in base and produce a visible
warning. Unknown color names are ignored with a warning. Invalid JSON, unsupported
versions, unsafe IDs, invalid names or malformed known colors are rejected without
changing the applied palette. Theme files are limited to 1 MiB.

An imported built-in palette becomes a custom copy. A conflicting custom ID also
gets a new ID, so importing a different definition cannot silently overwrite an
existing library entry. Importing an identical saved definition selects it.

**Export…** writes the complete preview palette, including all named colors, to
the selected file. The suggested extension is `.kiri-theme.json`.

Minimal example:

```json
{
  "format": "kiri-theme",
  "version": 1,
  "id": "custom.my-night-theme",
  "name": "My night theme",
  "base": "kiri.obsidian",
  "dark": true,
  "colors": {
    "background": "#101820",
    "text": "#e3edf5",
    "property": "#8fc7ff"
  }
}
```

Omitted roles inherit Obsidian in this example. Export includes the resulting
complete palette so a subsequent import reproduces the chosen colors.

## Named colors

| Area | Color names |
| --- | --- |
| Surfaces | `background`, `panel`, `toolbar`, `border`, `line` |
| Text and interaction | `text`, `muted`, `accent`, `selection`, `selection.text` |
| Lexical syntax | `comment`, `keyword`, `string`, `number`, `type` |
| Semantic identifiers | `variable`, `parameter`, `property`, `function`, `namespace`, `readonly` |
| Git additions/deletions | `added`, `removed` |
| Diagnostics | `diagnostic.error`, `diagnostic.warning`, `diagnostic.info`, `diagnostic.hint` |
| Terminal | `terminal.background`, `terminal.text`, `terminal.ansi0` through `terminal.ansi15` |

The same palette applies to Kiri's editor, file tree, tabs, launcher, terminal,
Problems panel, find/search, preferences, rename and comparison surfaces, and Git
views. Already-open search and comparison windows update when Apply is selected.
Existing terminal text and scrollback follow changes to default colors, while
explicit terminal RGB colors retain their meaning. Indexed ANSI colors use the
editable 16-color palette. Native menu chrome, file choosers and OS alerts follow
Haiku's desktop colors.

The preview warns about low contrast in main, panel, terminal or selection text.
It allows intentional color choices; inspect the finished palette on the
surfaces you use, including disabled controls and selected rows.

## Storage and identity

Definitions are stored as `<Kiri settings>/themes/<stable-id>.json`; normally this
is `/boot/home/config/settings/Kiri/themes/`. The menu reads up to 256 library
entries, skips invalid files and symbolic links, and sorts custom themes by name.
Preferences store the stable ID, so sorting or renaming a theme cannot select
another palette. Older numeric built-in settings migrate to stable IDs.

If a selected custom file is removed or invalid on restart, Kiri uses Obsidian
and reports the missing theme in Preferences. It retains the requested ID, so
restoring the file allows it to resolve again on a later restart. To remove a
custom definition, remove its JSON file from the library; no in-app deletion
command is provided.

Native tests cover custom light and dark themes through preview, Cancel, Apply,
export, malformed/incomplete import and restart, with document state preserved.
Portable tests validate the codec, safe defaults, file identity, library sorting,
and fallback when definitions disappear. See [Language analysis](LANGUAGE_ANALYSIS.md)
for semantic and diagnostic color behavior.
