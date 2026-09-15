# Manage Git stashes

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Missing**

## Reference behavior

CodeEdit provides stash creation and a repository list with apply and deletion actions.
Sources: [CodeEdit 0.3.6: create stash](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/SourceControl/Views/SourceControlStashView.swift), [CodeEdit 0.3.6: apply/delete stash](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/SourceControlNavigator/Repository/Views/SourceControlNavigatorRepositoryView.swift).

## Kiri today

[GitRepository](../../src/core/Git.h) has status, history, diff, stage/unstage, commit and permalink
APIs, but no stash API or corresponding [GitView](../../src/ui/GitView.cpp) controls.

## Candidate scope

List stashes with metadata and a diff preview. Add named stash creation, Apply, Pop and Drop, with
an explicit choice about untracked files. Make the distinction between applying and deleting
visible. Refresh open files carefully after restoring changes.

## Acceptance checks

- [ ] Stash and restore tracked changes in a real repository without changing unrelated files.
- [ ] Exercise include/exclude-untracked options, staged changes and an empty worktree.
- [ ] A conflicting Apply/Pop preserves the stash and exposes unresolved paths.
- [ ] Dirty editor buffers and externally changed stash lists cannot cause an operation against the wrong snapshot.

## Dependencies and review decisions

Reuse [external reload](../future_features/done/08-external-file-refresh.md) and [merge resolution](38-merge-conflicts.md). Decide how to identify stashes stably when their displayed numeric positions change; no automatic stash-on-pull is proposed by default.
