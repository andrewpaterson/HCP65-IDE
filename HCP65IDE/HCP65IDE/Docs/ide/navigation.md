# Finding Your Way

## Go to Class, File, Symbol and Action

| Keys | Opens |
|---|---|
| Ctrl+N | Go to Class |
| Ctrl+Shift+N | Go to File |
| Ctrl+Alt+Shift+N | Go to Symbol: any function, variable, type or namespace |
| Ctrl+Shift+A | Go to Action: any command of the IDE |

All four are one dialog with a tab for each.  Type part of a name and the best matches are listed as
you type, each with where it is, dimmer, after it.  Up, Down, Page Up and Page Down move through them
while you type; Tab and Shift+Tab change tab (so do the four keys above); Enter or a double click opens
the one selected; Escape closes the dialog.

Under the list is a **preview** of the selected result's code: the file, or the class or symbol where it
is defined, so you can see it is the one you want before you go.

With **nothing typed**, what you went to last is listed first: on the Files tab the files you have had
in editors, the one before this one at the top (so Ctrl+Shift+N, Enter goes back to it, as
[Recent Files](#recent-files) does); on the Classes and Symbols tabs what each took you to before; on
the Actions tab the commands you ran from it, the last one at the top.  The classes and symbols are kept
with the workspace, in its [`.hcp65user`](solutions.md#files-kept-beside-the-solution) file, and the
actions with [how you left the IDE](settings.md#where-settings-are-kept).

The **Actions** tab finds a command by its name, as it is in the menus (`recfil` finds Recent Files),
with its menu and its keys after it: a way to run something you have forgotten the keys or the menu of,
and to learn its keys.  Enter runs it.  If it can't be run where the caret is, the status line says so.

What you type is matched as IntelliJ IDEA matches it:

- The characters are found in the name in order, whatever their case, each straight after the one before
  or at the start of a word of the name.  `CTA` and `TeArea` both find [`CTextArea`]; `tarea` finds
  `TextArea`.  Words start at capitals, digits, and after underscores and dots.
- A `*` or a space lets the next character be anywhere further on.
- In Go to File, `/` separates folders: `wl/tf` finds `WindowLib/TextField.h`.
- A colon and a number at the end opens the file at that line: `TextArea.cpp:120`.  The preview shows
  that line.

Exact names come first, then names that start with what you typed, then the closest matches.

## Going to a definition

| Keys | What they do |
|---|---|
| Ctrl+B, Ctrl+click | Go to Source.  On a use: its definition, or its declaration if it has no definition.  On a macro's name: its `#define`.  On an `#include`: the file it includes.  On a declaration: a chooser of its usages, one of which opens. |
| F12 | Go to Definition, as Visual Studio's.  A macro's name goes to its `#define`. |
| Ctrl+Alt+B | Go to Implementation(s).  From a virtual function: the functions that override it.  From a class: the classes derived from it.  From a function's declaration: its definition. |
| Ctrl+U | Go to Super Method.  From a member function (named at the caret, or the one the caret is anywhere in): what it overrides in its base classes.  From a class: its base classes. |
| Ctrl+Shift+B | Go to Type Declaration: the class, enum or typedef that the variable, field, parameter or function at the caret is declared with, or that the expression there is. |
| Ctrl+Alt+Home | Go to Related Symbol: everything below, and the file's header or source, in one chooser under headings. |
| Ctrl+Alt+F7 | Show Usages: the chooser of the usages of the name at the caret, wherever it is. |
| Alt+O | Switch between a `.cpp` and its header. |
| Ctrl+Alt+Left, Ctrl+Alt+Right | Back and Forward. |

When one of these has a single place to go, it goes there.  When it has several it lists them in a
**chooser** under the caret, the nearest class's first, with a preview of the selected one's code beside
them:

- type and the rows are filtered as Go to filters (`rsq` finds `CRoundedSquare::Draw`);
- Up and Down move through them, Enter or a click goes to the one selected, Escape closes it;
- the usages' chooser has **Open in Find Window**, which lists them all in Find Usages.

When it has nowhere to go, the status line says so ("No implementations of CRoundedSquare.").
Go to Type Declaration reads the type from the declaration, so a built in type (`int`) or `auto` has
nothing to go to.  With the caret on no name it goes to the type of the expression the caret is in
(on the bracket of `pcShape->Clone()`, the class it returns), after checking the file as
[Type Info](#what-a-name-is) does there.

**Ctrl+click** puts the caret on the name clicked and goes as Ctrl+B does from there.  Shift+click
still extends the selection; Ctrl+click no longer does.  While Ctrl is held, the name under the pointer
that a click would go from is underlined, the pointer becomes a hand over it, and
[its declaration](#what-a-name-is) is shown under it.

**Back and Forward** go through the places the caret has been, as a browser's history does.  A jump
(Go to, Find Usages, F4, Next Occurrence, a file opened from Solution Explorer) puts where you were on
Back, as does a big move of the caret (a click far away, Ctrl+End) or choosing another tab.  Back reopens
a file if it was closed since.  The history is kept for the session only.

## What a name is

| Keys | What they show |
|---|---|
| Ctrl held, pointer on a name | The name's declaration, under it |
| Ctrl+Shift+I | **Quick Definition**: the definition's code, without leaving where you are |
| Ctrl+P | **Parameter Info**: the parameters of the call the caret is in |
| Ctrl+Shift+P | **Type Info**: the type of what is at the caret (a name, or the expression it is in), or of the selection |
| Ctrl+Q | **Quick Documentation**: what is written about the name at the caret |
| Pointer resting on a name | A tool tip: the name's declaration and the start of its comment |

The four commands are in the View menu.

- **The declaration under the pointer.**  Hold Ctrl and point at a name: it is underlined, and a small
  box under it shows the declaration as it is written (`virtual int Area(void) = 0`, `CShape* pcFirst`,
  `PM_Outline = 4`) and, dimmer, where it is: its class if it has one, its file and its line.  A member
  function's is as its class declares it, defaults and all.  A macro's name shows its `#define`'s line
  (`#define BIGGER(a, b) ((a) > (b) ? (a) : (b))`) and where it is.  Let Ctrl go, or move off the name,
  and both go.  A keyword and a number show nothing.
- **Quick Definition** opens a chooser at the caret with the code of the symbol's definition beside it
  (its declaration, if it is defined nowhere).  For a virtual function the functions that override it
  are listed too, and for a class the classes derived from it: Up and Down preview each.  Enter goes to
  the one selected; Escape closes it and leaves the caret where it was.  On a macro's name it shows the
  macro's `#define`.
- **Parameter Info** shows, over the caret's line, the parameters of the function whose call the caret
  is in, with the one the caret's argument is given to brighter than the rest.  If the class has other
  functions of that name, each has a row, the one called first.  It follows the caret from argument to
  argument and into a call inside the call, stays while you type, and goes when the caret leaves the
  brackets, on Escape or on a click.
- **Type Info** shows the type the name at the caret is declared with: a variable's, a field's or a
  parameter's (`CPainter*`, `int[PAINT_MAXIMUM]`), what a function returns, a class as it is declared.
  With an expression selected it shows that expression's type instead, worked out as
  [Introduce Variable](refactoring/introduce.md) works it out.  With nothing selected and the caret on
  no name (a call's bracket, an operator, the `&` of `&cCircle`, a number) it shows the type of the
  expression the caret is in.  That last one checks the whole file first, which is quick in a small
  file and takes a second or two in a large one.  It goes when the caret moves.
- **Quick Documentation** shows, under the caret's line, what is written about the name at the caret:
  its declaration, where it is, the comment over it and the documentation pages that name it.  See
  [below](#quick-documentation).
- **The tool tip.**  Rest the pointer on a name for a little over half a second and a box under it
  shows the same, shorter: the declaration, where it is, and the first three lines of the comment
  (three dots after them if there is more).  It goes when the pointer leaves the name, when you type or
  move the caret, on Escape and on a click, and it doesn't come back until the pointer is on another
  name.  It isn't shown while Ctrl is held (the declaration under the pointer is), while another popup
  or a menu is open, or while the editor hasn't the keys.  File, Settings..., **Editor** turns it off.

### Quick Documentation

Ctrl+Q on a name shows, under the caret's line:

- its declaration, in the code's colours, as the box under the pointer shows it;
- where it is declared, dimmer: its class if it has one, its file and its line;
- the comment over it, as it is written;
- **Documentation:** and the [documentation pages](documentation.md) that name it with a reference to
  code, each once, at the first place in it (`shapes.md:3`), at most eight.

The comment is the `//` lines directly over the name's definition (the banner over a function in its
`.cpp`), or, with none there, the `//` comment after it on its line, as a field has.  If the definition has
neither, the declaration's is shown.  The slashes are left out, and so are a banner's rules of slashes, so
an empty banner shows nothing.  Nothing in the comment is read as markup.  A macro's name shows its
`#define` and the comment over that.

The file and line the name is declared at, and each page, are in the link colour: click one to go there.

It shows sixteen lines.  With more, the mouse wheel over it, Page Up and Page Down, and Ctrl+Up and
Ctrl+Down scroll it, and the caret stays where it is.

It goes when the caret moves, on Escape, or on a click elsewhere.  Press Ctrl+Q again while it is showing
and it stays: it then follows the caret, showing each name the caret comes to and keeping the last where
there is none, until Ctrl+Q once more or Escape.  Where the caret is on nothing with a declaration, the
status line says so.

## Hierarchies

| Keys | What they show |
|---|---|
| Ctrl+H | **Type Hierarchy**: a class, its base classes and the classes derived from it |
| Ctrl+Shift+H | **Method Hierarchy**: which of those classes define a member function |
| Ctrl+Alt+H | **Call Hierarchy**: what calls a function, or what it calls |
| Alt+8 | The Hierarchy tool window, with the keys in its tree; pressed there, it hides the window |

Each opens in the **Hierarchy** tool window, in the tab that is showing unless that tab is pinned
(Pin Tab on its toolbar): then a new tab is made, so you can keep one hierarchy while you look at
another.  The commands are in the Navigate menu.

- **Type Hierarchy** is of the class at the caret: a class's name, the class of a member named there,
  or the class a variable is declared with.  The class is shown under its base class, and that under
  its own, with the classes derived from it under it.  A class with more than one base class is under
  the first it names, and says the others after its own name (`CLamp  also: CSwitchable, CNamed`).  The
  toolbar's **Supertypes Hierarchy** shows the class with all its base classes under it instead, and
  **Subtypes Hierarchy** the class with its derived classes alone; **Class Hierarchy** goes back.
- **Method Hierarchy** is of the member function at the caret, or the one the caret is anywhere in.  It
  shows the same classes, each with what it has of the function after its name: `+` it defines the
  function, `-` it inherits it from a base class, `abstract` it declares it without a body (`= 0`), and
  `!` it neither defines it nor inherits a definition, so something that makes one of it won't compile.
  Enter on a class that defines the function goes to its function.
- **Call Hierarchy** is of the function at the caret, or the one the caret is in.  Under it are the
  functions that call it, each with the file and line of its call (and how many calls, if more than
  one).  Open a caller and its own callers are looked for and shown, and so on up; a function already
  above itself says `recursive` and goes no further.  A function that overrides one of a base class
  can also be reached by a call of the base class's function, so those callers come after its own,
  each marked with the function it calls (`DrawAll  via CShape::Draw  Main.cpp:12`).  A call that names
  its class (`CSquare::Draw()`) always goes to that class's function, so it is listed for that one
  alone.  The toolbar's **Callee Hierarchy** turns it round: what the function calls, in the order it
  calls them, and what each of those calls as it is opened.  **Caller Hierarchy** turns it back.

In all three, the preview beside the tree shows the selected row's code, and Enter or a double click
goes there: a class's definition, a function's, or the call.  A row's chevron, or Left and Right, open
and close it.  Rerun makes the hierarchy again after the code has changed, and
[Next and Previous Occurrence](#next-and-previous-occurrence) step through its rows.

## Moving between files

| Keys | What they do |
|---|---|
| Ctrl+E | **Recent Files**: the files you have had in editors, the one before this one first |
| Ctrl+Shift+E | **Recent Locations**: the places you have been, with their code |
| Ctrl+Tab, Ctrl+Shift+Tab | The **Switcher**: the open files and the tool windows |
| Alt+Right, Alt+Left | **Select Next Tab** and **Select Previous Tab** |
| Ctrl+Shift+Backspace | **Last Edit Location** |
| Alt+F1 | **Select In**: show the file in Solution Explorer or Structure |

They are in the Navigate menu, and the Switcher and the tab commands in Window, where **Reopen Closed
Tab** is too: it opens the tab you closed last, with its caret where it was.

### Recent Files

Ctrl+E lists the files you have had in editors, the open ones first, each most recent first.  The file
you were in before this one is selected, so Ctrl+E then Enter goes back to it, and again returns.  Type
to narrow the list, as [Go to File](#go-to-class-file-symbol-and-action) matches names.  A file that has been
closed since opens with its caret where it was.  **Show Edited Only** lists just the files you have
changed since the IDE was opened, and **Show All** goes back.

The recent files are kept with the workspace, in its [`.hcp65user`](solutions.md#files-kept-beside-the-solution)
file: they are there the next time you open it.

### Recent Locations

Ctrl+Shift+E lists the places [Back](#going-to-a-definition) would return to, the newest first: each
place's line, its file and line number, and the code around it beside the list.  Type to narrow it.
**Show Changed Only** lists the places you have edited instead, and **Show All** goes back.

### The Switcher

Hold Ctrl and press Tab: the Switcher lists the open files, most recent first, then the tool windows.
Still holding Ctrl, Tab (or Down) moves on and Shift+Tab (or Up) moves back, round the ends; let Ctrl
go and the selected file or tool window is shown.  So a quick Ctrl+Tab goes to the file you were in
before, and another returns.  Delete (or Backspace) closes the selected file and leaves the Switcher
open on the next one, so several files can be closed without leaving it; a file with changes asks
whether to save them first.

### Edit locations

Ctrl+Shift+Backspace takes the caret to where you last edited, and again to the place edited before
that, in whichever file; edits within a few lines of each other count as one place.  **Next Edit
Location** (Navigate menu) comes forward again.  [Back](#going-to-a-definition) returns from one.  The
places follow their text as it is edited, and are kept for the session only.

### Select In

Alt+F1 asks where to show the active file: **Solution Explorer** selects it in the tree, which gets the
keys, and **Structure** opens its outline at the caret.  Solution Explorer can also follow the editor
by itself: see [Always Select Opened File](solutions.md#solution-explorer).

## Moving in a file

| Keys | What they do |
|---|---|
| Ctrl+F12 | File Structure: a chooser of everything the file declares. |
| Alt+7 | The Structure tool window: the same, as a tree that stays open.  Pressed in it, it hides the window. |
| Alt+Down, Alt+Up | Next and Previous Method: the caret goes to the next function's name, or the one before. |
| Ctrl+G | Go to Line: type a line, or a line and a column (`120` or `120:8`). |
| Ctrl+Shift+M | Move Caret to Matching Brace: from a bracket to the one that closes or opens it. |
| Ctrl+[, Ctrl+] | Move Caret to Code Block Start and End; with Shift the text between is selected. |

What a file declares is its **outline**: its namespaces, classes, enums, functions, fields, variables
and typedefs, each inside the one it is written in, with a function's parameters after its name.  The
outline is read from the file as it is in the editor, saved or not.

**File Structure** lists the outline in a chooser over the editor, each entry indented under the one it
is in with its line after it, and the one the caret is in selected.  Type and the entries are filtered
as Go to filters; Enter or a click puts the caret on the entry's name.

The **Structure** tool window, tabbed with Solution Explorer, shows the outline of the file you are
editing as a tree and keeps up as you edit.  The entry the caret is in is selected as the caret moves.
A click or Enter on an entry puts the caret on its name.  Its context menu sorts the entries by name in
place of the order they are in the file, and hides fields; both choices are kept.

**Go to Line** opens a small prompt over the editor with the caret's line and column in it.  Enter goes
there (a line past the end is the last line), and where you were goes on Back.  What isn't a line is
said to be wrong and the prompt stays; Escape leaves the caret alone.

### Breadcrumbs

Under the code is a strip that says what the caret is in, outermost first:

```text
Deep > CLadder > Climb
```

A namespace, a class in it, a function in the class.  It follows the caret.  A press on one puts the
caret on its name, and opens a chooser of what is beside it (a class's other members, say) so you can
go on to one of those: Escape if you don't want to.

### Sticky lines

When you scroll into a long function, its first line would be out of sight.  It is kept instead, at the
top of the view over the code, with the first lines of the class and the namespace it is in above it,
outermost first and five at most.  As you scroll on into the next function its first line takes the
place of the last one's.  A press on a kept line puts the caret on what it starts.

Breadcrumbs and sticky lines can each be turned off: File, Settings..., **Editor**.

### Bookmarks

| Keys | What they do |
|---|---|
| Ctrl+F11 | Toggle Bookmark on the caret's line. |
| Ctrl+Shift+F11 | Toggle Bookmark with Mnemonic: asks for a digit or a letter. |
| F2, Shift+F2 | Next and Previous Bookmark in this file, round its ends. |
| Ctrl+0 to Ctrl+9 | Go to the bookmark with that digit, in whichever file. |

A bookmarked line has a blue cell in the gutter, with its mnemonic in it if it has one, and a mark on
the scroll bar.  A press on the cell takes the bookmark off.  A mnemonic belongs to one line: giving it
to another takes it from the first.

The **Bookmarks** tool window (View, Bookmarks) lists them by file, each with its line's text and a
preview; Enter or a double click goes to one, and Delete (Remove Bookmark in its right-click menu) takes
the selected one away.

Bookmarks move with their lines as you edit, and are kept with the workspace, in your own
[`.hcp65user`](solutions.md#files-kept-beside-the-solution) file beside it.

### Changes, usages and overrides

| Keys | What they do |
|---|---|
| Ctrl+Alt+Shift+Down, Ctrl+Alt+Shift+Up | Next and Previous Change: the next lines changed since the last commit, or the ones before. |
| Ctrl+Shift+F7 | Highlight Usages in File. |

**Changed lines** are marked on the scroll bar as well as in the gutter, in the same colours, so you can
see where in a long file you have been working.

**Highlight Usages in File** keeps the usages of the name at the caret highlighted in this file while
you move about, marks them on the scroll bar, and says how many there are.  F3 and Shift+F3 step
through them; Escape, or an edit, ends it.  (Leaving the caret on a name highlights its usages too, but
only until the caret moves.)

**Override marks** are in the gutter beside what they are about:

| Mark | Beside | A press on it |
|---|---|---|
| `^` | a function that overrides one of a base class | goes to the function it overrides |
| `v` | a function that is overridden, or a class with derived classes | goes to what overrides it, or the derived classes |
| `x` | a function that does both | opens Go to Related Symbol's chooser |

### Typing to find a row

In Solution Explorer, the Structure tree, the Find Usages, Find, Duplicates, Bookmarks and Hierarchy
trees, and a chooser that has no field to type in: just type.  What you type is shown in the corner, and the first row from
the selected one that has it is selected.  It is matched as Go to matches names (`sexp` finds
`Solution Explorer`), or anywhere in the row whatever its case.  Up and Down go to the previous and
next row that matches, Backspace takes a character back, Escape ends it and Enter opens the row.  While
nothing matches, the corner's border is red and the selection stays where it was.

## Find and Replace in the file

| Keys | What they do |
|---|---|
| Ctrl+F | Find: a bar over the code. |
| Ctrl+R | Replace: the same bar with a second row. |
| F3, Shift+F3 | Find Next and Find Previous. |
| Ctrl+F3 | Find Word at Caret. |

**Find** opens a bar over the code, not a dialog, with the selected text in it (if it is on one line) or
the word at the caret.  As you type, every match in the file is highlighted and marked on the scroll
bar, the first one from the caret is selected, and the bar says which it is of how many: `3 of 17`.

- **Enter** or **F3** goes to the next match and **Shift+Enter** or **Shift+F3** to the previous, round
  the ends of the file.
- **Escape** closes the bar and leaves the match selected, with the keys back in the editor.
- **Match Case**, **Words** (whole words only) and **Regex** change what matches.  They are the same in
  every editor's bar and are remembered.
- **In Selection** keeps the search to the text that was selected when the bar opened.  It is ticked
  for you when that was more than a line.
- Edit the file while the bar is open and it searches again.

**Replace** adds what to replace with, **Replace** (the current match, then on to the next) and
**Replace All** (every match, as one edit: one Undo puts them all back).

With the bar closed, **F3** and **Shift+F3** look again for what you last searched for, from the caret,
and **Ctrl+F3** looks for the word the caret is in.

### Regular expressions

With **Regex** ticked, here and in [Find in Files](#find-in-files), what you type is a regular
expression, as IntelliJ IDEA's:

| Written | Matches |
|---|---|
| `.` | any character but a line's end |
| `[abc]` `[a-z0-9_]` `[^abc]` | one of a set, a range, or one that isn't |
| `\d` `\w` `\s` | a digit, a word character, white space; `\D` `\W` `\S` one that isn't |
| `^` `$` | the start and the end of a line |
| `\b` | the start or end of a word |
| `(x)` `(?:x)` | a group, and one that isn't remembered |
| `x\|y` | x or y |
| `x*` `x+` `x?` `x{2,3}` | none or more, one or more, none or one, two to three; add `?` for as few as will do |
| `\1` | what the first group matched, again |
| `\.` `\(` `\\` | the character itself |

A `{` that doesn't start a count is just a `{`, so `foo\(\) {` finds what it looks like.  In the
replacement `$1` is what the first group matched (`$0` all of the match), `\n` a new line and `\t` a
tab.  What isn't a regular expression is said to be wrong, in the bar, and finds nothing.  An expression
that would take for ever on some text (`(a+)+b` on a long run of `a`) is given up on.

## Find Usages

Alt+F7 finds every use of the symbol at the caret across the workspace and shows them in the **Find
Usages** tool window.  Shift+F12, Find All References, does the same and also lists its declarations and
definition, as Visual Studio's does.

```text
Usages of Init in Solution  14 usages
  Function call  9 usages
    BaseLib  9 usages
      Chars.cpp  3 usages
        CChars
          128  Init();
```

The tree groups the usages by **how each is used** (a call, a value read, a value written, a type in a
field's declaration, a base class...), then **project**, **file** and **class**, with each usage's line
under them, its line number dim and the name itself highlighted.  Select a line and its code is shown in
the preview on the right.  Every group and file starts open, however many usages there are: tens of
thousands in one file open and scroll as quickly as ten.

| In the tree | What it does |
|---|---|
| Enter, double click | Opens the usage; the editor gets the keys. |
| F4 | Jump to Source: goes to the symbol's own declaration or definition, whichever line is selected. |
| A click, the arrows | Only select (and preview), unless Navigate with Single Click is on. |
| Escape | Gives the keys back to the editor. |
| Space, typing | Nothing.  Space doesn't open a usage as Enter does, and nothing typed in the tree reaches the editor. |

Its toolbar, down the left:

| Button | What it does |
|---|---|
| Rerun | Finds the usages again, of the same symbol, wherever the caret is now. |
| Previous Occurrence, Next Occurrence | Step through the usages. |
| Expand All, Collapse All | Open or close every group. |
| Group by Usage Type, Project, Class | Each grouping on or off.  With all off, the tree is files and their lines. |
| Show Read Access, Show Write Access | For a variable or field, both on to begin with: turn one off to leave out the usages that read it, or that write it.  The first row says how many are hidden. |
| Scope | Shows this tab's usages in another [scope](#scopes), without finding them again. |
| Pin Tab | Keeps this tab: the next Find Usages makes a new one. |
| Preview | The preview on or off. |
| Navigate with Single Click | Open each usage as it is selected, by a click or the arrows. |
| Autoscroll from Source | As the caret moves in the editor, the usage nearest it in that file is selected here: the last at or before its line, or the first after.  The editor keeps the keys. |
| Export... | The rows as they are shown, indented, saved as a text file or copied to the clipboard. |

Right-clicking in the tree gives Jump to Source, Copy (Ctrl+C: the selected row's text), Expand All and
Collapse All.  The Find, Duplicates, Hierarchy, Bookmarks and Problems trees have the same menu.

A usage's line is coloured as code where its own text says how (keywords, numbers, strings, comments),
with the name found bold; what each name is, is only coloured in the preview.  Preview, Navigate with
Single Click, Autoscroll from Source and Export are on every results tool window: Find Usages, Find,
Duplicates, Bookmarks and Hierarchy.

Each Find Usages goes into the chosen tab, named for the symbol, unless that tab is pinned (a pinned tab's
name starts with `* `), in which case a new tab is made.  A tab is closed by its cross.  The groupings and
filters are the tool window's, the same for every tab, and are remembered between sessions.  The usages
follow edits: change a file and they stay on the same text.

**Find Usages Settings** (Ctrl+Alt+Shift+F7) chooses the scope usages are shown in, the directory for the
Directory scope, and whether Find Usages includes the declarations.  OK finds the usages of the symbol at
the caret with them, and Alt+F7 uses them from then on.  Shift+F12 uses the scope too, and always has the
declarations.  They are remembered between sessions.

### Scopes

| Scope | The files in it |
|---|---|
| Solution | Every solution of the workspace. |
| Startup Solution | The startup solution alone (only offered with more than one solution open). |
| Project | The active file's project. |
| Current File | The file in the editor. |
| Open Files | The files with a tab. |
| Directory | The files in a directory, and under it. |

The index is always of the whole workspace: a scope leaves usages out of the tree, it doesn't search
less.  The active file and its project are those of when the usages were found, so a tab shows the same
whatever is opened afterwards.  A header no project lists is in Solution only.  The Ctrl+B usages popup
always shows every usage, and [Rename](refactoring/rename.md) always changes every one.

## Find in Files

Ctrl+Shift+F looks for text in a scope's files.  Its dialog has a tab for each [scope](#scopes), Match
case, Words (whole words only) and Regex (a [regular expression](#regular-expressions)), a file mask,
and a preview of the first results as you type, each with its file's code under it.  Text selected in the editor (on one line) is what it looks for; otherwise what you looked
for last.  Enter opens the result selected.

**Open in Find Window** lists every result in the **Find** tool window beside Find Usages: a tree of the
files and their lines, with the same keys, preview and toolbar as Find Usages (without the groupings and
the filters).  Open files are searched as their editors have them, unsaved changes included.  The search
runs a little at a time, so the IDE never stops answering keys.

Each find is shown in the Find tool window's tab that is chosen, named for the text.  **Pin Tab**, on the
toolbar, keeps a tab as it is (a `*` goes before its name) and the next find gets a tab of its own.
**Rerun** looks again for the tab's own text, in its own scope.  A tab is closed by its cross.

On the **Directory** tab, the `...` button beside the directory opens a chooser of folders.

The **File mask** keeps the search to some of the scope's files: `*.cpp, *.h` is names with `*` and
`?`, parted by commas, whatever their case.  A `!` before one leaves its files out: `*.h, !Test*`.

### Replace in Files

Ctrl+Shift+R is the same dialog with **Replace with** (a regular expression's `$1` is a group).

- **Replace All** asks first, saying how many matches in how many files, then replaces them all.
- **Open in Find Window** lists the matches in the Find tool window, each with what it becomes after
  it:

    ```text
    Replace 'Area' with 'Size' in the solution  3 matches in 2 files
      Shapes.h  2 matches
        13: int  Area(void) override;  ->  Size
    ```

    Its toolbar's **Replace All** replaces them all, and **Replace Selected** the selected one, or every
    one in the selected file.

Each file is opened and changed in its editor, and all of them are one Undo.  They are left changed, not
saved: Save All (Ctrl+Shift+S) keeps them.  A match whose text has changed since it was found is left
alone, and the status line says how many were.

## Find Duplicates

Edit, Find Duplicates... (Ctrl+Alt+Shift+D), as IntelliJ IDEA's Locate Duplicates, finds where else the
code at the caret is written, whatever its variables are called there.

- **An expression selected** is looked for inside any statement: `x * x + y * y` is found in
  `d = sqrt(a * a + b * b);`.
- **Statements selected** (whole statements of one block) are looked for as a run of statements.
- **Nothing selected**: the function the caret is in.  First the places its whole body is written, then
  the runs of its statements written elsewhere, the longest first.  A run must be at least as big as the
  [Find Duplicates settings](settings.md#find-duplicates) ask (3 statements and 20 tokens to begin with,
  counting the statements inside an `if` or a loop).

The function's variables and parameters may have other names at another place, as long as each is
always the same other name there and that name is a variable or parameter of the function it is in.
Members, types and functions called must be written the same.  Literals must be the same too, unless
**Literals may differ** is ticked: then a number matches any number, a string any string and a
character any character.

The dialog says what it will look for, and asks for the [scope](#scopes) and the tick.  **Find** lists
what it found in the **Duplicates** tool window: a part of the function each (for a function), each
file under it, and each place's first line, which opens the place.  It has the same keys, preview and
toolbar as Find, and **Rerun** looks again for the same code in the same scope.  The search runs a
little at a time, so the IDE carries on meanwhile, and only files with the names the code is written
with are parsed.

## The tool windows' keys

| Keys | Tool window |
|---|---|
| Alt+1 | Solution Explorer, with the file you are editing selected |
| Alt+2 | Bookmarks |
| Alt+3 | Find Usages |
| Alt+4 | Run |
| Alt+5 | Debug |
| Alt+6 | Build |
| Alt+7 | Structure |
| Alt+8 | Hierarchy |
| Alt+9 | Find |
| Shift+Escape | Hides the tool window that has the keys |
| Ctrl+Shift+F12 | **Hide All Tool Windows**, and again to put them back |

A tool window's key shows the window, brings its tab to the front and puts the keys in it.  Press the
same key again while the keys are still in it and the window is hidden, and the keys go back to the
editor: so Alt+6 looks at the build's errors and Alt+6 again gets them out of the way.  If the window
is showing but the keys are elsewhere, its key just moves the keys to it.  Alt+1 is the exception: it
always shows Solution Explorer with the file you are editing selected, and never hides it.  Duplicates,
Indexing and Claude have no keys; all of them are in the View menu, each with its keys beside its name,
and the keys can be changed in [Settings](settings.md#the-keymap).

**Hide All Tool Windows** (Ctrl+Shift+F12, in the View menu) hides every docked tool window so the
editor has the whole window.  Press it again and they come back as they were: each group the size it
was, its tabs in the same order, the same tab in front.  If you show a tool window or two by their
keys while the rest are hidden, and hide them again, the next Ctrl+Shift+F12 still brings the whole
layout back.  A tool window that is floating, or auto hidden on an edge, is left where it is.  What was
hidden is remembered until the IDE is closed.

**Jump to Last Tool Window** (in the View menu; it has no key until you give it one) shows the tool
window that last had the keys, if it has been hidden since, and puts the keys back in it.

## Next and Previous Occurrence

Ctrl+Alt+Down and Ctrl+Alt+Up step through the results of the tool window with the keys: Find Usages',
Find's, Duplicates', a [hierarchy's](#hierarchies) rows, or the [Build tab's](building.md#the-build-tab) errors.  From an editor, they step through the one
such tool window that can be seen and has something to step to; if there are none, or more than one, the
status line says so.  Each step opens the place in the editor.
