# Searchable command palette

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Missing**

## Reference behavior

Fleet provides Go to Actions, with searchable action names, aliases and displayed shortcuts.
Sources: [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[Workspace::BuildMenus](../../src/ui/Workspace.cpp) defines menu commands. Open Quickly searches
paths, and Go to Symbol lists file symbols; neither searches actions.

## Candidate scope

Add a native command picker for actions such as Format with Prettier, New Terminal and Toggle Word
Wrap. Use stable action identifiers and one shared dispatch path for menus, shortcuts and the
picker. Show unavailable actions with a reason and preserve the originating editor or terminal
target.

## Acceptance checks

- [ ] Find an action using part of its name and execute it with the keyboard.
- [ ] Escape restores the original focus and makes no document changes.
- [ ] Commands from the palette behave like their menu equivalents, including disabled states and dirty-close prompts.
- [ ] Displayed shortcuts follow any user customization and match Haiku Command-modifier behavior.

## Dependencies and review decisions

No external tools are required. Define action identifiers jointly with [custom keybindings](03-custom-keybindings.md); choose the default palette shortcut during review.
