# Shared editing sessions

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Research** · size: **XL** · Kiri gap: **Missing**

## Reference behavior

Fleet’s published collaboration workflow allowed participants to edit shared projects and access development tools together. This is historical evidence, not a claim that Fleet’s hosted services remain available.
Sources: [Welcome to Fleet, November 2021](https://blog.jetbrains.com/blog/2021/11/29/welcome-to-fleet/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri’s [Document state](../../src/ui/Workspace.h) and [Recovery](../../src/core/Recovery.h) are
local to a single application session. VNC shares one desktop and input stream; it does not provide
concurrent editor participants or document synchronization.

## Candidate scope

Investigate invite-based sessions with participant identities, remote cursors, concurrent text
updates and explicit read/write access. Start with one shared document and a local test transport.
Decide separately whether terminal, tasks or debugging are shared and what permissions each
requires.

## Acceptance checks

- [ ] First compare synchronization approaches and demonstrate two clients converging after simultaneous edits.
- [ ] For a usable implementation, show joining, leaving, cursor presence and distinct read-only/edit permissions.
- [ ] Verify undo affects the intended participant’s work and reconnecting does not duplicate or lose edits.
- [ ] End a session and retain recoverable local documents with no remaining guest access.

## Dependencies and review decisions

Requires a collaboration model and service strategy beyond [remote development](45-remote-development.md). Resolve hosting, identity, offline edits and maintenance cost before commissioning an implementation goal.
