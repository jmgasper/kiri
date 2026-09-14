# Test discovery, results and targeted reruns

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Later** · size: **L** · Kiri gap: **Missing**

## Reference behavior

Fleet released a test results tree with selected-node reruns in 1.18, Jest support in 1.20 and Vitest support in 1.31.
Sources: [Fleet 1.18 release notes](https://blog.jetbrains.com/fleet/2023/05/fleet-preview-update-1-18-is-out-with-prettier-on-save-net-unit-testing-in-tree-test-rerun-safe-quitting-updated-debug-console-git-integration-improvements-and-more/), [Fleet 1.20 release notes](https://blog.jetbrains.com/fleet/2023/07/fleet-1-20-comes-with-json-formatting-without-smart-mode-eslint-support-on-save-jest-tests-node-js-debugger-and-more/), [Fleet 1.31 release notes](https://blog.jetbrains.com/fleet/2024/02/fleet-1-31-improved-markdown-experience-additional-run-configuration-macros-support-for-vitest-and-more/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri’s repository has automated test executables, and users can invoke them in the terminal. The
application’s [workspace](../../src/ui/Workspace.h) has no test discovery, structured results view
or rerun action.

## Candidate scope

Add a native test tree showing discovered tests and passed/failed/skipped/running results. Run all,
selected or previously failed tests, retain failure output and jump to source locations. Begin with
one test framework that can be exercised on Haiku; expose adapter boundaries for future frameworks.

## Acceptance checks

- [ ] Discover and run real tests on Haiku, including passing, failing and skipped examples.
- [ ] Rerun one failure without rerunning unrelated tests and retain understandable result history.
- [ ] Cancel a run or handle malformed/truncated output without marking unfinished tests passed.
- [ ] Navigate reported locations with Unicode paths and separate failures from process-launch errors.

## Dependencies and review decisions

Depends on [task execution](33-task-runner.md). Select the initial framework and result protocol during review; terminal commands alone do not satisfy this feature, and universal framework support is not assumed.
