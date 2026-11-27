# The IDE Checks

The IDE is tested by running it.  [`HCP65IDE/Checks/IDEChecks.py`] starts it scripted, has it open a solution,
press keys, run refactorings, answer dialogs and print what it did, and matches what it printed.  How to run them
and how a check is written is on [Checking the IDE](../checks.md).  This page is why they are built the way they
are: an IDE drawn into memory, a scripted run that takes no input from outside, fixtures of their own, and nothing
of the user's changed.

## Why the IDE is run, not unit tested

Most of what the IDE does is the meeting of several parts: the index, the editor, the dock, a dialog, the undo log,
the build, an emulator.  The libraries under it have their own tests (TestWindowLib, TestCppCompilerLib,
TestIndexLib, TestBuildLib, TestGitLib and the rest); the checks test the IDE as a whole, through what a user does
and what the IDE shows.  A check is a name, the `HCP65_IDE_*` variables that set the run up, the lines it must
print, in order, and lines it mustn't.  Matching printed lines rather than pictures keeps a check's meaning
readable and its failure explained: the last 40 lines of what the IDE printed come with it.

## HCP65IDERef: the IDE drawn into memory

The checks run on `HCP65IDERef` ([`HCP65IDE/HCP65IDERef.cpp`]): the same IDE, its window on WinRefLib's reference
screen instead of X11 or Windows (see
[Backends](../../libraries/windowlib/backends.md#the-reference-screen-winreflib)).  The three programs,
`HCP65IDE_Windows.cpp` (Windows), `HCP65IDE_Linux.cpp` (X11) and `HCP65IDERef.cpp`, only make their window factory and
native input and call the shared [`StartIDE`], [`RunIDE`] and [`StopIDE`] ([`HCP65IDE/IDEMain.cpp`]); the CMake
build makes them from one library, `HCP65IDELib`, and the Visual Studio solution has a Reference|x64 configuration
for it.  So the checks:

- need no display, and nothing the person at the machine types or clicks can reach them;
- run beside the user's own IDE, or a second run of the checks, without fighting over the focus;
- see the reference screen's input devices ([`CWinRefInput`]: the standard keyboard and mouse, never pressed), as a
  real platform has them;
- can check every present against what was drawn (`HCP65_IDE_VERIFY_PRESENT`), as well as every partial paint and
  every layout (`HCP65_IDE_VERIFY_REDRAW`, `HCP65_IDE_VERIFY_LAYOUT`), failing the check that showed one.

The reference windows wait for something to be due, as a real window does, unless `HCP65_IDE_CONTINUOUS` says to
paint continuously.  The windowed IDE isn't left untested: unless checks are named, a few smoke checks
(`NATIVE_SMOKE`, `explorer`) run on it too, and `-native` runs them all on it.  Run both ways, every check's output
was the same (2026-11-13), once an X11 window reported its creation size before it was shown: one check had found
it reporting 0 by 0 there.

## A scripted run

Any variable in the IDE's list of script variables (`gaszIDEScriptVariables` in [`HCP65IDE/IDEApp.cpp`]) makes a
run scripted.  `HCP65_IDE_EMULATOR`, the `VERIFY` variables and the reference screen's own options aren't in it:
they change how a run is checked, not what drives it.  A scripted run:

- has the platform's keyboard and mouse but takes no input from them: what is typed while its window has the focus
  would go into the files it edits, and a check saves them;
- has a script keyboard instead ([`CIDEScript::PressKey`]: the steps `press:`, `release:` and `tap:`), whose keys
  reach the window through the component input as a real keyboard's do, so a check of keyboard handling goes the
  real way (the `keys:` and `key:` steps run the bound command directly, and lay the window out after);
- keeps its window active whatever has the platform's focus ([`CWindow::SetKeepsActive`]): another program's window
  coming to the front mustn't make it ignore its script, as an inactive window would;
- has a clipboard of its own ([`CClipboard::SetDetached`]): its copies mustn't replace the person's, nor theirs
  land in the checks' clipboard history;
- reads none of the user's keymap or settings unless a variable names a file, and saves no layout or open solution
  unless `HCP65_IDE_SAVE_LAYOUT` says to ([`CIDEContent::SetKeepSettings`]);
- ends itself with `HCP65_IDE_SNAPSHOT`: the window is saved as a PNG that many ticks after the script has finished
  (waiting while the debugger's script or Claude is still running) and closed ([`CIDEWindowTick::SetSnapshot`]).
  IDEChecks.py sets it, three ticks, for every run.

[`CIDEScript`] runs the steps (`HCP65_IDE_EDIT`, and the IDE's commands in `HCP65_IDE_DEBUG`); the debugger's
commands are [`CIDEDebugging::SetDebugScript`]'s and its windows' [`CIDEDebugging::RunWindowsScript`]'s, so each
part's script lives beside the part.  A dialog the IDE shows is watched ([`CIDEScript::WatchDialog`]) and answered
by the caption of the button named in `HCP65_IDE_DIALOG_ANSWER`, after `HCP65_IDE_DIALOG_KEYS` are typed into it.
Printing is the script's job too: menus, keys, tabs, files on disk, the status line, the Call Stack and each
debugger window.

## Fixtures, and the user's files

A check works on a solution of its own wherever it can: the small solutions in `Checks/Input` (one per refactoring
and feature: `Calculator`, `Navigation`, `RenameFile`, `Debugging`: 37 in all), each copied into a temporary folder
for the run by its `Make...Copy` function, so a check can save, rename and build without touching the repository
and its results don't move as other code changes.  Opening all of BaseLib for the checks that only look at its
calculator files was 90% of each one's time; they open `Checks/Input/Calculator`, a project of just those files.
Checks about indexing at a real program's scale still open the user's ModPlay workspace, the default.

Nothing of the user's may change.  Each run is given recent solutions of its own (`HCP65_IDE_RECENT`); the files a
check edits without saving and the user's own files a scripted run mustn't write (`WATCHED`) are hashed before and
after each run, and a change fails the check; the user's recent solutions, workspace and layout (`RESTORED`) are
put back at the end if anything changed them.

The debugger's checks build `Checks/Input/Debugging` (a class, a recursive function, an array, a `char*`, an enum
and a float) for each processor and debug it in its emulator: the W65C816's in `HCP65EmulatorRef`, the HCP65
Emulator drawn into memory, as the start up program of a sparse copy of TestHCP65Lib's boot card (so TestHCP65Lib
must have run); the 65C64's in the X65C64 Emulator, a console program.  Each run starts from nothing (no build, no
saved breakpoints or watches: a saved `.hcp65user` would turn a breakpoint the script sets off again).  The checks
are written once, `DebuggingChecks(P)`, with what differs between the processors (the solution, the emulator, how a
pointer and an address print) in a table, and run on both.  See [The Debugger](debugger.md).

The Claude tab's checks run a stand-in for Claude Code (`Checks/Input/Claude/claude`, given by
`HCP65_IDE_CLAUDE_EXECUTABLE`), so they cost nothing and need no account.

## Real input

A scripted run takes no native input, so what only real input can test (dragging a docked pane, a press in a
component's empty box) is tried on the unscripted IDE in an X server of its own, Xephyr, driven through XTest:
nothing reaches the user's desktop, and nothing on it reaches the IDE.  It is a recipe, not a check
(`HCP65-IDE/CLAUDE.md`): the script keyboard covers most keyboard bugs, and found the editor's Control+End not
scrolling that way (2026-11-12).

## Decisions

- The checks run on the reference screen by default, with the windowed IDE's smoke checks after (2026-11-12, built
  2026-11-12 and 13).
- The reference screen gives a program the standard keyboard and mouse, never pressed, rather than a program with
  no native input guarding against it everywhere (2026-11-12).
- HCP65IDERef's `main` is `__cdecl` so Reference|x64 builds on Windows and the checks run there with no window too
  (the user, 2026-11-16).
- The checks that only need BaseLib's calculator files open a solution of just those (2026-11-19, CommonLibraries'
  [`Tests/TestSpeedPlan.txt`] step 2).
- Real input is tried in Xephyr, with the user's leave each time, never on the user's display (2026-11-08).
- The debugger's checks are written once for both processors (2026-11-24).

## Not done

- `HCP65EmulatorRef` has a Linux CMake build only: on Windows the debugger's checks expect
  `HCP65-Emulator/x64/Release/HCP65EmulatorRef.exe`, which no Visual Studio project builds.
- Whether the Windows window, like X11's before its fix, reports no size before it is shown: noted to check on
  Windows (2026-11-13), and nothing records that it was.
