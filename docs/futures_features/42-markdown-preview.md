# Live Markdown preview

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **L** · Kiri gap: **Partial**

## Reference behavior

Fleet 1.31 includes Markdown preview as well as syntax highlighting and completion.
Sources: [Fleet 1.31 release notes](https://blog.jetbrains.com/fleet/2024/02/fleet-1-31-improved-markdown-experience-additional-run-configuration-macros-support-for-vitest-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri highlights Markdown through [Editor::SetLanguage](../../src/ui/Editor.cpp).
[PreviewView](../../src/ui/PreviewView.cpp) displays decoded images, and Workspace routes other
binary files to hex; no Markdown renderer is present.

## Candidate scope

Render the unsaved Markdown buffer beside or instead of its source, with headings, lists, emphasis,
links, images, fenced code and tables according to a chosen dialect. Prefer a native renderer
consistent with Kiri’s no-browser-runtime requirement. Add explicit link activation and local
resource resolution.

## Acceptance checks

- [ ] Preview unsaved edits in representative README documents with relative links and images.
- [ ] Handle invalid/incomplete markup without interrupting editing or showing stale content.
- [ ] Raw HTML and remote resources follow a defined policy; preview rendering does not execute embedded scripts.
- [ ] Keyboard navigation, themes, resizing and long documents remain usable in QEMU.

## Dependencies and review decisions

Investigate native Markdown parsing/rendering on Haiku before fixing library choices. Coordinate with [splits](../future_features/done/01-split-editors.md); choose dialect, raw-HTML behavior and whether synchronized scrolling belongs in the first delivery.
