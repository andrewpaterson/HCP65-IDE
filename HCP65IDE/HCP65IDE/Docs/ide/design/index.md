# IDE Design

How the HCP65 IDE is built and why, for whoever changes it: the decisions behind each part, with their reasons and
dates, and what isn't done.  The [user pages](../index.md) say what the IDE does; these pages link to them rather
than repeat them.

## The goal

The IDE's end goal is refactoring C++ (the user, 2026-11-03 and 04).  Everything under it, the index of every name,
the type checking of every unit, undo across files, is groundwork for that.  Two references decide how it looks and
behaves (the user, 2026-11-04 and 2026-11-06):

- **Visual Studio's project model.**  The `.sln` and `.vcxproj` files are the projects, read and written as they
  are, so Visual Studio still opens the same solution.  The IDE adds files of its own beside them only for what
  Visual Studio has no place for (a workspace of several solutions, build configurations for the HCP65).
- **IntelliJ IDEA's behaviour.**  Its keys, its navigation, its refactorings and their dialogs, Alt+Enter, Problems
  Detected before a change, one undo for a refactoring.  What IntelliJ does is copied; where C++ needs something
  Java doesn't (templates, macros, headers and their definitions, hand written assembly), the user is asked before
  semantics are invented.

It also builds HCP65 programs and debugs them in the emulators.

## What it is built from

| Part | Where | Gives the IDE |
|---|---|---|
| WindowLib | CommonLibraries | The window, its components, the text area and code editor, docking, menus and commands ([WindowLib's design](../../libraries/windowlib/index.md)). |
| CppCompilerLib | CommonLibraries | The C++ front end: preprocessor, parser, the type checker the IDE's semantic pass runs ([`CCPPSemanticPass`]), and the symbol index ([`CCPPIndex`]).  See [the front end's design](../../compiler/design/front-end.md). |
| IndexLib | HCP65-IDE/Libraries | [`CWorkspaceIndex`]: a workspace indexed as its build configuration compiles it, and its documentation pages' references ([`CReferenceResolver`]). |
| VisualStudioLib | HCP65-IDE/Libraries | [`CVcxSolution`] and [`CVcxProject`]: the solutions and projects read, and the few edits the IDE makes to them. |
| BuildLib | HCP65-IDE/Libraries | [`CBuildWorkspace`] (the solutions open together), build and run configurations, and the builds (HCP65, MSVC, GCC). |
| GitLib | HCP65-IDE/Libraries | [`CGitRepositories`]: the git command line, file statuses and changed lines. |
| ClaudeLib | HCP65-IDE/Libraries | [`CClaudeCommand`]: Claude Code run headless for the Claude tab. |
| TemplateLib | HCP65-IDE/Libraries | [`CTemplateText`]: the file and code templates new files and generated code are written from. |
| StandardLib, BaseLib | CommonLibraries | Objects and [`Ptr`] (every part of the IDE is a [`CObject`]), text, containers, [`CUndoLog`]. |

The IDE itself is `HCP65IDE/HCP65IDE`: one class per feature, reached through [`CIDEContent`]
([Architecture](architecture.md)).  `hcp65index` (`HCP65Index`) indexes solutions with the same [`CWorkspaceIndex`]
for the website's references, so the IDE and the website resolve a reference the same way.

## The pages

| Page | Is |
|---|---|
| [Architecture](architecture.md) | [`CIDEContent`] and its getters, one class per feature, named commands, where state lives, the window's documents and tool windows, solutions and the files kept beside them, the Claude tab. |
| [The Code Model](code-model.md) | The index and how a workspace is indexed, the edits thread, the queries, documentation pages in the index, indexing speed. |
| [Refactoring](refactoring.md) | How the refactorings are built on the index and the compiler's checks: Rename, Rename File and the rest, templates, assembly labels, undo, what they refuse. |
| [Navigation](navigation.md) | Go to and the Go to dialogs, Find Usages, Find in Files. |
| [Editing](editing.md) | The selection, clipboard history, code templates, the startup project. |
| [The Debugger](debugger.md) | Debugging HCP65 programs in the emulators. |
| [Git](git.md) | GitLib, file statuses, change bars and diffs. |
| [Checks](checks.md) | How the IDE is tested: the scripted checks and the reference build. |

The plans the IDE is still being built from stay beside its code in `HCP65IDE/HCP65IDE`:
[`HCP65IDE/NextIDEWork.txt`] (what is missing), [`HCP65IDE/Navigation.txt`] (IntelliJ's navigation, a gap list the
user picks from) and the others the repository's `CLAUDE.md` lists.
