# Live Markdown preview

Choose **View → Markdown Preview** (Alt+Shift+V on Haiku's default keymap)
to render the active text buffer beside its source. The divider resizes the two
views. Toggle the command again to recover the source area. It works with
unsaved edits and unnamed documents, without saving or modifying the buffer.
Each editor pane owns its preview; views of the same document still share edits
and undo. Preview mode and divider proportions persist with saved tabs, including
tab moves, reopening, and session restoration.

**Focus Markdown Preview** (Alt+Ctrl+V) opens and focuses the preview. Up/Down,
Page Up/Down, Home/End and the mouse wheel scroll it. Tab moves through links,
Shift+Tab moves back, and Enter or Space activates the selected link. Escape
returns to the source. Reaching the end of the links also returns to the source.
Links are underlined and show their destination in a tooltip.

Scrolling follows in both directions using source-line positions, with
interpolation through wrapped paragraphs, code and images. Source folds, wrapping
and zoom are accounted for. Scrolling preserves the source caret, selection,
undo and dirty state. This is approximate alignment by source line, rather than
a pixel-for-pixel match between differently laid-out views.

## Dialect and resources

The native renderer uses the pinned MIT-licensed
[MD4C 0.5.2](../vendor/md4c/UPSTREAM.md) parser: CommonMark with tables,
strikethrough and task lists. It displays headings, paragraphs, ordered and
unordered nested lists, quotes, rules, emphasis, code spans and fenced/indented
code. Tables wrap within their columns and honor column alignment. Code uses a
fixed-width font. Body text follows the editor font size within 10–24 points;
the preview follows the current theme.

- **Raw HTML is displayed literally.** It does not create UI elements or execute
  scripts. There is no HTML engine, browser runtime, CSS or JavaScript execution.
- **Images are local only.** Relative paths resolve beside the document's saved
  path, including percent-escaped spaces and `../` paths. Save As changes that
  base. Absolute local paths are supported. Installed Haiku translators decode
  images; unavailable, unsupported or blocked images show their alternative
  text. An unnamed document must be saved before relative images can resolve.
- **Remote resources are never loaded while rendering.** HTTP(S) links open the
  system handler only when clicked or activated with the keyboard. Local file
  links open in Kiri. Markdown links can include a heading fragment; `#heading`
  navigates within the current preview. Other URL schemes, including `file:`,
  `data:` and `javascript:`, and network-path URLs are blocked.
- Heading IDs use lowercase ASCII letters, preserve Unicode letters, replace
  spaces with hyphens and remove punctuation. Duplicate IDs receive numeric
  suffixes. Raw HTML IDs are not interpreted.

## Responsiveness and bounds

Visible previews check for changes every 100 ms, then wait for 150 ms of quiet
before parsing on a cancellable worker. Pending edits clear the previous render;
results for an older revision are discarded. Incomplete Markdown is parsed as
ordinary Markdown and never changes or blocks source editing. Closing a preview
cancels its pending work. Hidden previews defer parsing until shown.

Preview pauses with an explanation above 2 MiB, 30,000 source lines, 150,000 parser
events, 128 nesting levels, or 60,000 layout fragments. Source editing remains
available; reducing the document resumes rendering. Native font measurements
are cached, and adjacent text fragments share a drawing operation.

At most 32 local image decodes are attempted per render. Each encoded file is
limited to 8 MiB. Decoded bitmap headers are checked before allocation: no more
than 32 MiB per image, 64 MiB total and 16,384 pixels on either axis. Images fit
the preview width and a maximum display height of 600 pixels. Resources refresh
with the next Markdown edit or when the preview is reopened.

Verification is in [portable parser/resource checks](../tests/MarkdownTests.cpp)
and [native workspace checks](../tests/MarkdownNativeTests.cpp). Run
`make check check-markdown-native` in Haiku, or the portable CMake/CTest suite
on Linux. See the [verification record](STATUS.md) for QEMU results.
