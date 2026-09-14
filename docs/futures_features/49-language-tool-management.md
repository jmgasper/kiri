# Install and inspect language tools from the app

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit 0.3.6 includes an explicitly experimental language-server installation menu and improved language-server output viewing.
Sources: [CodeEdit 0.3.6 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.6).

## Kiri today

Kiri already has a native [Language Tools window](../../src/ui/LanguageToolsWindow.cpp),
configurable commands, project-local resolution and an [installation
script](../../tools/install-language-tools.sh). It lacks a guided installer, resolved-version
display and dedicated server-log viewer.

## Candidate scope

Extend the existing dialog with resolved command/path, version, availability and recent log output.
Provide explicit install/update actions only for a small verified catalog of Haiku-compatible tools,
showing the package source and destination. Retain custom command configuration and project-local
precedence.

## Acceptance checks

- [ ] Recognize an existing project-local server and explain why it overrides a user installation.
- [ ] Install one supported missing tool in the VM and verify completion/symbols afterward.
- [ ] Show download/process failures, cancellation and logs without exposing credentials.
- [ ] Opening settings alone changes no installations; an interrupted update preserves the previous working command.

## Dependencies and review decisions

Start with diagnostics and reuse the current installer where suitable. Verify package availability and versions when implementing; the reference menu is experimental, not evidence of a mature universal installer.
