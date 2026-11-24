# Solutions and Workspaces

The IDE works on Visual Studio's files as they are.  A **solution** (`.sln`) lists **projects**
(`.vcxproj`), and each project lists its files, its include directories and its defines for each
configuration.  The IDE reads them, shows them in Solution Explorer, indexes their code as their
configuration would compile it, and writes back only the parts it changes (a file added or
renamed, a project item moved), so Visual Studio still opens the same solution.

## Opening

| File menu | What it does |
|---|---|
| Open Project/Solution... | Asks for a workspace (`.hcp65workspace`), a solution (`.sln`) or a project (`.vcxproj`) and opens it in place of what is open.  A project on its own is put in a solution of its own, as Visual Studio does. |
| Add Solution... | Opens another solution beside those that are open, so a program and the kernal it runs on can be worked on together. |
| Close Solution | Closes everything. |
| Recent Solutions | The solutions and workspaces opened before, the latest first. |
| Clear Recent Solutions | Empties that list. |

Before anything closes, the IDE asks about files with changes that aren't saved (Save, Don't Save,
Cancel), as Visual Studio does.  The same happens when the IDE exits.

The IDE opens what was open when it was last closed.  `HCP65IDE <file>` opens that solution,
project or workspace instead.

## Workspaces: several solutions at once

A program for the HCP65 is usually worked on with the kernal it runs on, which is a solution of its
own.  Add Solution opens both together, as a **workspace**.  The workspace is written to a
`.hcp65workspace` file beside the first solution, with that solution's name.  Opening the solution
again opens the whole workspace, with the solutions it was open with last time.

```xml
<Workspace RunConfiguration="ModPlay" Configuration="Release|W65C816">
	<Solution File="ModPlay\HCP65ModPlay.sln" Startup="true" StartupProject="HCP65ModPlay"/>
	<Solution File="..\HCP65Lib\Kernal\HCP65Kernal.sln"/>
	<RunConfiguration Name="ModPlay" Configuration="Release|W65C816" Tool="..\Emulator.exe" Arguments="--fill"/>
</Workspace>
```

Paths in it are relative to the workspace file.  It keeps:

- the solutions, and which is the **startup solution** (the one that is debugged);
- each solution's **startup project**, where one was chosen;
- the [run configurations](building.md#run-configurations), and the run and build configuration
  chosen last.

A project two solutions both have (BaseLib, say) is one project to the IDE: indexed once, searched
once, renamed once.

One configuration name is chosen for the whole workspace ("Release|W65C816"), and each solution
builds its configuration of that name.  Build builds every solution, in an order where a solution
whose program another loads (a kernal) is built first.

## Solution Explorer

Solution Explorer (View, Solution Explorer, or Alt+1, which also selects the active file) shows
each solution, its solution folders, its projects, each project's filters as folders, and the
files in them, as Visual Studio does.  Folders come first, then projects or files, each in
alphabetical order.  With more than one solution the startup solution's name is bold, as is the
startup project of a solution with more than one application.

Files are coloured by their [git status](git.md#colours): changed, added, not tracked or ignored.

Double-click a file, or press Enter on it, to open it.  Its right-click menu has:

| Item | What it does |
|---|---|
| Set as Startup Solution | Makes the selected solution the one that is debugged and run. |
| Set as Startup Project | Makes the selected application the solution's startup project: the one an HCP65 build links into the solution's program, and the one a GCC build's run starts. |
| Remove Solution | Takes the solution out of the workspace.  Its files aren't touched. |
| Rename... | [Rename File](refactoring/rename.md#rename-file) on the selected file. |
| Properties... | On a project: the licence its new files are written with (LGPL 3, AGPL 3 or None), kept in `<project>.hcp65project` beside the `.vcxproj`.  See [New Files and Templates](templates.md#a-projects-licence). |
| Always Select Opened File | Ticked, the tree selects the file of whichever editor is shown as you go from tab to tab, without Alt+1. |
| Open Files with Single Click | Ticked, one click on a file opens it. |

The two ticks are off to begin with and are kept in your [settings](settings.md#where-settings-are-kept).

Alt+Insert in Solution Explorer makes a [new file](templates.md#new-files) in the selected project,
filter or file's directory.

## The startup project

The startup project is the one of a solution's applications (its projects that aren't static
libraries) that is built into the solution's program and run, as Visual Studio's is.  An HCP65 build
links it with the solution's libraries and leaves the other applications out.  It is the one chosen
with Set as Startup Project, or the solution's first application if none was chosen.

## Files kept beside the solution

| File | What is in it |
|---|---|
| `<solution>.hcp65build` | The solution's [build configurations](building.md#build-configurations).  Shared, in version control. |
| `<workspace>.hcp65workspace` | The workspace, as above.  Shared. |
| `<project>.hcp65project` | A project's licence for new files.  Shared. |
| `fileTemplates\` | The workspace's [file and code templates](templates.md#schemes), where it has changed any.  Shared. |
| `<workspace>.layout.user` | The window layout and the files that were open.  Yours alone. |
| `<workspace>.hcp65user` | Your [breakpoints](debugging.md#breakpoints), [bookmarks](navigation.md#bookmarks), [recent files](navigation.md#recent-files) and the classes and symbols [Go to](navigation.md#go-to-class-file-symbol-and-action) went to last.  Yours alone. |

With no workspace, the `.layout.user` and `.hcp65user` files are named for the solution, or for the
project opened on its own.

## How the IDE understands the code

The IDE keeps a **code model** of everything open:

- Each file is preprocessed and parsed as its project's configuration builds it: its include
  directories, its defines, and Visual Studio's own as MSVC would see them.
- An **index** of every name the solutions' `.cpp` files declare, define and use: each use read,
  written or called, in its class or function, with base classes and virtual functions.  It is built
  on worker threads when a solution opens, and the [Indexing tab](building.md#the-indexing-tab)
  shows how far it has got.
- As you type, the file is parsed again after a short pause, for its colours and so the index has
  its new text.

Navigation, Find Usages and every refactoring work from this.  A refactoring checks that the files
it would change haven't changed since they were indexed, and does nothing if they have.
