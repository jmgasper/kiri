# Back/forward navigation and path breadcrumbs

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit has backward/forward file history and a jump bar showing the file’s ancestor path.
Sources: [CodeEdit 0.3.6: navigation history](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Editor/Models/Editor/Editor+History.swift), [CodeEdit 0.3.6: path jump bar](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Editor/JumpBar/Views/EditorJumpBarView.swift).

## Kiri today

Kiri restores tab positions and has recent items, quick-open and a current-file
[SymbolBar](../../src/ui/SymbolBar.cpp). [Workspace](../../src/ui/Workspace.h) has no
navigation-history stack; the status path is not a breadcrumb navigator.

## Candidate scope

Add Back and Forward for deliberate jumps, recording document identity and position rather than
every caret movement. Add clickable ancestor breadcrumbs for opening sibling files. The proposed
Kiri history extends the reference’s file history to useful cursor locations.

## Acceptance checks

- [ ] Jump through search results and symbols in several files, then return through the exact previous locations.
- [ ] A new jump after going Back clears the appropriate Forward history.
- [ ] Renamed, deleted or closed files are handled without opening duplicate or incorrect documents.
- [ ] Breadcrumbs remain usable with deep paths, Unicode names and narrow windows.

## Dependencies and review decisions

Decide whether history is workspace-wide or per split and whether breadcrumbs and history should be separate goals. Integrate with [cross-file code navigation](27-code-navigation.md) and [file operations](07-explorer-file-operations.md).
