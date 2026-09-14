# Automatic bracket and quote pairs

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit exposes brace completion and typing over closing delimiters. CotEditor offers automatic closing brackets and quotes by mode.
Sources: [CodeEdit 0.3.6: pair-completion settings](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/Settings/Pages/TextEditingSettings/Models/TextEditingSettings.swift), [CotEditor 7.1.0: settings mode](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/settings_mode.html).

## Kiri today

[Editor::NotificationReceived](../../src/ui/Editor.cpp) highlights matching braces and copies the
previous line’s indentation. It does not insert paired delimiters, wrap selections or skip an
automatically inserted closing character.

## Candidate scope

Optionally insert matching brackets and quotes, surround selected text, step over an existing
auto-inserted closer, and delete an empty generated pair together. Respect comments, strings,
escaping and the selected language. Keep the behavior independently configurable from brace
highlighting.

## Acceptance checks

- [ ] Typing an opening bracket inserts one matching closer; typing that closer advances rather than duplicates it.
- [ ] Apostrophes, escaped quotes and comments behave according to the active language profile.
- [ ] Wrapping multiple selections and deleting an empty pair produce predictable undo steps.
- [ ] Input-method composition, paste, completion insertion and read-only documents do not trigger unwanted pairs.

## Dependencies and review decisions

Use [syntax delimiter metadata](18-custom-syntax-profiles.md). Decide initial language coverage; this should not silently introduce a complete structural parser or auto-format engine.
