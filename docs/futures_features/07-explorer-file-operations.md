# Create, rename, move and trash files in the explorer

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **L** · Kiri gap: **Partial**

## Reference behavior

CodeEdit’s project navigator implements new files/folders, rename, moving items and Move to Trash, plus path-copy commands.
Sources: [CodeEdit 0.3.6: file operations](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/ProjectNavigator/OutlineView/ProjectNavigatorMenuActions.swift).

## Kiri today

[Explorer.cpp](../../src/ui/Explorer.cpp) loads directories and opens selected files. New File and
Save As exist in [Workspace.cpp](../../src/ui/Workspace.cpp), but the tree does not manage
filesystem entries.

## Candidate scope

Add native context-menu and keyboard actions for creation, rename, move, Trash, Copy Path and Reveal
in Tracker. Update open document identities, recovery records, language services, Git status and the
index when paths change. A move must preserve Haiku attributes and permissions.

## Acceptance checks

- [ ] Create a nested folder and file, rename an open dirty file, save it, and verify its destination and contents.
- [ ] Handle spaces, Unicode, symlinks, name collisions and permission errors without silently replacing another file.
- [ ] Move a folder containing open documents and keep tabs, symbols and quick-open results coherent.
- [ ] Trashing a dirty file offers a clear save/cancel choice; trashed items can be restored with Tracker.

## Dependencies and review decisions

Coordinate with [external refresh](08-external-file-refresh.md). Decide the initial drag-and-drop scope and how cross-volume moves are handled; permanent deletion need not be offered.
