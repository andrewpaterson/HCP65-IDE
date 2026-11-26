# Navigation

How the IDE's navigation is built: going from a symbol, moving in a file and between files, what a name is, the
hierarchies, finding text, Find Usages, Find in Files, Find Duplicates and the Go to dialog.  What each does for
the user is [Finding Your Way](../navigation.md); the index the symbol features ask is the [code
model](code-model.md).  Every feature copies IntelliJ IDEA's, and each is a class of its own reached through
[`CIDEContent`]'s getters, with its commands named in the keymap ("Navigate.GoToImplementation") so their keys can
be changed.

Most of it was built from one plan, NavigationPlan.txt (2026-11-20 and 21, kept beside the code as the record),
which the user asked for "with a focus on keeping code reusable".  It was arranged by what the features share,
twelve shared pieces built once each with their first user, and the features thin users of them.  Find Usages' tool
window, the Go to dialog and Find in Files came before it; the plan made them users of the same pieces.
[`HCP65IDE/Navigation.txt`] lists what IntelliJ has that the IDE hasn't: a list to choose from, not to finish.

## The symbol at the caret

Every command that works on the symbol at the caret finds it with [`CIDEDocuments::FindSymbolPlace`], so they all
agree where that is, and asks [`CIDECodeModel::GetSymbolAt`] for its key and kind.  Before a command that needs the
index to have the editor's text (Find Usages, Rename, Go to), [`CIDEDocuments::UpdateEditedTabsNow`] gives the
edited tabs to the edits thread at once rather than after its pause; [`CIDEEditorTab::IsIndexCurrent`] says whether
a tab's text is what the index has.

What the index knows and doesn't decides how a feature is made:

| Feature needs | Comes from |
|---|---|
| Base and derived classes | [`CIDECodeModel::FindClassBases`], [`CIDECodeModel::FindClassDerived`] |
| Overrides, both ways | [`CIDECodeModel::FindOverrideLocations`], and from them [`CIDECodeModel::FindSuperMethods`] and [`CIDECodeModel::FindImplementations`] |
| Callers and callees | [`CIDECodeModel::FindCallers`]: the function's places that are calls, by the function each is in; [`CIDECodeModel::FindCallees`]: a scan of every symbol's call places for those inside the function |
| A variable's type | Not in the index: [`CIDENavigator::FindTypeOf`] reads it from the declaration's syntax ([`CIDERefactorUnit::FindDeclaredTypeName`]) |
| An expression's type | The checked parse ([`CIDEIntroduceVariable::FindCaretExpressionType`]), only where the symbol at the caret gives nothing |
| A macro | Not in the index: [`CIDEConvertMacro::FindDefine`] finds its `#define` in the files' text |
| What a file declares, in order | Not in the index: the file's outline (below) |

## Going to places

A "go to" ends in [`CIDENavigator::GoToTargets`]: no target, and the status line says so; one, and it is gone to;
several, and a chooser under the caret lists them with a preview.  Go to Implementation, Super Method, Type
Declaration, Related Symbol, Control+click, the gutter's override marks and the usages popup all end there.  Go to
Source keeps its own rule for the one place it wants.

The chooser is WindowLib's [`CChooserPopup`] (a filter field or none, a list, a preview, a button), each row made
by [`AddNameRow`] as the Go to dialog's are: the icon, the name with its matched characters, where it is, dimmer.
The IDE's [`CIDEChooser`] fills it with places and opens it under the caret ([`CIDEChooser::OpenAtCaret`], at
[`CIDEDocuments::GetCaretPoint`]); its preview is a [`CIDELocationPreview`], which shows a file as its editor has
it if it is open and has the edits thread colour it a moment later.  The usages popup, File Structure, Recent
Files, Recent Locations, the Switcher, Select In, Quick Definition and the breadcrumbs' siblings are all
[`CIDEChooser`]s.

Everything that opens a place goes through [`CIDEDocuments::OpenLocation`] or [`CIDEDocuments::OpenFile`], which
tell [`CIDENavigationHistory`], so every jump is on Back.  Back and Forward are the session's: a hundred places, a
move of ten lines or more from where the caret had stayed for a second counting as one.  Places kept by the
history, Find Usages, Find in Files, Duplicates, the bookmarks and the recent places follow their files' edits:
[`CIDEDocuments::DocumentChanged`] calls each one's `#!text AdjustPlaces` by name, and each moves its places with
[`AdjustIDEPlace`].

Control+click is the text area's link press ([`CTextArea::SetLinkEvent`]): Control and button one, which used to
start a selection drag, is the link's, and [`CIDENavigator::LinkPressed`] puts the caret on the name and goes where
Go to Source would.

## What a name is

[`CIDEQuickInfo`] is everything shown about a name where it is written: the link and declaration with Control held,
Quick Definition, Parameter Info, Type Info, Quick Documentation and the tool tip over a name.  It has no hover or
caret event to hear: [`CIDEQuickInfo::Poll`], each window tick, asks the focus where the pointer is and whether
Control is down, and the editor where its caret is.  The link is an underline layer of the editor's
([`CTextArea::AddUnderlineLayer`]), and the pointer is [`NC_Hand`] over it ([`CIDEQuickInfo::ShowLinkAt`];
[`CIDEQuickInfo::ClearLink`] puts back the editor's [`NC_Text`] I-beam).  The popups are [`CInfoPopup`]s, which
take no keys, so Escape is heard by the editor.

What is shown is read from the declaring file's text, not from a parse ([`ReadIDESymbolInfo`],
[`ReadIDEDeclaration`]): a declaration is the statement its name is in, from the end of the statement or block
before it to its body or semicolon; a parameter's is between the commas round it.  That needs nothing but the text
and is quick enough to do as the pointer moves.  The comment ([`ReadIDEComment`]) is the `//` lines directly over
the definition, or the `//` after it on its line, else the declaration's, without the slashes or a banner's rules;
no markup is read from it.  [`ReadIDESymbolDocumentation`] adds the documentation pages that name the symbol, from
the index's places of usage [`CPPUT_Documentation`], at most eight.  Twenty lookups of three symbols' documentation
took 3 to 6 ms together (Release x64, the ModPlay workspace, files not open, 2026-11-26), so nothing is cached.

Parameter Info finds the call from the text before the caret ([`FindIDECall`]: the innermost open bracket after a
name that isn't a statement's), and lists the functions of that name that [`CIDECodeModel::FindSiblingFunctions`]
gives.  Type Info gives a name's declared type as it is written; for an expression it is the checked parse.  A
macro's name, which the index hasn't, falls back to [`CIDEConvertMacro::FindDefine`], which reads the files' text:
a place the name is written is its `#define`'s if `# define` is all before it on its line.  A name that isn't a
macro costs 2 to 3 ms over the IDE's 190 files (2026-11-21), so the pointer asks it too.

Quick Documentation's press on a place or a page is acted on at the next tick: a press on a popup that can't have
the keys takes them from the editor ([`CFocus::GetPressEvent`] is heard, not an action of the popup's).  The
popup's scrolling keys go before the editor's own Page Up and Page Down by [`CInputAction::SetGoesFirst`].

## The outline of a file

[`CIDEOutline`] is what a file declares, in order and nested ([`SIDEOutlineEntry`]: kind, name as written, what
follows it, whether it is a definition, where its name and all of it are, the entry it is in).  It is gathered in
the IDE by a walk of the syntax tree the edits thread already has ([`CIDEOutline::Gather`]), not by the index, so
nothing in CppCompilerLib changed; its entries have no index keys and no access, and macros aren't in it.  A tab
keeps it with the change count it was parsed at, and what needs it now (File Structure, Next Method) has the tab
parsed then and there, as Extend Selection does; what only shows it waits for the thread.

[`CIDEStructure`] reads it for File Structure, the Structure tool window ([`CIDEStructurePane`]), Next and Previous
Method (by [`FindIDEStepPlace`], the stepping that changed lines, bookmarks and highlighted usages use too) and Go
to Line ([`CPromptPopup`]).  It keeps each editor's breadcrumbs ([`CIDEBreadcrumbs`]) and sticky lines
([`CIDEStickyLines`]) up to date by polling at each tick ([`CIDEStructure::Poll`]).  The bars round an editor are
its tab's: [`CIDEEditorTab::SetTopBar`] (the Find bar), [`CIDEEditorTab::SetBottomBar`] (breadcrumbs) and
[`CIDEEditorTab::SetOverlay`] (sticky lines), round the holder the dock is given ([`CIDEEditorTab::GetHolder`]).

The brace commands (Control+Shift+M, Control+[ and Control+]) are WindowLib's, from the text alone, in every text
area: a bracket in a string or a comment counts.

## Marks on lines

The gutter has a column of icons for each kind of mark after the debugger's glyph column
([`CCodeEditor::SetIconColumns`]), so a line can have a breakpoint, a bookmark and an override mark; the icons are
characters, not images.  [`CIDEBookmarks`] has the first column and [`CIDEMarks`] the second: `^` for a function
that overrides, `v` for one overridden or a class with derived classes, `x` for both.  The override marks are
worked out on the main thread when the active tab's outline or the index changes ([`CIDEMarks::Poll`]), for at most
2000 of a file's functions and classes.

The scroll bar's marks ([`CScrollBar::SetMarks`]) are on its own track, under the thumb: Find's matches, bookmarks,
changed lines in their bars' colours, kept usages, and the active check's problems.  Highlight Usages in File is a
second highlight layer of the editor's ([`CTextArea::AddHighlightLayer`]); an edit ends it, as the layer's ranges
are of the text as it was.

The bookmarks, the recent files and Go to's recent classes and symbols are parts ([`CIDEUserPart`]) of the
workspace's `.hcp65user` ([`CIDEUserFile`]), which several features write a part of.

## Between files

[`CIDERecent`] keeps the files shown in editors, most recent first and closed ones too, and the places edited (an
edit within three lines of the last is the same place).  It hears of the editor in front from
[`CIDEDocuments::SetActiveTab`], which every change of the active tab goes through, of a closing tab from
[`CIDEContent::PaneClosed`], and of edits from [`CIDEDocuments::DocumentChanged`].  The fifty most recent files are
written to the `.hcp65user` two seconds after they change, only where settings are kept
([`CIDEContent::IsKeepingSettings`]), so a check on a solution in the repository leaves no file beside it.

The Switcher is a [`CIDEChooser`] with nothing to type in whose list hears Control let go.  The tool windows' keys,
Hide All Tool Windows and Jump to Last Tool Window are [`CIDEToolWindows`]: "has the keys" is in front with the
window's focus in the pane, and the last tool window is found as the content ticks ([`CIDEToolWindows::Poll`]), not
from the dock's active pane event, as the tab that comes to the front as a pane is hidden is the active pane for a
moment.  What Hide All hid is kept, each with its dock restore place ([`SIDEHiddenToolWindow`]), until it is put
back.  Building it found a docking bug, fixed in WindowLib ([`CDockRestorer::DockDefault`];
[Docking](../../libraries/windowlib/docking.md)).

## Hierarchies

[`CIDEHierarchy`] is the one Hierarchy tool window, a tab for each hierarchy asked for.  A tab is a
[`CIDEHierarchySearch`] that keeps its rows ([`SIDEHierarchyNode`]) and makes its pane's tree from them; a row is a
results pane's result ([`CIDEResultsPane::AddSymbol`]), so the preview, Enter, Rerun and Next Occurrence are the
pane's.

- Type Hierarchy follows a class's first base class upwards; a class with more says so on its row ("also:",
  [`CIDEHierarchy::NoteOtherBases`]).
- Method Hierarchy's marks are read from the family [`CIDECodeModel::FindImplementations`] and
  [`CIDECodeModel::FindSuperMethods`] give, each one's class its key's scope; "abstract" is read from the
  declaration's text.
- Call Hierarchy finds callers as each row is opened: the tree says when a node opens
  ([`CTreeContainer::GetExpandedEvent`]), and a caller not yet looked into has a "..." row under it so that it can
  be.  A virtual function's callers are followed by the callers of what it overrides, marked "via", leaving out
  calls that name their class ([`CIDEHierarchy::RemoveQualifiedCalls`]: such a call isn't virtual).  Enter doesn't
  open and close a row here ([`CTreeContainer::SetActivationToggles`]).

[`CIDENavigator::FindCaretFunction`] asks the file's outline for the function round the caret before it parses.

## Results windows

Find Usages, Find, Duplicates, Hierarchy, Bookmarks and Problems each show a [`CIDEResultsPane`]: a tree, a
[`CCodePreview`] across a sash ([`CSplitContainer`]), and a toolbar down its left.  Its owner, a
[`CIDEResultsSource`], fills it between `#!text BeginShow` and `#!text EndShow` and says where each result is.  The
pane opens a result on Enter, steps through them, gives the keys back on Escape, and has Rerun, Expand and Collapse
All, Preview, Navigate with Single Click, Autoscroll from Source and Export ([`CIDEResultsPane::AddViewCommands`]).
Its commands are its own, with no keys.

A row's code is coloured by a scan of its own line ([`FindIDELineColours`]: directives, comments, strings,
characters, numbers, keywords), not by what its names are, which would take a parse of every file.  Autoscroll from
Source is told by [`CIDEOccurrences::Poll`] when the active caret has moved to another line.  Typing in the tree
finds a row ([`CListContainer::SetTypeToFind`], the pattern in a note over the corner, [`CScrollPane::SetNote`]),
as in Solution Explorer and the Structure tree; Space opens nothing.  The tree makes rows only for what is in view:
with 20,004 usages, opening every node went from 4800 ms to 1 to 3 ms (Release x64, 2026-11-17), so every file
starts open.

[`CIDEResultsTabs`] is the tabs and the pin, shared by Find Usages, Find and Hierarchy: each search derives from
[`CIDEResultsSearch`] and each owner is a [`CIDEResultsTabsOwner`].  A new search takes the chosen tab unless it is
pinned ([`CIDEResultsTabs::GetSearchForNew`]); a new tab's pane has the settings of the one before.

Next and Previous Occurrence ([`CIDEOccurrences`]) step the [`CIDESteppable`] that has the keys, or from an editor
the one steppable window that can be seen and has something to step to.  The steppables are named in one function,
not added to a list.

## Find Usages

[`CIDEUsages`] keeps every usage it found ([`SIDEFoundUsages`]), each with its project and class worked out when it
is found, as the index can be built again before the tree is regrouped.  Its tree shows those in its scope.
[`CIDEScope`] is shared with Find in Files: made for where the IDE is when something is looked for
([`CIDEScope::Capture`]: the active file, its project, the open files, the directory), so what was found stays the
same whatever is opened after, and its kind can change without looking again ([`CIDEScope::SetScope`]).  The index
is of the whole workspace: a scope leaves usages out, it doesn't look in less.  The usages popup is
[`CIDEUsages::ShowUsagesPopup`]'s chooser, always of every usage.

## Finding text

The search is BaseLib's [`CTextSearch`], given a text and its length: case, whole words, or a regular expression
([`CRegex`], written for the IDE, not `#!text std::regex`), whose `$1` groups the replacement takes.  The Find bar
([`CIDEFindBar`], one for each editor tab, made when first opened) highlights its matches as a layer of the
editor's; of a layer with very many ranges the 2000 nearest the current one are drawn.  It searches again after an
edit by polling at each tick ([`CIDEFindBar::Update`]).  [`CIDEFind`] keeps what every bar shares.

Find in Files is WindowLib's [`CFindInFilesDialog`], whose searcher ([`CFindInFilesSearcher`]) is the IDE's
[`CIDEFindInFiles`].  The search is a step at a time between the dialog's ticks ([`CIDEFindInFiles::SearchSome`]),
starting again with each character typed, so typing never waits.  Each result row is a grid of its own, the line on
the left and its file on the right, each shrinking in proportion to its width, rather than columns of a table (the
user, 2026-11-11).  The preview is coloured by the edits thread ([`CIDEDocuments::HighlightPreview`]), and the
dialog's ticks ask for what it made.  Replace in Files edits the files in their editors, all as one undo group
([`CIDEEdit::BeginUndoGroup`]), and leaves them for Save All, as Rename does.  A result whose text has changed
since it was found is left.

## Find Duplicates

Find Duplicates ([Finding Your Way](../navigation.md#find-duplicates), its
[settings](../settings.md#find-duplicates)) is IntelliJ's Locate Duplicates: where else in a scope the code at the
caret is written, its names apart.  It was built on 2026-11-16 (HCP65-IDE f8289bc) as EndOfPlanTasks.txt's task a.

The comparison is [`CIDEDuplicateMatcher`]: a run of one unit's tokens against a run of another's, token for token,
but for the run's free names (the variables and parameters of the function it is in, named alone, not after `.`,
`->` or `::`), each of which may be another name at the other place, always the same other.  With literals free, a
literal may be another of the same kind.  The names found for the free names are kept in the free names' order and
filled in as [`CIDEDuplicateMatcher::Match`] goes, so a function's statements compared one after another share
them.  [`CIDEDuplicatePlaceFinder`] walks a unit's syntax for where a run could be written again: an expression of
as many tokens, or statements of one block that are as many tokens together.

Extract Method's duplicates ([`CIDEExtractMethod::FindDuplicates`]) were the first user of this kind of comparison,
in their own file only; the comparison was taken out of Extract Method into [`CIDEDuplicateMatcher`], which both
now use.  Extract Method still looks only in the selection's file, and keeps its own checks of what it found (the
same variables given and returned, a function of the same class).

[`CIDEDuplicates`] looks for a selected expression inside any statement, selected statements as a run of
statements, or, with nothing selected, the function the caret is in: the places its whole body is written, then the
runs of its statements written elsewhere ([`SIDEDuplicateGroup`]), longest first, no smaller than
[`CIDEDuplicatesPage`]'s minimum (3 statements, counting those inside them, and 20 tokens to begin with).  The
scope is a [`CIDEScope`], as Find Usages' and Find in Files' are.  Its files are searched a few at a time at each
window tick, and a file is parsed ([`CIDERefactorUnit`]) only if it has every name some statement looked for must
be written with.  The Duplicates tool window is a [`CIDEResultsPane`] with a group for each part of what was looked
for; its places follow edits and Next Occurrence steps through them.  It has no tabs.

## The Go to dialog

Go to Class, File, Symbol and Action is WindowLib's [`CGoToDialog`], a native window, not a popup; its searcher
([`CGoToSearcher`]) is [`CIDENavigator`] ([`CIDENavigator::GoToSearch`]).  Names are matched by BaseLib's
[`CNameMatcher`]: camel humps ("CTA" finds CTextArea), `*` or a space letting the next character be anywhere, and
paths by folder pieces ([`CNameMatcher::MatchPath`]: "wl/tf" finds WindowLib/TextField.h).  Classes and symbols
come from [`CIDECodeModel::FindGoToSymbols`], which leaves out locals (their keys have an `@`); files from the code
model's list of the projects' files.

A modal dialog doesn't run its owner's commands, so [`CGoToDialog::AddTab`] copies the keys of the command that
opens each tab into the dialog's own command set: pressed in the dialog, Control+N, Control+Shift+N and
Control+Alt+Shift+N change the tab and search the text already typed.  With nothing typed the recent items are
given the highest scores: Recent Files' files, the last thirty classes and symbols gone to (kept in the
`.hcp65user`), and the last fifteen actions (kept in the settings: they are the IDE's, not a workspace's).  The
Actions tab lists [`CIDEContent`]'s commands by caption and runs the one chosen at the next tick.

## Checking it

The checks drive all of this through the script ([`CIDEScript`]; [Checking the IDE](../checks.md)) on small
solutions of their own: Checks/Input/Navigation (CShape, CCircle, CSquare and a long file), QuickInfo,
Documentation and Bases.

| Hook | Does |
|---|---|
| `#!text HCP65_IDE_GOTO=<tab>:<pattern>` | Go to with the pattern typed: the best results printed, with the time, and the first opened. |
| `#!text HCP65_IDE_FIND=<scope>:<options>:<text>` | Find in Files (with `#!text HCP65_IDE_FIND_DIRECTORY`, `#!text _MASK`, `#!text _REPLACE`): the first results and preview printed. |
| `#!text HCP65_IDE_DIALOG_SNAPSHOT`, `#!text HCP65_IDE_DIALOG_ANSWER` | A dialog saved as an image, or answered ("Open in Find Window"). |
| `#!text edit:` steps | `#!text controlclick:L:C`, `#!text printchooser`, `#!text chooserbutton`, `#!text gotolist:`, `#!text printinfo` (with the pointer's shape), `#!text printcrumbs`, `#!text printtoolwindows`, `#!text switcher:close` and others. |

A scripted run only exits when `#!text HCP65_IDE_SNAPSHOT` is set.  The script's keyboard (`#!text edit:press:`,
`#!text edit:tap:`) has a fixed list of keys.  A scripted click needs its line in view, and a first pointer step
can run before the layout settles: print the breadcrumbs first.

## Decisions

- Go to (the user, 2026-11-06): Class on Control+N, File on Control+Shift+N, Symbol on Control+Alt+Shift+N, with
  Alt added to key chords for it; no All tab, no Shift Shift, no Text tab; the solutions' files only; a dialog in
  its own window, not a popup.  The same keys pressed in the dialog change its tab (the user, 2026-11-11).
- Find in Files (the user, 2026-11-11): scopes In Solution, In Project (the active file's) and Directory; rows with
  the found text left and the file darker right, each row sized by its own contents, not a table; the code
  previewed under the rows; a button that lists every result in the Find tool window.
- Find Usages' tool window (the user, 2026-11-12): the preview right of the tree, on a new [`CSplitContainer`];
  Enter opens a usage and F4 goes to the declaration whatever row is selected; Space doesn't open a usage as Enter
  does; a tab for each search, which can be pinned; the scopes Solution, Startup Solution, Project, Current File,
  Open Files and Directory, one list with Find in Files, chosen in a settings dialog and on the toolbar,
  remembered, honoured by Find Usages and Find All References but not the usages popup or Rename; Include
  Declarations for Alt+F7, while Shift+F12 always has them; no text occurrences.  The session widened the Space
  ruling to "typed characters go to the editor"; the user corrected it on 2026-11-20 and it was taken out.
- Navigation.txt stays a list: "Keep this as a list; we're not closing all the gaps" (the user, 2026-11-12).
- NavigationPlan.txt's decisions (the user, 2026-11-20): the stages in the order A, B, D, C, E, F, G, H; Build
  moved to Control+F9 so Go to Type Declaration has Control+Shift+B (confirmed 2026-11-26); Show Bookmarks has no
  key (Shift+F11 is Step Out), the tool window is the list; no middle click to go to a declaration; Control+click
  goes to it, taking Control and button one from the selection drag; typing finds a row in every tree, the results
  trees too; marks on the scroll bar; no Go to Test; no count of usages over declarations; breadcrumbs and sticky
  lines both, each with an off setting; an Actions tab in Go to; bookmarks and recent files kept with the
  workspace, Back, Forward and the places edited the session's; one Hierarchy window with tabs; no commands to
  split the editor until asked; no navigation bar; of the refactorings, only the usages popup's callers moved onto
  the shared pieces, the rest when each is next changed.  The user said to build all of it ("Implement everything
  from 1 through to 66").
- Quick Documentation and the tool tip over a name were put off "later if at all" (2026-11-20), then asked for and
  built as [`HCP65IDE/NextIDEWork.txt`]'s item 11 (the user, 2026-11-26).
- The follow-ups (the user, 2026-11-21): Call Hierarchy's "via" callers always shown and marked, only what the
  function overrides followed; Type Hierarchy's "also:" note on the class and any class over it with more than one
  base; tool window keys Alt+2 Bookmarks, 3 Find Usages, 4 Run, 5 Debug, 6 Build, 9 Find, each hiding its window
  when pressed there, Alt+7 and Alt+8 toggling too, Alt+1 as it was, Hide All on Control+Shift+F12 for the session,
  Jump to Last Tool Window with no key.  Hide All leaves floating and auto hidden windows, as a floating one hidden
  would come back docked (told to the user).
- Type Info and Go to Type Declaration on an expression parse and check the file each press: 47 ms in BaseLib's
  CalculatorParser.cpp, 1.7 s in IDEUsages.cpp (2026-11-21), over the plan's 100 ms rule; the user chose "accept
  the wait" over leaving it out or parsing on a thread.  It runs only where the symbol at the caret gives nothing.
- A symbol's declaration and comment are read from its file's text, not a parse: no unit cache was needed.
- Find Duplicates (the user, 2026-11-16, asked for as "Look for duplicates of selected text / function"): Find
  Duplicates... on the Edit menu after Find in Files, on Control+Alt+Shift+D; expressions too, as IntelliJ's, a
  selected expression found inside any statement and a run of statements as a run of statements; a selection looked
  for whole, a function's whole body first and then runs of its statements, longest first, the smallest a setting
  (3 statements and 20 tokens to begin with); literals must match unless the dialog's tick, remembered in Settings,
  lets them differ within their kind.

## Not done

From [`HCP65IDE/Navigation.txt`]'s gaps and what building left:

- Next and Previous Highlighted Error.  Navigation.txt says the editor has no errors to go to; since 2026-11-23 it
  has the check's problems (the Problems tool window, the scroll bar's marks), but no command steps through them.
- A navigation bar, commands that split the editor, Go to Test and the count of usages over a declaration (ruled
  out); Select In offers Solution Explorer and Structure only.
- A macro has no usages, hierarchy or rename through navigation; its `#define` is the first of its name.
- Type Info on an expression waits for a checked parse (1.7 s in a large file); a parse on a thread would remove
  it.
- Method Hierarchy's classes are the class view's, up the first base class only; Call Hierarchy has no scope.
- Quick Documentation reads `//` comments only, not `/* */`, and no markup.
- Parameter Info misses a function a using-declaration brings in.
- Replace in Files has no tick for each result.
- A results row isn't coloured by what its names are.
- Go to's recent actions are the IDE's and its recent classes and symbols the workspace's, not shared between tabs.
- The Duplicates window has one results pane, without [`CIDEResultsTabs`]' tabs and pin; Extract Method's
  duplicates are looked for in the selection's file only.
- Duplicates, Indexing and Claude have no tool window keys; Hide All is the session's and leaves floating windows.
- A TODO tool window.
- Not moved onto shared code (as decided): the refactorings' own "function the caret is in" and unit caches; the
  lists of who follows edits, who is stepped through and who has settings, still named where they are called.
- Not run: an MSVC build of the navigation work (its project files were edited by hand), the IDE on Windows, and a
  real pointer beyond the checks' script.
