# Optional document minimap

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **M** · Kiri gap: **Missing**

## Reference behavior

CodeEdit 0.3.5 added a minimap with a draggable view of the visible region.
Sources: [CodeEdit 0.3.5 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.5).

## Kiri today

[Editor.cpp](../../src/ui/Editor.cpp) configures line numbers, folding and scrolling, but no minimap
view. Current syntax highlighting is deliberately disabled above 8 MiB.

## Candidate scope

Add an optional narrow overview of the active document with a viewport indicator and click/drag
navigation. It should follow the active split, theme and folding state. Render a bounded
representation rather than keeping a second full editable buffer merely for the overview.

## Acceptance checks

- [ ] Navigate between distant parts of a file by clicking and dragging the minimap.
- [ ] Keep the viewport marker accurate after edits, wrapping, folding and zoom changes.
- [ ] Toggle it off and recover editor space without changing text or the cursor position.
- [ ] Measure memory and typing responsiveness on small, 8 MiB and 200 MiB files; degrade or disable the overview clearly when needed.

## Dependencies and review decisions

Investigate Scintilla document sharing and rendering costs on Haiku. Coordinate with [split editors](../future_features/done/01-split-editors.md); a column ruler is a [separate smaller feature](22-column-guides.md).
