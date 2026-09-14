# Optional automatic saving to disk

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit has an automatic-save-to-disk setting. Fleet 1.20 made its autosave delay configurable.
Sources: [CodeEdit 0.3.6: autosave setting](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Settings/Pages/GeneralSettings/GeneralSettingsView.swift), [CodeEdit 0.3.6: autosave behavior](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Documents/CodeFileDocument/CodeFileDocument.swift), [Fleet 1.20 release notes](https://blog.jetbrains.com/fleet/2023/07/fleet-1-20-comes-with-json-formatting-without-smart-mode-eslint-support-on-save-jest-tests-node-js-debugger-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri’s [RecoveryTick](../../src/ui/Workspace.cpp) writes periodic private drafts, and explicit Save
writes the destination through safe-save checks. Recovery protects interrupted work but does not
update the project file automatically.

## Candidate scope

Add opt-in autosave after an idle delay, with a visible enabled state and a sensible lower bound.
Save only named writable documents through the existing snapshot/conflict path. Suspend during
operations that change document identity and preserve the independent recovery mechanism.

## Acceptance checks

- [ ] Enable autosave, edit a named file, and verify disk changes after the selected idle interval.
- [ ] A new untitled file remains a draft until a destination is chosen.
- [ ] External edits, permission failures and rapid typing never cause silent data loss or repeated modal prompts.
- [ ] Autosave preserves Undo, dirty indicators and recovery, and does not trigger recursive format/save cycles.

## Dependencies and review decisions

Coordinate with [external reload](08-external-file-refresh.md) and [format on save](32-formatting-providers.md). Decide whether focus-loss saving and project overrides belong in the first goal; saving can trigger external build/watch tools.
