# HCP65-IDE

A C++ IDE: Visual Studio's project model (`.sln` and `.vcxproj` are the projects), IntelliJ IDEA's behaviour.
Its end goal is refactoring C++; it also builds HCP65 programs and debugs them in the emulators.  It needs
CommonLibraries and HCP65 checked out beside it (`../CommonLibraries`, `../HCP65`).

## Layout

- `HCP65IDE/HCP65IDE`: the IDE, one class per feature (`IDE*.cpp`) reached through `CIDEContent`'s getters.
  Commands are named strings (`"Refactor.Rename"`, `"Navigate.FindUsages"`).
- `HCP65IDE/Checks`: the IDE checks (`IDEChecks.py`) and their fixtures (`Input/`).
- `HCP65Build`: `hcp65build`, the command-line builder for HCP65 programs.
- `HCP65Index`: `hcp65index`, which indexes solutions with IndexLib and writes the symbol table the website's
  references to code resolve against (`../HCP65/Website/DocsPlan.txt` D).
- `Libraries/`: VisualStudioLib (reads `.sln`/`.vcxproj`), BuildLib, IndexLib, GitLib (the git command line),
  ClaudeLib (the Claude tab), TemplateLib; `Tests/Test<Lib>` for each.
- The C++ front end and the symbol index are CommonLibraries' CppCompilerLib (`CCPPIndex`).  IndexLib's
  `CWorkspaceIndex` indexes a workspace with them as its build configuration compiles it (configuration,
  include folders, defines, the system headers: MinGW's on Linux); the IDE's code model (`CIDECodeModel`) derives
  from it and adds the edits thread and the queries its features make, and a program without a window can index
  a workspace the same way.

## Documentation

- User documentation: `HCP65IDE/HCP65IDE/Docs/ide`, items of `HCP65IDE.vcxproj`; the website gathers it
  (`../HCP65/Website/mkdocs.yml`, `extra.sources`).  `hcp65build`'s page is `HCP65Build/Docs/compiler/building.md`.
- Name code in a page with a reference: [`CTextArea`], [`CTextArea::ReplaceRanges`], [`CShape::Init(int)`] or
  [`BaseLib/Chars.h`].  The site build links it to its line on GitHub and fails on one that names nothing; a plain
  `CTextArea` naming a real class is a warning.  A span that isn't C++ but looks it is written `#!text abs`.
- Plans beside the code in `HCP65IDE/HCP65IDE`: `NextIDEWork.txt` (what's missing), `Navigation.txt` (the
  IntelliJ navigation gap list) and `NavigationPlan.txt`, `Selection.txt`, `RenameFile.txt`,
  `Refactorings.txt`, `RefactoringQuestions.txt`, `DebuggerPlan.txt`, `Debugger65C64Plan.txt`,
  `DebuggerGapsPlan.txt`, `CodeTemplates.txt`.

## Building and testing

- `cmake -S HCP65IDE -B ../build/HCP65IDERelease -DCMAKE_BUILD_TYPE=Release`, then `cmake --build`.  Targets:
  `HCP65IDE` (a window), `HCP65IDERef` (draws into memory; the checks use it), `hcp65build` and `hcp65index`.
- Run it from `HCP65IDE/HCP65IDE`: with no argument it reopens the last solution; a `.sln` or `.vcxproj`
  argument opens that.  Any `HCP65_IDE_*` environment variable makes a run scripted (no native input).
- Checks: `cd HCP65IDE/Checks && python3 IDEChecks.py [check names]`.  They run `HCP65IDERef` and restore
  the user's recent-files and layout files.  Delete `Checks/__pycache__` if a run leaves it.
- Library tests build with CMake as in CommonLibraries (`cmake -S Tests/TestBuildLib -B
  ../build/TestBuildLib64Release -DCMAKE_BUILD_TYPE=Release -DBASELIB_64=ON`).

## Rules and recipes

- Copy IntelliJ IDEA's behaviour; ask before inventing semantics.  `Navigation.txt` is a gap list: build only what
  the user picks from it.
- Commit IDE changes before trialling a refactoring on real code.
- A scripted check whose input intermittently "does nothing": another window took the focus (scripted runs keep the
  window active with `CWindow::SetKeepsActive`); log `GetFocus()->IsActive()` first.
- Real input: run the IDE unscripted in `Xephyr :5` (ask the user first; close it after) and drive it with XTest
  from Python ctypes; set `HOME` to a scratch folder so the user's recent files are untouched.  Often a scripted
  run's keyboard steps (`edit:press:`, `edit:tap:`) are enough.
- Claude tab checks use a stand-in for Claude Code (`HCP65IDE/Checks/Input/Claude/claude`), so they cost nothing.

## Working in these repositories

<!-- This section is the same in every Codaphela repository's CLAUDE.md: change them together. -->

- Several Claude sessions work in these checkouts at once, on the main branch (no private branches or worktrees).
  Run `git status` before touching a file and leave other sessions' uncommitted work alone.  Stage and commit only
  your own files, by path: `git add <paths>` and `git commit -m "..." -- <paths>`; never `git add -u`/`-A` or
  `commit -a`, and not `repos.py commit` (it stages everything).  Check `git show --stat HEAD` after.
- Commit messages have no Co-Authored-By line.  Make commits logical: one change each, its tests with it.
- Bring work from a copy into the main checkout with a 3-way merge (`git merge-file`, or `diff -u base mine |
  patch`), never by copying files over it: another session may have changed the target since.
- Before building a step of a shared plan, `git fetch` and read `git log HEAD..origin/<branch>` for the plan's
  files: a session on another machine may have done it.
- The shared build folders (`../build/*`) can hold binaries linked from another session's half-finished edits.  A
  failure seen only there: build the same sources in a private build folder; if that passes, rebuild the shared one
  clean.  Tests rewrite `Output/` in their project folder: run from a copy of that folder when another session may
  be running the same suite.
- Build, run and measure in Release x64 by default; Debug for full test sweeps or chasing a problem.  Say which build
  a number came from.  A speed claim that feeds a decision needs a like-for-like A/B (same commit but the change, in
  the whole test program) judged by instruction counts (callgrind) with the clock beside them.
- A decision the user states while planning settles that question only: plan with them, and wait to be told to
  build.
- Put decisions to the user one at a time, each explained plainly (what changes, what it costs), with the
  recommended option first, especially outside their speciality.
- Show images as full `file://` paths to the real files; don't make enlarged copies.
- Code files are LF.  `.sln`, `.vcxproj` and `.filters` are CRLF with a BOM: edit them as bytes and check `git diff
  --stat` shows only your lines.  Test `Input` files are kept byte for byte.
