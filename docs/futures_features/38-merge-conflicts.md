# Three-way merge conflict resolution

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Missing**

## Reference behavior

Fleet 1.35 introduced a three-window merge editor showing both inputs and the result.
Sources: [Fleet 1.35 release notes](https://blog.jetbrains.com/fleet/2024/05/fleet-1-35-is-out-introducing-ai-powered-multi-line-code-completion-for-python-and-kotlin-windows-arm-support-wildcards-multiple-ui-improvements/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri’s [GitView](../../src/ui/GitView.cpp) displays read-only unified patches and stages whole
files. The [README](../../README.md) explicitly notes that no interactive merge-conflict editor
exists.

## Candidate scope

Detect unmerged paths and open a dedicated comparison of base, ours, theirs and the editable result,
with clearly named sides. Offer per-conflict choices, manual edits, next/previous conflict and
explicit completion. Preserve the source revisions used to start the resolution.

## Acceptance checks

- [ ] Create a real conflict and resolve separate hunks using each side and a manual combination.
- [ ] Compare the saved result with expected bytes and stage only after explicit completion.
- [ ] Handle CRLF, Unicode, edits made outside the merge view and cancellation without losing either input.
- [ ] Explain unsupported binary, rename/delete and directory conflicts rather than presenting a misleading text merge.

## Dependencies and review decisions

Build on [diff views](40-side-by-side-diffs.md) and shared document ownership. Decide whether initiating merges belongs in a later goal; resolving an existing conflict is independently useful even while Pull stays fast-forward-only.
