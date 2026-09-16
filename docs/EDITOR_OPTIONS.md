# Indentation, column guides and minimaps

## Defaults and document settings

Open **Edit → Preferences… → Editing Defaults…** to choose tabs or spaces,
indent width, tab width, guide columns and whether to show minimaps. Choose
**OK** in Editing Defaults, then **Apply** or **OK** in Preferences. Cancel in
Preferences discards unapplied choices. Defaults persist between sessions.

**Edit → Document Settings…**, also available in the editor's context menu,
shows the effective indentation and guides for the current document. Each
property lists its origin: defaults, an EditorConfig file and line, or a document
override. The status bar shows the indentation style; its tooltip includes these
details. Malformed configuration produces an **EditorConfig warning** with an
explanation in the dialog and tooltip.

Check **Override indentation for this document** or **Override guides for this
document** to supply explicit settings. Clear the checkbox to inherit again.
All panes showing that document use the same settings. Overrides survive session
restoration, reopening closed tabs and detaching a tab into another window.
Reopening a split retains newer settings from another pane that still has the
document open.
Changing these settings preserves existing text, selections, dirty state and
undo history.

## EditorConfig

Kiri resolves settings in this order:

1. Editing defaults.
2. Matching ancestor `.editorconfig` files, from parent to child. Within each
   file, later matching sections take precedence. `root = true` in the preamble
   stops the upward search; the open project folder is not an implicit boundary.
3. Explicit document overrides.

`unset` removes an inherited property's effect and restores its default.
Patterns support `*`, `**`, `?`, character classes, escaped characters, brace
alternatives and numeric ranges. Patterns containing a slash are relative to
the configuration's folder; a pattern such as `*.js` also matches descendants.
These rules follow the [EditorConfig specification](https://spec.editorconfig.org/).

Kiri applies the following properties:

| Property | Kiri behavior |
| --- | --- |
| `indent_style` | `space` or `tab`. |
| `indent_size` | Indentation width from 1 to 16, or `tab` to use the tab width. |
| `tab_width` | Tab stops from 1 to 16. When absent, a numeric `indent_size` supplies this width. |
| `max_line_length` | Extension: one visual guide from 1 to 1000, or `off`. It does not reformat text. |

For example, this keeps JavaScript at two spaces and Makefile recipes at tabs:

```ini
root = true

[*]
indent_style = space
indent_size = 2
max_line_length = 80

[Makefile]
indent_style = tab
indent_size = tab
tab_width = 4
```

Without a matching rule or override, Kiri uses defaults; it does not infer an
indentation style from existing text. Tabs fill as many complete tab stops as
possible, with spaces for any remainder. Invalid supported values are ignored
and explained. Unknown properties are ignored. `end_of_line`, `charset`,
`trim_trailing_whitespace` and `insert_final_newline` appear as informational
notes because this feature does not apply them. Existing encoding and save
behavior continue; explicit conversion belongs to
[line-ending conversion](futures_features/13-line-ending-conversion.md).

Configuration reads run on a worker when a file opens, is recovered, or is saved
under a new name. Save As resolves the destination's rules and retains document
overrides. A background check every two seconds detects changed, created and
deleted ancestor configuration files, including files outside the open project.
Checks stop while an earlier check for that document is pending.

The resolver uses Kiri's existing PCRE2 dependency and retains property origins.
[Genio](https://github.com/Genio-The-Haiku-IDE/Genio) demonstrates an EditorConfig C
library option on Haiku; its development files were absent from the beta5 test
VM. Kiri's bounded resolver adds no package dependency. Limits are 1 MiB per
configuration, 8 MiB total, 128 ancestor folders, 4096 sections per file and 4096
resolved properties. Pattern matching also has depth, memory and work limits.
Limits and unreadable or malformed files are reported in the settings details.

## Typing and pasting

**Tab** and **Shift+Tab** use the effective widths. Enter copies the preceding
indentation up to the insertion position, using the document's current line
ending. A newline and its indentation form one undo step. Multiple carets retain
their own indentation; an active completion popup keeps its Enter behavior.

For a multiline paste of at most 8 MiB into a line's leading whitespace, Kiri
removes common clipboard indentation, aligns following lines to the destination,
and writes indentation with the effective tabs/spaces setting. Relative column
indentation is preserved; Kiri does not guess the source's indentation width.
Inline and larger pastes retain their indentation. Paste line endings follow
the destination document. Multiple selections and rectangular selections retain
Scintilla's native clipboard behavior. These actions affect inserted text only.

## Column guides

Enter up to eight columns from 1 to 1000, separated by commas or spaces, such as
`80, 100`. Empty input or `off` disables guides. Duplicates are removed and
columns are sorted. EditorConfig can supply one preferred guide; a document
override can supply several or turn them off.

A guide at N marks the boundary after N space-character widths in the editor's
default font. Tabs advance through the configured tab stops. With a proportional
font, the guide is a visual space-width ruler, so N arbitrary characters do not
necessarily end at that line. Guides follow zoom and horizontal scrolling;
wrapped rows show the same visual ruler. They do not reflow or reformat text.
Rendering uses [Scintilla's multiple-edge API](https://www.scintilla.org/ScintillaDoc.html#LongLines).
Guide color comes from the theme's editable `border` role.

## Minimap

Use **View → Minimap** or the checkbox in Editing Defaults. The choice persists
and applies to editor views. Each pane has its own overview and viewport marker,
following that pane's folding, wrapping, zoom and scrolling. Display lines use a
fixed two-pixel spacing: short files leave empty space below, and longer files
scroll through the overview with the editor. Click a location in the overview
to center that region in the editor, or drag the marker to scroll across the
document while preserving the caret and selection. Turning it off frees the
cell cache and returns 96 pixels to editing.

The overview samples the existing Scintilla document; it does not create another
text buffer, lexer or layout. It draws text occupancy in available lexical
colors, with at most 1024 consecutive display rows and 80 columns, reading at
most 256 bytes per sampled line. Text that has not been styled yet uses the
available base color; later style notifications refresh it. Semantic indicators
are not sampled.

The cell cache is capped at 160 KiB per view, plus the small palette and native
widget overhead. A 150 ms timer updates visible views; content is resampled when
text, styling, indentation, display layout or the sampled line range changes.
Scroll updates reuse the content cache while its line range stays the same.
Above 32 MiB or 500,000 document lines, a visible
**Minimap paused** message replaces the overview and the cell cache is released.
The editor remains usable. See [native measurements](PERFORMANCE.md#minimap).

## Verification

Native checks cover tabs and spaces in one workspace, nested rules, invalid
configuration, overrides, shared panes, Save As, Preferences preview/Cancel/Apply,
session persistence and unchanged document state. Guide checks use three installed
font families, sizes 10/18/26 and zoom levels -2/0/3. Minimap checks cover compact
short files, resize stability, line-aligned clicks, distant dragging, folds,
wrapping, zoom, edits, width recovery and bounded small/8 MiB/200 MiB behavior.
Live mouse and keyboard checks exercised the dialogs,
light and dark guides, minimap navigation, Tab/Shift+Tab, newline indentation and
undo in JavaScript and Makefiles.

The portable resolver tests include UTF-8 paths, nested precedence, `unset`,
glob syntax, malformed input and limits. An independent run of the official
[EditorConfig glob corpus](https://github.com/editorconfig/editorconfig-core-test/tree/master/glob)
passed 130 cases. This is matching evidence, not a claim of complete EditorConfig
core/plugin conformance or support for every standard property.

```sh
# Portable core on the host
cmake -S . -B build-host
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure

# Native Haiku integration and focused editor-options checks
make -j4 check check-native check-language build-haiku/kiri_workspace_tests
build-haiku/kiri_workspace_tests --editor-options
build-haiku/kiri_workspace_tests

# Inspect a resolved path or an individual configuration fixture
build-haiku/kiri_document_settings_tests --resolve /path/to/file.js
build-haiku/kiri_document_settings_tests --config /path/to/.editorconfig src/file.js
```

See the [verification record](STATUS.md) for check counts and environment details.
