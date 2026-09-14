# Additional formatters and format on save

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

Fleet supports formatting beyond one tool, including JSON formatting, and released Prettier-on-save integration in 1.18.
Sources: [Fleet 1.18 release notes](https://blog.jetbrains.com/fleet/2023/05/fleet-preview-update-1-18-is-out-with-prettier-on-save-net-unit-testing-in-tree-test-rerun-safe-quitting-updated-debug-console-git-integration-improvements-and-more/), [Fleet 1.20 release notes](https://blog.jetbrains.com/fleet/2023/07/fleet-1-20-comes-with-json-formatting-without-smart-mode-eslint-support-on-save-jest-tests-node-js-debugger-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri’s [FormatDocument](../../src/ui/WorkspaceLanguage.cpp) runs Prettier manually with stale-result
protection and one Undo. It has no LSP formatting/range-formatting command or format-on-save
preference.

## Candidate scope

Generalize formatting behind a per-language provider choice: existing Prettier, capable LSP servers,
or explicitly configured external commands. Add optional format on save with one defined pipeline:
snapshot, format, validate revision, then save. Selection formatting should appear only when
supported.

## Acceptance checks

- [ ] Format C++ using a real supported tool and preserve the existing Prettier workflow.
- [ ] Format a selection without touching unrelated text when the provider supports ranges.
- [ ] A slow, failed or outdated formatter cannot replace newer typing or cause recursive saves.
- [ ] Undo, dirty state, recovery, project rules and external-file conflicts remain predictable for manual and save-triggered formatting.

## Dependencies and review decisions

Decide provider precedence and what Save does after a formatter failure. Coordinate with [EditorConfig](09-editorconfig-indentation.md) and [autosave](48-autosave.md); a working Haiku formatter must be demonstrated before promising language coverage.
