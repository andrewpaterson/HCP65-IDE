# The Editor

Each open file is a tab in the editor: its text in a fixed width font, line numbers in the gutter,
and C++ coloured as it is parsed.  A tab's name ends with `*` while it has changes that aren't saved.
Ctrl+S saves the file, Ctrl+Shift+S saves them all, and Ctrl+F4 closes the tab.  A file is saved with
the line ends it had: lines typed into a file whose lines end with a carriage return and line feed get
both too.

The keys are Visual Studio's and IntelliJ IDEA's where those agree, and IntelliJ's where they don't.
All of them can be changed in [Settings](settings.md#the-keymap).

Over text you can edit or select (an editor, a text field, a preview) the pointer is an I-beam.

## Moving and typing

| Keys | What they do |
|---|---|
| Arrows, Home, End, Page Up, Page Down | Move the caret.  Home and End go to the line's start and end. |
| Ctrl+Left, Ctrl+Right | The previous or next word. |
| Ctrl+Home, Ctrl+End | The start or end of the file. |
| Ctrl+Up, Ctrl+Down | Scroll the view a line without moving the caret. |
| Enter | A new line.  It isn't indented: type the indent, or Tab. |
| Shift+Enter | A new line after the caret's line, wherever the caret is in it; the caret goes to it. |
| Ctrl+Shift+Enter | A new line before the caret's line. |
| Tab | Spaces to the next tab stop.  With lines selected, indents them all. |
| Shift+Tab | Outdents the caret's line, or the selected lines, by a tab's worth. |
| Backspace, Delete | The character before or after the caret, or the selection. |
| Ctrl+Backspace, Ctrl+Delete | The word before or after the caret. |
| Ctrl+D | Duplicates the selection, the copy selected after it; with nothing selected, the caret's line. |
| Ctrl+Y | Deletes the caret's line. |
| Insert | Overwrite on or off. |

Tab types spaces.  The refactorings write the house style's tabs, so code they write lines up with
the code round it.

## Selecting

| Keys and clicks | What they select |
|---|---|
| Shift and any moving key | From where the caret was to where it goes. |
| Ctrl+A | All of it. |
| Double-click | The word.  Dragging on extends by whole words. |
| Triple-click | The line, with its line feed.  Dragging on extends by whole lines. |
| Click a line number | The line.  Dragging in the gutter selects whole lines; Shift+click extends by lines. |
| Ctrl+W | Extend Selection: the next bigger piece of code round the selection. |
| Ctrl+Shift+W | Shrink Selection: back through what Ctrl+W extended through. |
| Alt+Shift and an arrow, Home or End | A box (column) selection.  Alt+Shift+drag makes one with the mouse. |

### Extend and Shrink Selection

Ctrl+W grows the selection to the smallest piece of code round it that is bigger: the word, then the
inside of the quotes or brackets, then them with their quotes or brackets, the expression, the
statement, the line without its indent, whole lines, the block, the function, the whole file.  The
pieces come from the C++ parse of the file as well as from its text, so `pcTank->Fill(2)` grows from
`Fill` to `Fill(2)` to the whole call.  If the file has changed since it was last parsed, Ctrl+W parses
it there and then.  Ctrl+Shift+W goes back the way it came, until the selection is changed some other
way.

### Box selection

Alt+Shift with the arrows, Home or End (Visual Studio's keys) makes a box: the same columns on each of
the lines.  Typing, Backspace, Delete, Tab, Cut and Paste act on every line's part of the box as one
undo step; a box with no width is a caret on every line.  Copy gives the lines joined by line feeds.
Pasting as many lines as the box has puts one on each line; anything else goes on every line.  Shift+Tab,
Ctrl+D, Ctrl+Y and Cut with no width work on the box's whole lines.  Moving without Shift, Escape or a
click ends it.

### Drag and drop

Press in the selected text and drag: a drop caret follows the pointer, and letting go moves the text
there as one undo step.  Hold Ctrl as you let go to copy it instead.  Escape cancels.  Letting go
without dragging just puts the caret there.  It works within one editor, not between editors or
windows.

## Cut, copy and paste

Cut with nothing selected cuts the caret's line.  Every text in the IDE (the editors, the dialogs'
fields, the Build output, the Claude tab's request) cuts and copies into one history.  Ctrl+Shift+V,
Paste from History, shows it: choose an earlier text and it is pasted where the caret is.

## Undo and redo

Ctrl+Z and Ctrl+Shift+Z undo and redo across every open file, in the order the changes were made, not
only the active file's.  A refactoring that changed six files is one undo.  A file a refactoring
renamed is renamed back; a file it made is taken away again (off the disk, out of the projects and
out of git's index), its text kept so redo can make it again.

Undo won't undo what you can't see.  If the next change to undo is in a file that isn't active, or one
its editor is scrolled away from, the first Ctrl+Z shows it (the status line saying to press again) and
the second undoes it.

## What the editor shows

- **The selection's other occurrences.**  Select some text on one line and its other occurrences in the
  file are drawn in a quieter colour.
- **The usages of the symbol at the caret.**  Leave the caret on a name the index knows for a moment and
  its uses in this file are highlighted, its declarations and definition in the write colour, as
  IntelliJ does.  They go when the caret leaves the name or the file is edited, and come back once the
  index has caught up.
- **Change bars.**  Lines changed since the last commit are marked in the gutter.  Click a bar to see
  the old lines, with Rollback and Show Diff.  See [Git](git.md#change-bars).
- **Listings.**  After an HCP65 build, a line's generated assembly can be shown under it: click the mark
  in the gutter.  See [Building](building.md#listings-in-the-editor).
- **Breakpoints.**  F9 or a click in the gutter's first column.  See
  [Debugging](debugging.md#breakpoints).
- **Bookmarks and override marks.**  In the gutter's next two columns.  See
  [Bookmarks](navigation.md#bookmarks) and [overrides](navigation.md#changes-usages-and-overrides).
- **Marks on the scroll bar.**  Find's matches, bookmarks, changed lines and usages kept highlighted
  each mark where they are in the file; press the scroll bar at one to go there.

## Not there yet

There is no automatic indenting and no code completion.  Find and Replace are in
[Finding Your Way](navigation.md#find-and-replace-in-the-file).
