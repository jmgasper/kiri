# Git branch creation and switching

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit implements a branch picker and interfaces for creating, renaming and switching branches.
Sources: [CodeEdit 0.3.6: branch switching](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/CodeEditUI/Views/ToolbarBranchPicker.swift), [CodeEdit 0.3.6: branch creation](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/SourceControl/Views/SourceControlNewBranchView.swift), [CodeEdit 0.3.6: branch rename](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/SourceControl/Views/SourceControlRenameBranchView.swift).

## Kiri today

[GitRepository](../../src/core/Git.h) and [GitView](../../src/ui/GitView.cpp) show branch/history
information and implement fetch, fast-forward pull and push. They provide no branch-management
controls.

## Candidate scope

Add a searchable local/remote branch picker, Create Branch, Switch and Rename. Explain detached HEAD
and missing upstream state. Check dirty buffers and working-tree changes before switching, then
refresh files, index, status and language services coherently.

## Acceptance checks

- [ ] Create and switch branches in a real temporary repository, including a remote-tracking branch.
- [ ] Protect unsaved buffers and show Git’s refusal when checkout would overwrite work.
- [ ] Handle detached HEAD, unborn repositories, invalid names and external branch changes.
- [ ] Keep the graph, active branch label, open files and search index consistent after switching.

## Dependencies and review decisions

Coordinate with [external reload](../future_features/done/08-external-file-refresh.md) and [stashes](37-git-stashes.md). Automatic stashing, branch deletion, merge and rebase should be separately reviewed rather than implied by branch switching.
