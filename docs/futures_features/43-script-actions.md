# User scripts that transform editor text

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CotEditor’s Script menu runs UNIX scripts with document/selection input and can route output back to the editor or a new document.
Sources: [CotEditor 7.1.0: UNIX script input and output](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/script_unixscript.html).

## Kiri today

Kiri runs Prettier over unsaved text and has configurable LSP processes, but
[LanguageTools](../../src/core/LanguageTools.h) is not a general script-action registry. Terminal
commands do not automatically receive or replace the editor selection.

## Candidate scope

Add named user script actions with a command, arguments, input scope and output destination: replace
selection/document, new document, clipboard or output panel. Send UTF-8 on standard input and
display standard error separately. Reuse formatting’s revision validation for replacement actions.

## Acceptance checks

- [ ] Run a local script over an unsaved selection and apply its result in one Undo step.
- [ ] A failing script, invalid UTF-8, excessive output or timeout leaves the original text intact.
- [ ] Further typing, tab closure and project switching invalidate delayed edits safely.
- [ ] Commands receive literal paths/arguments correctly; installing a script definition does not execute it.

## Dependencies and review decisions

Share execution plumbing with [tasks](33-task-runner.md), but keep text transformations distinct from builds. Decide the initial metadata format and script menu location; a general binary plugin ABI is a separate design question.
