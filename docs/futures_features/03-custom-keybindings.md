# Customizable keyboard shortcuts

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Missing**

## Reference behavior

Fleet lets users assign, remove and restore shortcuts, and choose alternative keymaps.
Sources: [Fleet custom keymaps, September 2024](https://blog.jetbrains.com/fleet/2024/09/fleet-plugins-now-with-sustom-keymaps/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[Workspace::BuildMenus](../../src/ui/Workspace.cpp) hardcodes application shortcuts;
[EditorSettings](../../src/ui/EditorSettings.h) stores font and theme settings. Scintilla and the
terminal also handle keys independently.

## Candidate scope

Provide a searchable shortcut preferences page with action names, key capture, conflict reporting
and Restore Defaults. Persist user overrides against stable actions. Resolve application, editor and
terminal contexts explicitly, including Haiku’s configurable Command modifier.

## Acceptance checks

- [ ] Rebind Save, Find and a pane command, restart, and use the new bindings successfully.
- [ ] Menus and the command palette display the effective shortcut.
- [ ] Report a conflicting assignment before replacing it; restoring defaults removes the override.
- [ ] Typing, input methods, Ctrl+Space completion and terminal Ctrl+C/Ctrl+D continue to work.

## Dependencies and review decisions

Build on the [command registry](02-command-palette.md). Preset keymaps and multi-key chords are possible follow-ups; decide whether the first goal needs either.
