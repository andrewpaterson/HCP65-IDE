# HCP65 IDE

The HCP65 IDE is the C++ development environment for the HCP65.  It looks like Visual Studio, with
a menu bar, a toolbar, documents in the middle and tool windows docked round them.  It behaves
like IntelliJ IDEA: the same navigation, the same Alt+Enter, and the same refactorings, done to C++
rather than Java.

It opens Visual Studio solutions (`.sln`) and projects (`.vcxproj`) as they are, so the same
solution still builds in Visual Studio.  It builds them for the HCP65's processors with the
[HCP65 C++ compiler](../compiler/index.md), or natively with MSVC or GCC.  It runs and debugs
programs in the HCP65 emulator.  It understands the code through an index of every name the
solutions declare and use, and its refactorings change the code through that index and the
compiler's own type checking.

It runs on Windows (`HCP65IDE.exe`) and Linux (`HCP65IDE`).  The source is in
`HCP65-IDE/HCP65IDE`.

## The window

```text
+--------------------------------------------------------------------------------------+
| File  Edit  Navigate  View  Git  Build  Debug  Window                                 |
| [Run configuration v] [Start]                                         main             |
+-------------------+--------------------------------------------------------------------+
| Solution Explorer |  Shapes.h  |  Shapes.cpp*  |  Main.cpp                             |
|                   |                                                                    |
|  Namespaces       |   1 | #include "Shapes.h"                                          |
|   + Shapes.h      |   2 |                                                              |
|   + Shapes.cpp    |   3 | namespace Geometry                                           |
|   + Main.cpp      |   4 | {                                                            |
|                   |                                                                    |
+-------------------+--------------------------------------------------------------------+
| Find Usages | Find | Build | Indexing | Run | Debug | Claude                          |
|                                                                                       |
+--------------------------------------------------------------------------------------+
| Status line                                                                           |
+--------------------------------------------------------------------------------------+
```

- **The menu bar and toolbar.**  The toolbar has the [run
  configuration](building.md#run-configurations) chosen, Start beside it (Stop while it runs),
  and at its right the git branch that is checked out.  Right-clicking in the editor gives the
  editor's menu (Go to Source, Find Usages, Show Context Actions, Rename, Refactor, Cut, Copy,
  Paste); right-clicking in Solution Explorer gives its own.
- **The documents.**  Each open file is a tab in the editor, a [diff](git.md#diffs) is a tab of its
  own, and a tab's name ends with `*` while it has changes that aren't saved.
- **The tool windows.**  Solution Explorer and Structure on the left; Find Usages, Find, Duplicates,
  Bookmarks, Hierarchy, Build, Indexing, Run, Debug and Claude underneath.  Each can be dragged to another edge, tabbed with others, floated in a window of its
  own, auto hidden on an edge, or closed.  View opens a closed one again, and Window, Reset
  Window Layout puts them all back where they start.  Shift+Escape hides the tool window that has
  the keys, most have [a key of their own](navigation.md#the-tool-windows-keys) (Alt+1 to Alt+9) that
  shows or hides them, and Ctrl+Shift+F12 hides them all to give the editor the whole window.
- **The status line.**  What the last command did, or why it couldn't.  Every refactoring says what
  it changed here ("Moved Geometry::Area to Shapes: 1 use; 1 label in assembly changed.").

The layout and the files that were open are kept beside the solution, in
`<solution>.layout.user`, and come back when the solution is opened again.

## The pages

| Page | What it covers |
|---|---|
| [Solutions and Workspaces](solutions.md) | Opening solutions and projects, several solutions at once, Solution Explorer, the startup solution and project, the files kept beside a solution. |
| [The Editor](editor.md) | Typing, moving and selecting, box selection, Extend Selection, drag and drop, undo across files, paste history, code completion, the highlights and the gutter. |
| [Finding Your Way](navigation.md) | Go to Class, File, Symbol and Action; Go to Source, Implementation and Super Method; what a name is (its declaration, Quick Definition, Parameter Info, Type Info, Quick Documentation, its tool tip); the Type, Method and Call Hierarchies; Recent Files, the Switcher and edit locations; File Structure, breadcrumbs, bookmarks; Find and Replace, in the file and in files; Find Usages; Next and Previous Occurrence. |
| [Building and Running](building.md) | Build configurations and toolchains, the Build and Indexing tabs, listings in the gutter, run configurations and the Run tab. |
| [Debugging](debugging.md) | Debugging in the HCP65 emulator: breakpoints, stepping, the Debug tab, loading a kernal, attaching. |
| [Git](git.md) | File colours, change bars, Rollback, and diffs against HEAD, a branch, a revision or another file. |
| [New Files and Templates](templates.md) | New C++ Class, Header and Source, the File and Code Templates, their language and variables, project licences, Save File as Template. |
| [Refactoring](refactoring/index.md) | What every refactoring does, and how they all behave. |
| [The Claude Tab](claude-tab.md) | Asking Claude Code to make changes from inside the IDE. |
| [Settings and Keys](settings.md) | The Settings dialog, the keymap, and every key the IDE has. |
| [Checking the IDE](checks.md) | How the IDE is tested: its scripted checks. |

## Conventions in these pages

- Keys are written as the IDE's keymap writes them: Ctrl+Alt+V, Shift+F6.  Every one can be
  changed in [Settings](settings.md#the-keymap).
- "The caret" is the text cursor; "at the caret" means the name or expression it is in.
- The examples are in the house style the IDE writes: Hungarian prefixes (`miCount`, `pcPoint`),
  tabs to line up names, and a banner of slashes over each function definition.
