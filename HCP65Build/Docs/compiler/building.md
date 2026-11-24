# Building Programs

`hcp65build` compiles and links HCP65 programs from the command line, the same way the HCP65
IDE does.  It builds in one of two ways:

- **From a solution.**  A Visual Studio `.sln` or `.vcxproj` says which files and include
  directories each project has, and an `.hcp65build` file beside it says how to build them for
  the HCP65.  The same solution still builds natively in Visual Studio.
- **From a plain command line.**  The sources, include directories and output are all given as
  arguments, with no solution.

It is built with the HCP65 IDE (`HCP65-IDE/HCP65Build`).  On Linux it's `hcp65build`; on Windows
`hcp65build.exe`.

## Building a solution

```text
hcp65build <file.sln | file.vcxproj | file.hcp65build | file.hcp65workspace> [options]
```

An `.hcp65build` file builds the `.sln` of the same name beside it, or the `.vcxproj` if there is
no solution.

| Option | Meaning |
|---|---|
| <code>-config "Release&#124;W65C816"</code> | The configuration to build, from the `.hcp65build` file.  Default: the workspace's last one, else the first HCP65 one. |
| `-startup <project>` | The project whose program is built.  Default: the one the workspace names, else the first. |
| `-card` | Also copy each executable, module and carded image (such as `OS.SYS`) onto the configuration's SD card image (`Card`, `CardPath`). |
| `-show-plain <file>` | Write the build as a [plain command line](#a-plain-command-line) instead of building.  `hcp65build @<file>` then builds the same thing. |

The program goes in `Output/<toolchain>/<configuration>` and the listings in `Listing/`, beside
the solution.  For example, from `HCP65/ModPlay`:

```text
hcp65build HCP65ModPlay.hcp65build -config "Release|W65C816"
```

### The .hcp65build file

An `.hcp65build` file is XML: a list of configurations, each naming a toolchain, the Visual Studio
configuration whose files and settings it builds, and what to link.  This is
`HCP65/ModPlay/HCP65ModPlay.hcp65build`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<BuildConfigurations>
	<Configuration Name="Release|W65C816" Toolchain="W65C816" Solution="Release|Win32"
	               BreakpointSpace="User" Card="..\HCP65-Emulator\TestHCP65Lib\TestHCP65Lib\Output\BootCard.img"
	               CardPath="\modplay.exe">
		<Executable File="modplay.exe" Entry="UserStart" Limit="0x200000">
			<Symbol Name="UserHeapSize" Value="0x180000"/>
		</Executable>
		<Load File="..\Boot\Output\W65C816\Release\Boot.rom" Address="0x0"/>
		<Lines File="..\Kernal\Output\W65C816\Release\OS.SYS" CardPath="\OS\OS.SYS"/>
	</Configuration>
</BuildConfigurations>
```

| Attribute of `Configuration` | Meaning |
|---|---|
| `Name` | What the configuration is chosen by: its configuration and platform either side of a <code>&#124;</code>. |
| `Toolchain` | `W65C816` or `65C64`.  A configuration can also name a native toolchain (MSVC or GCC), which the IDE uses to build for the host. |
| `Solution` | The Visual Studio configuration whose projects, files, include directories and defines are built (<code>Debug&#124;Win32</code>). |
| `NoFloats="true"` | Floating point is an error.  The kernal is built this way. |
| `MathTables="false"` | Build to run without the maths tables in memory: the multiplies use the table-free runtime.  For the kernal, its drivers and the operating system, which run before the tables are loaded. |
| `PreferSize="true"` | Prefer smaller code to faster where the compiler has the choice. |
| `Exceptions="true"` | Compiles the program with C++ exceptions: `throw`, `try` and `catch`.  Without it they are errors, and no code pays for them; with it a program is a few percent bigger and slower.  See [Exceptions](language.md#exceptions). |
| `WarnMissingOverride="false"` | No warning for a function that overrides a base class's virtual function without being declared `override` (or `final`).  The warning is on unless a configuration says this. |
| `KeepUnusedObjects="true"` | Link a global whose making or destroying runs code (a constructor, a destructor, an initialiser that isn't a constant) though nothing refers to it, so it is made and destroyed as the standard says.  Without it such a global goes with the rest of what nothing uses. |
| `WarnStringToPointer="true"` | Warn when a string literal, which is `const`, becomes a `char*`.  C++14 has no such conversion; the compiler still makes it, as code written before is full of it, and says nothing unless a configuration asks. |
| `MultiWordCasts="true"` | Accept `unsigned int(3)`, a cast written as a type of more than one word and brackets, as the Microsoft compiler does.  Without it that is an error, as C++14 has it: write `unsigned(3)` or `(unsigned int)3`. |
| `CConversions="true"` | The sources are C: a `void*` becomes a pointer to any object and a number an enum's value with no cast, an enum is incremented, and a pointer is compared with `0` by `<` and `>`.  C++ refuses each of these. |
| `StubMissing="true"` | A function used but defined by no unit is a warning, and the program stops if it gets there.  Without it the build fails naming each one.  A module (a driver) is never checked: what it doesn't define is imported from the kernal when it is loaded. |
| `Debug="true"` | Built for the debugger: each variable, parameters too, is kept in its frame where the debugger reads it, nothing is inlined, and every function has a frame of its own.  The rest is still optimised, so the kernal still fits.  Usually set on the `Debug\|W65C816` configuration. |
| `Unoptimised="Init, Shell"` | These projects (by name, separated by commas) are built for the debugger without the optimiser at all, so stepping follows every line as it is written: for a program that fits and needn't be fast. |
| `BreakpointSpace` | The emulator's address space (`Kernel` or `User`) that the IDE's breakpoints go in. |
| `Card`, `CardPath` | An SD card image, and the path on its FAT volume, that `-card` and the IDE's debugger copy the program to. |

A configuration has at most one of these, which says what the build writes.  Without one the
units are only compiled.

| Element | Writes |
|---|---|
| `<Executable File Entry Limit>` | A program the HCP65 kernal loads and runs.  `<Symbol Name Value/>` children are absolute symbols the loader or the program reads, such as `UserHeapSize`. |
| `<Image File Size Fill Limit Load Text>` | A file of a fixed size, such as a ROM.  `<Region Start End Offset/>` children say where the code linked at each range of addresses goes in the file; unused bytes are `Fill`.  `Text` (optional) is where the code starts. |
| `<Module File Entry Limit>` | A relocatable driver the kernal can load anywhere in a bank. |

`Limit` is the highest address the linker may place anything at.  `<Load File Address/>` is a
file the IDE's debugger loads into the emulator first: the boot loader's ROM for a program to run
on.

`<Lines File Space CardPath/>` is a program whose line table the debugger reads without loading
it: the kernal, `OS.SYS`, which the boot loader loads from the card where it is linked.  Its
source breakpoints go in the breakpoint space `Space` (`Kernel` if left out), in every process,
and stepping into it shows its source.  `CardPath`, if given, is where it should be on the card:
when debugging starts, the Build tab says if the card's copy isn't the one built, since its lines
would then be wrong.

The debugger reads the same for the workspace's other solutions: a program's (an executable on a
card) when the kernal starts it, a driver's (a module on a card) when the kernal loads it, and an
image's on a card (`OS.SYS`) as it is.  A `<Lines>` that names one of those is read once.
Numbers are decimal or `0x` hexadecimal, and paths are relative to the `.hcp65build` file.

[Linker and Outputs](linker.md) describes the three kinds of file.

## A plain command line

```text
hcp65build -program <name> [-dir <dir>] [-D <define>]... <source>... [-lib <name> ...]... [options]
```

Each `-program` or `-lib` starts a project, and the sources after it are its units.  A library's
units are linked only when something uses them.  Sources can be `.cpp` or `.asm`.

| Option | Meaning |
|---|---|
| `-program <name>`, `-lib <name>` | Start a project.  Its sources follow. |
| `-dir <dir>` | The project's directory.  Its sources are relative to it (default: the current directory). |
| `-D <name>[=<value>]` | A define for the project's units. |
| `-I <dir>`, `-I+ <dir>` | An include directory; `-I+` lists all the headers in its sub directories when the build starts.  Before the first `-program` or `-lib` it's every unit's; after, that project's. |
| <code>-target W65C816&#124;65C64</code> | The processor (default `W65C816`). |
| `-nofloats` | Floating point is an error. |
| `-no-mathtables` | Build without the maths tables. |
| `-prefer-size` | Prefer smaller code to faster. |
| `-no-override-warning` | No warning for an override not declared `override`. |
| `-exceptions` | C++ exceptions: `throw`, `try` and `catch` are compiled (see [Exceptions](language.md#exceptions)). |
| `-keep-unused-objects` | Link a global that is made or destroyed by code though nothing uses it. |
| `-warn-string-to-pointer` | Warn when a string literal becomes a `char*`. |
| `-multi-word-casts` | Accept `unsigned int(3)`, a cast whose type is more than one word. |
| `-c-conversions` | The sources are C: a `void*` and a number convert as C has them. |
| `-stub-missing` | As `StubMissing="true"`: a missing function stops the program instead of failing the build. |
| `-debug` | As `Debug="true"`: built for the debugger. |
| `-unoptimised <project>` | As `Unoptimised`: the project isn't optimised.  Give it again for each project. |
| `-exe <file> -entry <symbol> [-limit <n>] [-symbol <name>=<n>]...` | Link an executable. |
| `-image <file> -size <n> [-fill <n>] [-limit <n>] [-load <n>] -region <start>,<end>,<offset>...` | Link an image. |
| `-module <file> -entry <symbol> [-limit <n>]` | Link a module (a driver). |
| `-o <dir>`, `-listings <dir>` | Where the program and the listings go (default: `Output/<target>/Plain` and `Listing/<target>/Plain`). |

Options for both ways of building:

| Option | Meaning |
|---|---|
| `-libc65 <directory>` | Libc65.  Default: the first `HCP65/Libraries/Libc65` found going up from the tool or the file. |
| `-quiet` | Leave out the progress lines. |
| `-layouts <file>` | Write each class as the build lays it out to the file (see [Class layouts](#class-layouts)). |
| `@<file>` | Read arguments from a file, one a line.  Lines starting with `#` are comments, and a file can name other `@` files. |
| `-help` | List the options. |

Numbers are decimal, `0x` or `$` hexadecimal.  The exit code is 0 when the build succeeded, 1
when it failed and 2 for a bad command line or file.

### Class layouts

`-layouts <file>` writes a line for each class the build's sources lay out: its name, where it's
defined, its size, its alignment, the most `alignas` asks of it, then where each base, the vtable
pointer and each field is.  A bit-field has its first bit after a dot.

```text
hcp65build -program Shapes Shapes.cpp -layouts Shapes.txt
```

```text
SInner Shapes.cpp(1) size 6 align 2 alignas 0 c@0 l@2
SOuter Shapes.cpp(2) size 16 align 2 alignas 0 base SInner@0 d@6 s@14
```

The lines are in order and each class is there once, so two files compare with any diff tool:
the same sources built for the 65C64 (`SInner` is 8 bytes there, with `l` at 4), or before and
after a class or the compiler changed.  A class no code needs whole isn't laid out and isn't
listed.  A build with no classes writes no file, and one of several solutions writes the last
solution's.

### An example

A program the HCP65 kernal runs needs UserLib's start up, `UserStart.asm`, and its kernal call
stubs.  Libc65's units include BaseLib's headers, so `CommonLibraries/Libraries` must be an
include directory.  From a directory beside `HCP65` and `CommonLibraries`:

```text
hcp65build -program Hello -I+ CommonLibraries/Libraries -I+ HCP65/Libraries/UserLib
           Hello.cpp HCP65/Libraries/UserLib/UserStart.asm HCP65/Libraries/UserLib/UserCalls.asm
           -exe hello.exe -entry UserStart -limit 0x200000 -symbol UserHeapSize=0x20000
```

with `Hello.cpp` a `printf("Hello\n")` in `main`, writes

```text
Process (Hello.cpp)
Process (ctype.cpp)
...
Wrote 65788 bytes to Output/W65C816/Plain/hello.exe.
Wrote its debug info to Output/W65C816/Plain/hello.exe.debug.
Plain|W65C816 build succeeded.
```

The `Process (...)` lines are each unit being preprocessed, Libc65's among them.  The easiest
way to get a working command line for a real program is to let `-show-plain` write it from a
solution.

Sources can be relative to the project's `-dir` or full paths.  An include directory given with
`-I` finds headers in its sub directories by their path (`#include "BaseLib/Chars.h"`), as a C++
compiler's does; `-I+` lists every header below it when the build starts.  An executable's or
module's entry is always kept, even when nothing in the program calls it.

Without `-exe`, `-image` or `-module` the units are compiled but not linked, which is a quick way
to see what the compiler writes for a file:

```text
hcp65build -program Sum Sum.cpp -target 65C64 -listings Listing
```

writes `Listing/Sum.lst`.  [Listings, Profiling and Testing](testing.md#listings) shows one.

## From C++

`hcp65build` and the IDE drive `CCPP65Builder`, which any program can use: the test suites do.

```cpp
CCPP65Builder	cBuilder;

cBuilder.Init();
cBuilder.SetTarget(CPPT_W65C816);				//Before anything is added.
cBuilder.AddIncludeDirectory(szLibc65, false, true);
cBuilder.AddIncludeDirectory(szCommonLibraries, true, false);
cBuilder.AddUnit("Hello", szDirectory, "Hello.cpp");
cBuilder.AddLibc65(szLibc65);					//Libc65 and the runtime, linked as libraries.
cBuilder.SetRemoveUnused(true);
...
cBuilder.Kill();
```

| Method | Does |
|---|---|
| `SetTarget` | The processor.  It picks the predefined macros, the back end, the assembler and the linker's bank size. |
| `GetOptions` | The `CCPPCompileOptions`: speed or size, the optimiser, inlining, register arguments, the assembly units. |
| `AddUnit`, `AddVisualStudioProject`, `AddProjectSources` | The units to compile.  `.asm` units are assembled. |
| `AddLibc65`, `AddRuntime`, `AddMathTables` | Libc65's units, the W65C816 runtime, and the maths tables, as libraries. |
| `SetNoFloats`, `SetMathTables`, `SetLimit`, `SetStubMissing`, `SetRemoveUnused` | As the options above. |
| `SetListingDirectory`, `SetLineTable` | Listings, and the line table a debugger reads. |
| `SetLayoutsFile` | The file each class's layout is written to (see [Class layouts](#class-layouts)). |
| `SetJobs`, `SetHeaderCache`, `SetLazyHeaders` | Compile units on several threads; share preprocessed headers between units; parse only the header code a unit uses. |
| `SetFrameProfile` | A measured profile for the W65C816's aligned frames (see [W65C816 Code](w65c816.md#frames)). |
| `GetStatistics` | What the compile counted and timed. |
