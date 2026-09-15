# Future feature proposals

Researched **2026-09-14** against Kiri **0.0.1 alpha**, source commit
`5ab4ba5517cf04f9de8ab05aad596f9cb9933215`.

This catalog contains **39 draft proposals and 11 completed features** based on features found in CodeEdit,
CotEditor and JetBrains Fleet. Each file describes the reference behavior,
Kiri's current support, a candidate scope, acceptance checks, dependencies and
decisions to make before handing it to Codex as an implementation goal. Completed
features 01, 04, 05, 06, 08, 20, 25, 26, 29, 31 and 40 are archived in [the done folder](../future_features/done/);
their acceptance checks record implementation verification.

The scopes, priorities and sizes are suggestions for the owner to edit. The
draft acceptance checkboxes describe future verification; they do not report tests
already run. Existing editor capabilities are recorded in [the baseline and
source notes](SOURCES.md) so that future work builds on them.

## Reference editors

| Editor | Evidence used | Most useful areas for Kiri |
| --- | --- | --- |
| [CodeEdit](https://www.codeedit.app/) | Released **0.3.6** source and 0.3.5/0.3.6 release notes; still described as in development. | Split/tab workflows, file management, tasks, themes, semantic highlighting and Git controls. |
| [CotEditor](https://coteditor.com/) | **7.1.0** release and its bundled English help, pinned to a source commit. | Encoding and Unicode tools, search/replacement, syntax customization, text manipulation, scripts and printing. |
| [Fleet](https://www.jetbrains.com/fleet/) | Dated JetBrains announcements describing its earlier preview releases. | Actions/keymaps, semantic navigation, diagnostics/refactoring, test/debug workflows, advanced diffs and remote collaboration. |

**Fleet is a historical reference.** JetBrains stopped releasing Fleet updates.
Downloads ended on December 22, 2025; its announcement now identifies Air as a successor. This
catalog uses Fleet-specific evidence and does not assume that old services still
work or import features introduced later in Air. See the [official Fleet
announcement](https://blog.jetbrains.com/fleet/2025/12/the-future-of-fleet/) and
[research method](SOURCES.md).

In the tables, **CE** = CodeEdit, **CO** = CotEditor, and **F** = historical Fleet.
These codes identify supporting references; an omitted editor is **unassessed**
for that feature, not necessarily missing it. **Partial** means Kiri has a
related capability but lacks the specific proposed workflow. **Missing** means
the reviewed Kiri code has no implementation of that workflow. Exact evidence
and qualifications appear in each feature file.

## Suggested review order

- **Soon:** everyday workflow gaps and useful foundations; 10 proposals.
- **Next:** useful additions after the relevant foundations; 17 proposals.
- **Later:** optional polish or more specialized workflows; 8 proposals.
- **Research:** resolve Haiku/tooling/architecture feasibility first; 4 proposals.

Sizes indicate implementation breadth, not time estimates: **S** is a contained
command or UI addition, **M** crosses a few existing modules, **L** needs a new
model or substantial coordination, and **XL** needs architectural research.
IDs are stable references, not an implementation sequence.

A useful first review set is [comment toggling](10-toggle-comments.md),
[indentation settings](09-editorconfig-indentation.md),
[external refresh](../future_features/done/08-external-file-refresh.md),
[diagnostics](../future_features/done/26-diagnostics.md), [code navigation](27-code-navigation.md) and
[saved tasks](33-task-runner.md). Implemented [splits](../future_features/done/01-split-editors.md)
provide shared-view foundations; [explorer operations](07-explorer-file-operations.md)
remain a larger candidate.

## Workspace and navigation

| ID | Proposal | Evidence | Kiri gap | Priority / size |
| --- | --- | --- | --- | --- |
| 01 | [Split editors and shared document views](../future_features/done/01-split-editors.md) | CE, CO, F | Done | Implemented / L |
| 02 | [Searchable command palette](02-command-palette.md) | F | Missing | Soon / M |
| 03 | [Custom keyboard shortcuts](03-custom-keybindings.md) | F | Missing | Soon / M |
| 20 | [Custom themes and import/export](../future_features/done/20-custom-themes.md) | CE, CO | Done | Implemented / M |
| 21 | [Document minimap](21-minimap.md) | CE | Missing | Later / M |
| 23 | [Distraction-free mode](23-focus-mode.md) | CE | Partial | Later / S |
| 24 | [Navigation history and breadcrumbs](24-navigation-history.md) | CE | Partial | Next / M |
| 25 | [Preview tabs and reopen closed tabs](../future_features/done/25-tab-workflows.md) | CE, F | Done | Implemented / M |

## Search and file workflows

| ID | Proposal | Evidence | Kiri gap | Priority / size |
| --- | --- | --- | --- | --- |
| 04 | [Regex and scoped document replacement](../future_features/done/04-advanced-find-replace.md) | CO | Done | Implemented / M |
| 05 | [Filtered and regex project search](../future_features/done/05-scoped-project-search.md) | CE, F | Done | Implemented / M |
| 06 | [Previewed project-wide replacement](../future_features/done/06-project-replace.md) | CE | Done | Implemented / L |
| 07 | [Explorer file operations](07-explorer-file-operations.md) | CE | Partial | Soon / L |
| 08 | [External reload and automatic project refresh](../future_features/done/08-external-file-refresh.md) | CE | Done | Implemented / M |
| 16 | [Saved replacement sequences](16-replacement-pipelines.md) | CO | Missing | Later / M |
| 48 | [Optional autosave to disk](48-autosave.md) | CE, F | Partial | Next / M |

## Editing and text formats

| ID | Proposal | Evidence | Kiri gap | Priority / size |
| --- | --- | --- | --- | --- |
| 09 | [Indentation settings and EditorConfig](09-editorconfig-indentation.md) | CO, F | Partial | Soon / M |
| 10 | [Language-aware comment toggling](10-toggle-comments.md) | CO, F | Missing | Soon / S |
| 11 | [Automatic bracket and quote pairs](11-paired-delimiters.md) | CE, CO | Partial | Next / M |
| 12 | [Editable legacy encodings and conversion](12-text-encodings.md) | CO | Partial | Soon / L |
| 13 | [Line-ending conversion and inspection](13-line-ending-conversion.md) | CO | Partial | Soon / M |
| 14 | [Unicode inspection and normalization](14-unicode-tools.md) | CO | Missing | Next / M |
| 15 | [Line sorting with numeric/pattern keys](15-line-sorting.md) | CO | Missing | Next / M |
| 17 | [Snippets and completion placeholders](17-snippets.md) | CO, F | Partial | Next / L |
| 18 | [Syntax profiles and server-free outlines](18-custom-syntax-profiles.md) | CO | Partial | Next / L |
| 19 | [Document/selection information inspector](19-document-statistics.md) | CO | Partial | Later / M |
| 22 | [Column guides](22-column-guides.md) | CE | Missing | Next / S |

## Language tools

| ID | Proposal | Evidence | Kiri gap | Priority / size |
| --- | --- | --- | --- | --- |
| 26 | [Diagnostics and Problems panel](../future_features/done/26-diagnostics.md) | F | Done | Implemented / L |
| 27 | [Definitions, references and workspace symbols](27-code-navigation.md) | F | Partial | Soon / L |
| 28 | [Quick documentation and signature help](28-hover-signature-help.md) | F | Missing | Next / M |
| 29 | [Previewed symbol rename](../future_features/done/29-symbol-rename.md) | F | Done | Implemented / L |
| 30 | [Quick fixes and code actions](30-code-actions.md) | F | Missing | Next / L |
| 31 | [Semantic highlighting](../future_features/done/31-semantic-highlighting.md) | CE | Done | Implemented / M |
| 32 | [Additional formatters and format on save](32-formatting-providers.md) | F | Partial | Next / M |
| 49 | [Language-tool installation and logs](49-language-tool-management.md) | CE, experimental | Partial | Later / M |

## Git

| ID | Proposal | Evidence | Kiri gap | Priority / size |
| --- | --- | --- | --- | --- |
| 36 | [Branch creation and switching](36-git-branches.md) | CE | Partial | Soon / M |
| 37 | [Stash management](37-git-stashes.md) | CE | Missing | Next / M |
| 38 | [Three-way conflict resolution](38-merge-conflicts.md) | F | Missing | Next / L |
| 39 | [Line-level blame](39-git-blame.md) | F | Partial | Next / M |
| 40 | [Side-by-side diffs](../future_features/done/40-side-by-side-diffs.md) | F | Done | Implemented / L |
| 41 | [Partial staging and unstaging](41-partial-git-staging.md) | F | Partial | Next / L |

## Tools and larger directions

| ID | Proposal | Evidence | Kiri gap | Priority / size |
| --- | --- | --- | --- | --- |
| 33 | [Saved build/run tasks](33-task-runner.md) | CE, F | Partial | Soon / M |
| 34 | [Test discovery, results and reruns](34-test-runner.md) | F | Missing | Later / L |
| 35 | [Integrated debugging](35-debugger.md) | F | Missing | Research / XL |
| 42 | [Live Markdown preview](42-markdown-preview.md) | F | Partial | Next / L |
| 43 | [User script actions](43-script-actions.md) | CO | Partial | Next / M |
| 44 | [Terminal profiles](44-terminal-profiles.md) | F | Partial | Later / M |
| 45 | [Remote development over SSH](45-remote-development.md) | F | Missing | Research / XL |
| 46 | [Shared editing sessions](46-collaborative-editing.md) | F | Missing | Research / XL |
| 47 | [Optional AI assistance](47-ai-assistance.md) | F | Missing | Research / XL |
| 50 | [Native printing and print-to-file](50-printing.md) | CO | Missing | Later / M |

## Dependencies worth settling early

1. **Command dispatch:** palette and keybindings should share stable actions and
   focus rules; subsequent commands can use the same registry.
2. **Document versus view ownership:** source splits need shared buffers and
   separate positions. Reuse that work for previews and diff views where suitable.
3. **Checked multi-document edits:** project replacement, rename and code actions
   need version/stamp validation, a defined save policy and recovery from partial
   failure. Choose this model once and reuse it.
4. **File identity and change handling:** explorer operations, branch switching,
   stashes, autosave and remote files all interact with external changes and drafts.
5. **Language metadata:** indentation, syntax selection, comment delimiters,
   pairing, snippets and fallback outlines should agree about the active language.
6. **Process lifecycle:** named tasks can provide cancellation and output handling
   for testing, scripts and a later debugger without conflating their user flows.

Before creating an implementation goal, edit its scope and review decisions,
confirm its prerequisites against the then-current code, and select a real Haiku
fixture for the acceptance checks. Several files name closely related deliveries
that can be split into smaller goals while preserving the complete feature plan.
