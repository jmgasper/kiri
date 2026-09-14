# Quick documentation and signature help

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Next** · size: **M** · Kiri gap: **Missing**

## Reference behavior

Fleet documents quick documentation and type/parameter information in its language support.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/), [Kotlin support in Fleet, October 2023](https://blog.jetbrains.com/kotlin/2023/10/kotlin-support-in-jetbrains-fleet/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri requests plain-text completion items and shows labels, but
[WorkspaceLanguage.cpp](../../src/ui/WorkspaceLanguage.cpp) has no hover or signatureHelp requests.
Its [Editor](../../src/ui/Editor.h) provides no documentation or parameter popup.

## Candidate scope

Show bounded hover documentation on a deliberate hover or keyboard command. Display call signatures
and highlight the active parameter while typing a call. Use native text rendering with a small
supported markup subset and a keyboard path for reading/dismissing the popup.

## Acceptance checks

- [ ] Show documentation and overload/parameter details from a real configured server.
- [ ] Update the active parameter while editing nested calls and moving the caret.
- [ ] Dismiss or invalidate popups after tab changes, stale responses and server restarts.
- [ ] Long documentation, malformed markup, missing documentation and Unicode positions remain usable.

## Dependencies and review decisions

These are two LSP requests sharing a popup presentation layer; decide whether to deliver them together. Link activation should be explicit, and documentation must not introduce a browser runtime.
