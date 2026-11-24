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
`Release|W65C816`.

| Toolchain | What builds it |
|---|---|
| W65C816 | The [HCP65 C++ compiler](../compiler/index.md), for the HCP65's own processor. |
| 65C64 | The HCP65 C++ compiler, for the [65C64](../65c64/index.md). |
| MSVC | MSBuild, as Visual Studio builds the solution configuration (Windows only; the newest Visual Studio's MSBuild, found with vswhere, or the one on the path). |
| GCC | `g++` and `ar` from the path: each project compiled into `Output\GCC\<configuration>\<project>\`, each static library archived, each application linked with all of the solution's libraries. |

A solution's build configurations are in its `.hcp65build` file, beside the `.sln`.  Its format, and the
command line tool `hcp65build` that builds the same way outside the IDE, are on [Building
Programs](../compiler/building.md).

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
opening each in the editor.

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
| Build configuration | The build configuration it builds and debugs ("Release&#124;W65C816"). |
| Tool | The program Start runs.  Empty for the default (below). |
| Working directory | The folder it runs in. |
| Arguments | Given to the tool, split at spaces. |

Paths may be relative to the workspace's file.  Without a tool, one that builds for the W65C816 starts the
emulator, and one that builds with GCC starts what it linked of the startup solution's startup project, in
the project's folder.  The run configurations are kept in the [workspace file](solutions.md#workspaces-several-solutions-at-once).
It works on copies, so Cancel changes nothing; OK isn't allowed while a name is empty or used twice.

## Running

Start Without Debugging (Ctrl+F5, or the toolbar's Start) runs the chosen run configuration's tool without
the debugger.  The **Run** tab shows what the tool writes and, along its bottom, that it is running (blue)
or has finished well (green) or not (red).  Stop Running (the toolbar's Stop) ends it, and it is stopped
when the IDE closes.

To run and debug in the emulator, see [Debugging](debugging.md).
