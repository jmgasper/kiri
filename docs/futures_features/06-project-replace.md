# Previewed replacement across project files

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **L** · Kiri gap: **Missing**

## Reference behavior

CodeEdit’s Find navigator includes Replace All across matching files.
Sources: [CodeEdit 0.3.6: project replacement](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/FindNavigator/FindNavigatorForm.swift), [CodeEdit 0.3.6: replacement results](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/FindNavigator/FindNavigatorView.swift).

## Kiri today

[Project.h](../../src/core/Project.h) exposes search results only. Kiri’s replacement commands
operate on the current [Editor](../../src/ui/Editor.cpp); there is no multi-file edit plan or
replacement preview.

## Candidate scope

Build a preview of replacements grouped by file, with per-file and per-match selection before Apply.
Use current buffers for open documents and checked snapshots for closed files. Report exactly which
files changed, failed or became stale. Design recovery for a partly applied operation.

## Acceptance checks

- [ ] Preview a replacement across saved and dirty files and exclude selected matches before applying.
- [ ] Detect edits or disk changes since preview and refresh affected entries without overwriting newer text.
- [ ] Preserve encoding metadata, line endings, attributes and permissions through existing safe-save helpers.
- [ ] Undo open-buffer edits and provide an explicit restore path for disk writes; cancellation reports any completed writes.

## Dependencies and review decisions

Build on [scoped search](05-scoped-project-search.md). Decide whether closed files are opened as dirty buffers or written after approval; specify that choice before an implementation goal. Reuse transaction work with [rename](29-symbol-rename.md).
