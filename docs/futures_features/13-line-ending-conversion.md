# Line-ending selection and mixed-ending inspection

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CotEditor identifies inconsistent line endings and offers normalization through its Line Endings menu.
Sources: [CotEditor 7.1.0: find inconsistent line endings](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_find_inconsistent_line_endings.html).

## Kiri today

[ReadFile](../../src/core/FileIO.cpp) detects an EOL mode and preserves input bytes.
[Workspace::UpdateStatus](../../src/ui/Workspace.cpp) displays LF/CRLF/CR, but users cannot select a
new mode or inspect a mixture through the UI.

## Candidate scope

Add an EOL selector and counts for LF, CRLF and CR. Distinguish the ending used for newly inserted
lines from explicit conversion of the whole document. Provide an undoable Normalize Line Endings
command and a way to jump to inconsistent endings.

## Acceptance checks

- [ ] Open a mixed-ending fixture and report accurate counts without silently rewriting it.
- [ ] Convert to each supported ending, save/reopen and verify exact bytes.
- [ ] One Undo restores the original mixture, selection and dirty state.
- [ ] Handle empty files, a final line without an ending, Unicode and large files without freezing the UI.

## Dependencies and review decisions

Integrate with [EditorConfig](../future_features/done/09-editorconfig-indentation.md). Decide whether opening a mixed file shows a passive status indicator or a prompt; preservation remains the default until conversion is requested.
