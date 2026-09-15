# Filtered and regular-expression project search

Implemented 2026-09-15 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Soon** · size: **M**

## Reference behavior

CodeEdit exposes include/exclude filters in its Find navigator. Fleet documents regex and other matching options for project text search.
Sources: [CodeEdit 0.3.6: search filters](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/NavigatorArea/FindNavigator/FindNavigatorForm.swift), [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

[SearchProject](../../../src/core/Project.cpp) performs literal, line-based search over the index.
[SearchWindow](../../../src/ui/Workspace.cpp) always requests case-insensitive matching and offers no
folder, glob or regex controls. Git ignore awareness already exists.

## Implemented scope

Add folder scope, include/exclude globs, case sensitivity, whole-word matching and regex queries.
Group results by file and expose skipped/truncated counts. Clearly state whether results reflect
disk or unsaved buffers; the proposed first version keeps disk search and labels it accordingly.

## Acceptance checks

- [x] Search only selected source folders while excluding generated files, and open a result at the correct Unicode-aware position.
- [x] Case-sensitive and regex fixtures produce expected results, including multiple matches on one line.
- [x] Changing query or project cancels old work; stale results never replace current results.
- [x] Large repositories remain usable and show binary, size, permission and result-limit exclusions.

## Dependencies and review decisions

Depends on the query semantics in [document search](04-advanced-find-replace.md), not its UI. Decide whether ignored files can be included explicitly and whether multiline regex belongs in the first goal.

## Decision:

Ignored files can be included explicitly, but multiline regex does not belong in the first goal.

## Completed policy and verification

Project search exposes semicolon-separated folder/include/exclude filters,
case, whole words, regex and explicit ignored-file inclusion. Results use disk
text, group matches by file, show Unicode columns and report exclusion/limit
counts. Project regex is single-line, as decided. Worker cancellation plus query
and project generations reject obsolete results. Portable tests cover filtering,
multiple matches, Unicode navigation positions and exclusions; native workspace
tests exercise rapid query changes and switching projects.

See [search and refactoring](../../SEARCH_AND_REFACTORING.md),
[portable search/edit checks](../../../tests/SearchTests.cpp),
[native editor checks](../../../tests/NativeTests.cpp),
[workspace checks](../../../tests/WorkspaceTests.cpp),
[real language-server checks](../../../tests/LanguageTests.cpp) and
[the verification record](../../STATUS.md).
