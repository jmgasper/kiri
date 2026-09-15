# Optional document minimap

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Later** · size: **M**

## Reference behavior

CodeEdit 0.3.5 added a minimap with a draggable view of the visible region.
Sources: [CodeEdit 0.3.5 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5).

## Baseline before implementation

[Editor.cpp](../../../src/ui/Editor.cpp) configures line numbers, folding and scrolling, but no minimap
view. Current syntax highlighting is deliberately disabled above 8 MiB.

## Implemented scope

Add an optional narrow overview of the active document with a viewport indicator and click/drag
navigation. It should follow the active split, theme and folding state. Render a bounded
representation rather than keeping a second full editable buffer merely for the overview.

## Acceptance checks

- [x] Navigate between distant parts of a file by clicking and dragging the minimap.
- [x] Keep the viewport marker accurate after edits, wrapping, folding and zoom changes.
- [x] Toggle it off and recover editor space without changing text or the cursor position.
- [x] Measure memory and typing responsiveness on small, 8 MiB and 200 MiB files; degrade or disable the overview clearly when needed.

## Dependencies and review decisions

Investigate Scintilla document sharing and rendering costs on Haiku. Coordinate with [split editors](01-split-editors.md); a column ruler is a [separate smaller feature](22-column-guides.md).

## Completed policy and verification

Each editor view has an optional 96-pixel native overview of its existing Scintilla
document. Click and marker drag navigate without moving the caret or selection.
The map follows that pane's folds, wrapping, zoom, edits and lexical styling.
View → Minimap and Editing Defaults share a persisted toggle; disabling frees
the cells and restores editor width.

The cache is bounded to 1024 × 80 cells (160 KiB), with a 256-byte prefix sampled
per line and a 150 ms timer for visible views. There is no second text buffer,
lexer or layout. Above 32 MiB or 500,000 document lines the overview visibly
pauses and releases its cache. Native checks cover small, 8 MiB and 200 MiB
fixtures, cache bounds, typing latency, navigation and view changes. Live mouse
checks confirm distant clicks and marker drags. Measured costs and the limits of
resident-area accounting are recorded in the performance guide.

See [editor options](../../EDITOR_OPTIONS.md),
[native workspace checks](../../../tests/WorkspaceTests.cpp),
[native measurements](../../PERFORMANCE.md#minimap) and the
[verification record](../../STATUS.md).
