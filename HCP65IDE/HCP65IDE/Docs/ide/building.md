# Building and Running

## The run configuration

The combo box at the left of the toolbar chooses a **run configuration**: a name for one way of building
and running what is open.  It says which **build configuration** Build builds, and what Start (Ctrl+F5)
and Start Debugging (F5) run.  Its last item, Edit Configurations..., edits them.  The code model also
parses the files as that build configuration's solution configuration builds them.

## Build configurations

A build configuration is one way a solution is built: a name, a **toolchain**, and the Visual Studio
solution configuration ("Release|Win32") whose projects' files, include directories and defines it builds
with.  Its name is its configuration and platform either side of a `|`, as Visual Studio names them:
`Release|HCP65C816`.  Each toolchain has a platform of its own: `HCP65C816` and `HCP65C64` for the HCP65 compiler,
`GCC-x64` for GCC; an MSVC configuration has the solution's (`Release|x64`).  A name with a platform these once had
(`Release|W65C816`, `Release|65C64`, `Release|GCC`) is still read, as the name it has now, in a
workspace file and `hcp65build -config`.  A configuration's files are in folders named for it:
`Output\HCP65C816\Release\`.

| Toolchain | What builds it |
|---|---|
| W65C816 | The [HCP65 C++ compiler](../compiler/index.md), for the HCP65's own processor. |
| 65C64 | The HCP65 C++ compiler, for the [65C64](../65c64/index.md). |
| MSVC | MSBuild, as Visual Studio builds the solution configuration (Windows only; the newest Visual Studio's MSBuild, found with vswhere, or the one on the path). |
| GCC | `g++` and `ar` from the path, on Linux: each project compiled into `Output\GCC-x64\<configuration>\Objects\<project>\`, each static library archived (`lib<project>.a`), each application linked with all of the solution's libraries into `Output\GCC-x64\<configuration>\<project>`. |

A project can have a configuration of its own for a toolchain, a group in it or in a property sheet it imports for
the toolchain's platform (`Release|GCC-x64`), which Visual Studio never sees.  A GCC or HCP65 build uses it when the
project has it, whichever solution configuration the build configuration names:

- GCC is given the configuration's defines, then its `ClCompile` `AdditionalOptions` as `g++`'s own arguments; the
  link its `Link` `AdditionalOptions` and its `AdditionalDependencies`, a bare name as `-l<name>` (`X11` is `-lX11`).
  The configuration's type (library or application) and include directories are those of the project's x64
  configuration of the same name.
- A project without one is built as its x64 configuration, with that one's include directories and defines and no
  options, and the build says which projects those are.  The Codaphela projects get theirs from a property sheet
  (`CommonLibraries/Common.props`), which CommonLibraries' CMake builds read too.

The HCP65 compiler and GCC compile a project's source files but those excluded from the solution configuration built
(Visual Studio's Excluded From Build, on the file or on every file of a project), as MSBuild does.

A solution can list a toolchain's configurations itself: a solution configuration whose platform is a toolchain's
(`Release|GCC-x64`) is that toolchain's build configuration, and its rows say which projects it builds, as they do
for Visual Studio's.  A solution that lists none is given each toolchain's for each of its configuration
names.

How an HCP65 configuration compiles and links is in the projects (their `HCP65Compile` and `HCP65Link` groups).
That, and the command line tool `hcp65build` that builds the same way outside the IDE, are on
[Building Programs](../compiler/building.md).

### Edit Build Configurations

Build, Edit Build Configurations... shows the startup solution's build configurations on the left, which are
its solution configurations, and the one selected on the right.  What it edits is in the solution, its projects
and the property sheets they import, each file changed only in the lines meant, so Visual Studio and git see an
ordinary edit.

Above the options are the configuration's name, its toolchain and the project whose own code's options show.
The options are its toolchain's: the HCP65 compiler's for an HCP65 configuration, g++'s for a GCC one, and
MSVC's for any other.  They are in two parts:

- **The whole program**: the startup project's options, which everything linked into the program is compiled
  with, and what the program is linked into.
- **The project's own code**: the options of the project chosen in the Project drop-down.

An HCP65 configuration's:

| Option | Meaning |
|---|---|
| No floating point | Floating point is an error (a kernal has none). |
| Maths tables in memory | Ticked to begin with.  Unticked builds it to run without the maths tables in memory (the kernal, its drivers and the operating system, which loads them). |
| Prefer size to speed | The compiler picks smaller code where it has the choice (a ROM's kernal). |
| C++ exceptions | The program is compiled with `throw`, `try` and `catch`, which are errors without it. |
| Keep unused globals that are made or destroyed | A global nothing refers to is linked, made and destroyed; without it, it is left out with its constructor and destructor. |
| A missing function stops the program, not the build | A function used but defined by no unit is a warning, and the program stops if it gets there; without it, it is an error in the Build tab, naming it. |
| Executable, Module, Image | The file the program is linked into: one of the three. |
| Entry, Limit | An executable's or a module's entry, and the address the program must end before. |
| Executable's symbols | `Name=Value`, separated by semicolons. |
| Image's size, fill, load address, text address, regions | An image's; its regions are `Start-End@Offset`, separated by semicolons. |
| Built for the debugger | Every variable is kept where the [debugger](debugging.md) reads it, nothing is inlined and every function has a frame of its own, the rest still optimised. |
| Compiled without the optimiser | The project is built for the debugger without the optimiser at all, so stepping follows every line. |
| Warn about overrides not declared override | Ticked to begin with: a function that overrides a base class's virtual function without `override` (or `final`) is a warning in the Build tab. |
| Warn when a string literal becomes a char* | Each string literal given to a `char*` is a warning in the Build tab. |
| Accept casts like unsigned int(3) | A cast written as a type of more than one word and brackets is accepted, as the Microsoft compiler accepts it; without it, it is an error. |
| C sources: void* and numbers convert as in C | For a project written in C: a `void*` becomes any object's pointer and a number an enum's value with no cast. |
| Defines, Additional include directories | The project's defines and include directories, separated by semicolons.  They add up, as g++'s do (below). |

A GCC configuration's:

| Option | Meaning |
|---|---|
| Libraries linked | What the program is linked with besides the solution's libraries, separated by semicolons: a plain name is `-l<name>` (`X11` is `-lX11`). |
| Linker options | g++'s options when it links, separated by spaces. |
| Defines | The project's defines, separated by semicolons. |
| Additional include directories | The project's include directories, separated by semicolons, besides its `IncludePath`. |
| Compiler options | g++'s options when it compiles the project, separated by spaces. |

These add up.  Each place has a line of its own, and a line that names the setting itself
(`%(AdditionalOptions) -g`) adds to what the less particular places say, so the option is all of them: the line
under the field, **In all**, says what they come to.  The field shows one place's line and the drop-down chooses
whose, moving nothing.  It starts on the project's own line, so what is typed is the project's: a sheet's line,
which every project that imports it has, is changed only by choosing the sheet first.  A line typed for the project is written so that it adds (its options after the sheet's,
where one can undo one of theirs); a field emptied takes that place's line away.  A sheet has a line for the
platform and may have one for the configuration too: each is its own place in the drop-down, and changing either
changes it for every project that imports the sheet.

An MSVC configuration's are the compiler and linker settings the projects and their property sheets set, under
the names Visual Studio's property pages give their values.  Most have values to choose from; **(not set)** is one
of them, and choosing it takes the setting's line away where its value was from.  The lists (the defines, the
include and library directories, the libraries, the warnings turned off) and the other options add up as g++'s
do.  The page is in sections, each under a heading that folds it away and opens it again:

| Section | Settings |
|---|---|
| Link | The program's: subsystem, libraries linked, library directories, default libraries ignored, debug information, link time code generation, leaving out what nothing refers to, folding identical functions, addresses above 2 GB, safe exception handlers, target machine, other linker options. |
| General | Defines, additional include directories, debug information, compiling on every processor, precompiled header, browse information, other compiler options. |
| Optimisation | Optimisation, inlining, intrinsic functions, favouring size or speed, whole program optimisation, parallel code generation. |
| Code generation | Runtime library, C++ exceptions, calling convention, instruction set, floating point and its exceptions, pooling string literals, a section for each function, leaving out what nothing refers to, OpenMP. |
| Checks and warnings | Runtime checks, buffer security check, SDL checks, EH continuation metadata, warning level, warnings as errors, warnings turned off. |

A setting the dialog doesn't show is left as it is in the files.  Not shown yet: the project's `IncludePath` and
its other properties (its type, character set and toolset), and the settings of Visual Studio's property pages
that no project here sets.

Beside each option is a drop-down that says where its value comes from:

| Comes from | Meaning |
|---|---|
| This project, Release only | A line of the project's own for the one configuration. |
| This project, every HCP65C816 configuration | A line of the project's own for the platform: Debug's too. |
| Shared.props, 14 projects | A property sheet the project imports, with how many of the solution's projects share it. |
| The program's (Kernal) | Nothing of the project's says it: its own code is compiled as the startup project's is. |
| Default | Nothing says it. |

Changing an option's value writes it where the drop-down says, so a sheet's value changes for every project that
takes it.  The program's and the default are no line to change: the value then becomes the project's own, for the
platform.  Choosing another place moves the value: one of the project's own makes it the project's, and choosing
the sheet, the program or the default again takes the project's lines away.  A sheet that doesn't say an option
isn't offered for it: add it to the sheet's file.

**Add** copies the configuration selected under a new name, on the same toolchain; **Remove** takes one away, and
the name renames it.  All three change the solution's rows only: a new configuration builds each project as the
one it copies does, and the projects keep the configurations they have.  A project is given a configuration of
its own, a copy of the one it was built as, when one of its options is first made to differ for the new one alone;
the note under the options says so before OK does it, and the status line after.

**Clean Up** saves as OK does and then lists the projects' configurations that no open solution builds, which
Remove and renaming leave behind, each a box to tick, and removes those ticked.  None is ticked to begin with: a
solution that isn't open may build one.

It works on a copy, so Cancel changes nothing, and OK isn't allowed while a name is empty, used twice or has a `|`
in it, or the linking couldn't be read by a build (two files named to link, a number that isn't one).  How a
program is debugged (its card, what is loaded with it) is a [run configuration's](#run-configurations).

## Building

Build, Build (Ctrl+F9) builds every solution of the workspace in the chosen build configuration, in
order, a solution whose program another loads (a kernal) first.  The IDE goes on working while it builds.
It builds the files as they are on disk, so save first (Ctrl+Shift+S saves them all).

A GCC build leaves what is up to date.  A source is compiled again only if it, a header it includes or the
command line it is compiled with has changed since its object was made; a library is archived and a program
linked only if something they are made from was.  The Build tab says how much was left (`Up to date: 41 of
43.`).  Build, **Rebuild All** builds everything whatever is there.  An MSVC build is MSBuild's, which keeps its
own account of what is up to date and is told to rebuild; an HCP65 build compiles everything either way.

### The Build tab

The Build tab under the editor shows what the build writes, kept scrolled to its end, over a bar showing
how far it has got: blue while it runs, green when it has succeeded, red when it hasn't.  With more than
one solution, a line says which is being built.

Next and Previous Occurrence (Ctrl+Alt+Down and Up) step through the build's errors and warnings,
opening each in the editor.  Enter, a double click or F4 on a message's line opens it too.

Right-clicking in the tab gives Copy, Copy All (the whole output, whatever is selected), Select All, Jump
to Source (what F4 does) and Clear All, which empties the tab.  The Indexing tab has the same menu.

### The Indexing tab

The Indexing tab beside it follows the code model's index as it is built: a bar (blue while it runs,
then green, or red if some files had problems), the files it couldn't find for an `#include`, each once,
and how long it took.

### Listings in the editor

An HCP65 build writes a listing of the code each line compiled to.  After a build the editor shows a
mark in the gutter beside each line that has code; click it to show that code under the line, titled
with its listing and function ("Main.lst - _main").  A line whose code is in several places (a `for`'s
parts) shows each, with `...` between them.  Click again to hide it.  See [Listings, Profiling and
Testing](../compiler/testing.md#listings).

## Run configurations

Edit Configurations... (the last item of the toolbar's combo box, or Debug, Edit Configurations...)
shows the run configurations on the left, Add and Remove under them, and the one selected in a form:

| Field | Meaning |
|---|---|
| Name | What the toolbar shows. |
| Build configuration | The build configuration it builds and debugs ("Release&#124;HCP65C816"). |
| Tool | The program Start runs.  Empty for the default (below). |
| Working directory | The folder it runs in. |
| Arguments | Given to the tool, split at spaces. |
| Load first, At | For an HCP65 program: a file the debugger loads into the emulator first, and the address it goes at (the boot loader's ROM at `0x0`). |
| Card | For an HCP65 program: the card it runs on, chosen by name from the card files found in the solutions' folders, the three folders above each, and a `Cards` folder of any of those.  What the card file says is shown under it: its image, then each program's path on the card and the file that goes there. |
| Stored in | Where the run configuration is kept: your workspace (private) or beside one of the solutions (shared). |

**Open Card File** closes the dialog and opens the selected run configuration's card file, where its programs
are edited.

Paths may be relative to the workspace's file.  Without a tool, one that builds for the W65C816 starts the
emulator, and one that builds with GCC starts what it linked of the startup solution's startup project, in
the project's folder.  It works on copies, so Cancel changes nothing; OK isn't allowed while a name is empty or
used twice.

### Private and shared

A private run configuration is kept in the [workspace file](solutions.md#workspaces-several-solutions-at-once),
which is yours alone (Stored in: My workspace).  A shared one is a file of its own, `<Name>.hcp65run` in the `.run` folder beside a solution,
committed with the solution so everyone has it, as IntelliJ IDEA's "Store as project file" does.  It is read with
its solution, and its paths are relative to the solution's folder.

A shared run configuration says how its solution's program is debugged, so choosing one in the toolbar also makes
its solution the startup solution; making a solution the startup solution chooses its run configuration for the
configuration being built (Debug stays Debug).  A private one belongs to no solution and leaves the startup
solution as it is.  If a private one has the name of a shared one, the private one is renamed (`ModPlay
(private)`).  A private one that is nothing but a build configuration's name, as each build configuration is given
when there are no others, goes once a shared one builds that configuration.

### Debugging an HCP65 program

A run configuration for an HCP65 program also says how it is debugged.  This is
`HCP65/ModPlay/.run/ModPlay Debug.hcp65run`:

```xml
<RunConfiguration Name="ModPlay Debug" Configuration="Debug|HCP65C816" Card="..\Cards\BootCard.hcp65card">
	<Load File="..\Boot\Output\HCP65C816\Release\Boot.rom" Address="0x0"/>
</RunConfiguration>
```

| | Meaning |
|---|---|
| `Card` | The card it runs on: a card file (below). |
| `<Load File Address/>` | A file the debugger loads into the emulator first: the boot loader's ROM. |
| `BreakpointSpace` | The emulator's address space the breakpoints go in (`Kernel` or `User`).  Left out, it is worked out: `User` for an executable, `Kernel` for a module or an image. |

The dialog edits the card and one file loaded first.  A run configuration with more than one `<Load>` shows the
first and keeps them all, and `BreakpointSpace` isn't in the dialog: both are written in the file.  Moving a run
configuration with Stored in writes its paths again for where it now is, so they name the same files.

A **card file** (`BootCard.hcp65card`) says what is on one SD card image, once, for every run configuration that
debugs on it.  This is `HCP65/Cards/VIACard.hcp65card`:

```xml
<Card Image="..\HCP65-Emulator\TestHCP65Lib\TestHCP65Lib\Output\VIACard.img">
	<Program File="..\Drivers\VIABoard\Output\$(Platform)\$(Configuration)\VIA.DRV" Path="\OS\Drivers\VIA.DRV"/>
	<Program File="..\Kernal\Output\HCP65C816\Release\OS.SYS" Path="\OS\OS.SYS" Lines="Kernel"/>
	<Program File="..\VIATest\Output\$(Platform)\$(Configuration)\viatest.exe" Path="\viatest.exe"/>
</Card>
```

Each `<Program>` is a file a build makes and its path on the card's FAT volume; paths are relative to the card
file.  `$(Configuration)` and `$(Platform)` are those of the build configuration being debugged, so one line
serves Debug and Release.  When debugging starts, each solution of the workspace whose program the card lists is
built and written to the card at its path, and its line table is read as the kernal starts or loads it.  `Lines`
marks a program that runs where it is linked, the kernal: when the workspace hasn't its solution, its line table
is read from the file as it is, its breakpoints in that breakpoint space, and the Build tab says if the card's
copy isn't the one built.

## Running

Start Without Debugging (Ctrl+F5, or the toolbar's Start) runs the chosen run configuration's tool without
the debugger.  The **Run** tab shows what the tool writes and, along its bottom, that it is running (blue)
or has finished well (green) or not (red).  Stop Running (the toolbar's Stop) ends it, and it is stopped
when the IDE closes.  Right-clicking in the tab gives Copy, Copy All, Select All, Clear All, Start Without
Debugging and Stop Running.

To run and debug in the emulator, see [Debugging](debugging.md).
