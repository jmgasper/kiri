# Native source printing and print-to-file

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **M** · Kiri gap: **Missing**

## Reference behavior

CotEditor prints documents or selections with options for line numbers, colors, font size and page headers/footers, and uses macOS printing for PDF export.
Sources: [CotEditor 7.1.0: print](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_print.html).

## Kiri today

[Workspace::BuildMenus](../../src/ui/Workspace.cpp) has no Page Setup or Print actions, and Kiri has
no print layout path. Image/hex previews do not provide source printing.

## Candidate scope

Add native Page Setup and Print for the current source document or selection, with pagination, line
numbers and configurable headers. Use a print-friendly theme and include unsaved text. Support
print-to-file only through a verified Haiku backend or an explicitly chosen export implementation.

## Acceptance checks

- [ ] Print a multi-page UTF-8 source fixture with correct page breaks, line numbers and selected ranges.
- [ ] Long lines, tabs and large font settings produce readable output without truncated text.
- [ ] Printing a dirty buffer reflects current text without saving or changing the document.
- [ ] Cancel or encounter an unavailable printer/backend without losing edits; verify exported output when supported.

## Dependencies and review decisions

Investigate Haiku Printing Kit and available output drivers before promising PDF export. Decide whether syntax colors, print preview and arbitrary binary previews belong in the first goal; macOS PDF behavior is not a Haiku capability claim.
