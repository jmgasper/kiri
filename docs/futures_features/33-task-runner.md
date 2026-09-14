# Saved build and run tasks

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Soon** · size: **M** · Kiri gap: **Partial**

## Reference behavior

CodeEdit stores task commands, working directories and environment variables, and provides start/stop controls with terminal output. Fleet supports run configurations and macros.
Sources: [CodeEdit 0.3.6: task configuration](https://github.com/CodeEditApp/CodeEdit/blob/05704cb7d2b19892ffcb9742b0ab3cc6116af165/CodeEdit/Features/CEWorkspaceSettings/Models/CETask.swift), [CodeEdit 0.3.6 release notes](https://github.com/CodeEditApp/CodeEdit/releases/tag/v0.3.6), [Fleet 1.31 release notes](https://blog.jetbrains.com/fleet/2024/02/fleet-1-31-improved-markdown-experience-additional-run-configuration-macros-support-for-vitest-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri has real independent PTY [terminal sessions](../../src/ui/TerminalPanel.cpp) and bounded
[process execution](../../src/core/Process.h), but no named project tasks, run controls or task
status model.

## Candidate scope

Define named project tasks such as Build, Run and Check, with a working directory,
arguments/environment, start/stop/rerun and retained output. Show running/succeeded/failed/cancelled
state and elapsed time. Make compiler file:line output navigable for a reviewed set of formats.

## Acceptance checks

- [ ] Run a real Haiku Makefile build, navigate an intentional compiler error, fix it and rerun.
- [ ] Use paths with spaces, project-relative directories and environment overrides correctly.
- [ ] Stop the entire task process group while other terminals and tasks remain usable.
- [ ] Closing a workspace or opening repository-provided task definitions follows an explicit policy; merely opening the project does not run tasks.

## Dependencies and review decisions

Build on Process, PtySession and TerminalPanel. Decide whether configurations store argument arrays, shell text, or both with distinct semantics. [Test UI](34-test-runner.md) and [debugging](35-debugger.md) can reuse task lifecycle infrastructure.
