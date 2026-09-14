# Integrated debugging

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Research** · size: **XL** · Kiri gap: **Missing**

## Reference behavior

Fleet’s Node.js debugger supports breakpoints, stepping and variable inspection; the release also documents exception breakpoints.
Sources: [Fleet 1.20 release notes](https://blog.jetbrains.com/fleet/2023/07/fleet-1-20-comes-with-json-formatting-without-smart-mode-eslint-support-on-save-jest-tests-node-js-debugger-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri exposes [terminal sessions](../../src/ui/TerminalView.h), but no debugger transport, breakpoint
model, call stack or variables UI. The [README](../../README.md) explicitly excludes debugging from
the alpha.

## Candidate scope

Design a native debug workspace with launch/attach, verified breakpoints, continue/pause/step
controls, threads/stack frames, variables and expression evaluation. Investigate a Debug Adapter
Protocol client and available Haiku adapters or an alternative debugger integration. Treat transport
choice and local tool availability as research gates.

## Acceptance checks

- [ ] First document a working debugger/adapter and demonstrate launch, stop and variable inspection on the current Haiku VM.
- [ ] For the implementation goal, debug a small real program and navigate between stack frames and source lines.
- [ ] Show unverified or moved breakpoints accurately after edits or missing debug information.
- [ ] Handle process exit, detach, adapter failure and cancellation without losing source edits or leaving orphaned debug sessions.

## Dependencies and review decisions

Reuse [task lifecycle](33-task-runner.md). Select one supported runtime first; do not assume Linux adapters run on Haiku. The initial handoff should resolve feasibility and then refine the implementation scope.
