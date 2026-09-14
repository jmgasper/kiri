# Definitions, references and workspace symbols

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **L** · Kiri gap: **Partial**

## Reference behavior

Fleet Smart Mode supports navigation to definitions and usages; its Goto search also finds top-level symbols across a project.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/), [Fleet shortcuts, April 2024](https://blog.jetbrains.com/fleet/2024/04/10-fleet-shortcuts-to-boost-your-productivity/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[WorkspaceLanguage.cpp](../../src/ui/WorkspaceLanguage.cpp) provides completion and documentSymbol
features but has no definition, references or workspace/symbol request. Kiri’s symbol bar covers the
active file, and project text search does not resolve semantic references in other files.

## Candidate scope

Add Go to Definition, Find References and Workspace Symbol search using advertised LSP capabilities.
Show a choice when several targets exist and a grouped results panel for references. Open target
locations through one URI-to-document path and retain a return location.

## Acceptance checks

- [ ] Navigate from a use to a declaration in another file using real TypeScript and clangd projects.
- [ ] Find semantic references without treating unrelated identifiers with identical spelling as matches.
- [ ] Handle multiple targets, unsaved buffers, spaces and Unicode in paths and positions.
- [ ] Cancel old requests, report unsupported features and return to the originating location.

## Dependencies and review decisions

Share location parsing with diagnostics and [navigation history](24-navigation-history.md). The owner may split the three commands into individual goals; do not advertise all three after implementing only one.
