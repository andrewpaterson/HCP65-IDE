# Building and Running the IDE

The IDE is built from source, on Linux with CMake or on Windows with Visual Studio.  It needs three repositories
checked out side by side in one folder, as their paths to each other expect:

```text
<folder>/
    CommonLibraries/
    HCP65/
    HCP65-IDE/
```

The commands below are run from that folder unless they say otherwise.

## Linux

Install a C++ compiler, CMake, and MinGW's headers.  The IDE's code index reads Windows code through MinGW's
headers, which stand in for the Windows SDK's on Linux:

```bash
sudo apt install g++ cmake g++-mingw-w64-x86-64-win32 mingw-w64-common
```

Build it as Release:

```bash
cmake -S HCP65-IDE/HCP65IDE -B build/HCP65IDERelease -DCMAKE_BUILD_TYPE=Release
cmake --build build/HCP65IDERelease -j16
```

That builds the IDE, `HCP65IDE`, and its command line tools, [`hcp65build`](../compiler/building.md) and
`hcp65index`, in `build/HCP65IDERelease`.  On Linux the IDE is 64 bit only.

Run it from its source folder, `HCP65-IDE/HCP65IDE/HCP65IDE`.  It finds its fonts (`Resources/`), the HCP65's C
library and the emulators by paths relative to that folder:

```bash
cd HCP65-IDE/HCP65IDE/HCP65IDE
../../../build/HCP65IDERelease/HCP65IDE
```

Without MinGW's headers almost nothing parses: Go to Source, Find Usages and Rename are dim or find nothing, and
when a solution opens the status line says "MinGW's headers weren't found, so code navigation won't work".

## Windows

Install Visual Studio 2019 or later with the **Desktop development with C++** workload, which brings the MSVC
compiler and the Windows SDK.  The projects use the 2019 toolset (v142); a later Visual Studio offers to retarget
them.  The code index uses Visual Studio's and the Windows SDK's own headers, so nothing else is needed.

Open `HCP65-IDE\HCP65IDE\HCP65IDE.sln`, choose the **Release** configuration and the **MSVC-x64** platform, and
build the solution.  The IDE is `HCP65-IDE\HCP65IDE\x64\Release\HCP65IDE.exe`, with `hcp65build.exe` and
`hcp65index.exe` beside it.

Start it from Visual Studio (Ctrl+F5), which runs it in the project's folder, `HCP65-IDE\HCP65IDE\HCP65IDE`, or
from a command prompt in that folder:

```text
cd HCP65-IDE\HCP65IDE\HCP65IDE
..\x64\Release\HCP65IDE.exe
```

## Opening a solution

With no argument the IDE opens the solution that was open when it was last closed.  A `.sln`, `.vcxproj` or
`.hcp65workspace` argument opens that instead, and File opens another (see [Solutions and
Workspaces](solutions.md)):

```bash
../../../build/HCP65IDERelease/HCP65IDE ../../../HCP65/VIATest/HCP65VIATest.sln
```

## Building and running W65C816 programs

The IDE has the HCP65 C++ compiler built in, so building a program for the HCP65's W65C816 needs nothing more
than the IDE.  It finds the compiler's C library, Libc65, in `HCP65/Libraries/Libc65`.

1. Open the program's solution, for example `HCP65/VIATest/HCP65VIATest.sln`.
2. Choose a run configuration that builds for the W65C816 in the toolbar's combo box: "VIATest Release" builds
   `Release|HCP65C816`.
3. Build (Ctrl+F9).  The Build tab shows the compiler's messages, and the program goes in
   `Output/HCP65C816/Release/` beside the solution, its listings in `Listing/`.

`hcp65build`, built with the IDE, builds the same solution the same way from the command line:

```bash
cd HCP65/VIATest
../../build/HCP65IDERelease/hcp65build HCP65VIATest.sln -config "Release|HCP65C816"
```

A W65C816 program runs in the HCP65 Emulator (`HCP65/HCP65-Emulator`), which the IDE starts itself.  Build it
as the IDE is built, as Release, where the IDE looks for it:

- Linux: `cmake -S HCP65/HCP65-Emulator/HCP65Emulator -B build/HCP65Emulator -DCMAKE_BUILD_TYPE=Release`,
  then `cmake --build build/HCP65Emulator -j16`.
- Windows: `HCP65\HCP65-Emulator\HCP65Emulator.sln`, Release and MSVC-x64.

A program for the kernal runs from an SD card image, which the boot loader boots from.  Before it is first run:

- Build the boot loader's ROM, which the run configuration loads first:
  `hcp65build HCP65/Boot/HCP65Boot.sln -config "Release|HCP65C816"`.
- Build the kernal, which the card holds as `\OS\OS.SYS`: `hcp65build HCP65/Kernal/HCP65Kernal.sln -config
  "Release|HCP65C816"`.
- Make the card images by running the HCP65's tests once (`HCP65/HCP65-Emulator/TestHCP65Lib`): they write
  `BootCard.img`, `VIACard.img` and the others to their `Output` folder, where the card files expect them.

Then build the program (Ctrl+F9) and Start Debugging (F5): it writes the program onto the card, starts the emulator
and runs it.  Start Without Debugging (Ctrl+F5) runs it without the debugger.  How a run configuration chooses the card and what is
loaded is in [Building and Running](building.md#debugging-an-hcp65-program); the debugger itself is in
[Debugging](debugging.md).
