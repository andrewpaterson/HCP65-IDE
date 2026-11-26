# The Debugger

The IDE debugs W65C816 programs in the HCP65 Emulator and 65C64 programs in the X65C64 Emulator, with Visual
Studio's Debug menu, keys and windows.  What it shows and how it is used is on [Debugging](../debugging.md).  This
page is how it is put together, from the compiler to the windows.

| Part | Where | Does |
|---|---|---|
| The compiler | CppCompilerLib | Writes debug records into the assembly: functions, frames, variables and where they are, types, inlined calls. |
| The linker | Assembler65Lib | Links them into `<program>.debug` beside the program ([`CAsm65DebugInfo`]: the line table and [`CAsm65DebugRecords`]). |
| The stack walker | Assembler65Lib | [`CAsm65StackWalker`]: the calls a stopped processor is in, from its registers, memory and records. |
| Values and watches | CppCompilerLib | [`CCPP65DebugValues`] (a value formatted, its children, an edit) and [`CCPP65DebugWatch`] (a watch expression). |
| The protocol | EmulatorDebugLib | [`CDebugClient`], [`CDebugServer`], [`CDebugTarget`]: the debugger and an emulator, over shared memory or the network. |
| The emulators | HCP65 | [`CHCP65DebugTarget`] in the HCP65 Emulator; [`CFlat65C64DebugTarget`] in the X65C64 Emulator. |
| The IDE | HCP65IDE | [`CIDEDebugging`] (the Debug menu), [`CIDEDebugger`] (the client's owner), [`CIDEDebugStop`] (a stop), [`CIDEDebugWindows`] and [`CIDEValuesPane`] (the windows). |

The walker, values and watches are libraries, not the IDE's, so the compiler's tests use the same code: a compiled
program is run on the fast emulator, stopped, walked and its variables read with nothing of the IDE's.

## The emulator and the protocol

The IDE knows nothing of a processor.  The emulator says what it is in its welcome: its registers (with their
flags' letters), how its addresses are written, whether it decodes instructions, has processes, loads modules or
takes data breakpoints (`DEBUG_CAN_*`); and it disassembles its own code ([`EDebugMessage::DM_Disassemble`]).  The
Debug tab ([`CIDEDebugView`]) shows what it was told.  So one debugger serves both processors; the only processor
knowledge on the IDE's side is what the debug records carry.

[`EmulatorDebugLib/DebugProtocol.h`] (version 8) is a request and reply protocol: each request has an ID, and a
request that moves the processor (continue, step, step range, step out, reset) is always answered by a stop or
running message with its ID, so the debugger knows what the processor is doing without guessing.  The IDE starts
the emulator itself on a shared memory channel named for it, the emulator holding before its first instruction
(`--debug-shm`, `--debug-wait`, from [`EmulatorDebugLib/DebugLaunch.h`]) until the IDE has loaded what it loads and
given the breakpoints; Attach connects to one started with `--debug-tcp`.  The emulator's end, the channel and the
server, is [`CEmulatorDebugger`], shared by both emulators.

A target with a kernel and user mode has breakpoint **spaces** ("Kernel", "User"); one that runs processes tells of
each started and ended, stopping before a new one runs so its breakpoints can be given first; one that loads
modules (the kernal's drivers) tells where each went.  Memory is read in a space too
([`CDebugTarget::PeekSpaceMemory`]): a stop in a kernal call reads the calling process's stack in user mode's
space, as that process sees it ([`SDebugStopped::uiUserProcess`]).

Memory is read through a page cache ([`CDebugMemoryCache`]): [`CDebugClient::ReadMemory`] answers from the pages
read since the last stop and is false for the rest, and [`CDebugClient::FetchMemory`] asks for every page anything
missed in one request.  The Call Stack, Locals, Autos and the Watches each read without waiting on the others, a
page is read once a stop, and whoever missed something is shown again when its reply comes.  This replaced replies
kept by request ID (2026-11-24, protocol 7): the IDE looks at the debugger every 20 ms (`IDE_DEBUG_POLL`) while the
emulator answers about every millisecond, so a walk that waited for each frame's bytes in turn would have cost a
tick a frame.

## Breakpoints and line tables

[`CIDEDebugger`] keeps the breakpoints, by address and by source line, whether or not it is debugging, and gives
them all to the emulator whenever one changes.  A source breakpoint is bound through the line tables of up to eight
programs ([`SIDESourceBreakpoint`], `IDE_DEBUG_LINE_TABLES`): to each place its line's code starts, or the next
line with code.  Whose a table's breakpoints are says where they go ([`SIDELineTableTarget`]):

| The table's program | Its breakpoints |
|---|---|
| The configuration's own | In the configuration's breakpoint space; only in its process if the kernal runs it as one. |
| Loaded with it (a kernal) | Every space and process. |
| Another solution's program the kernal runs ([`CIDEDebugger::AddProcessLineTable`]) | In its space, in its process only, unbound while it isn't running. |
| A driver the kernal loads ([`CIDEDebugger::AddModuleLineTable`]) | Moved to where the module was loaded; unbound until it is. |

At a stop the table closest to where it stopped wins ([`CIDEDebugger::GetTableRank`]: the process, then the space,
then every space), since programs in different processes share addresses.  The kernal is loaded by the boot loader
from the card, not by the IDE, so its table is only right if the card's `OS.SYS` is the one built:
[`CIDEDebugging::CheckCardImages`] compares them and says so in the Build tab, and debugging goes on either way.
Source breakpoints and watches are saved in the workspace's `.hcp65user` ([`CIDEUserFile`]); data breakpoints
aren't.

## Stepping

Instruction steps are the emulator's: it decodes calls, so Step Over and Step Out need nothing from the IDE.  A
line step ([`CIDEDebugger::StepLine`]) is one step range ([`EDebugMessage::DM_StepRange`]) over the line's code in
every place it is.  [`CDebugServer::StepRange`] steps over calls by counting calls and returns, as Step Out does
([`CDebugServer::StepOutStep`]), not by the stack's depth: a line's ranges include every place its code is, so in a
recursion the depth alone took the deeper call's run of the same line for the line's end (fixed 2026-11-24).

Step Into from a line goes on by itself, up to 64 steps, until it is on a new line ([`CIDEDebugger::StepOnInto`]):
a function's entry code before its first line is stepped over, and a function with no lines (a runtime routine) is
stepped out of.

Inlined calls are steps of their own.  Step Over adds the spans of the calls inlined at the line to the range
([`CIDEDebugger::AddInlinedCalls`]); Step Out of an inlined call is a step range over its spans, since the
emulator's call counting can't see it.  A line step that stops at the first instruction of an inlined call it
didn't start in is **at the call** ([`CIDEDebugger::FindAtCall`]): none of the call has run, so the windows and the
editor show the call's line in its caller, as Visual Studio does ([`CIDEDebugStop::HideAtCall`] takes its rows out
of the walk).  Step Into from there enters it without running anything; Step Over steps over its spans.

## A stop: walking the stack

At each stop [`CIDEDebugStop`] walks the stack, works out each call's line, and makes the values each window shows,
all read through [`CIDEDebugMemory`], the client's page cache as the walker's memory.  When memory it missed comes,
it walks and shows again ([`CIDEDebugStop::Update`]).

- **The registers** are matched to the records' by name ([`CIDEDebugger::GetStackRegisters`]), so the walker needs
  nothing of the emulator's numbering.
- **The walker** is the one for the processor the emulator says it is ([`Asm65NewStackWalker`]), made when it first
  walks one ([`CIDEDebugStop::UseWalker`]).
- **The records** of every program running there are given to it, each in the memory it runs in
  ([`CIDEDebugger::AddStackRecords`]): a kernal's in kernel mode's, the process's in user mode's.  So a walk from a
  kernal call goes on through "[Kernal call]" into the program that made it.
- At most 64 calls (`IDE_DEBUG_MOST_FRAMES`); code with no records ends the walk as "[External Code]".

### The frame model

A call's **frame address** is S as its caller had it just before the call: the return address is just below it, and
it doesn't move while the call runs.  So everything about a call, its variables in memory, the registers it saved
for its caller, its return address, is a fixed distance from it, whatever the code does with its registers.  What a
function's records say is how to find the frame address once its body runs: a register plus an offset (V after
`#!text cpy V, S`, D after `#!text tcd`, S itself), the address held at a register plus an offset (the W65C816's
aligned frame, S kept in the frame), or "follow S" (frameless code: the walker follows the instructions from the
function's start).  In a prologue or epilogue the walker always follows the instructions.  The directives are in
[Linker and Outputs](../../compiler/linker.md#debug-info).

One walker interprets the rules for both processors.  [`CAsm65StackWalker`] has the records, the walk, the rules,
the saved registers and the return; a processor's part ([`CAsmW65C816StackWalker`], [`CAsm65C64StackWalker`]) gives
only what the rules can't: how to follow its instructions, how its calls push their return, its address widths.
The W65C816's frame kinds were rewritten as rules (records version 4) with every walk unchanged frame for frame.

The kernal's hand written entry code describes itself in the same directives ([`Kernal/Start.asm`]'s COP and IRQ
stubs, [`Kernal/KernalInterrupts.asm`]'s dispatch, [`UserLib/UserCalls.asm`]): a kernel's interrupt from user mode
has the process's S and D saved on the kernal's stack, which the stub pushes before it calls into C++.  Stopped in
the few instructions where a stub has moved S but not yet pushed them, the walk ends at the stub.

An inlined call is a row of its own above the function it was inlined into ([`CAsm65StackWalker::Walk`] with
inlines), with its own variables, its line the innermost code's and its caller's row at the call.

### Checking walks

A wrong walk is the worst a debugger can do, so the walker has an oracle: [`CAsm65FrameCheck`] (and
[`CAsmW65C816FrameCheck`], [`CAsm65C64FrameCheck`]) follows a running program's calls a step at a time, and
[`CAsm65FrameCheck::RunWalking`] walks before every step and compares.  Every W65C816 and 65C64 test program built
with debug records is walked at every step in the normal runs of TestCppCompilerLib; those that compare generated
code or cycle counts build without records.  Turned on for all of them (2026-11-24), the suite went from about 75 s
to 84 s (12 jobs), and it found two W65C816 bugs the first day.  TestCPP65Debug's every-step check
([`TestCppCompilerLib/TestCPP65Debug.cpp`]) also reads each described variable at every step of a Release run and
checks the value is one it held in the Debug run.

## Values, watches and the windows

[`CCPP65DebugValues`] formats a value as Visual Studio's Locals do, lists its children (bases, fields, elements,
what a pointer points at), finds an object's dynamic class from its vtable's type information (`CShape* {CSquare}`,
no records needed), and turns typed text into a write ([`SCPP65DebugWrite`]: memory in a space, or a register).  It
lives in CppCompilerLib because the vtable layout and the records' reader are there.  The processor's address width
and `int` size come from the records, which say which processor they are for.

[`CCPP65DebugWatch`] evaluates watch expressions with a small grammar of its own, not [`CCPPParser`]: the parser
needs a unit's declarations to tell a cast from an expression, and its type checker has the host's sizes.  It binds
names as it goes: locals in scope at the line, `this`'s members, statics, globals, enumerators.  A type an
expression needs that no unit described (a `bool`, a pointer to a class) is added to the records
([`CAsm65DebugRecords::FindTypeIndex`]).  Autos' rule and the Call Stack's text are its too
([`CCPP65DebugWatch::Autos`], [`CCPP65DebugValues::AppendCall`]).

The windows ([`CIDEDebugWindows`]) are docked together beside the editors on the right, not with the Debug tab, and
hidden until shown; Call Stack and Locals are shown when debugging starts.  Locals, Autos, Watch 1 to 4, QuickWatch
and a data tip are each a [`CIDEValuesPane`], one tree of values on WindowLib's [`CTreeTable`] (Name | Value |
Type):

- children are read when a value is opened (until then one empty child, so its chevron shows);
- which values are open, and which is selected, are kept by path ("cSquare/CShape") from one showing to the next;
- a value is red when it differs from what the same path showed at the stop before;
- a written value is read back, not assumed: every window shows again after a write.

A variable in a register is written to the register ([`CIDEDebugger::WriteStackRegister`]), in the innermost call
only: a caller's register is wherever its callee saved it, or nowhere.  A data tip shows after the pointer has
rested on a name for 500 ms.

## What the compiler records

Debugging needs the compiler's help: by default it packs temporaries, promotes locals out of memory and inlines,
and nothing tied a variable to where it was.  A build configuration's `Debug="true"` and `Unoptimised` (see
[Building Programs](../../compiler/building.md)) choose between three builds:

| Build | The optimiser | Variables | Inlining |
|---|---|---|---|
| Debug ([`CCPPCompileOptions::mbDebuggable`]) | On | Each named variable and parameter in a frame slot of its own that always holds its value | None; frames keep their shape |
| Unoptimised (per project) | Off | As Debug; every line steps as written | None |
| Release | On | Where the optimiser left them: in memory, in a register, or a constant, over address ranges | Inlined calls are rows of their own |

Debug is a middle mode, as GCC's `-Og`, because an unoptimised kernal doesn't fit.  Measured 2026-11-23
(Release|W65C816 builds of HCP65 at the time): the kernal's `OS.SYS` is 186,537 bytes optimised, ending at $03DBDB
of its region's $040000; with no optimiser it grows 18% and runs past the region, and so does the boot loader ROM.
ModPlay optimised needs 8.9 M cycles a second of audio; unoptimised, 34.6 M, more than the board's 25 MHz.  The
middle mode (optimiser on, slots kept, no inliner) made the kernal 4% bigger, 2.3 KB to spare, and ModPlay 22.1 M
cycles.

Release describes what it can without changing a byte of code.  The optimiser leaves a debug value marker where
promotion takes a variable out of its slot ([`SIRDebugValue`], kept beside a block's instructions, not among them:
[IR and Optimiser Design](../../compiler/design/ir.md)); [`CIRDebugPlaces`] works out which value each variable
holds before each instruction, live values only; each back end says where that value is (a register, a pseudo
register, the frame, or a constant) and the front end writes it as address ranges.  Outside its ranges a variable
is "(optimised away)".

Two rules hold for all of it:

- **Debug information never changes code.**  [`CCPPBuild::SetCheckDebugCode`] (`TEST_CPP65_SAME_CODE=1` on
  TestCppCompilerLib) compiles everything with and without it and compares the code.
- **Never show a wrong value.**  A slot is described only while it is the variable's own; a register only while the
  value is live in it; a caller's register only if its callee saved it.  Anything else is "(optimised away)".

The markers' cost was measured before more was built on them (2026-11-24, `hcp65build` Release x64 from clean
copies, the kernal built Release|W65C816, callgrind): 27% more instructions to compile the kernal, mostly the place
markers taken out and put back as source markers are, and a place string made for every variable before every
instruction.  One marker pass for both kinds and places made only where a value changes brought it to 8.0% (483.2 G
instructions against 447.4 G; the clock 6.5%), and the debug file 22% bigger.

## Data breakpoints

The simulators watch memory with [`CDataWatches`] (a read set and a write set of [`CBreakpoints`] bitmaps, any
number of addresses).  The W65C816's reads and writes go through bank tables that leave out the banks a watch is
in, and the 65C64's fast paths have limits that drop to zero while a kind is watched; instruction fetches have
their own paths and are never watched.  So a run with no data breakpoints costs nothing: measured 2026-11-24
(Release x64, a load and store loop, three runs) at 170 to 178 M instructions a second on the W65C816 and 107 to
111 on the 65C64, before and after, and no separate run loop was needed.

The server keeps the data breakpoints and fills each space's watches from them, adding each byte's alias in another
space ([`CDebugTarget::GetWatchAlias`]: the HCP65's process memory as kernel mode sees it through the remap), so
the kernal writing into a process's buffer stops too.  A watched access that isn't one of its breakpoints' (another
process's) only ends the run early.  The IDE ([`SIDEDataBreakpoint`]) keeps each one's type and last bytes to say
what changed.

## The emulators

The **HCP65 Emulator** is the HCP65's W65C816, memory map, kernal and devices ([`CHCP65DebugTarget`]; see [The
HCP65 emulator](../../hcp65/emulator.md)).  `HCP65EmulatorRef`, built beside it on Linux, is the same emulator
drawn into memory with no display or sound, for the IDE's checks ([`HCP65Emulator/HCP65EmulatorRef.cpp`]).

The **X65C64 Emulator** ([`X65C64Emulator/X65C64Emulator.h`]) is a console program: a 65C64 on [`CAsm65Memory`]'s
flat 2MB with its console and host file device, what the 65C64's compiler tests run programs on, through
[`CFlat65C64DebugTarget`].  There is no 65C64 computer to emulate, so it has no kernal, processes, interrupts or
timers.  Console input is a register the program writes to take the next byte ([`CAsm65Memory::SetConsoleInput`]),
so a debugger reading memory changes nothing; the IDE starts it with no input, so [`stdin`] reads end of file.  A
65C64 program lists Libc65's [`Libc65/Start65C64.asm`] as a source, as a W65C816 program lists
[`UserLib/UserStart.asm`]; it is the compiler's own start up, and a test keeps the two the same.

Each toolchain has a default emulator ([`CIDEDebugging::SetDefaultEmulator`]): the HCP65 Emulator for W65C816
configurations, the X65C64 Emulator for 65C64 ones, from the shared build folders on Linux.  `HCP65_IDE_EMULATOR`
and `HCP65_IDE_65C64_EMULATOR` name others (the checks' reference emulator); they don't make a run scripted.

A program that calls a function nothing defines fails to build ([`CHCP65Build::CheckMissing`]): the linker would
otherwise stub it with a stop, and the program would build, run and stop with no reason given.  A configuration
that wants the stubs says `StubMissing="true"`.  A module's (a driver's) missing names are its imports, which the
kernal's loader resolves, so modules aren't checked.

## Tests

| Test | Checks |
|---|---|
| TestAssembler65Lib | The directives linked and read back; the walker on hand written frames of each shape, every step ([`TestAssembler65Lib/TestAsm65StackWalker.cpp`] and its 65C64, interrupt and inline variants). |
| TestCppCompilerLib ([`TestCppCompilerLib/TestCPP65Debug.cpp`]) | On both processors, Release, Debug and unoptimised: frames, the stack in a recursion, values, edits, watches, the windows' text, Release variables, inlined rows, constants, every value Release shows one the Debug run held. |
| TestEmulatorDebugLib | The protocol on both flat targets: memory pages, steps, step ranges in a recursion, data breakpoints. |
| TestMicroprocessorLib | Each simulator's data watches. |
| TestHCP65Lib | The kernal's Walk (each of a program's kernal calls walked up to `main`), data breakpoints in kernel and user space. |
| The IDE's checks | The `debug-*` checks, written once and run on both processors ([The IDE Checks](checks.md)). |

## Decisions

- Visual Studio's windows, not IntelliJ's single Debug window: separate Call Stack, Locals, Autos and Watch 1-4,
  Name | Value | Type, values edited in place, as the Debug menu and keys already were (the user, 2026-11-23).
- No function calls in watches: a call runs the program inside a stop, can hit a breakpoint, change memory or never
  return, and the kernal's single COP save area makes it unsafe there (the user, 2026-11-23; asked again with F in
  2026-11-24 and left unbuilt).
- The W65C816 first, the 65C64 after; data tips as a late step; the call stack through the kernal's COP and IRQ
  stubs in the first version, since the kernal and a program are debugged together (the user, 2026-11-23).
- Debug builds: first unoptimised, then, once the kernal and boot loader were measured not to fit and ModPlay not
  to keep up, re-decided as "Middle + per-project -O0": the optimiser on with every named variable kept in its
  slot, and a per-project tick for no optimisation at all (the user, 2026-11-23).
- The file is `<program>.debug`, replacing `.lines`; values can be edited; format suffixes and Hexadecimal Display
  as Visual Studio's; watches kept per workspace beside the breakpoints (my choices of 2026-11-23, not asked).
- The frame model for both processors, the W65C816's records rewritten in it with its walks unchanged ("That sounds
  good, move both to it please", 2026-11-24).  It came from the user's correction: "Reconsider this plan in terms
  of release builds (you're focusing on V as the only frame again, which it needn't be)".  A frame is never
  described by V or D alone.
- Release variables on both processors, and every register a function saves described, not only those a walk needs
  (the user, 2026-11-24).
- One walker base interpreting the rules, a derived class per processor, not composition ("Those choices are fine",
  2026-11-24).
- The 65C64 is debugged on a flat 65C64 console emulator in the HCP65 repository, X65C64-Emulator; the same day
  HCP65/W65C64 became HCP65/X65C64-Proposal and Kicad/W65C64 Kicad/X65C64, "to keep the 65C64 near the end
  alphabetically" (the user, 2026-11-24).
- A 65C64 program lists its start up, Libc65's `Start65C64.asm`, as a W65C816 program lists `UserStart.asm`;
  BuildLib adds nothing (the user, 2026-11-24).
- Release describes a parameter where it was passed in memory, which needs no copy ("describe parameters that were
  already passed in memory, which needs no copy", 2026-11-24).
- `this` in Release left to the markers rather than a copy that costs Release code ("Leave it until F1",
  2026-11-24); the markers then gave it.
- The every-step walk check on for every test program that doesn't compare generated code, both processors (the
  user, 2026-11-24).
- A missing function is a build error; `StubMissing="true"` for a configuration that wants stubs ("Your
  recommendation", 2026-11-24).
- No console input in the IDE for now ("none for now", 2026-11-24).
- After the first version: data breakpoints, then optimised code's variables, then inlined frames ("F5, F1, F2
  (Recommended)", 2026-11-24); calls in watches not built.
- Optimised code's variables as location ranges from debug value markers ("Location ranges (Recommended)"), with
  full markers in the IR rather than a single-value subset ("We're going to do full markers"), 2026-11-24.
- A caller's variable in a register its callee didn't save shows "(optimised away)", the same text as a variable
  that is nowhere: no new wording and no guessing (the user, 2026-11-24).
- A Call Stack row per inlined level, "[inlined]", each selectable as a call ("A row per inlined level
  (Recommended)", 2026-11-24).
- Data breakpoints stop on writes, with reads as an option ("Writes and an option for reads."), cost nothing while
  none is set, watch the processor's accesses only ("CPU writes only"), and one on a local stays set until removed
  or debugging stops ("Stays until removed (Recommended)"), all 2026-11-24.
- The values the optimiser knows (constants, addresses in the frame) are shown, as Visual Studio does; Step Into at
  an inlined call enters it running nothing; the markers' cost measured, then cut, before more was built on them
  (the user, 2026-11-24).
- Debug information never changes code, and a value is never shown where it might be wrong (the rules of the plan
  for the later items, 2026-11-24).

The plans this was built from were `DebuggerPlan.txt` (last changed in HCP65-IDE f8523d8), `Debugger65C64Plan.txt`
(04f4dfe), `DebuggerGapsPlan.txt` (e4c9499), `DebuggerFPlan.txt` (8788ce5) and `DebuggerFixesPlan.txt` (e874819),
in `HCP65IDE/HCP65IDE`.

## Not done

- Function calls in watch expressions (decided against, 2026-11-23).
- Writes that aren't the processor's: the SD card's DMA, the blitter, the X65C64 Emulator's drive and host file
  commands don't stop a data breakpoint (decided, 2026-11-24).
- Dragging a selection into a Watch (Add Watch and QuickWatch take the selection instead); the Call Stack has no
  Copy and no Show External Code, and no keys (Visual Studio's Control+Alt+C is Introduce Constant here).
- A caller's variable kept in a 65C64 pseudo register unit its callee saved: the walker skips memory saves, and the
  C++ back end keeps no units across calls yet, so no compiled program has one (TestIR65Backend covers the
  records).
- A walker test on a made-up processor: the two processors' tests exercise every part of the base.
- Console input for a program the IDE starts; interrupts, timers, a kernal and processes on the X65C64 Emulator,
  which wait for a 65C64 computer.
- The time from a stop to Locals filled was worked out from the code, not measured.
- `HCP65EmulatorRef` has only a Linux CMake build: the IDE's debugging checks expect a Visual Studio one on Windows
  that no project builds.
