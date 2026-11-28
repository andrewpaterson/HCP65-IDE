# Architecture

The IDE is a WindowLib application: one window whose content, [`CIDEContent`], holds the menu bar, the toolbar, the
dock and the status line, and an object for each feature of the IDE.  The features don't know each other directly:
each reaches the others, and what they share, through the content's getters.

## The content and its features

[`CIDEApp`] makes the window and its content and runs it (`HCP65IDE_Linux.cpp` and `HCP65IDE_Windows.cpp` are the two
`main`s; `HCP65IDERef.cpp` is the reference build, drawn into memory for the [checks](checks.md)).  The content is
a StandardLib [`CObject`]; so is each feature, held by the content in a [`Ptr`].  A feature is given the content
when it is made (`Init(CIDEContent*)`) and keeps it as a plain pointer, not held: the content holds it.

One class per feature, in its own `IDE*.cpp`: [`CIDERename`], [`CIDEUsages`], [`CIDEDocuments`], [`CIDENavigator`],
[`CIDEFindInFiles`], [`CIDEBuilding`], [`CIDEDebugging`], [`CIDEGitStatus`], [`CIDEAskClaude`]: 58 in all, among
them one for each refactoring.  The content's header lists them with a line each.  The getters are of two kinds:

| Getters | For |
|---|---|
| The features: [`CIDEContent::GetDocuments`], [`CIDEContent::GetRename`], [`CIDEContent::GetSolutions`] ... | A feature asking another to act: a refactoring opening a file's editor, Rename asking Rename File to rename a class's files. |
| What they share: [`CIDEContent::GetWindow`], [`CIDEContent::GetTheme`], [`CIDEContent::GetDialogLook`], [`CIDEContent::GetCommands`], [`CIDEContent::GetDock`], the icons ([`CIDEContent::GetSymbolIcon`] ...), [`CIDEContent::GetCodeModel`], [`CIDEContent::GetWorkspace`], [`CIDEContent::ShowStatus`], [`CIDEContent::IsKeepingSettings`] | The window and its look, the code model and the workspace, the status line. |

Two things are still the content's own: the workspace ([`CBuildWorkspace`], the solutions open) and the code model
([`CIDECodeModel`], see [The Code Model](code-model.md)), which nearly every feature uses.

## Commands

A command is a named string, not an enumerator: `#!text "Refactor.Rename"`, `#!text "Navigate.FindUsages"`.  Each
feature adds its own to the window's [`CCommandSet`] (its `AddCommands`, called in [`CIDEContent::AddCommands`]):
the name, the caption, IntelliJ's keys, and the function it runs, with an enabled check where it can be greyed
([`CCommand::SetEnabledCheck`]).  Menus, the toolbar, Show Context Actions and the checks' scripts find a command
by its name ([`CCommandSet::Find`]), and the user's keymap names it too, so a command can be rebound without the
code knowing.

Each feature also puts its own items in the menus: the menu bar's menus are made by their owners
([`CIDEEdit::CreateEditMenu`], [`CIDENavigator::CreateNavigateMenu`], [`CIDEBuilding::CreateBuildMenu`] ...), the
editor's right-click menu by each feature's `AddEditorItems`, and Show Context Actions by each refactoring's
`AddContextActions` ([`CIDEContextActions`]).  The other context menus are their owners' too: a results tree's is its
pane's ([`CIDEResultsPane`], which Find and Bookmarks add commands to), a tab of output's is a [`CIDEOutputMenu`]
that its owner adds commands to, the diff tabs share one ([`CIDEDiffs::GetDiffMenu`]), and a document tab's is the
dock's with the IDE's items added ([`CIDEContent::DocumentMenuCreated`]).  A menu item has its own click event
([`CMenuItem::GetClickEvent`]), so nothing in the content dispatches on which item was clicked.  View is made each
time it opens, from the dock's tool windows ([`CIDEContent::ViewMenuOpening`]).

A command that shows a dialog doesn't show it from the key press: it posts itself to the window ([`CWindow::Post`],
as [`CIDERename::QueueRename`] does) and shows it after the input has been handled.

## Where state lives

State belongs to the feature that uses it.  The build running and its listings are [`CIDEBuilding`]'s; the git
repositories, the branch label and the change shown are [`CIDEGitStatus`]'s; the diff tabs are [`CIDEDiffs`]'s; the
chosen build configuration is [`CIDEConfigurations`]'s; Solution Explorer, its menu and its docking are
[`CIDESolutions`]'s.  Each feature has its own dialogs and file choosers.  Only [`CIDEScript`], which drives the
IDE for the checks, is a friend of the classes it looks inside.

What a feature keeps between runs it reads and writes itself:

| Kept | Where | How |
|---|---|---|
| How the IDE was left (Find Usages' groups, Rename's search ticks) | The user's settings file | Each feature's `ReadSettings` and `WriteSettings` ([`CIDESettings`]). |
| The user's keys | The user's keymap file | [`CCommandSet`], only the commands that differ. |
| Breakpoints, bookmarks, recent files | `<workspace>.hcp65user` beside the workspace | Each a part of [`CIDEUserFile`] (`AddPart`), written whole when one changes. |
| The layout and the files open | `<solution>.layout.user` | [`CIDEContent::SaveLayout`], [`CIDEContent::LoadLayout`]. |
| The solutions open together, startup solution, run configurations | `<solution>.hcp65workspace` | [`CBuildWorkspace`]. |
| Build configurations for the HCP65 | The solution's rows and the projects' HCP65 groups | [`CBuildConfiguration::ReadProject`], [`CBuildConfigurations`]; Edit Build Configurations writes them with [`CBuildOptionEdits`], and g++'s options in the projects' and sheets' GCC-x64 groups the same way. |
| Shared run configurations | `.run/<Name>.hcp65run` beside a solution | [`CRunConfiguration`], [`CBuildWorkspace::ApplyRunConfiguration`]. |
| Cards | `<Card>.hcp65card` | [`CBuildCard`]. |
| A project's licence for new files, its website | `<project>.hcp65project` | Solution Explorer's Properties ([Documentation Pages](../documentation.md#which-website)). |

Where each user file is kept is on [Settings](../settings.md).

## The tick

Work that follows the user, rather than answering a key, is polled.  The window's tick calls [`CIDEContent::Tick`],
which polls the build, the run, the debugger and Claude's output, then each feature that keeps up with the editor:
the edited tabs given to the code model, the status line, breadcrumbs, the caret's usages, the navigation history,
the quick info popups.  Each poll says how many milliseconds until it next has something to do, and the tick asks
the window for the soonest ([`CWindow::RequestTick`]), so the IDE ticks only when something is due.

## Windows, documents and tool windows

The window's middle is WindowLib's dock ([`CDockManager`]; [its design](../../libraries/windowlib/docking.md)),
Visual Studio's docking: documents in the document well and tool windows round it.  The content is the dock's
listener: it makes a document's pane from its ID when a layout is read ([`CIDEContent::CreatePane`]: an ID
`#!text Document:<path>` opens that file, `#!text Diff:<n>` is a diff), approves a pane closing (unsaved files are
[`CIDECloseGuard`]'s) and adds the IDE's items to a document tab's menu.

Each tool window is made by the feature it shows, with a fixed ID that the layout file keeps:
`#!text SolutionExplorer`, `#!text Structure`, `#!text FindUsages`, `#!text Find`, `#!text Duplicates`,
`#!text Bookmarks`, `#!text Hierarchy`, `#!text Build`, `#!text Indexing`, `#!text Problems`, `#!text Run`,
`#!text Debug` and `#!text Claude`.  The windows of results (Find Usages, Find, Duplicates, Hierarchy, Bookmarks,
Problems) are all one [`CIDEResultsPane`], a tree with a preview and IntelliJ's toolbar, and those that keep a tab
per search are [`CIDEResultsTabs`].  Their keys, Hide All and Jump to Last are [`CIDEToolWindows`]'.

An open file is a [`CIDEEditorTab`] (a [`CCodeEditor`] with its file, project and git state), and the tabs are
[`CIDEDocuments`]'.  A refactoring that changes a file that isn't open opens its editor without showing it
([`CIDEDocuments::OpenFileTab`]), so every change is made in an editor and can be undone there.

## Solutions and projects

The projects are Visual Studio's, read by VisualStudioLib: [`CVcxSolution`] for a `.sln` (its projects and solution
configurations), [`CVcxProject`] for a `.vcxproj` and its `.filters` (items, filters, configurations, include
directories and defines, with `$(...)` macros expanded).  A `.sln`'s paths can differ in case from the disk's, so a
path is found on disk whatever its case ([`ResolveVcxPath`]).  The IDE writes a project only where it changes
something, as text, changing only the `Include` values it means to: an item renamed ([`CVcxProject::RenameItem`])
or added beside another ([`CVcxProject::AddItemBeside`]).  The rest of the file stays as Visual Studio wrote it.
This isn't optional: the Linux CMake builds read their sources from the `.vcxproj` items.

A project is read as MSBuild reads it, as far as the IDE needs ([`CVcxProject::ReadProject`]):

- **Property sheets.**  An `<Import>` of a sheet is read where it stands, so a group after it replaces what the
  sheet set, or adds to it where its value names itself (`%(PreprocessorDefinitions)`, `$(IncludePath)`).  A
  sheet's path is from the file that imports it, or uses `$(ProjectDir)` or `$(MSBuildThisFileDirectory)`;
  Microsoft's own imports, and one that needs `$(SolutionDir)`, are skipped.  [`CVcxProject::GetSheet`] lists them.
- **Conditions.**  Three are understood: on a whole configuration (`'$(Configuration)|$(Platform)'=='Debug|x64'`),
  on a configuration name and on a platform ([`ParseVcxCondition`]).  Any other is false.  A condition on an
  `<Import>` or its `<ImportGroup>` limits which configurations the sheet's groups are for.
- **Toolchains' configurations.**  A group for a platform that is a toolchain's (`GCC-x64`, `HCP65C816`, `HCP65C64`)
  makes the project a configuration Visual Studio never sees, since it isn't in `<ProjectConfigurations>`
  ([`CVcxConfiguration::IsToolchain`]).  It has its own defines, options and HCP65 settings, and takes what a
  project says once per Visual Studio configuration, its type and include directories, from the configuration it is
  built like (the include directories always, since a project adds its own to Visual Studio's configurations): `Release|x64` for `Release|GCC-x64`, `Release|Win32` for the HCP65's.
- **Files left out.**  `ExcludedFromBuild` on an item, or on every source through an item definition
  ([`CVcxProject::IsCompiled`]).

A build's own arguments for a toolchain are a tool's `AdditionalOptions` ([`CVcxProject::GetAdditionalOptions`]),
and what it links, `Link`'s `AdditionalDependencies`.  The plan they come from is `ConfigurationsPlan.txt`.

Several solutions open together are a workspace ([`CBuildWorkspace`]), a program with the kernal it runs on.  A
project two solutions both have is one project to the IDE: indexed, searched and renamed once.  One configuration
name is chosen for the workspace, and each solution builds its own solution configuration of that name.  What the
user sees is on [Solutions and Workspaces](../solutions.md); the startup solution and project are on
[Editing](editing.md).

## The Claude tab

The Claude tab ([The Claude Tab](../claude-tab.md)) sends a request to Claude Code run headless, as GitLib runs
git:  the user's own `claude`, signed in as they are, with their settings.  ClaudeLib does the running and reading:

| Class | Does |
|---|---|
| [`CClaudeCommand`] | One request: `#!text claude -p <request> --output-format stream-json`, with `#!text --resume` and the session's id so the conversation carries on, `#!text --add-dir` for each folder it may use, and the permission mode. |
| [`CClaudeStream`] | Reads the output, a JSON object a line, as it arrives in pieces, into events; keeps the session's id, how the run finished and the files it changed. |
| [`CClaudeEvent`] | One thing that happened: text, a tool used, its result, the end.  A line can be several. |
| [`CJSONDocument`] | The JSON reader: BaseLib's reads into a markup document and doesn't take bare `true`, `false` and `null` or `\u` escapes, which Claude's output has. |

In the IDE, [`CIDEAskClaude`] makes the request (where the caret and selection are, the files saved first), polls
the run at each tick, and reads again the open files Claude changed, all of them one undo group; [`CIDEClaudePane`]
is the tab; [`CIDEClaudePage`] is Settings' page for the folder Claude Code is started in ([`EIDEClaudeDirectory`]:
the startup solution's folder, its repository, or a folder chosen).  That folder is where Claude Code looks for its
`CLAUDE.md` and keeps its conversations, so a request from another folder starts a new conversation.

- **Folders.**  Claude Code refuses to read or search outside the folders it is given, and a solution's projects
  are mostly outside its folder, so each request adds the repository of every solution and project in the workspace
  ([`CIDEAskClaude::AddClaudeDirectories`]).  ClaudeLib sets `CLAUDE_CODE_ADDITIONAL_DIRECTORIES_CLAUDE_MD` in the
  IDE's environment, so those repositories' `CLAUDE.md` files are read too.
- **Permissions.**  No one can answer a question from a headless run, so the mode is `#!text acceptEdits`: Claude
  can read and change files in its folders, and a tool it would have to ask for (running a command) is refused,
  which the finished event lists.
- **Enter sends, Shift+Enter starts a line** in the request box ([`CTextArea::SetEnterEvent`]).  The box is Stretch
  in both directions: a text area that isn't word wrapped is as wide as its text, so an empty one aligned Left has
  no width, and a press in its scroll pane hits nothing.
- **The checks** run a stand-in script in Claude Code's place (`#!text HCP65_IDE_CLAUDE_EXECUTABLE`,
  [`SetClaudeExecutable`]), so they cost nothing ([Checks](checks.md)).

## Decisions

- One class per feature, behind [`CIDEContent`]'s getters (2026-11-06, the user's cleanup): the private state the
  content had of other features moved to them (the build to [`CIDEBuilding`], git to [`CIDEGitStatus`], diffs to
  [`CIDEDiffs`], the configuration to [`CIDEConfigurations`]), each feature has its own dialogs, and the `friend`
  lines went but [`CIDEScript`]'s.  Built 2026-11-06 (HCP65-IDE a0679e5 to 831c5f8).
- A menu item has its own click event, and each class adds and handles its own menu items (the user, 2026-11-06),
  so the content's one click handler and its 25 or so menu members went (CommonLibraries ebf5fc141, HCP65-IDE
  831c5f8).
- View is made from the dock's tool windows (the user, 2026-11-06), so a new tool window needs no menu code.
- Solution Explorer belongs to [`CIDESolutions`] (the user, 2026-11-06): its making, docking, menu and refreshes
  (HCP65-IDE d5d919c).
- Commands are named strings, not enumerators, so menus, keymaps and scripts name them the same way.
- The `.sln` and `.vcxproj` files are the projects (the user, 2026-11-03 and 04): the IDE never keeps a project
  model of its own beside them.

## Not done

- Exclude and Remove in the Find Usages and Duplicates trees' menus (IntelliJ's): the results are kept by
  their index, the tree can't lose a node, and nothing here reads an exclusion as IntelliJ's refactoring
  previews do; [`HCP65IDE/NextIDEWork.txt`], item 8.
- No MSVC build or Windows run has been tried since 2026-11-17 ([`HCP65IDE/NextIDEWork.txt`]).
- On Windows Claude Code is started with `CreateProcess` and the name `claude`, which finds only a `claude.exe` on
  the path, not the `claude.cmd` an npm install makes.  Not tried.
