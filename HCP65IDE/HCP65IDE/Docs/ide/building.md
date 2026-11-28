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
(`Release|W65C816`, `Release|65C64`, `Release|GCC`) is still read, as the name it has now, in a `.hcp65build` file, a
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
for Visual Studio's.  A solution that lists none, and has no `.hcp65build` file, is given each toolchain's for each
of its configuration names.

How an HCP65 configuration compiles and links is in the projects (their `HCP65Compile` and `HCP65Link` groups).
A solution may still have a `.hcp65build` file beside it, which lists build configurations under names of its
own and says what its projects don't.  Both, and the command line tool `hcp65build` that builds the same way
outside the IDE, are on [Building Programs](../compiler/building.md).

### Edit Build Configurations

Build, Edit Build Configurations... shows the solution's build configurations on the left and the one
selected in a form on the right:

| Field | Meaning |
|---|---|
| Name | <code>Configuration&#124;Platform</code>, which the run configurations choose it by. |
| Toolchain | W65C816, 65C64, MSVC or GCC. |
| Solution configuration | The Visual Studio configuration it builds the projects as. |
| Breakpoint space | The emulator's space breakpoints are put in ("Kernel" or "User"); empty for every space.  See [Debugging](debugging.md#breakpoint-spaces). |
| No floating point | Floating point is an error (a kernal has none). |
| Maths tables in memory | Off builds it to run without the maths tables in memory (the kernal, its drivers and the operating system, which loads them). |
| Prefer size to speed | The compiler picks smaller code where it has the choice (a ROM's kernal). |
| Warn about overrides not declared override | Ticked to begin with: a function that overrides a base class's virtual function without `override` (or `final`) is a warning in the Build tab.  Unticked writes `WarnMissingOverride="false"`. |
| C++ exceptions | Unticked to begin with.  Ticked writes `Exceptions="true"`: the program is compiled with `throw`, `try` and `catch`, which are errors without it. |
| Keep unused globals that are made or destroyed | Unticked to begin with: a global nothing refers to is left out with its constructor and destructor.  Ticked writes `KeepUnusedObjects="true"`, and such globals are linked, made and destroyed. |
| Warn when a string literal becomes a char* | Unticked to begin with.  Ticked writes `WarnStringToPointer="true"`: each string literal given to a `char*` is a warning in the Build tab. |
| Accept casts like unsigned int(3) | Unticked to begin with: a cast written as a type of more than one word and brackets is an error.  Ticked writes `MultiWordCasts="true"`, and it is accepted as the Microsoft compiler accepts it. |
| C sources: void* and numbers convert as in C | Unticked to begin with.  Ticked writes `CConversions="true"` for a project written in C: a `void*` becomes any object's pointer and a number an enum's value with no cast. |
| A missing function stops the program instead of failing the build | Unticked to begin with: a function used but defined by no unit is an error in the Build tab, naming it.  Ticked writes `StubMissing="true"`: it is a warning, and the program stops if it gets there. |
| Built for the debugger | Unticked to begin with.  Ticked writes `Debug="true"`: every variable is kept where the [debugger](debugging.md) reads it, nothing is inlined and every function has a frame of its own, the rest still optimised. |
| Projects not optimised | Projects (by name, separated by commas) built for the debugger without the optimiser, so stepping follows every line.  Written as `Unoptimised`. |
| Card | The emulator's SD card image the program is written onto. |
| Path on card | Where on the card's FAT volume (`\boot.exe`). |

It says what the configuration links (an image, an executable or a module) and what it loads with it
(a kernal), but doesn't edit those: **Open File** closes the dialog and opens the `.hcp65build` file to
edit them there.  **Add** copies the one selected, with all that is in it; **Remove** takes it away.  It
works on a copy, so Cancel changes nothing, and OK isn't allowed while a name is empty or used twice, a
card is given without a path, or an MSVC or GCC configuration has anything only an HCP65 build can do.

## Building

Build, Build (Ctrl+F9) builds every solution of the workspace in the chosen build configuration, in
order, a solution whose program another loads (a kernal) first.  The IDE goes on working while it builds.
It builds the files as they are on disk, so save first (Ctrl+Shift+S saves them all).

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
