# Named terminal profiles

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **M** · Kiri gap: **Partial**

## Reference behavior

Fleet 1.41 exposes terminal profiles and a New Terminal with Profile action.
Sources: [Fleet 1.41 release notes](https://blog.jetbrains.com/fleet/2024/10/fleet-1-41-is-here-with-new-bundled-keymaps-unlimited-splits-improved-typing-latency-and-editor-responsiveness-and-many-more-enhancements/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri already supports multiple independent terminal tabs.
[PtySession::Start](../../src/core/Terminal.cpp) selects the inherited SHELL or a bash fallback and
starts an interactive shell in the supplied directory; profiles are not configurable.

## Candidate scope

Allow named profiles with shell executable, argument list, working-directory rule and environment
overrides. Add New Terminal with Profile and a default profile setting. Keep an existing terminal’s
configuration fixed when preferences or the active project change.

## Acceptance checks

- [ ] Launch two profiles with different environments and directories and verify isolation.
- [ ] Persist and rename profiles without changing already running shells.
- [ ] Missing executables and invalid directories show an error without leaving a broken tab or orphan process.
- [ ] Shell arguments with spaces, Unicode paths, Ctrl+C, resize, clipboard and alternate-screen programs work in the VM.

## Dependencies and review decisions

Reuse TerminalPanel and PtySession, extending their launch parameters. Environment overrides can contain secrets, so define their storage and visibility before implementation. Terminal search and richer selection are separate potential enhancements.
