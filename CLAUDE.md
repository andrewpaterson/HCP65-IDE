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
  include folders, defines, the system headers: MinGW's on Linux, Linux's own for a GCC configuration); the IDE's
  code model (`CIDECodeModel`) derives from it and adds the edits thread and the queries its features make, and a
  program without a window can index a workspace the same way.  It also reads the projects' Markdown pages:
  their references to code ([`Name`]) are
  resolved by `CReferenceResolver`, the same code the website's build uses through `hcp65index -resolve`, and
  become index places of usage `CPPUT_Documentation` (`../HCP65/Website/DocsPlan.txt` E; the IDE's
  `Docs/ide/documentation.md`).

## Documentation

- User documentation: `HCP65IDE/HCP65IDE/Docs/ide`, items of `HCP65IDE.vcxproj`; the website gathers it
  (`../HCP65/Website/mkdocs.yml`, `extra.sources`).  `hcp65build`'s page is `HCP65Build/Docs/compiler/building.md`.
- Name code in a page with a reference: [`CTextArea`], [`CTextArea::ReplaceRanges`], [`CShape::Init(int)`] or
  [`BaseLib/Chars.h`].  The site build links it to its line on GitHub and fails on one that names nothing; a plain
  `CTextArea` naming a real class is a warning.  A span that isn't C++ but looks it is written `#!text abs`.
- A ```` ```cpp ```` block in a page compiles (the site build checks it, `hcp65index -examples`, as the project the
  page is an item of compiles): a hidden comment under the page's title, `<!-- cpp`, then the `#include`s and the
  declarations its examples use without declaring, then `-->`, goes before each block; a block of statements is
  compiled in a function.  ```` ```{.cpp .no-check} ```` is for one that shows an edit, not an API.
- Design pages (how it is built and why, its decisions and what isn't done): `HCP65IDE/HCP65IDE/Docs/ide/design`
  (architecture, the code model, refactoring, navigation, editing, the debugger, git, the checks).  Read the page
  before changing what it covers.
- A design page's main classes name it in their headers, one line above the class, `//Design page: Docs/...`
  (`DocsPlan.txt` G1); a new design page adds it to its own.
- Plans beside the code in `HCP65IDE/HCP65IDE`, still being built from: `NextIDEWork.txt` (what's missing),
  `Navigation.txt` (the IntelliJ navigation gap list), `Refactorings.txt`, `GettersNotConst.txt` (questions for the
  user).  Finished and kept only as the record the code cites (a note at the end names the page; don't add to them):
  `NavigationPlan.txt`, `CodeTemplates.txt`, `RenameFile.txt`, `RefactoringQuestions.txt`, `EndOfPlanTasks.txt`,
  `DebuggerPlan.txt`, `Debugger65C64Plan.txt`, `DebuggerFPlan.txt`, `DebuggerFixesPlan.txt`, `DebuggerGapsPlan.txt`.

## Building and testing

- `cmake -S HCP65IDE -B ../build/HCP65IDERelease -DCMAKE_BUILD_TYPE=Release`, then `cmake --build`.  Targets:
  `HCP65IDE` (a window), `HCP65IDERef` (draws into memory; the checks use it), `hcp65build` and `hcp65index`.
- Run it from `HCP65IDE/HCP65IDE`: with no argument it reopens the last solution; a `.sln` or `.vcxproj`
  argument opens that.  Any `HCP65_IDE_*` environment variable makes a run scripted (no native input).
- Checks: `cd HCP65IDE/Checks && python3 IDEChecks.py [check names]`.  They run `HCP65IDERef` and restore
  the user's recent-files and layout files.  Delete `Checks/__pycache__` if a run leaves it.
- The debugger checks run `../build/HCP65EmulatorRelease/HCP65EmulatorRef` and `../build/X65C64Emulator` on
  TestHCP65Lib's `Output/BootCard.img`: build both and run TestHCP65Lib first, and rebuild both after a change to the
  debug protocol or records, or the checks wait for an emulator that doesn't answer.
- A scripted run by hand exits only when `HCP65_IDE_SNAPSHOT` is set; `HCP65_IDE_SNAPSHOT_TICKS` (40000 for the
  git status) lets work done on the tick show in it.  The script steps are `gaszIDEScriptCommands` in
  `IDEScript.cpp`; editor steps start `edit:`.  Results shown from the index in its order differ between runs: sort
  them (`SortIDELocations`) before printing.
- A test run from a private copy of its folder needs its siblings linked beside it (TestBuildLib reads
  `../../TestVisualStudioLib`) and an `Output` folder.
- Library tests build with CMake as in CommonLibraries (`cmake -S Tests/TestBuildLib -B
  ../build/TestBuildLib64Release -DCMAKE_BUILD_TYPE=Release -DBASELIB_64=ON`).

## Rules and recipes

- Copy IntelliJ IDEA's behaviour; ask before inventing semantics.  `Navigation.txt` is a gap list: build only what
  the user picks from it.
- Commit IDE changes before trialling a refactoring on real code.
- A scripted check whose input intermittently "does nothing": another window took the focus (scripted runs keep the
  window active with `CWindow::SetKeepsActive`); log `GetFocus()->IsActive()` first.
- Screenshots of a real window: `xwd -id` on the IDE's own window only, never the root window (it shows the user's
  other programs), and with nothing over it (an overlapping window's pixels are captured too); delete it at once if
  it caught anything else.
- Real input: run the IDE unscripted in `Xephyr :5 -screen 1600x1000 -ac` (ask the user first; close it after) and
  drive it with XTest from Python ctypes (libXtst has no header here; refuse displays `:0` and `:1` in the script).
  There is no window manager in it: give the focus with `XSetInputFocus`; screenshots with `XGetImage`; open files by
  double-clicking Solution Explorer's rows.  Set `HOME` to a scratch folder so the user's recent files are
  untouched.  Often a scripted run's keyboard steps (`edit:press:`, `edit:tap:`, `edit:printview`) are enough: they
  go through the components as real keys do, where `keys:` runs the bound command and then lays the window out.
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
- Every tracked file says what it is (`HCP65/Website/tools/check_files.py`, in the website's build): an item of its
  project (plans and notes too), test data, or a row of the `README.md` of a folder above it, whose table
  (`| Files | Are | From |`) says source, generated (with the command and its inputs) or third-party (with where from
  and which version).
- Code files are LF.  `.sln`, `.vcxproj` and `.filters` are CRLF with a BOM: edit them as bytes and check `git diff
  --stat` shows only your lines.  Test `Input` files are kept byte for byte.
