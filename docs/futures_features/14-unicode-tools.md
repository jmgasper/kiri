# Unicode character inspection and normalization

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Missing**

## Reference behavior

CotEditor’s character inspector shows constituent Unicode code points and names; its normalization commands convert selected text to standard Unicode forms.
Sources: [CotEditor 7.1.0: Unicode character inspector](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_inspect_character.html), [CotEditor 7.1.0: Unicode normalization](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_normalize_unicode.html).

## Kiri today

Kiri supports UTF-8 editing and [LSP position conversion](../../src/core/LanguageProtocol.cpp).
[Workspace’s menus](../../src/ui/Workspace.cpp) expose whitespace display but no character metadata
inspector or Unicode normalization command.

## Candidate scope

Add Inspect Character for the selection, showing code points, names, UTF-8 bytes and
invisible/control characters. Add explicit NFC/NFD normalization of selected text. Offer
compatibility forms only with a clear description of their effect; never normalize source code
implicitly.

## Acceptance checks

- [ ] Inspect a combining accent, a joined emoji and a zero-width character with all constituent code points shown.
- [ ] Normalize canonically equivalent text to NFC/NFD and undo the transformation exactly.
- [ ] Keep byte offsets, selections and language-server positions valid after length-changing normalization.
- [ ] A large selection is bounded or summarized, and read-only documents permit inspection only.

## Dependencies and review decisions

Confirm a Unicode data/library source available on Haiku. Decide whether normalization is in the initial goal or a follow-up; [document statistics](19-document-statistics.md) can share character segmentation.
