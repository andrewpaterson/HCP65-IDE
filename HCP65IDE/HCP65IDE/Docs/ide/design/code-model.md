# The Code Model

What the IDE knows of the code is an index of every name the workspace's C and C++ files declare and use, built by
the compiler's own front end as the build configuration compiles each file.  Navigation, Find Usages and every
refactoring ask it; none of them reads C++ as text.

| Part | Where | Does |
|---|---|---|
| [`CCPPIndex`] | CppCompilerLib | The index: each symbol's places, the classes' bases and the virtual functions. |
| [`CCPPSemanticPass`] | CppCompilerLib | Types a unit's expressions with the compiler's type checker, so `pc->Init()` and an overloaded call are resolved. |
| [`CWorkspaceIndex`] | IndexLib | A workspace indexed as its configuration compiles it, on worker threads, and its documentation pages. |
| [`CIDECodeModel`] | the IDE | Derives from [`CWorkspaceIndex`]: the edits thread and the queries the IDE's features make. |
| [`CIDERefactorUnit`] | the IDE | One file parsed with a kept check, for a refactoring ([Refactoring](refactoring.md#the-building-blocks)). |

## The index

[`CCPPIndex`] is the compiler's, not the IDE's: the IDE drives it and shows what it finds.  A symbol is the same
symbol in every translation unit that names it, matched by its key ([`CCPPSymbolKey`]): its qualified name, a
function's with its parameter types (`#!text CChars::Append(char const*)`), and a local's the place it is declared
(`#!text @File.cpp:12:5`).

Each symbol ([`CCPPIndexSymbol`]) has its places ([`SCPPIndexLocation`]): the file, line, column and length of each
time its name is written, with

- its role ([`ECPPIndexRole`]): a declaration, a definition or a reference;
- its usage ([`ECPPUsageType`]): a call, a read, a write, a base class, a field's type, a template argument, a
  documentation page's reference and so on, worked out from the syntax round it, as IntelliJ groups Find Usages;
- the class or function definition it is in;
- flags: a call the parser can't tell the overload of (a reference of each it could be), a name a macro made, a
  declaration inside `extern "C"`, and a name in a template's body that an instance resolved.

The class hierarchy is kept from what each file says (a class's bases, a function declared virtual), so it can be
forgotten with the file ([`CCPPIndex::RemoveFile`]); [`CCPPIndex::FindOverrides`] answers from it.
[`CCPPIndex::FindFamily`] answers from the places an instance resolved: a family is the members of different
classes one template's body names by one name (`mp->Init()` in `CHolder<M>`), which a refactoring must change
together.

A unit is gathered without touching the index ([`CCPPIndex::Gather`] into a [`CCPPIndexUnit`]), so units are
gathered on several threads at once, and then merged ([`CCPPIndex::Merge`]) one at a time.  A header's names are
the same in every unit that includes it, so a unit skips the names of a file another unit has indexed already
([`CCPPIndexedFiles`]); it keeps the names its semantic pass resolved there, because a template's body is typed
only in the units that instantiate it.

The parser alone can't say what `o.Init()` names or which overload `f(x)` calls.  [`CCPPSemanticPass`] runs the
compiler's own type checker over the unit, checking only (target [`CPPT_Host`], no code made) and going on past
what it can't type, and gives each such name the symbol and overload it resolved to.  A name in a template's body
can resolve to something different in each instance, and each is kept.  The index also checks each template as
itself ([`CCPPSemanticPass::SetTemplatesAsThemselves`]), so what a template nothing instantiates names is in it
too.

## How a workspace is indexed

[`CWorkspaceIndex`] indexes the workspace the way its build compiles it:

- **The configuration.**  One name for the workspace ("Release|W65C816"); each solution builds its own solution
  configuration of that name, and each project the project configuration the solution maps it to, else its first.
  Choosing another configuration indexes again ([`CIDEConfigurations`]).
- **The projects.**  A CppCompilerLib project ([`CProject`]) for each of the workspace's, once however many
  solutions have it, with the project configuration's include directories and defines
  ([`CVcxProject::GetIncludeDirectories`], [`CVcxProject::GetPreprocessorDefinitions`]), and the defines MSVC makes
  itself (`_MSC_VER`, `_WIN32`, `_WIN64` for x64, `_DEBUG` for Debug).  The build configuration's compile options
  ([`CBuildConfiguration::SetCompileOptions`]: exceptions, the conversions allowed) go to the type checker, so a
  file is checked as it is built.
- **The system headers.**  The compiler's own headers on this machine come after the project's directories
  ([`CWorkspaceIndex::AddDefaultSystemDirectories`], [`FindSystemHeaderDirectories`]).  On Linux MinGW's headers
  stand in for the Windows SDK's, so the index also defines GCC's names for the target (`__x86_64__`, `__i386__`),
  without which MinGW's `winnt.h` stops with an `#error`.  If they aren't installed, almost nothing parses, and
  [`CWorkspaceIndex::IsMissingSystemDirectories`] says so.
- **The files.**  Every `ClCompile` item that is C or C++ ([`IsIndexedSourceFile`]; an HCP65 project's `.asm` items
  aren't), each a job on 8 worker threads.  Headers are indexed through the units that include them, their
  preprocessed output kept for all the units in one [`CPPHeaderCache`].  The first unit to index a header is its
  includer ([`CWorkspaceIndex::GetIncluder`]): an edited header is parsed in that unit.
- **Problems.**  What a unit couldn't include is kept once and shown on the Indexing tab ([`CIDEIndexing`]), with
  the time the whole index took.

`hcp65index` makes a [`CWorkspaceIndex`] the same way, without a window, for the website's symbol table.

## The edits thread

The index is built from the files on disk; an open file with edits is indexed again from its editor.  That is
[`CIDECodeModel`]'s own work, on one edits thread beside the indexing threads:

1. A tab notices its editor changed (its change count).  Once it has been left alone for 400 ms
   ([`CIDEEditorTab::TakeEdits`]), or at once before Rename, Find Usages, Go to or a refactoring asks the index
   ([`CIDEDocuments::UpdateEditedTabsNow`]), a copy of its text is given to the edits thread
   ([`CIDECodeModel::ProcessEditedFiles`]).
2. The thread parses the file once and makes everything from that parse: the highlights, the syntax ranges, the
   outline (breadcrumbs, File Structure), the index's places for the file (its old ones forgotten), and the
   problems a build would find (the Problems tab's Current File).  A header is parsed in the unit that includes it,
   with what comes before it there ([`CWorkspaceIndex::SetHeaderText`]); only the headers' cached output that
   depends on it is forgotten.
3. The UI thread takes the results ([`CIDECodeModel::TakeEditedFiles`]) and applies the highlights, outline and
   problems only if the editor hasn't changed since its text was copied.

An edited tab is indexed again only once the workspace's indexing has finished, and again whenever the index is
built afresh from the files.  A documentation page edited is resolved again the same way.

## Queries

[`CIDECodeModel`] answers the IDE's features from the index, copying places out of it ([`SIDELocation`]) so nothing
holds the index's own memory: [`CIDECodeModel::FindDefinition`], [`CIDECodeModel::FindReferences`],
[`CIDECodeModel::FindRenameTarget`], [`CIDECodeModel::FindOverriders`], [`CIDECodeModel::FindImplementations`],
[`CIDECodeModel::FindSuperMethods`], [`CIDECodeModel::FindCallers`], [`CIDECodeModel::FindClassBases`],
[`CIDECodeModel::FindGoToSymbols`] and the rest.  The `#include` queries read the files' texts, not the index, and
find an `#include` as the compiler would ([`CIDECodeModel::FindIncludeFile`]: beside the includer for `""`, then
the project's directories, then the system's), so Rename File and Optimise #includes change only the `#include`s
that find the file.

## Code completion

Completion needs the type of the expression before a `.`, `->` or `::` in the time of a key, and a checked parse of
the file takes 47 ms in a BaseLib file and 1.7 s in `IDEUsages.cpp` (2026-11-21).  So it has two paths, both asked
at once (built 2026-11-26, [`HCP65IDE/NextIDEWork.txt`]'s item 5; the user page is
[the editor's](../editor.md#code-completion)):

- The light path ([`CIDELightTyper`]) works out the expression from the text and the index alone, with no parse:
  names, members, calls by what they return, indexings, `this`, casts, typedefs.  It answers only what it is sure
  of, so the list opens at once when it can.
- The kept parse ([`CIDEExpressionTyper`]) checks the file on the edits thread ([`CIDECodeModel::RunEditsJob`]) with
  the expression put in brackets where it was written and the line finished, so nothing unfinished is left for the
  parser: IntelliJ's dummy name done the other way round.  Where the light path had nothing, the list opens empty
  with a note that it is working, and closes if the parse has nothing either.

The list ([`CIDECompletion`]) is WindowLib's [`CChooserPopup`] driven by the editor, which keeps the keys; it only
grows ([`CChooserPopup::ItemsChanged`] keeps the selection).  The index has no access, so [`CIDEClassAccess`] reads
a class's from its text.  Complete Code by Type ([`CIDEExpectedTypes`]) uses the light path only and doesn't wait.
Members of a `CModPlayer*` took 15 ms from the key and the names in scope starting `C` (21,709) 33 ms (Release x64,
the ModPlay workspace, 2026-11-26); before the index was asked only for the names wanted, the member list took
385 ms.

## Documentation pages

A project's Markdown items are documentation pages ([Documentation Pages](../documentation.md)).  Once the C++ is
indexed, each page's references to code are resolved by [`CReferenceResolver`], the same code the website's build
runs through `hcp65index -resolve`, and each becomes a place of what it names with usage [`CPPUT_Documentation`]
([`CWorkspaceIndex::IndexPages`]).  So Find Usages lists the pages, and Rename changes them, as it changes code.

What the workspace doesn't declare is looked up in the website's symbol table
([`CWorkspaceIndex::SetSiteSymbols`]):  such a reference links, but is no place in the index.  The website is the
`mkdocs.yml` that is an item of a project ([`CWorkspaceIndex::FindDocsSite`]), or else one a project's properties
name ([`CIDECodeModel::FindDocsSite`]).

## Speed

Measured 2026-11-06 on the 782 units of HCP65Kernal with 8 threads, a Linux Release build with frame pointers, by
instruction counts (other sessions loaded the machine): the startup index went from 1002G to 593G instructions,
14.1 s to 9.6 s on a quiet machine, with the index the same but for overloads now resolved.  The fixes:

- the syntax tree's child loops were quadratic, and are a visitor ([`CCPPSyntaxChildVisitor`]);
- a symbol's key is cached for the unit ([`CCPPIndex::GetUnitKey`]);
- the type table is looked up by kind and base chain, in the same first-match order;
- a header another unit indexed isn't gathered again ([`CCPPIndexedFiles`]), but the semantic pass's names in it
  are.

What is left is structural: every unit parses and type checks all its headers again (parse about 35% of the time,
the semantic pass about 45%).  Only a cache of headers across units would cut that.  16 threads took 11.6 s against
8's 13.6 s on the same run, for 700 MB more: not worth it.

Checking each template as itself in the index was measured on 2026-11-15 on HCP65ModPlay.sln (793 files,
Debug|W65C816, a Release x64 IDE, three runs each): 25.1 to 25.3 s without, 26.7 to 27.0 s with, about 6% more.  It
is on.

## Decisions

- The index and the semantic pass live in CppCompilerLib; the IDE only drives them and shows what they find (the
  user, 2026-11-04).
- A symbol is the same across units by its qualified name and signature, a local by its declaring place (the user,
  2026-11-04): the user chose the signature over the first declaration's place, knowing a signature's types need
  normalising (typedefs, where `const` is written).
- The semantic pass reuses the compiler's type checker, the front end the 65C64 and W65C816 back ends share, rather
  than a checker of its own (the user, 2026-11-06), so anything the front end gets wrong shows in both the IDE and
  the build.
- The indexing setup is a library, IndexLib's [`CWorkspaceIndex`], which [`CIDECodeModel`] derives from, so the IDE
  and `hcp65index` index a workspace identically (the user, 2026-11-24; `HCP65/Website/DocsPlan.txt` decision 20).
- Templates are checked as themselves in the index: 6% more time (above), turned on (the user, 2026-11-15, having
  asked for it to be measured).
- 8 indexing threads, not 16 (2026-11-06, above).
- A 65 configuration is to be indexed as `hcp65build` compiles it: none of the host's headers or MSVC's defines,
  the target's own defines from the code the build uses (the user, 2026-11-26).  Not built yet.
- Claude's tools on the index (find definition, find usages, the pages that cover a symbol) are to be served by the
  running IDE only, whose index is live and has the unsaved edits (the user, 2026-11-24; `DocsPlan.txt` decision
  18).  Not built yet.

## Not done

- A cache of parsed headers across units, the one large cut left in indexing time.
- A 65 configuration is indexed with the host's system headers and MSVC's defines, so code in `#ifdef _WIN32` is
  indexed as live and the 65 branch beside it isn't, and Find Usages and Rename can miss what is built
  ([`HCP65IDE/NextIDEWork.txt`], 9b).
- Code an `#if` leaves out in the chosen configuration isn't in the index, and a file only another configuration
  builds is only text.  Macros aren't in the index at all: what needs them reads the files' texts.
- A partial specialisation isn't checked as itself, and a template's value parameter is 1 in the check
  (`M maItems[N]` is an array of 1).
- After a modified tab is closed without saving the file is indexed again from the disk
  ([`CIDEDocuments::ReindexDiscarded`]), but places others keep in it (Find Usages' results, Back's) stay where the
  discarded edits moved them ([`HCP65IDE/NextIDEWork.txt`], 9a).
- The index over MCP for Claude (`DocsPlan.txt` H4).
