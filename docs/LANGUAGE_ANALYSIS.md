# Diagnostics and semantic highlighting

Kiri displays analysis for **open source files** using the configured language
servers. Install or configure them through **Edit → Language Tools…**; see
[Language tools](LANGUAGE_TOOLS.md).

## Problems

Open **View → Problems** (`Alt+Shift+M` with Haiku's default Command key).
The native panel groups problems by file and lets you filter errors, warnings,
information, or hints. Double-click a problem, or press Enter, to select its
range in the editor. The details area contains the full message and server code.
**Next** and **Previous**, also available in the Search menu, navigate the selected
severity and wrap at the ends. Hover over an underline to see its message.

The panel shows reports for open text documents. Closing the last view of a file
removes its entries. Opening an additional pane shares the same diagnostics and
semantic ranges. A comparison or binary preview does not request analysis.
The status line reports counts; its tooltip includes the state of each open file.
Missing tools, unsupported full semantic tokens, and files above 8 MiB have
explicit fallback states. If a server never publishes diagnostics, the panel
reports that no report was received after five seconds. A server with no change
synchronization keeps analysis paused after an edit.

## Keeping reports current

Editing clears analysis decorations immediately, before Haiku delivers its
asynchronous Scintilla notifications. File synchronization is debounced by
250 ms. Cursor movement and ordinary selection preserve decorations.

Versioned diagnostic reports must match the exact synchronized input revision,
text, server generation, and document identity. Reopening a path receives a new
identity and a monotonically increasing LSP version. Delayed work is discarded
after changes, closure, or **Search → Restart Language Servers**. Undoing back to
identical text still creates a new input revision.

Some servers, including the tested TypeScript language server, omit diagnostic
versions. Kiri detects this and uses a separate server session containing an
immutable snapshot of the open source documents. After 600 ms of stable input,
it rebuilds that session. Changing any document in the snapshot invalidates all
of its reports, including reports already queued for parsing. Main server
completion, symbols, rename, and semantic tokens continue normally. This trades
additional startup time and server memory for reliable revision matching;
TypeScript diagnostics may take longer to return after typing.

Kiri currently consumes push diagnostics. Pull diagnostics, closed-file problem
indexing, diagnostic fixes, and related-location navigation are separate work.
A server remains responsible for recomputing semantic meaning, including changes
to dependencies outside Kiri's open buffers.

## Semantic colors

**Edit → Preferences… → Language-server semantic highlighting** enables or
disables this layer. It is enabled by default and saved in preferences.
Lexilla syntax highlighting remains active beneath the semantic colors and is
used whenever the server lacks full-token support or returns an invalid response.

| Server role | Theme color |
| --- | --- |
| Type, class, enum, interface, struct, type parameter | `type` |
| Parameter | `parameter` |
| Property, event | `property` |
| Function, method | `function` |
| Variable | `variable` |
| Namespace | `namespace` |
| Enum member, readonly variable/property | `readonly` |
| Keyword, modifier | `keyword` |

The `deprecated` modifier adds a strike-through. Other modifiers are ignored.
Unknown token types retain lexical coloring. Full relative token responses are
supported; delta responses, overlapping ranges, and multiline tokens are not
requested. Invalid responses clear the semantic layer and explain the fallback.
Custom themes can edit every role, as described in [Custom themes](CUSTOM_THEMES.md).

Indicators are allocated separately: find uses 8, semantic colors 9–16,
deprecation 17, comparisons 20, and diagnostic severities 21–24.

## Limits and validation

- Language analysis is limited to UTF-8 documents up to **8 MiB**. Larger files
  remain editable using Kiri's existing large-file path.
- At most **2,000 diagnostics per file** and **10,000 panel entries** are shown,
  with a truncation message. Individual diagnostic text fields are bounded to
  8,192 bytes without splitting a UTF-8 character.
- Full semantic responses are limited to **250,000 tokens**. Positions use the
  negotiated UTF-8 or UTF-16 encoding; malformed ranges and split Unicode
  characters are rejected. The portable decoder also covers UTF-32.
- An 8 MiB fixture with 91,180 semantic tokens took about **120 ms** to apply
  indicators on the Haiku VM; clearing them and inserting text took about
  **8 ms**. Response decoding took **30 ms** in the optimized native build; the first
  default host build measurement was **137 ms**.
  These are fixture measurements, not a guarantee of server analysis time.

Portable tests cover theme files, Unicode boundaries, malformed reports and
response bounds. Native workspace tests exercise shared panes, actual RPC
notifications, delayed results, no-op revisions, closure/reopening, missing
servers, unsupported capabilities and the size limit. The real-server check
shows an error, jumps, fixes and clears it with both TypeScript and clangd, then
checks insertion above tokens, tab changes and server restart:

```sh
make check check-native check-language check-workspace
build-haiku/kiri_workspace_tests --real-analysis /boot/home/config/settings/Kiri
```

Protocol references: [LSP push diagnostics](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/publishDiagnostics.md)
and [semantic tokens](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/semanticTokens.md).
