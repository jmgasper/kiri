# Optional AI assistance with reviewed edits

Draft for owner review · researched 2026-09-14 · [Back to index](README.md)

Suggested priority: **Research** · size: **XL** · Kiri gap: **Missing**

## Reference behavior

Fleet documented AI-assisted explanations, code generation, refactoring suggestions and commit-message generation. Availability depended on services; the retired product is a historical reference.
Sources: [Fleet editing and Smart Mode, April 2024](https://blog.jetbrains.com/fleet/2024/04/polyglot-programming-is-a-thing/). Fleet references describe the retired product; see [source status](SOURCES.md).

## Kiri today

Kiri’s [language integration](../../src/ui/WorkspaceLanguage.cpp) provides Prettier formatting, LSP
completion and symbols. There is no built-in model-provider integration, chat, contextual prompt UI
or AI edit-review flow.

## Candidate scope

Evaluate an opt-in assistant starting with explaining selected code and proposing an edit for
explicit review. Let users choose the provider and see which files/context will be sent. Keep
proposals separate from the live document until accepted and validate revisions at application time.

## Acceptance checks

- [ ] First demonstrate a provider connection from Haiku and document dependencies, configuration and expected running costs.
- [ ] Show the exact chosen context and return an explanation or a previewed edit with cancellation.
- [ ] An accepted edit is undoable; rejected, failed or stale responses do not change the document.
- [ ] Normal editing works without credentials or network access, and disabling the feature stops requests.

## Dependencies and review decisions

Make provider choice, data handling and cost explicit during review. Reuse the edit transaction from [rename](29-symbol-rename.md); autonomous command execution, multi-agent orchestration and full-project indexing are separate possible goals.
