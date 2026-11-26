# Editing

How the IDE's editing is built: the editor tab, the selection, the clipboard history, the file and code templates
new files and generated code are written from, and the startup project.  What they do for the user is [The
Editor](../editor.md), [New Files and Templates](../templates.md) and [Solutions and
Workspaces](../solutions.md#the-startup-project).

The editing itself, the caret, the selection, box selection, Extend Selection, drag and drop, undo of one text and
the clipboard, isn't the IDE's: it is BaseLib's [`CTextEditing`] and WindowLib's [`CTextEditingInput`], shared by
every text area and field of every application, and its design is WindowLib's [Text
Area](../../libraries/windowlib/text-area.md) and [Input](../../libraries/windowlib/input.md).  This page is what
the IDE adds on top and why.

## The editor tab

An open file is a [`CIDEEditorTab`]: WindowLib's [`CCodeEditor`] in a scroll pane, with the bars round it (the Find
bar, breadcrumbs, sticky lines: [Navigation](navigation.md#the-outline-of-a-file)).  The tab keeps what the edits
thread worked out from its text, each with the editor's change count it was made at: the highlights, the syntax
ranges for Extend Selection, the outline, and the check's problems.  How the edits thread makes them is [The Code
Model](code-model.md).  When the tab is made it turns on the selected text's occurrences
([`CTextArea::SetHighlightOccurrences`]) and makes itself the editor's source of syntax ranges
([`CTextArea::SetSelectionRanges`]).

## Selection

The selection was built from Selection.txt in nine steps, each a commit, all on 2026-11-12 (the plan is in the
history up to HCP65-IDE a8a62b0).  Most of it is in the libraries, so every text area has it:

| Step | Where | What |
|---|---|---|
| Double and triple click | [`SClickCount`], [`CTextNavigation::FindWordAround`], [`CTextNavigation::FindLineAround`] | A word, a line with its line feed; dragging on by words or lines. |
| Gutter | [`CTextEditingInput::StartLineDrag`] | A press on a line number selects the line; the glyph, icon, mark and change bar columns keep their clicks. |
| Drag and drop | [`CTextEditing::MoveText`], [`CTextEditingInput::SetTextDragEnabled`] | The selected text moved, or copied with Control, as one undo step. |
| Occurrences | [`CTextNavigation::FindOccurrences`], [`CTextArea::SetHighlightOccurrences`] | The selected text's other places, found at the next tick, at most 1000. |
| Usages at the caret | [`CIDECaretUsages`], [`CTextArea::SetOccurrenceRanges`] | The IDE's: below. |
| Extend and Shrink | [`CTextEditing::ExtendSelection`], [`CTextEditing::ShrinkSelection`], [`CTextSelectionRanges`] | Control+W and Control+Shift+W, over the text's ranges and those a view adds. |
| Syntax ranges | [`CCPPHighlighter::FindSyntaxRanges`], [`CIDEEditorTab`] | The IDE's: below. |
| Box engine | [`CTextEditing::BoxSelect`], [`STextBox`] | Rows between two corners; typing, paste and cut act on every row as one edit. |
| Box input and drawing | [`CIC_StartDragBox`], [`CTextAreaShapes::AddBox`] | Alt+Shift with the arrows, Home, End or button one; a rectangle on each row. |

Two reviews the same day tidied what the steps had left: the input has one drag with a kind
([`CTextEditingInput`]), the text area's own pass-throughs went ([`CTextArea::GetEditing`] is called instead), the
line commands act on a box's lines ([`CTextEditing::SelectBoxLines`]), and a selection's drag held still past the
view's edge goes on scrolling ([`CFocus::RepeatDrag`]).  The Edit menu has Extend Selection and Shrink Selection
after Select All.

### Usages at the caret

[`CIDECaretUsages`] is polled from the IDE's tick.  Once the active editor's caret has stayed 300 ms on a symbol,
and the index has the tab's text ([`CIDEEditorTab::IsIndexCurrent`]), it finds the symbol as every command does
([`CIDEDocuments::FindSymbolPlace`]) and its places in this file only ([`CIDECodeModel::FindFileReferences`], not
the overrides'), and gives them to the editor; declarations and definitions are drawn in the write colour.  Moving
the caret within them keeps them; moving off, another tab or an edit clears them.  The index has a tab's text when
the change count the edits thread last merged ([`CIDEEditorTab::Indexed`], from [`CIDEDocuments::TakeEditedFiles`])
is the editor's; while the solution is indexing nothing is looked up.  A selection hides them and shows the
selected text's own occurrences.

### Extend Selection's syntax

The parse the edits thread makes for an edited file's colours also gives every syntax node's range
([`CCPPHighlighter::FindSyntaxRanges`]): each node whose first and last tokens are written in that file (not made
by a macro, not from another file), as lines and columns, sorted.  A node ending in a macro's tokens has none.  The
thread puts them in [`SIDEEditedFile::asSyntaxRanges`]; the tab converts them once to ranges of its text
([`CIDEEditorTab::masSyntaxRanges`]) and gives Extend Selection those around the selection, mixed with the text's
own ranges (inside a string or a comment the parser has no nodes).  Lines and columns rather than bytes, as the
highlights are, so a file's carriage returns don't matter.

When the ranges are behind the text, the tab raises [`CIDEEditorTab::GetSyntaxBehindEvent`] and the documents work
the edited tabs out there and then ([`CIDEDocuments::UpdateEditedTabsNow`]), on the UI thread, as Find Usages does:
the user's choice was to parse on demand rather than fall back to the text's ranges.  A header gives only what its
includer's parse reached.

## The clipboard history

The history, its dialog and Paste from History are WindowLib's
([Input](../../libraries/windowlib/input.md#clipboard-history-and-paste-from-history)).  The IDE's part is
[`CIDEPasteHistory`], the history's [`CClipboardChooser`]: it shows WindowLib's [`CPasteHistoryDialog`] over
whichever window asked (the IDE's, or a dialog whose field had the keys), its preview in the code font, and has the
script watch it ([`CIDEScript::WatchDialog`]) so a check can answer it.  Every text in the IDE cuts and copies into
the one history.  Scripted runs detach the clipboard ([`CClipboard::SetDetached`]), so a check never touches what
the person at the machine copied.

## Undo

Undo and redo go through every open file's edits in the order they were made: [`CIDEEdit`] keeps a BaseLib
[`CUndoLog`] fed by each editor's undo events, each editor's own history being its [`CTextUndo`], and doesn't undo
what can't be seen ([`CRevealingUndo`]).  A refactoring's edits of several files, a file it renamed or made, are
one group.  How the refactorings use it is [Refactoring](refactoring.md); a text's own undo is [Text
Area](../../libraries/windowlib/text-area.md#undo).

## File and code templates

Every file the IDE makes and every piece of code its generators write comes from a template, as IntelliJ IDEA's
File and Code Templates: the user's intent was "templates always", so new files come out consistent.  Built from
CodeTemplates.txt in five steps on 2026-11-16 (the plan is kept beside the code as the record).  Before it, a
refactoring that made a file copied the old file's licence comment, guard form and blank lines; that inference is
gone, and only what a template can't know (the class's text, its includes) is taken from the code.

### TemplateLib

The engine is a library of its own, HCP65-IDE's TemplateLib, on BaseLib only, tested by TestTemplateLib.  It is a
subset of Apache Velocity, so a template written for IntelliJ or CLion works here and the other way:

- [`CTemplateText::Parse`] parses a template once into nodes ([`STemplateNode`]: text, references with method
  chains, `#!text #set`, `#!text #if`, `#!text #foreach`, `#!text #parse`, `#!text #include`) and expressions
  ([`STemplateExpression`]), or gives an error with its line and column.  A word after `#` that isn't a directive
  is text, so `#!text #include "x.h"` and `#!text #ifndef` pass through; a C++ `#!text #else` or `#!text #end` is
  written `\#else`.
- A line with only directives and comments on it is taken out with its line end (Velocity 2's "lines"), so a
  template can have its `#!text #if` on lines of their own.
- [`CTemplateText::Evaluate`] applies it with [`CTemplateValues`] (strings and lists, a `#!text #foreach`'s
  variable over any of its name) and a [`CTemplateIncludes`] that finds what `#!text #parse` names.  A reference
  with no value is written as it is, as Velocity does; [`CTemplateText::FindMissing`] lists them, for New's dialog
  to ask.  An include that parses itself (20 deep) or a `#!text #foreach` past 10,000 stops with an error rather
  than running for ever.
- Strings have Java's methods (`#!text toUpperCase()`, `#!text substring(i, j)`, `#!text replace(a, b)` and the
  like) and three of the IDE's own that Velocity has no word for, kept as methods so the syntax stays Velocity's:
  `#!text toUpperSnakeCase()` ("IDEMoveClass" gives "IDE_MOVE_CLASS", the include guards' rule),
  `#!text toLowerSnakeCase()` and `#!text withoutPrefix()` ("CKeg" gives "Keg").

### In the IDE

[`CIDEFileTemplates`] has every template by its tab (Files, Includes, Code) and name: the built-in ones, compiled
in, and the Default and Workspace schemes' changes, read from each scheme's `#!text fileTemplates` directory (a
template's text in `#!text <Tab>\<Name>.ft`, all of them listed in `#!text Templates.xml`) when a solution opens
([`CIDESolutions::ReadFileTemplates`]).  Each scheme's template is an entry of its own, the latest used, so taking
a scheme's away ([`CIDEFileTemplates::Remove`]) gives the one under it back, as IntelliJ's Reset does.  It is the
[`CTemplateIncludes`] its templates' `#!text #parse("File Header")` reads.

- The predefined variables are filled by one function for every caller, [`SetIDETemplateFileVariables`]: the file's
  and class's names, the guard, the project, the directory, the dates, the author (Settings' field, else git's
  user.name) and the project's licence.
- A project's licence is kept in a file of the IDE's beside its `.vcxproj`, `#!text <Project>.hcp65project`
  ([`ReadIDEProjectLicence`], [`WriteIDEProjectProperties`]), as the `.vcxproj.user` is the user's own: LGPL 3
  unless set, chosen in the project's Properties.  The checks' projects have `#!text Licence="None"`, as their
  files have no licence comment.
- The refactorings that make files (Move Class, Copy Class, Extract Superclass, Interface and Class) write them
  from C++ Header and C++ Source ([`CIDEMoveClass::AppendNewFileText`]); the generators write from the Code tab
  (Definition Banner, New Function Body, Getter and Setter Body, Template Interface Comment).
- The built-in templates write, byte for byte, what the IDE wrote before there were templates: every check passed
  unchanged when the generators moved onto them, which was the proof that nothing changed.
- [`CIDEFileTemplatesPage`] is the Settings page; [`CIDENewFile`] is New (Alt+Insert, File, New) and Save File as
  Template.  New's files are added to the project and its filters ([`CVcxProject::AddItem`]) and to a
  CMakeLists.txt that lists the files beside them, staged in git and opened, and one undo takes them away
  ([`CIDEMoveClass::BeginCreating`]'s machinery: the undo log keeps where New put them, for the redo).

## The startup project

A solution can have several applications, its projects that aren't static libraries (an HCP65 program's projects
are Makefile projects, so they count).  The startup project is the one built into the solution's program and run,
as Visual Studio's: the one chosen, kept by name in the workspace file (`#!text StartupProject="X"` on its
`#!text <Solution>`, written only when one was chosen), else the solution's first application
([`CBuildWorkspaceSolution::GetStartupProject`]).

- An HCP65 build links the whole solution into one program, so it builds only the startup project and the libraries
  ([`CBuild::GetProjects`] with one application); GCC and MSVC builds still build every application, as Visual
  Studio does.
- A run configuration with no tool, on a GCC configuration, runs what GCC linked of the startup solution's startup
  project, in the project's folder ([`CGCCBuild::GetExecutablePath`], from [`CIDEDebugging`]).  MSVC has no such
  default.
- Solution Explorer's Set as Startup Project ([`CIDESolutions::SetStartupProject`]) chooses it, and shows it bold
  when the solution has more than one application ([`CSolutionExplorer::IsStartupNode`]).
  `hcp65build -startup <project>` chooses it on the command line ([`CBuildCommandLine`]).

Its tests are TestBuildLib's, on Input/Workspace/Tools (First, Second and a shared library), and the checks
startup-project-default, -set, -library and -build (`#!text HCP65_IDE_STARTUP_PROJECT=<solution>:<project>`, the
script's `#!text printstartup` and `#!text printworkspace`).

## Checking it

The editor checks drive it through the script ([Checking the IDE](../checks.md)): editor-clicks,
editor-gutter-select, editor-drag-text, editor-occurrences, editor-caret-usages, editor-extend-selection,
editor-box-selection, editor-paste-history and editor-paste-block, with steps such as `#!text doubleclick:L:C`,
`#!text gutterclick:L`, `#!text dragtext:`, `#!text printoccurrences`, `#!text printcaretusages`, `#!text extend`,
`#!text shrink`, `#!text keys:<keys>` and `#!text printbox`.  The templates' checks are new-class, new-class-undo,
new-class-refused, new-asks-variables, new-header, save-file-as-template, settings-templates (with
`#!text HCP65_IDE_TEMPLATE_EDITS`), settings-templates-keep, definitions-generate-scheme, project-licence and
move-class-licence.

Two things met building the selection: two presses within 4 pixels in quick succession are now a double click, so a
test presses somewhere else first; and the scripted IDE has no keyboard device to hold Control, so dragging text to
copy it is tested in TestWindowLib, not by the checks.

## Decisions

- Selection (the user, 2026-11-12): Extend and Shrink Selection, box selection, and the pointer's extras; not
  multiple carets.  Extend Selection parses on demand when the syntax ranges are behind.  Box selection is Visual
  Studio's (Shift+Alt with the arrows or a drag, every line edited).  The occurrences are both the selected text's
  matches and, as IntelliJ's, the usages of the symbol at the caret from the index.
- How a box and the line commands meet was left to the session (2026-11-12): back tab, delete line, duplicate and
  cut line act on the box's lines; enter and deleting a word end the box at its caret corner.
- Clipboard history (the user, 2026-11-12): "as close to IntelliJ as possible"; the rest is [Input's
  decisions](../../libraries/windowlib/input.md#decisions).
- Templates (the user, 2026-11-16): every file a refactoring makes is written from them, always.  Not Live
  Templates, which are a feature of their own; the engine is the one they would use.  T1: the File Header picks the
  licence by a project setting with `${PROJECT_NAME}` in it, every project meant to be LGPL (the AGPL was chosen by
  mistake on GitHub).  T2: the second scheme is the workspace's, not the solution's.  T3: Velocity's syntax, the
  IDE's own string functions as methods.  T4: `${AUTHOR}` from a Settings field, else git's user.name.  T5: New on
  Alt+Insert, not Control+Shift+A.
- The startup project (2026-11-11): the user asked for one ("we have a startup solution but not a startup project
  within a solution - shouldn't that be a thing?") and for the run configuration side to be built properly.  What
  an application is, the first application as the default, an HCP65 build building only it and GCC running it were
  the session's choices, which the user hasn't reviewed.

## Not done

- Selection: multiple carets (not chosen); the status line doesn't say "Parsing..." before an on-demand parse (it
  would only show after it); the guard for ranges still behind after that parse (a tab whose project has gone)
  isn't checked; Alt+drag isn't checked in the IDE (the window manager may take it), only in TestWindowLib.
- Caret usages: an edited tab after the index is rebuilt from the files isn't checked.
- No automatic indenting, comment commands or Move Line, and Tab types spaces: [`HCP65IDE/NextIDEWork.txt`]'s item
  6, decided (Tab types a tab, in a box and in the pages too) and not built.  No code completion (item 5) or
  Reformat Code (item 10).
- Templates: Overriding Function Body and Forward Declaration were planned and not made, as nothing writes them;
  `${NAMESPACES_OPEN}` and `${NAMESPACES_CLOSE}` are empty; of the variables the plan listed, `${SOLUTION_NAME}`,
  `${BASE_CLASS}`, `${BASE_INCLUDE}` and `${FORWARD_DECLARATIONS}` aren't set (a refactoring hands a template only
  `${INCLUDES}` and `${BODY}`); no Live Templates; no Other tab.
- The startup project: MSVC has no default run of it.
