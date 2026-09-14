# Split editors and shared document views

Implemented 2026-09-14 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: Soon · size: L

## Reference behavior

CotEditor splits views of one document; CodeEdit and Fleet support editor splits for arranging files. Fleet 1.41 added more flexible split layouts.
Sources: [CotEditor 7.1.0: split editor](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_split.html), [Fleet 1.41 release notes](https://blog.jetbrains.com/fleet/2024/10/fleet-1-41-is-here-with-new-bundled-keymaps-unlimited-splits-improved-typing-latency-and-editor-responsiveness-and-many-more-enhancements/), [CodeEdit 0.3.6: split commands](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Editor/TabBar/Views/EditorTabBarContextMenu.swift). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[Workspace.h](../../../src/ui/Workspace.h) gives each document one editor and uses a card layout to
display one document at a time. The sidebar and terminal splitters do not provide simultaneous
source views.

## Implemented scope

Add Split Right and Split Down, a tab group in each pane, and keyboard focus switching. Two views of
one file should share text, dirty state and undo history while retaining separate selections and
scroll positions. Nested splits are included in the first implementation.

## Acceptance checks

- [x] Edit a file in either of two panes and see the change immediately in the other; Undo and Save remain consistent.
- [x] Open different files, switch focus, and verify find, formatting, symbols and completion target the focused pane.
- [x] Close a view without closing the remaining view; closing the last dirty view keeps the save/cancel flow.
- [x] Restore the reviewed layout and positions after restart; resizing and large files remain responsive in QEMU.

## Dependencies and review decisions

Document ownership must be separated from view ownership before implementation. Nested splits should be supported in the first goal; coordinate with [tab workflows](25-tab-workflows.md).

## Implementation and verification

[WorkspacePanes.cpp](../../../src/ui/WorkspacePanes.cpp) separates logical files
from pane-local tabs and builds a recursive tree of native splitters. Editors
share Scintilla document references, revision/recovery state and undo history;
view positions remain independent. Background formatting retains its originating
view, and completion requests are scoped to that view. Layout, divider weights,
tab order, focus and per-view positions restore across sessions.

The native [workspace suite](../../../tests/WorkspaceTests.cpp) checks shared
edits, undo/save, focus routing, asynchronous language tools, nested restoration,
recovery and concurrent opens. A 32 MiB split completed in about 20 ms in QEMU.
Live keyboard/mouse checks covered nested splits, independent scrolling and the
final dirty view's Cancel/Save flow. See the [usage guide](../../EDITOR_PANES.md)
and [verification record](../../STATUS.md).

The shared-view API follows [Scintilla's multiple-view documentation](https://www.scintilla.org/ScintillaDoc.html#MultipleViews).
