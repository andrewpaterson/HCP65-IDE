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
| Enter | A new line, indented as the line it leaves.  See [Indenting](#indenting). |
| Shift+Enter | A new line after the caret's line, wherever the caret is in it; the caret goes to it. |
| Ctrl+Shift+Enter | A new line before the caret's line. |
| Tab | A tab.  With lines selected, indents them all by one. |
| Shift+Tab | Outdents the caret's line, or the selected lines: a tab, or a tab's worth of spaces. |
| Backspace, Delete | The character before or after the caret, or the selection. |
| Ctrl+Backspace, Ctrl+Delete | The word before or after the caret. |
| Ctrl+D | Duplicates the selection, the copy selected after it; with nothing selected, the caret's line. |
| Ctrl+Y | Deletes the caret's line. |
| Ctrl+Shift+Up, Ctrl+Shift+Down | **Move Line Up** and **Down**: the caret's line, or the selected lines, change places with the line beside them. |
| Ctrl+K then C | **Comment Selection**: comments out the caret's line, or the selected lines. |
| Ctrl+K then U | **Uncomment Selection**: takes the comment marks off them. |
| Ctrl+Shift+/ | **Comment with Block Comment**: makes the selection a comment, or stops it being one. |
| Insert | Overwrite on or off. |

The last five are in the Edit menu too.

### Indenting

Tab types a tab character, in every kind of file and on every row of a box selection, as the house style
and the refactorings write them.

Enter starts the new line with the indent of the line it leaves, tabs and spaces as they are.  Shift+Enter
and Ctrl+Shift+Enter do the same.  In C and C++:

- after a line that ends with `{` the new line has a tab more;
- Enter between a `{` and its `}` puts the `}` on a line of its own, under the line of its `{`, and leaves
  the caret on the line between;
- a `}` typed on a line of its own goes under the line of the `{` it closes.

Braces in a string or a comment count as braces.

### Comments

Ctrl+K then C puts the comment mark at the indent of each line that has anything on it, as the house style
writes a comment; on several lines the marks go in one column, that of the line indented least, so the code
keeps its shape.  Doing it again adds another mark.  Ctrl+K then U takes one mark off each line that starts
with one after its indent.  The second key works with Ctrl still held or let go.  The caret and the
selection stay where they are.  Ctrl+Shift+/ puts the block marks round the
selection, which stays selected, or takes them off a selection that has them; with nothing selected it
puts both at the caret and the caret between them.

| File | Line comment | Block comment |
|---|---|---|
| C and C++ | `//` | `/* */` |
| Assembly (`.asm`, `.inc`, `.s`) | `;` | none |
| Documentation pages (`.md`) | none | `<!-- -->` |

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

## Code completion

A list under the caret of what can be written there.  It opens by itself straight after you type `.`,
`->` or `::`, and anywhere else on **Complete Code** (Ctrl+Space, in the Edit menu).  It does not open as
you type a name.  **Complete Code by Type** (Ctrl+Shift+Space) is the same list with only what has the
type wanted where the caret is.

| Where the caret is | What is listed |
|---|---|
| After `x.` or `x->` | The fields and functions of what `x` is, its base classes' after its own. |
| After `N::` | What the class, enum or namespace `N` declares; after `::` alone, what is outside any scope. |
| On a name by itself | What is in scope: the function's parameters and local variables, the class's members, what the namespaces round the caret and the scope outside any declare, an enum's enumerators, then the language's words. |
| In an `#include`'s name | The folders and headers of the directories it is looked for in. |

Each row is an icon for what it is, its name (a function's with its parameters) and, dimmer, its type or
what a function returns; where that wasn't read, the scope it is in.  A class's member is listed only
where it can be used: a private one inside its own class's functions, a protected one there and in the
classes derived from it.

Go on typing and the list is filtered by the name so far.  The capitals of a name match too: `GSN` finds
`GetSelectedNode`.  Names that start with what is typed come first, the nearest in scope before the rest.

| Key | What it does |
|---|---|
| Up, Down, Page Up, Page Down | Move through the list, round its ends. |
| Enter | Writes the selected entry in place of what was typed of it. |
| Tab | Writes it in place of the whole name the caret is in. |
| Escape | Closes the list and leaves what was typed. |

A click on a row writes it too.  A function is written with its brackets: the caret goes between them if
it takes anything, and [Parameter Info](navigation.md#what-a-name-is) shows what; if a bracket follows
already, none is added.  A folder of an `#include` is written with its slash and the list opens again for
what is in it; a header ends the name with its quote or bracket.  Complete Code with only one entry for
what is typed writes it at once.

The list closes when the caret leaves the name, when nothing matches what is typed, and on a click
elsewhere.  Nothing is listed in a comment, a string or a number.

Ctrl+Q while the list is open shows [Quick Documentation](navigation.md#quick-documentation) of the
selected entry beside the list; it follows the selection and goes with the list.

### How the list is found

What `x` is, is worked out twice.

- **At once**, from the index and the text, without parsing the file, so that the list is there on the
  key in a large workspace too.  That covers names, members, calls (by what the function returns),
  indexing, `this`, a cast written in brackets (`((CTruck*)pcVehicle)->`), typedefs, and `->` on a class
  template that holds something (`Ptr<CGauge>`), which is taken to reach what it holds.  It does not
  cover an expression with an operator in it, a function whose overloads return different things, or a
  template's own parameters.  Using directives aren't followed, and a local variable is offered from the
  start of its function, not only after its declaration.
- **By the compiler's own type checker**, after `.` and `->`: the file is parsed and checked on another
  thread, as it is for the Problems tab, which takes as long as that does (little for a small file, a
  second or two for a large one).  What it finds that isn't listed yet is added when it comes.  The list
  only grows, and the row you have selected stays selected.  Where the first way found nothing the list
  opens empty with *Working out what this is...* under it, and closes if the type checker finds nothing
  either.

So that the list is quick in a large workspace, a list of what is in scope (or of what a namespace has) is
of the names with a word starting with the first character you typed, which are the only ones that can
match.  Before you type anything it is the first five hundred, the nearest in scope.

### Complete Code by Type

Ctrl+Shift+Space lists only what has the type wanted where the caret is, what is exactly that type first
and then pointers to classes derived from it.  The type wanted is:

| Where the caret is | The type wanted |
|---|---|
| An argument of a call | The parameter it is given to, of each function of that name. |
| The right of `=`, `==` or `!=` | What the left is; for a variable being declared, its type. |
| After `return` | What the function returns. |
| After `case` | What the `switch` it is in switches on: an enum's enumerators. |

A function is listed by what it returns.  Of what is in scope it looks at the nearest two hundred and
fifty (the function's own, then its class's, then what is round it), as each one's declaration is read
for its type.  Where no type is known to be wanted, or nothing has it, the status line says so.

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
