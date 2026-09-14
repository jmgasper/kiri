# Editable legacy encodings and encoding conversion

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **L** · Kiri gap: **Partial**

## Reference behavior

CotEditor can reopen text with a chosen encoding, convert the encoding used for saving, and list incompatible characters.
Sources: [CotEditor 7.1.0: encoding conversion](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_change_encoding.html), [CotEditor 7.1.0: reopen with encoding](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_interpret_encoding.html), [CotEditor 7.1.0: encoding compatibility warnings](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_inspect_usedchar.html).

## Kiri today

[FileData](../../src/core/FileIO.h) records UTF-8 validity and BOM presence.
[Workspace::OpenFile](../../src/ui/Workspace.cpp) opens other encodings read-only; existing UTF-8
saves preserve a BOM but do not transcode.

## Candidate scope

Keep editor text in UTF-8 and track the original external encoding. Add Reopen with Encoding and
Save with Encoding, with an explicit initial set such as UTF-16, Windows-1252 and Shift JIS. Show
conversion failures at their character positions and preserve the original bytes until a valid save
succeeds.

## Acceptance checks

- [ ] Round-trip representative supported encodings without unintended character or line-ending changes.
- [ ] Reopen an incorrectly decoded file while protecting unsaved edits.
- [ ] Attempt a conversion containing an unrepresentable emoji and receive a precise failure without lossy saving.
- [ ] Recovery, Save As, external conflict detection and Haiku attributes remain correct for converted files.

## Dependencies and review decisions

Confirm conversion-library availability on the target Haiku image before choosing formats. Coordinate metadata and recovery changes with [line endings](13-line-ending-conversion.md); encoding detection must show uncertainty and permit override.
