# HCP65-IDE

A C++ IDE: Visual Studio's project model (`.sln` and `.vcxproj` are the projects), IntelliJ IDEA's behaviour.
Its end goal is refactoring C++; it also builds HCP65 programs and debugs them in the emulators.  It needs
CommonLibraries and HCP65 checked out beside it (`../CommonLibraries`, `../HCP65`).

## Layout

- `HCP65IDE/HCP65IDE`: the IDE, one class per feature (`IDE*.cpp`) reached through `CIDEContent`'s getters.
  Commands are named strings (`"Refactor.Rename"`, `"Navigate.FindUsages"`).
- `HCP65IDE/Checks`: the IDE checks (`IDEChecks.py`) and their fixtures (`Input/`).
- `HCP65Build`: `hcp65build`, the command-line builder for HCP65 programs.
- `Libraries/`: VisualStudioLib (reads `.sln`/`.vcxproj`), BuildLib, GitLib (the git command line),
  ClaudeLib (the Claude tab), TemplateLib; `Tests/Test<Lib>` for each.
- The C++ front end and the symbol index are CommonLibraries' CppCompilerLib (`CCPPIndex`); the IDE drives
  them and shows the results.

## Documentation

- User documentation: `../HCP65/Website/docs/ide` (to move to `HCP65IDE/Docs`, see
  `../HCP65/Website/DocsPlan.txt`).
- Plans beside the code in `HCP65IDE/HCP65IDE`: `NextIDEWork.txt` (what's missing), `Navigation.txt` (the
  IntelliJ navigation gap list) and `NavigationPlan.txt`, `Selection.txt`, `RenameFile.txt`,
  `Refactorings.txt`, `RefactoringQuestions.txt`, `DebuggerPlan.txt`, `Debugger65C64Plan.txt`,
  `DebuggerGapsPlan.txt`, `CodeTemplates.txt`.

## Building and testing

- `cmake -S HCP65IDE -B ../build/HCP65IDERelease -DCMAKE_BUILD_TYPE=Release`, then `cmake --build`.  Targets:
  `HCP65IDE` (a window), `HCP65IDERef` (draws into memory; the checks use it) and `hcp65build`.
- Run it from `HCP65IDE/HCP65IDE`: with no argument it reopens the last solution; a `.sln` or `.vcxproj`
  argument opens that.  Any `HCP65_IDE_*` environment variable makes a run scripted (no native input).
- Checks: `cd HCP65IDE/Checks && python3 IDEChecks.py [check names]`.  They run `HCP65IDERef` and restore
  the user's recent-files and layout files.  Delete `Checks/__pycache__` if a run leaves it.
- Library tests build with CMake as in CommonLibraries (`cmake -S Tests/TestBuildLib -B
  ../build/TestBuildLib64Release -DCMAKE_BUILD_TYPE=Release -DBASELIB_64=ON`).
