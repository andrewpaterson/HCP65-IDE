# Settings and Keys

File, Settings... (Ctrl+Alt+S) has five pages: **Keymap**, [File and Code
Templates](templates.md#the-templates), **Find Duplicates**, [Claude](claude-tab.md) and **Editor**.  What
OK changes applies straight away.

## The keymap

The Keymap page lists every command of the IDE in the groups you know them by (File, Edit, Navigate,
Refactor, Build, Debug, Solution Explorer, Editor Actions...), each with its keys, as IntelliJ IDEA's does.
Type in its filter to find one.  The buttons beside the list:

| Button | What it does |
|---|---|
| Add Shortcut... | Opens **Add Shortcut**: press the keys you want for the selected command.  It says where they work ("Everywhere", or the editor) and what giving them to the command would mean (another command losing them, say).  OK isn't allowed for keys that can't be given. |
| Remove | Takes the selected keys away from the command. |
| Reset | Gives the command its own keys back. |
| Reset All | Gives every command its own keys back. |

- A key belongs to one command: giving it to another moves it, and the first loses it.
- Keys that type a character can't be given to a command.
- A command that works everywhere can't take the keys a dialog or a text field needs.

Only the keys you change are kept, in `Keymap.xml` in your settings directory, so a new version of the
IDE's own keys still reaches every command you haven't changed.

## Find Duplicates

The smallest run of a function's statements that [Find Duplicates](navigation.md#find-duplicates)
lists: **Statements** (counting those inside an `if` or a loop, 3 to begin with) and **Tokens** (20).  Both
must be reached.  A function's whole body written elsewhere is always listed.  **Literals may differ** is
what the Find Duplicates dialog's tick starts as.

## Editor

Three ticks, all on to begin with: **Show breadcrumbs** (what the caret is in, under the code), **Show
sticky lines** (the first lines of what the top of the view is in, kept over the code) and **Show what a
name is when the pointer rests on it** (the tool tip over a name).  See
[Moving in a file](navigation.md#breadcrumbs) and [What a name is](navigation.md#what-a-name-is).

## Where settings are kept

| File | What is in it |
|---|---|
| `Keymap.xml` | The keys you have changed. |
| `Settings.xml` | How you left the IDE: Find Usages' groupings, filters, scope and preview; Find in Files' options and scope; Find Duplicates' scope, tick and minimums; the template Author; where Claude is started; the Editor page's ticks and the Structure tree's sorting; Find's Match Case, Words and Regex; Solution Explorer's Always Select Opened File and Open Files with Single Click; the commands Go to Action ran last. |
| `RecentSolutions.xml` | File, Recent Solutions, and what was open last. |
| `fileTemplates\` | Your Default scheme's [templates](templates.md#schemes). |

They are in `%APPDATA%\HCP65IDE` on Windows and `~/.config/HCP65IDE` on Linux.  What is kept per
solution (the layout, the breakpoints) is beside the solution: see [Solutions and
Workspaces](solutions.md#files-kept-beside-the-solution).

## Every key

### File and windows

| Keys | Command |
|---|---|
| Ctrl+S, Ctrl+Shift+S | Save, Save All |
| Alt+Insert | New (a file from a template) |
| Ctrl+Alt+S | Settings |
| Ctrl+F4 | Close the document |
| Alt+1 | Solution Explorer, with the active file selected |
| Alt+2, Alt+3, Alt+4, Alt+5, Alt+6 | Bookmarks, Find Usages, Run, Debug, Build: shown, or hidden if the keys are in it |
| Alt+7 | Structure: the active file's outline (hidden if the keys are in it) |
| Alt+8, Alt+9 | Hierarchy, Find (the same) |
| Shift+Escape | Hide the tool window that has the keys |
| Ctrl+Shift+F12 | Hide All Tool Windows, and again to put them back |
| Ctrl+Tab, Ctrl+Shift+Tab | The Switcher: the open files and the tool windows |
| Ctrl+Shift+I | Quick Definition |
| Ctrl+P | Parameter Info |
| Ctrl+Shift+P | Type Info |
| Ctrl+Q | Quick Documentation |
| Alt+Right, Alt+Left | Select Next Tab and Select Previous Tab |
| Menu, Shift+F10 | The context menu |
| F4 | Jump to Source (from a tool window) |
| F7, Shift+F7 | Next and Previous Difference (in a diff) |

### Edit

| Keys | Command |
|---|---|
| Ctrl+Z, Ctrl+Shift+Z | Undo, Redo (across every open file) |
| Ctrl+X, Shift+Delete | Cut (the line, with nothing selected) |
| Ctrl+C, Ctrl+Insert | Copy |
| Ctrl+V, Shift+Insert | Paste |
| Ctrl+Shift+V | Paste from History |
| Ctrl+Space | Complete Code |
| Ctrl+Shift+Space | Complete Code by Type |
| Ctrl+A | Select All |
| Ctrl+W, Ctrl+Shift+W | Extend Selection, Shrink Selection |
| Ctrl+F, Ctrl+R | Find and Replace in the file |
| F3, Shift+F3 | Find Next and Find Previous |
| Ctrl+F3 | Find Word at Caret |
| Ctrl+Shift+F | Find in Files |
| Ctrl+Shift+R | Replace in Files |
| Ctrl+Alt+Shift+D | Find Duplicates |

### The editor

| Keys | Command |
|---|---|
| Arrows, Home, End, Page Up, Page Down | Move (with Shift: select) |
| Ctrl+Left, Ctrl+Right | Previous and Next Word (with Shift: select) |
| Ctrl+Home, Ctrl+End | Document Start and End (with Shift: select) |
| Ctrl+Up, Ctrl+Down | Scroll Up and Down |
| Alt+Shift+Arrows, Alt+Shift+Home, Alt+Shift+End | Column (box) select |
| Enter, Numpad Enter | Enter |
| Shift+Enter | Enter at Line End: a new line after this one |
| Ctrl+Shift+Enter | Enter at Line Start: a new line before this one |
| Tab, Shift+Tab | Tab (indent), Back Tab (outdent) |
| Backspace, Delete | Delete a character |
| Ctrl+Backspace, Ctrl+Delete | Delete Word Left and Right |
| Ctrl+D | Duplicate |
| Ctrl+Y | Delete Line |
| Ctrl+Shift+Up, Ctrl+Shift+Down | Move Line Up, Move Line Down |
| Ctrl+K then C | Comment Selection |
| Ctrl+K then U | Uncomment Selection |
| Ctrl+Shift+/ | Comment with Block Comment |
| Insert | Toggle Overwrite |
| Escape | Clear the selection, end a box |

### Navigate

| Keys | Command |
|---|---|
| Ctrl+N | Go to Class |
| Ctrl+Shift+N | Go to File |
| Ctrl+Alt+Shift+N | Go to Symbol |
| Ctrl+Shift+A | Go to Action |
| Ctrl+B, Ctrl+click | Go to Source (or the usages, on a declaration) |
| F12 | Go to Definition |
| Ctrl+Alt+B | Go to Implementation(s) |
| Ctrl+U | Go to Super Method |
| Ctrl+Shift+B | Go to Type Declaration |
| Ctrl+Alt+Home | Go to Related Symbol |
| Ctrl+Alt+F7 | Show Usages |
| Alt+O | Switch between header and source |
| Ctrl+F12 | File Structure |
| Alt+Down, Alt+Up | Next and Previous Method |
| Ctrl+G | Go to Line |
| Ctrl+F11, Ctrl+Shift+F11 | Toggle Bookmark, Toggle Bookmark with Mnemonic |
| F2, Shift+F2 | Next and Previous Bookmark |
| Ctrl+0 to Ctrl+9 | Go to Bookmark 0 to 9 |
| Ctrl+Alt+Shift+Down, Ctrl+Alt+Shift+Up | Next and Previous Change |
| Ctrl+Shift+F7 | Highlight Usages in File |
| Ctrl+H, Ctrl+Shift+H, Ctrl+Alt+H | Type Hierarchy, Method Hierarchy, Call Hierarchy |
| Ctrl+E, Ctrl+Shift+E | Recent Files, Recent Locations |
| Ctrl+Shift+Backspace | Last Edit Location |
| Alt+F1 | Select In |
| Ctrl+Shift+M | Move Caret to Matching Brace |
| Ctrl+[, Ctrl+] | Move Caret to Code Block Start and End (with Shift, select to it) |
| Alt+F7 | Find Usages |
| Shift+F12 | Find All References |
| Ctrl+Alt+Shift+F7 | Find Usages Settings |
| Ctrl+Alt+Down, Ctrl+Alt+Up | Next and Previous Occurrence |
| Ctrl+Alt+Left, Ctrl+Alt+Right | Back and Forward |

### Refactor

| Keys | Command |
|---|---|
| Alt+Enter | Show Context Actions |
| Shift+F6 | Rename |
| Alt+Delete | Safe Delete |
| Ctrl+F6 | Change Signature |
| F6 | Move |
| Ctrl+Alt+V | Introduce Variable |
| Ctrl+Alt+F | Introduce Field |
| Ctrl+Alt+C | Introduce Constant |
| Ctrl+Alt+P | Introduce Parameter |
| Ctrl+Alt+K | Introduce Typedef |
| Ctrl+Alt+N | Inline |
| Ctrl+Alt+M | Extract Method |
| Ctrl+Shift+D | Generate Definition |
| Ctrl+Alt+O | Optimise #includes |

The other refactorings have no keys: use the [Refactor menu](refactoring/index.md#the-refactor-menu) or
Alt+Enter.

### Build, run and debug

| Keys | Command |
|---|---|
| Ctrl+F9 | Build |
| F5 | Start Debugging, Continue |
| Ctrl+F5 | Start Without Debugging |
| Shift+F5 | Stop Debugging |
| Ctrl+Shift+F5 | Restart |
| Ctrl+Pause | Break All |
| F11, F10, Shift+F11 | Step Into, Step Over, Step Out |
| F9 | Toggle Breakpoint |
