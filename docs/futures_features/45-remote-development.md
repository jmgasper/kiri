# Remote development over SSH

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Research** · size: **XL** · Kiri gap: **Missing**

## Reference behavior

Fleet supports SSH remote workspaces with editing and development tooling hosted remotely.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/), [Fleet 1.41 release notes](https://blog.jetbrains.com/fleet/2024/10/fleet-1-41-is-here-with-new-bundled-keymaps-unlimited-splits-improved-typing-latency-and-editor-responsiveness-and-many-more-enhancements/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

[Workspace](../../src/ui/Workspace.h), [FileIO](../../src/core/FileIO.h), Git and LSP assume local
paths/processes. An SSH command in Kiri’s terminal can reach a server, but it does not turn the file
tree or editor services into a remote workspace.

## Candidate scope

Explore a native Haiku client with authenticated SSH transport and a remote helper for files,
search, Git, terminals and language services. Make the active host visible. Establish document
versioning, reconnect behavior and path mapping before attempting a full remote UI.

## Acceptance checks

- [ ] First demonstrate transport and one end-to-end remote open/edit/save cycle with a disposable host.
- [ ] For implementation, show remote search, a terminal and a language operation using the intended remote toolchain.
- [ ] Interrupt and restore connectivity without losing unsaved edits or silently overwriting remote changes.
- [ ] Measure latency, helper lifecycle and supported host platforms; document host-key and credential handling.

## Dependencies and review decisions

This needs an architecture/feasibility goal before a feature goal. Fleet’s backend cannot be assumed reusable, and Docker/WSL integrations are not implied. [External refresh](../future_features/done/08-external-file-refresh.md) and [tasks](33-task-runner.md) provide useful foundations.
