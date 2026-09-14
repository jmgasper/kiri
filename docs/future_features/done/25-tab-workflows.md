# Preview tabs and reopening closed documents

Implemented 2026-09-14 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: Next · size: M

## Reference behavior

CodeEdit distinguishes temporary preview tabs from tabs kept open. Fleet 1.38 added reopening previously closed tabs.
Sources: [CodeEdit 0.3.6: temporary tabs](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Editor/Models/Editor/Editor.swift), [CodeEdit 0.3.6: Keep Open](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Editor/TabBar/Views/EditorTabBarContextMenu.swift), [Fleet 1.38 release notes](https://blog.jetbrains.com/fleet/2024/07/fleet-1-38-is-here-with-support-for-virtual-environments-created-by-anaconda-auto-refresh-for-log-files-improved-proxy-setting-detection-and-much-more/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[TabStrip](../../../src/ui/TabStrip.h) supports selecting, closing, Close All and Close Others.
[Workspace](../../../src/ui/Workspace.h) stores open documents but no preview-tab marker or recently
closed stack.

## Implemented scope

Make preview opening optional: selecting search/tree results can reuse one clean preview tab, while
editing or Keep Open makes it permanent. Add Reopen Closed Tab with the saved position. Keep this
distinction visible and avoid treating a preview as permission to discard edits.

## Acceptance checks

- [x] Browse several files through one preview tab and keep one explicitly.
- [x] Typing in a preview makes it permanent before another result is opened.
- [x] Reopen recently closed saved files in order, restoring positions without duplicating existing tabs.
- [x] Missing files, cancelled closes and discarded untitled drafts follow a clearly stated policy.

## Dependencies and review decisions

Coordinate identities and per-group behavior with [splits](01-split-editors.md). Decide whether closed untitled drafts should be recoverable here; session crash recovery is already implemented and is not this feature.

## Completed policy and verification

Preview opening is enabled by default and can be disabled in View. Each pane
reuses one italic preview; editing, Save, Split or Keep Open makes it permanent.
Undoing that edit does not restore preview status. Double-clicking a tab keeps
it open; Enter or double-click in the tree/search opens a permanent tab.

Reopen Closed Tab retains the last 64 explicitly closed saved-file views during
the current workspace session. It restores the original pane when available,
otherwise the active pane, and reuses an existing tab in that pane. Missing
files consume their history entry with a status message. Cancelled closes and
replaced previews create no history. Explicitly discarded untitled drafts are
not retained; discarded saved files reopen from disk. Crash recovery is separate.

The [workspace tests](../../../tests/WorkspaceTests.cpp) cover preview reuse,
edits before delayed notifications, edit/undo promotion, Keep Open and Close
shortcuts, saved positions, duplicate prevention and missing files. Native UI
checks also exercised italic previews, double-click Keep Open, editing/undo,
Cancel, Save, Reopen and explicit untitled Discard. See the
[usage guide](../../EDITOR_PANES.md) and [verification record](../../STATUS.md).
