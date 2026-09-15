# Saved sequences of text replacements

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **M** · Kiri gap: **Missing**

## Reference behavior

CotEditor’s Multiple Replace applies ordered replacement rules and saves named definitions for reuse and import/export.
Sources: [CotEditor 7.1.0: multiple replacement rules](https://github.com/coteditor/CotEditor/blob/545031e89383b7a20b1b2be4a846fca02dbf9c0f/CotEditor/Resources/CotEditor.help/Contents/Resources/en.lproj/pgs/howto_multiple_replace.html).

## Kiri today

[Editor::ReplaceAll](../../src/ui/Editor.cpp) applies one literal pair at a time. There is no
persisted rule collection, ordered preview or multi-rule command.

## Candidate scope

Let users name an ordered list of find/replace rules, enable or disable each rule, choose literal or
regex matching, and run the list on a selection or document. Preview the final result because later
rules operate on earlier output. Store portable, versioned definitions.

## Acceptance checks

- [ ] Run a two-rule fixture where the second rule consumes the first rule’s output and show the correct final preview.
- [ ] Apply the complete sequence in one undoable edit and retain text if a rule is invalid.
- [ ] Cancel or reject a stale preview after further typing without losing edits.
- [ ] Export, import and reorder a definition, preserving escaping and Unicode values.

## Dependencies and review decisions

Depends on [advanced find/replace](../future_features/done/04-advanced-find-replace.md). Keep the first goal within one document; [project replacement](../future_features/done/06-project-replace.md) supplies any future multi-file transaction layer.
