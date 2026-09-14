# Document and selection information inspector

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CotEditor’s Document inspector shows file metadata and counts of lines, words and characters for the document and selection.
Sources: [CotEditor 7.1.0: document information](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_inspect_fileinfo.html), [CotEditor 7.1.0: character counts](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_count_characters.html).

## Kiri today

[Workspace::UpdateStatus](../../src/ui/Workspace.cpp) shows path, cursor position, language, UTF-8
and EOL mode. Kiri does not expose selection counts, word counts or a document metadata panel.

## Candidate scope

Add an optional native inspector for path, size, modification time, permissions, encoding and line
endings, plus document/selection line, word, code-point and user-perceived character counts. Label
byte counts separately and allow copying values. Compute expensive counts in cancellable background
work.

## Acceptance checks

- [ ] Verify counts using ASCII, combining marks, emoji, CJK text and mixed line endings.
- [ ] Change the selection while counting and discard obsolete results.
- [ ] An unsaved document reports in-memory size separately from the disk file size.
- [ ] Inspect a 200 MiB document without a full synchronous recount after every keystroke.

## Dependencies and review decisions

Define word boundaries and grapheme counting explicitly and share Unicode support with [character tools](14-unicode-tools.md). Metadata editing and Finder-specific tags are outside the initial proposal.
