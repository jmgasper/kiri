# Live Markdown preview

Implemented 2026-09-16 · original research 2026-09-14 · [Back to index](../../futures_features/README.md)

Status: **Done** · original priority: **Next** · size: **L**

## Reference behavior

Fleet 1.31 includes Markdown preview as well as syntax highlighting and completion.
Sources: [Fleet 1.31 release notes](https://blog.jetbrains.com/fleet/2024/02/fleet-1-31-improved-markdown-experience-additional-run-configuration-macros-support-for-vitest-and-more/). Fleet references describe the retired product; see [source status](../../futures_features/SOURCES.md).

## Baseline before implementation

Kiri highlights Markdown through [Editor::SetLanguage](../../../src/ui/Editor.cpp).
[PreviewView](../../../src/ui/PreviewView.cpp) displays decoded images, and Workspace routes other
binary files to hex; no Markdown renderer is present.

## Implemented scope

Render the unsaved Markdown buffer beside or instead of its source, with headings, lists, emphasis,
links, images, fenced code and tables according to a chosen dialect. Prefer a native renderer
consistent with Kiri’s no-browser-runtime requirement. Add explicit link activation and local
resource resolution.

## Acceptance checks

- [x] Preview unsaved edits in representative README documents with relative links and images.
- [x] Handle invalid/incomplete markup without interrupting editing or showing stale content.
- [x] Raw HTML and remote resources follow a defined policy; preview rendering does not execute embedded scripts.
- [x] Keyboard navigation, themes, resizing and long documents remain usable in QEMU.

## Dependencies and review decisions

Investigate native Markdown parsing/rendering on Haiku before fixing library choices. Coordinate with [splits](01-split-editors.md); choose dialect, raw-HTML behavior and whether synchronized scrolling belongs in the first delivery.

## Decisions

Synchronized scrolling is part of the first delivery, in both directions.

## Implementation and verification

View → Markdown Preview renders the unsaved Scintilla buffer beside its source
using MD4C 0.5.2 and a native Haiku view. The dialect is CommonMark with tables,
strikethrough and task lists. Headings, nested lists, emphasis, reference links,
local images and fenced code render without an HTML engine. HTML remains literal
text; remote images and executable URL schemes are blocked. HTTP(S) handlers
launch only on explicit link activation. Local links open in Kiri; heading
fragments navigate within or between Markdown files.

Source positions drive two-way scrolling, including wrapping, folding, zoom and
resizing. Each pane owns its preview while keeping the existing shared text/undo
buffer. Preview mode survives tab moves, reopening, detaching and session restore.
Keyboard navigation, light/dark themes and source focus are supported.

Parsing and image decoding run on a cancellable worker. Revisions guard result
delivery and pending edits clear the old render. Large or complex documents show
a paused message and remain editable. Font measurements are cached for long
documents. Encoded image sizes and decoded bitmap headers are bounded.

See [usage and limits](../../MARKDOWN_PREVIEW.md),
[portable checks](../../../tests/MarkdownTests.cpp),
[native checks](../../../tests/MarkdownNativeTests.cpp) and
[QEMU verification](../../STATUS.md).
