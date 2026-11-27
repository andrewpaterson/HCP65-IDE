# Debugging

The IDE debugs W65C816 programs in the [HCP65 emulator](../hcp65/emulator.md), and 65C64 programs in
the X65C64 Emulator (see [Debugging a 65C64 program](#debugging-a-65c64-program)), with Visual Studio's
Debug menu and keys.  It knows nothing of the processor itself: the emulator describes its registers and
addresses and disassembles its own code, so the same debugger works for the W65C816 and the 65C64.  It
runs the emulator with a shared memory channel to it, or attaches to one already running over the network.

## The Debug menu

| Item | Keys | What it does |
|---|---|---|
| Start Debugging | F5 | Starts the emulator under the debugger, running to the first breakpoint; while stopped, continues. |
| Start Without Debugging | Ctrl+F5 | [Runs](building.md#running) the run configuration's tool without the debugger. |
| Stop Debugging | Shift+F5 | Ends the emulator. |
| Stop Running | | Ends what Ctrl+F5 started. |
| Restart | Ctrl+Shift+F5 | Starts it again from the beginning. |
| Break All | Ctrl+Pause | Stops it where it is. |
| Attach to Emulator... | | Connects to an emulator already running, by the port it printed (started with `--debug-tcp`). |
| Step Into | F11 | One step, into a call (stopped at an inlined call, into it without running anything).  Not debugging, starts the emulator stopped at its first instruction. |
| Step Over | F10 | One step, over a call (and over a call the optimiser inlined there; a step that comes to an inlined call stops at the call, before any of it runs).  Not debugging, as F11. |
| Step Out | Shift+F11 | Runs until the function returns; in a call the optimiser inlined, until its code is left. |
| Toggle Breakpoint | F9 | A breakpoint on the caret's line, or at the disassembly's instruction under the caret. |
| New Breakpoint at Address... | | A breakpoint at an address typed in hexadecimal (a leading `$` is allowed). |
| New Data Breakpoint... | | A [data breakpoint](#data-breakpoints) on an expression's value or at an address. |
| Delete All Breakpoints | | Takes them all away, data breakpoints too. |
| Edit Configurations... | | The [run configurations](building.md#run-configurations). |
| Windows > | | Call Stack, Locals, Autos, Watch 1-4, and Hexadecimal Display (see [The debugger's windows](#the-debuggers-windows)). |
| QuickWatch... | Shift+F9 | The expression at the caret, in a dialog (see [QuickWatch](#quickwatch)). |

A step is one instruction when the disassembly has the keys, and one source line in a source file.

F5 debugs what was last built: build first (Ctrl+F9).

## What debugging starts

Start Debugging uses the chosen run configuration's build configuration:

1. It starts the emulator: the run configuration's tool, or the default emulator for the
   configuration's processor (the HCP65 Emulator for a W65C816 configuration, the X65C64 Emulator for a
   65C64 one).  An emulator for another processor than the configuration's is refused as soon as it
   says what it is.  MSVC and GCC configurations can't be debugged.
2. It loads each `<Load>` of the configuration (the HCP65's boot loader ROM) into the emulator at its
   address, then the configuration's own built image if it has no card, while the emulator waits at
   its first instruction.
3. A built executable, module or image with a card is written onto the configuration's SD card image
   (Card, at Path on card): a program for the kernal to run, a driver for it to load, or the kernal
   itself, `\OS\OS.SYS`, for the boot loader to load.
4. It checks the kernal on the card (see [The kernal on the card](#the-kernal-on-the-card)).
5. It resets the emulator so it starts from what was loaded, and runs.

What it did, and the emulator's own output, go in the Build tab.

## The Debug tab

The Debug tab under the editor shows the **disassembly** beside the **registers**:

- the disassembly is the instructions round the program address, `=>` at it and `*` at each breakpoint;
- the registers are the processor's state at the last stop, as the emulator describes it, and the
  breakpoints.

F9 in the disassembly toggles a breakpoint at the instruction the caret is on.  When it stops at code a
built program has a line for, that file is opened with its line marked.

## The debugger's windows

Visual Studio's Call Stack, Locals, Autos and Watch windows, docked together beside the editors on the
right.  Call Stack and Locals are shown when debugging starts; the others from Debug > Windows.  Each
shows where the emulator stopped, and is empty while it runs.  They need the program built for the
debugger (a configuration's Debug, see [Building](building.md)) to show everything.  A Release build shows
its calls and the variables it keeps in memory (objects, arrays and anything whose address is taken), the
parameters that stay where they were passed (an object passed by value, a parameter whose address is taken),
and, wherever the optimiser keeps their values, its other variables, parameters and `this`: in a register,
a pseudo register or the frame, as Visual Studio shows optimised code, and a value the optimiser knows (a
constant, the address of a variable in the frame) as that value.  A variable is `(optimised away)`, in the
Call Stack too, where it has no value kept anywhere: before its first value, after its value's last use, or
in a register a call it made didn't save.  A variable in a register can be
edited in the innermost call (the register is written), but has no address and can't be watched.

### Call Stack

A row per call the program is in, found by walking the stack from the stop through the programs running
there (the kernal's calls and interrupts too, on into the process that made them), with each parameter's
type, name and value and the line it is at:

    modplay.exe!Scale(SPoint* psPoint = 0x00eff5, int iBy = 2) Line 5

The innermost has a yellow arrow; code with no debug records (the start up) is `[External Code]`.  A call
the optimiser inlined (a Release build) has a row of its own, as Visual Studio's inline frames, above the
function it was inlined into, which is at the call's line:

    modplay.exe!Area(SPoint* ps = 0x00eff5) Line 9 [inlined]
    modplay.exe!main() Line 25

Enter or a double click on a call chooses it: its line is shown in its editor, with a green arrow in the
gutter, and Locals, Autos and the Watches show that call (an inlined call's own variables).

### Locals, Autos and Watch

Each is a tree of values, Name | Value | Type:

- **Locals**: the chosen call's parameters and locals in scope at its line, `this` first.
- **Autos**: what the line and the line before use (names and member chains such as `p->x`, not calls),
  and, just after a Step Out, what the function returned (`Sum returned`).
- **Watch 1-4**: expressions typed into the name of the empty last row (or added by the editor's Add
  Watch), changed by typing over one and removed by emptying it.  They are kept with the breakpoints in
  the workspace's `.hcp65user`.

An object, an array or a pointer opens to its members, elements or what it points at, read only when
opened.  A value is shown as its type has it: an `int` in decimal (hexadecimal with Hexadecimal Display),
a `char` as `99 'c'`, an enum by its enumerators (`EF_A | EF_C`), a `char*` with its string, an object
through a base class pointer with its own class (`CShape* {CSquare}`).  A value that changed since the
last stop is red.  Typing into a Value writes it, if what is typed fits its type.  A row's context menu
has Break When Value Changes, a [data breakpoint](#data-breakpoints) on it.

Watch expressions are evaluated with the program's types (an `int` is 16 bits on the W65C816, 32 on the
65C64): names (locals, then
`this`'s members, statics and globals, enumerators), `. -> [] * &`, casts, `sizeof`, and the arithmetic,
comparison and logical operators.  Functions aren't called and nothing is assigned.  A suffix formats the
result as Visual Studio's do: `,x` hexadecimal, `,d` decimal, `,s` a string alone, `,10` a pointer as an
array of 10.

### QuickWatch

Shift+F9 (or the editor's QuickWatch) shows the expression at the caret, or what is selected, with its
value: Reevaluate evaluates what is typed, Add Watch adds it to Watch 1.

### Data tips

The pointer resting on a name in the editor while stopped shows its value under it, opened like a Locals
row; a member's (`pcCounter->miCount`) from the name it is a member of.  It goes when the pointer moves off
the name and the tip.

## Breakpoints

F9 in a source file, or a click in the gutter's first column, toggles a breakpoint on that line.  It is
drawn as a red cell; a red `o` means it isn't **bound** yet.  A build writes debug info beside each
program (`<program>.debug`), whose line table binds a line's breakpoint to the addresses its code is at:

- in the configuration's own program, in its [breakpoint space](#breakpoint-spaces);
- in what is loaded with it (the boot loader's ROM), in every space;
- in the kernal on the card, `OS.SYS`, in its own breakpoint space and every process: from another
  solution of the workspace that builds an image onto the card, or from a `<Lines>` of the
  configuration ([Building Programs](../compiler/building.md));
- in a program of another solution of the workspace that the kernal runs from the card, in that
  program's own process, and only while it runs;
- in a driver the kernal loads, once it is loaded, wherever it was loaded.

A breakpoint waits unbound while its program hasn't been built, or isn't running.  Source breakpoints are
kept when debugging stops and saved in `<workspace>.hcp65user`, yours alone.

### Data breakpoints

A data breakpoint stops the program after an instruction writes a value, as Visual Studio's do: Break
When Value Changes on a Locals, Autos or Watch row (again on the same row, Remove Data Breakpoint), or New
Data Breakpoint... in the Debug menu, which takes an expression (`cCounter.miCount`, `gaiValues[2]`,
evaluated in the call shown) or an address (`$12:3400`, with how many bytes).  Its tick makes reads stop
too.  It watches a value of 1 to 8 bytes in memory; one in a register can't be watched.

The program stops after the instruction, which has done its write, and the Debug tab says what changed:
"Data breakpoint: cCounter.miCount changed from 0 to 3 at $01:0141".  Only the processor's own reads and
writes are watched: a device's (the SD card's DMA, the blitter, the drive and host file commands of the
X65C64 Emulator) aren't.  Instructions fetched never stop one.

Data breakpoints last until removed or until debugging stops; they aren't saved.  One on a local stays
after its function returns, watching the same bytes.  On the HCP65 a data breakpoint on a program's
value is in user mode for that program's process; the kernal writing the same memory (a buffer it was
given) stops it too.

### Breakpoint spaces

The HCP65's kernal and the programs it runs share addresses, so the emulator keeps breakpoints in
**spaces**: "Kernel" and "User".  A build configuration's Breakpoint space says which its program's
breakpoints go in; empty, they stop in every space.

### A program the kernal runs

When the configuration's program is one the kernal runs as a process (its file on the card, say
`\OS\SHELL.EXE`), its breakpoints stop only in that process.  The emulator tells the IDE as each process
starts and ends; when the program's process starts, the emulator stops before it runs so its breakpoints
can be given, and goes on.  That stop isn't shown.

### The kernal on the card

The kernal isn't loaded by the IDE: the boot loader reads it from the card.  Its line table is only
right if the card's `OS.SYS` is the one that was built, so when debugging starts the IDE compares
them (a `<Lines>` says which file on the card with its `CardPath`).  If they differ, the Build tab
says that the card's `\OS\OS.SYS` isn't the one built and its source lines may be wrong; if the
card hasn't one, that the source lines aren't of what runs.  Debugging goes on either way.
Debugging the kernal's own configuration once puts the one built on the card.

## Debugging a 65C64 program

A 65C64 program runs from reset in the X65C64 Emulator (`HCP65/X65C64-Emulator`): a 65C64 with 2MB of
memory, a console at `00FF:0100` and the host file device at `00FF:0200` (so [`printf`] and [`fopen`] work), as
the compiler's own tests run their programs.  It is a console program, with no window: what the program
prints goes in the Build tab.  The IDE gives it no input, so [`getchar`] and [`fgets`] on [`stdin`] read `EOF`.  It stops when the program does (`stp`), or when the debugger goes.

The program is an image the IDE loads at its address, and reset starts it there: the emulator's reset
vector is the load address, `$10000`.  As a W65C816 program lists `UserStart.asm`, a 65C64 project lists
Libc65's `Start65C64.asm` first: it sets the stack and the registers' widths, runs the global
initialisers, calls `main` and stops with its result in A.  A configuration for it:

```xml
<Configuration Name="Debug|HCP65C64" Toolchain="65C64" Solution="Debug|Win32" Debug="true">
	<Image File="program.bin" Size="0x100000" Load="0x10000">
		<Region Start="0x10000" End="0x110000" Offset="0x0"/>
	</Image>
</Configuration>
```

The emulator runs without the IDE too:

```text
X65C64Emulator program.bin [--load <address>] [--host <directory>]
```

It loads the file at `--load`'s address (`$10000` if not given) and runs it, its files in `--host`'s
directory (the current one if not given), and exits with `main`'s result.  The debugging arguments
(`--debug-shm`, `--debug-tcp`, `--debug-wait`, `--exit-on-detach`) are the HCP65 emulator's.

## Attaching

An emulator started with `--debug-tcp` prints the port it listens on.  Attach to Emulator... asks for it
and connects; then it is debugged as if the IDE had started it.
