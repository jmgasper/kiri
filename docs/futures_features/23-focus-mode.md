# One-command distraction-free mode

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **S** · Kiri gap: **Partial**

## Reference behavior

CodeEdit 0.3.5 added a Hide Interface command for focused editing.
Sources: [CodeEdit 0.3.5 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5).

## Kiri today

[Workspace::BuildMenus](../../src/ui/Workspace.cpp) toggles Files, Terminal and Source Control
separately. Kiri has no single command that temporarily hides interface panels and restores the
previous arrangement.

## Candidate scope

Add Focus Mode that gives the editor more space and remembers panel visibility and splitter sizes.
Provide a discoverable exit command and keyboard shortcut. Decide whether tabs and the symbol bar
remain visible, and how a requested search or terminal temporarily appears.

## Acceptance checks

- [ ] Enter from different sidebar/terminal configurations and restore each configuration on exit.
- [ ] Use editing and navigation commands with the mouse and keyboard while focused.
- [ ] Resizing or changing themes does not prevent leaving the mode.
- [ ] A dirty-close prompt or tool error remains visible and reachable.

## Dependencies and review decisions

This is a workspace presentation change and needs no new rendering framework. Decide whether to persist the mode across restarts and how it interacts with [split views](../future_features/done/01-split-editors.md).
