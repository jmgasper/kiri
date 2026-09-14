# Editor panes and tab workflows

**View → Split Right** or **Split Down** adds a pane showing the current file.
Repeat either command inside any pane to build a nested layout. Drag the dividers
to resize panes. Each pane has its own tabs; opening another file puts it in the
active pane. Right-clicking a tab also offers both split commands.

The active pane has an accent line above its selected tab. Click its text or tab,
or use the focus commands to change panes. Find/replace, zoom, wrapping, symbols,
completion and formatting use the active editor. A formatting request retains
the view from which it was started if you switch panes while it runs.

| Command | Default Haiku shortcut |
| --- | --- |
| Split Right | Alt+\ |
| Split Down | Alt+Shift+\ |
| Focus Next Pane | Alt+Ctrl+] |
| Focus Previous Pane | Alt+Ctrl+[ |
| Keep Open | Alt+K |
| Reopen Closed Tab | Alt+Shift+W |
| Close Tab | Alt+W |

Haiku's Command modifier defaults to Alt; the menus follow your system mapping.
Focus cycles in layout order, through the left/top branch before the right/bottom
branch. **View → Close Pane** closes that pane's tabs. **Close all** and **Close
others** in a document tab's context menu apply to its pane. Closing an empty
pane collapses its divider; the workspace always retains one pane.

## Shared documents

Views of the same file share text, dirty state, undo history, saves, recovery and
language-server state. Selections, scrolling, folding, wrapping and zoom belong
to each view. Saving from either view updates the same file and clears both dirty
indicators when the saved text still matches. Save All saves each document once.
Reloading an externally changed file explicitly replaces its contents in every
view and clears its undo history.

Closing one view leaves the document open in its other panes. The final dirty
view offers Cancel, Discard and Save. Cancel stops an outstanding Close all or
Close Pane operation. Edits made while a save is running remain dirty and keep
the final view's save protection.

## Preview tabs

**View → Preview Tabs** enables or disables preview opening and persists between
sessions. It is enabled by default. A single click or arrow-key selection in the
file tree, Open Quickly or project search opens a preview without taking focus
away from browsing. Enter or a double-click opens the selected file permanently.

Preview names are italic. Each pane reuses at most one clean preview when you
select another result. Editing, saving, splitting or **Keep Open** makes the tab
permanent. Double-clicking the tab also keeps it open. Undoing the first edit
does not turn the tab back into a preview. Disabling previews keeps existing
tabs open. A failed or superseded load never replaces the current preview.

## Reopen policy

**File → Reopen Closed Tab** restores up to 64 explicitly closed saved-file views,
most recent first, during the current workspace session. It restores selection,
scrolling, wrap, zoom and folds in the original pane, or the active pane if the
original pane has closed. If that pane already contains the file, Kiri selects
its existing tab. If another pane has it open, the restored view shares its
buffer. There is always one logical document per path.

- Replacing a preview does not add it to the closed-tab history.
- Cancelled closes add no history entry.
- Missing files produce a status message and remove that history entry; the
  next invocation tries the next entry.
- Discarding changes to a saved file reopens its disk contents.
- Untitled tabs, including explicitly discarded drafts, are not retained in
  this history. Interrupted-session draft recovery remains separate.

## Session restoration

Kiri remembers the nested layout, divider proportions, tab order, active pane
and tab, and each source view's selection, scroll offsets, zoom, wrap and folds.
It loads each saved document once and attaches shared views to it. Missing saved
files are skipped. Legacy sessions with a flat tab list open in one pane.
Unsaved crash-recovery snapshots are stored once per document and restore shared
views; recovered drafts absent from the saved layout remain accessible.

Native coverage lives in [WorkspaceTests.cpp](../tests/WorkspaceTests.cpp):
`make check-workspace` exercises real Haiku windows, asynchronous loading and
saving, shared Scintilla documents, preview reuse, reopen history, nested
restoration and recovery. The existing editor, language and portable core suites
remain available through `make check check-native check-language`.
