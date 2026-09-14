# Filtered and regular-expression project search

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit exposes include/exclude filters in its Find navigator. Fleet documents regex and other matching options for project text search.
Sources: [CodeEdit 0.3.6: search filters](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/FindNavigator/FindNavigatorForm.swift), [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[SearchProject](../../src/core/Project.cpp) performs literal, line-based search over the index.
[SearchWindow](../../src/ui/Workspace.cpp) always requests case-insensitive matching and offers no
folder, glob or regex controls. Git ignore awareness already exists.

## Candidate scope

Add folder scope, include/exclude globs, case sensitivity, whole-word matching and regex queries.
Group results by file and expose skipped/truncated counts. Clearly state whether results reflect
disk or unsaved buffers; the proposed first version keeps disk search and labels it accordingly.

## Acceptance checks

- [ ] Search only selected source folders while excluding generated files, and open a result at the correct Unicode-aware position.
- [ ] Case-sensitive and regex fixtures produce expected results, including multiple matches on one line.
- [ ] Changing query or project cancels old work; stale results never replace current results.
- [ ] Large repositories remain usable and show binary, size, permission and result-limit exclusions.

## Dependencies and review decisions

Depends on the query semantics in [document search](04-advanced-find-replace.md), not its UI. Decide whether ignored files can be included explicitly and whether multiline regex belongs in the first goal.
