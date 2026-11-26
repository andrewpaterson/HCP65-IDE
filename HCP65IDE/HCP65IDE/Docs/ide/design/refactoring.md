# Refactoring

Every refactoring is built on the [code model](code-model.md): what a name is and where it is written comes from
the index, and the types of expressions from the compiler's own type checker, never from the text.  What each
refactoring does is on the [Refactoring](../refactoring/index.md) pages; this page is how they are built, and the
rulings that shaped them.

The intent (the user, 2026-11-14): the IDE refactors C++ as IntelliJ IDEA refactors Java, each refactoring whole,
with its dialog, its preview and its conflicts, and templates planned for from the start rather than left as a
limit.  Every refactoring the plan named is built: IntelliJ's, and C++ ones IntelliJ hasn't (declarations and
definitions, `#include`s, macros, templates, namespaces, pointers and references).

## The building blocks

| Block | Is |
|---|---|
| [`CIDECodeModel`] | What is at the caret and everywhere it is named: [`CIDECodeModel::FindRenameTarget`], [`CIDECodeModel::GetSymbolLocations`], [`CIDECodeModel::FindNeeds`] (what a file needs declared), [`CIDECodeModel::FindIncludesOf`] ... |
| [`CIDERefactorUnit`] | One file parsed as its project builds it, from its editor's text (a header in a unit that includes it), with its syntax tree to find the node at a place, and if asked a kept check of it ([`CCPPSemanticPass`]): the type of any expression, the type written as C++, and how a variable is changed where it is used ([`CCPPSemanticPass::FindChanges`]). |
| [`CIDERefactorUnit::AppendDeclaredType`] | A variable's or function's type copied from its declaration, so [`uint32`] and [`size`] stay what they are: the compiler's types have no typedefs in them. |
| [`CIDETextEdits`] | One file's changes, given against its text as it is now and made as one edit of its editor. |
| [`CIDEFileEdits`] | The changes to several files: each file's editor opened, not shown, when first asked for, and all applied as one undo group ([`CIDEFileEdits::Apply`]). |
| [`CIDEFileEdits::CountErrors`] | The files' errors as they are and as they would be: a refactoring that can't tell by itself whether what it leaves compiles asks before and after, and does nothing if there would be more. |
| [`CIDEChangeSignature::ShowConflicts`] | IntelliJ's Problems Detected, shared by the refactorings: what stands in the way, with Continue and Cancel, before anything changes. |
| [`SIDECaretContext`] | What is at the caret or selected, found once for Show Context Actions; each refactoring's `AddContextActions` offers itself from it ([`CIDEContextActions`]). |
| [`CUndoLog`] | The undo across files (below). |

A refactoring runs in the same order every time: the edited tabs are indexed first
([`CIDEDocuments::UpdateEditedTabsNow`]), so the index has what the editors have; what is to change is found from
the index and the refactor unit; the dialog asks what is the user's to say; conflicts are shown; the edits are
worked out for every file, checked where the refactoring can't otherwise know the code still compiles, and applied
as one group; the status line says what was done.  Nothing changes if a file's editor no longer has the text that
was indexed or searched.

A refactoring that needs to know what the declaration and the definition of a member are to each other asks
[`CIDEDefinitions`] (a member declared in the header, defined in the `.cpp`, or a class template's defined outside
it in the header).  Flow over a selection (which variables it reads, changes and leaves live after it, and how it
exits) is worked out from the syntax and [`CCPPSemanticPass::FindChanges`] by Extract Method
([`CIDEExtractMethod`], [`CIDEExitFinder`]), not from the compiler's liveness, which is the IR's and which a check
doesn't make.

## Rename

[`CIDECodeModel::FindRenameTarget`] finds the symbol at the caret (the name the caret is in or just after, or a
selection inside one name) and every place its name is written: a class's constructors and destructor with it, a
virtual function with what it overrides and what overrides it, and a member with its family (below), and theirs in
turn until nothing new is found.  It refuses an operator, a destructor or a compiler type, and any symbol with a
place it couldn't change: in a file that isn't in the solution, made by a macro, or a call that can't be told from
a call to another overload.

[`CIDERename::RenameSymbol`] then changes each place in its file's editor, opening it unshown if it isn't open, one
edit per file ([`CTextArea::ReplaceRanges`]) and one undo across them all.  It first checks each place still reads
the old name.  Rename's searches, IntelliJ's two ticks, add places the index doesn't have:

- **comments and strings**: the old name as a word in them;
- **text occurrences**: the name as a word in code the index has nothing at ([`CIDECodeModel::FindRoleAt`]), which
  is code an `#if` leaves out in this configuration, and in the projects' `.txt` and `.md` items.

The ticks are remembered apart for types (on) and for everything else (off): a member's name as text is often
another thing's (`CPreprocessor::Kill` found 106 unrelated `Kill`s).  A documentation page's references are places
in the index ([`CPPUT_Documentation`]), so they are renamed as code is, ticks or not.

**Hand written assembly.**  HCP65 programs call compiled C++ from assembly by the compiler's labels, so Rename also
changes the solution's `.asm` and `.inc` items ([`HCP65IDE/IDERenameAssembly.h`]): the name where a label has it
(`#!text _CFatVolume__Init`), a type's name and length in a parameter list or template argument
(`#!text PC17CFatInfoSectorCache`, its length and the lengths of the names round it), generated layout constants
(`#!text SIZEOF_CFatVolume`, `#!text CFatVolume_muiID`), and the name in `;` comments.  How a symbol is named is
its kind ([`EIDEAssemblyRename`]).  An overloaded function's label is changed only when its mangled parameters fit
this overload and no other; one that could be another's is left, and counted on the status line.  A function inside
`extern "C"` is labelled by its name alone (the index flags its declarations).  Rename finds labels as written, for
either processor.  Change Signature, and Convert Function to Member Function and back, ask the compiler for a
function's label before and after ([`CIDERefactorUnit::AppendFunctionLabel`], [`CIDEChangeSignature::FindLabels`]),
for both processors whatever configuration is chosen.

**A class's files.**  A type with a one letter prefix whose files have its stem is renamed with them when Rename's
"Rename files" tick is on (below).

## Rename File

[`CIDEFileRename`] renames files in their directories and changes everything that names them, as IntelliJ's rename
of a file does:

1. Every `#include` that finds the file, found as the compiler would find it ([`CIDECodeModel::FindIncludesOf`],
   each resolved from its own file and project), so `"Preprocessor.h"`, `"CppCompilerLib/Preprocessor.h"` and
   `"../CppCompilerLib/Preprocessor.h"` all change and an `#include` of another file of the same name doesn't.  The
   files are scanned as text, so `#include`s in code an `#if` leaves out change too.  Only the name changes.
2. The documentation pages' links and snippet paths that name it (through the website's map, [`CDocsSite`]).
3. The item in every project that has it, in the `.vcxproj` and its `.filters` ([`CVcxProject::RenameItem`]).  Not
   optional: the Linux builds read their sources from the projects' items.
4. The file on disk, with `git mv` where it is tracked ([`CGitRepository::Move`]).
5. Its editor tab, the undo log ([`CUndoLog::Renamed`]), and the code model, whose projects are made again
   ([`CIDECodeModel::Reset`]): their header lists were stale.

Everything is checked first (the new names are file names, none is taken on disk or in a project, every file is in
the workspace), and nothing changes if one check fails.  The files edited are saved, since the files on disk moved.

A type's files are found by one rule ([`CIDEFileRename::FindClassFiles`]): its name without the one letter prefix
is the stem (`CPreprocessor`: `Preprocessor`), and its files are `Stem.h`, `.hpp`, `.inl`, `.cpp` and `.c`, and the
house style's platform parts `Stem_Anything.cpp`, in the directory of the file that declares it, in the workspace's
projects, the index having it declared in one of them.  A file that only shares the name is never taken.  Rename
File offers the same files as a file's related files.  The new stem is the new name without the same prefix
([`CCPPPreprocessor`]: `CPPPreprocessor_Linux.cpp`).

## Refactorings that make and delete files

Move Class, Move to File, Copy Class, Extract Superclass, Interface and Class make files; Safe Delete deletes a
class's files when nothing else is left in them.  A file made is an item of every project the old one is an item
of, beside it ([`CVcxProject::AddItemBeside`]), a line of a `CMakeLists.txt` that lists the old one, staged in git
([`CGitRepository::Add`]), and a step of the undo log ([`CUndoLog::Created`]), so undo takes it away again (off the
disk, out of the projects, unstaged) and redo makes it again with its text.  A file deleted is the same the other
way round ([`CUndoLog::Deleted`]).  New files are written from the [file templates](../templates.md), so they come
out as the New commands make them.

## Templates

Templates are a large part of this code (the user, 2026-11-14), and a name in a template's body is one of three
kinds: one that doesn't depend on the arguments (`miUsedElements`), one that depends only on the template's own
shape (`mcArray.Add(x)` where `mcArray` is a `CArrayTemplate<M>`), and one only an instance decides (`pv->Init()`).

- **Every instance is evidence.**  A name only an instance decides is a reference of what it resolves to in each
  instance, in whichever unit the instance is.  Each instance's resolution is kept
  ([`CCPPSemanticPass::GetNextResolved`]) and flagged in the index, so Find Usages of `CFoo::Init` has `mp->Init()`
  in `CHolder<M>`.
- **Families.**  That `mp->Init()` ties together every class `CHolder` is instantiated with: an interface nobody
  wrote.  [`CCPPIndex::FindFamily`] gives the members that share such a place, and Rename, Change Signature and
  Make Member Function const change the family together, as a virtual function's overrides are.  Write Template's
  Interface ([`CIDETemplateInterface`]) writes the family down, as a comment or an abstract class.
- **The template as itself.**  The type checker checks a template's body once with each type parameter a
  placeholder class written as its own name ([`CCPPInstantiator::PlaceholderClass`],
  [`CCPPCheck::CheckTemplatesAsThemselves`]), so a refactoring in a template writes types in the template's words
  (`M*`, not `CFoo*`).  What only an argument would have isn't there, so such an expression has no type, and a
  refactoring that would need it says its type depends on the template's arguments.
- **Where template code goes.**  A class template's member defined outside it is written
  `#!text template<class M> void CArray<M>::Add(...)` in the header, never in a `.cpp`.
- **Classes a macro made.**  BaseLib's arrays and strings were families of classes made by including one text with
  different macros: templates in what they did, macros in what they were, with no source of their own.  They were
  made real templates instead of teaching the refactorings about them (below).

## Undo

The editors' undo is Logicim's kind: a snapshot of the text after each edit ([`CTextUndo`], on BaseLib's
[`CUndoStack`], which LogiCim's circuit editor uses too; see [the text area's
undo](../../libraries/windowlib/text-area.md#undo)).  Across files the IDE keeps [`CUndoLog`] ([`CIDEEdit`]): every
document's steps in the order they were made, as IntelliJ's global undo.  The edits made between
[`CUndoLog::BeginGroup`] and [`CUndoLog::EndGroup`] are one group, and undoing any of them, Ctrl+Z in any of the
files, undoes them all.  A step can also be a file renamed, made or deleted, undone by renaming it back, taking it
away or making it again.  The log only records: each editor undoes its own step with its own stack and tells the
log.

## What they refuse

- **Code a macro made.**  It has no source of its own, so a refactoring refuses where its selection or a place it
  would change is in a macro's expansion (the index flags such names), and says so.  Convert Macro and Inline Macro
  find a macro's `#define` and uses by its name in the files' texts, since macros aren't in the index.
- **Files outside the solution.**  A place in a file the workspace's projects don't have can't be changed; Rename
  refuses the symbol.
- **What can't be told apart**: a call the parser can't tell the overload of, an assembly label that fits more than
  one overload (left, and counted).
- **What wouldn't compile.**  Optimise #includes, Convert Macro, the pointer and reference conversions, Make Member
  Function const and static and the function and member function conversions check the changed files with
  [`CIDEFileEdits::CountErrors`] and change nothing if there would be more errors.  The check is the IDE's, not a
  build: a net, not a proof.
- **Code an `#if` leaves out**, and files only another configuration builds, aren't in the index: Rename's text
  search finds them, as unsure.

## Decisions

The refactorings' rulings are the user's unless said; most were answers to questions asked while building them
(2026-11-14 to 16).

**The plan**

- No half-measures (2026-11-14): "The aim is to make the HCP65 fully useful".  Each refactoring is IntelliJ's
  whole, built in checked steps, and the work is not ordered by what is cheapest.
- Templates are planned for from the start (2026-11-14), not a limit: every refactoring says what it does in one.
- A template family is renamed all together or not at all (2026-11-15): renaming one alone leaves the template not
  compiling for the others, so there is no choice to offer.
- Classes a macro makes become real templates rather than the refactorings learning them (2026-11-15):
  `CArrayBlockBase<SIZE>`, `CArrayTemplateBase<M, BLOCK>`, `CCharsBase<STRING, TEXT>` and the rest, their old names
  kept by classes deriving from them (`CommonLibraries/Libraries/BaseLib/ArrayTemplatesPlan.txt`, done 2026-11-15).
- Show Context Actions (Alt+Enter) was put at the top of the queue (2026-11-15); every refactoring added since has
  an `AddContextActions`.
- Commit the IDE's changes before trialling a refactoring on real code (2026-11-13).
- The IDE's checker is made stricter rather than a build run after a refactoring (2026-11-15: "the user can
  recompile after they refactor"): a type used before it is declared is an error in every kind of declaration
  ([`CCPPCheck::ReportUndeclaredTypes`]).
- Templates are checked as themselves in the index too, once measured at 6% (2026-11-15;
  [Speed](code-model.md#speed)).
- WindowLib got an editable table ([`CTable`], 2026-11-15) before the refactorings went on, and the dialogs of
  Change Signature, Pull Members Up, Push Members Down and the Extracts are made of it.

**Rename and Rename File**

- An editor right-click menu with Rename... (greyed where nothing can be renamed) and a Refactor menu (2026-11-06).
- Rename's two searches, as IntelliJ's (2026-11-13, the comments one asked for by the user).
- Rename File (2026-11-13): a tracked file is renamed with `git mv`; the files it edits are saved; it is undoable,
  the whole refactoring one group; a type's files are the stem rule's only (`PreprocessorHeaderCache.cpp` keeps its
  name though it has `CPreprocessor`'s members).
- Change Signature's labels are changed for both processors, whatever configuration is chosen (2026-11-15).

**What they write**

- Locals are declared at the top of the function, lined up with the others, always (2026-11-15).
- Constants are `#define`s, as the code's are; Introduce Constant has a tick to replace every occurrence in the
  file, off, and doesn't offer the header (2026-11-15).
- Introduce Variable's suggested names follow the code's prefixes (2026-11-16: no change).  Replacing every
  occurrence warns in Problems Detected where something in the expression changes between them (2026-11-15).
- Introduce Field has "assign it in Init" as a choice (2026-11-16); there are Init functions here, not
  constructors.
- Introduce Parameter may replace a parameter the expression is the only use of (2026-11-16).
- Introduce Typedef in a header can name the type in the files that include it, a tick, off, the files shown first
  (2026-11-15); a class template's typedefs go together at its top (2026-11-16).
- A new definition goes after the definition of the function declared before it, under a banner like its
  (2026-11-16:  no change); Move Definition into Class keeps one statement of up to 60 characters on the
  declaration's line (2026-11-16: no change).
- Extract Method passes what it changes by pointer, a tick on by default (2026-11-15), a plain pointer with no
  `const`, named with a `p` before its name; the new function is protected, with a Static tick where it uses
  nothing of the object; it finds duplicates of the selection in its file and offers to replace them (2026-11-15).
- Inline (Ctrl+Alt+N) is one command for variables and functions (2026-11-16); Inline Variable has no dialog, undo
  is one key (2026-11-15); a constant of a file or class can be inlined (2026-11-15).  Inline Function gives a
  changed parameter a variable and takes a call out of an expression (2026-11-15); a call on `&cOther` inlines as
  `cOther.Kind()` (2026-11-16).
- A pulled-up field goes after the base class's last field (2026-11-15); a function that comes to override one is
  marked `override` (2026-11-15); a member added to a class goes in the column the class keeps for virtual and
  static (2026-11-15).
- Extract Class writes an empty `Init` and a call of it; its members stay public (2026-11-15).  An extracted
  interface has no virtual destructor (2026-11-15).
- Encapsulate Field's getter is `const`, and both accessors are one line each at the bottom of the class's `.cpp`
  (2026-11-15).
- Change Signature leaves a kept parameter one name everywhere, and Unify Parameter Names does only that
  (2026-11-15).
- Undo takes away the files a refactoring made (2026-11-15), and they are staged in git when made (2026-11-15).
  Safe Delete deletes a class's emptied files outright, not to a recycle bin (2026-11-15).
- Optimise #includes sorts, and Sort #includes is gone (2026-11-15): `<>` first, then each library's, the lowest
  library first (StandardLib's [`COrderer`] orders them), then the file's own directory's; it shows the other files
  it would give an `#include` before changing them (2026-11-15).  Forward declarations go after the `#include`s, in
  `.cpp` files too (2026-11-16).
- Convert Macro keeps the one-line form, the macro's name and its brackets (2026-11-15); renaming is a separate
  step of the user's.
- Convert Pointer to Reference doesn't rename; a reference is named as the object it refers to; a field may be made
  a pointer, but a pointer field is never made a reference: the classes here set fields in `Init` (2026-11-15).
- Extract Template Parameter on a class keeps the old name as a class deriving from the template with only its
  constructors, not a typedef, so its forward declarations stay valid; the compiler warns where the instance is
  written out under that class ([`CCPPCheck::ReportNamedInstances`]) (2026-11-15).  Remove Template Parameter folds
  such a class back (2026-11-15).
- Convert Enum to Enum Class writes the casts its values need, with a tick to refuse instead (2026-11-16).
- Write Template's Interface's class form offers, unticked, to make the classes used with it derive from it; the
  name is `C<Template>Interface` (2026-11-16).
- Move to Namespace is a command of its own, not F6's (2026-11-16); uses are qualified by default (2026-11-16); a
  new namespace block is in the house style (2026-11-16); a class's old namespace's functions that take it are
  offered to move with it, ticked (2026-11-16).
- Copy Class has no key (F5 is Start Debugging here) and is on Alt+Insert's New menu; the class's name in comments
  in the copy is the new one (2026-11-16).
- Generate Definition has CLion's Ctrl+Shift+D; the others have no keys, Alt+Enter offering them (2026-11-15).
- Move to File's field completes over the solution's headers, and a button opens a chooser of the solution's files
  (2026-11-15).
- New files are written from file and code templates, as IntelliJ's and CLion's (2026-11-16); built
  (`HCP65IDE/CodeTemplates.txt`).

## Not done

From the plan's last state (`HCP65IDE/Refactorings.txt`, 2026-11-16):

- **Specialisations.**  A template and its explicit and partial specialisations aren't taken together: an explicit
  specialisation of a function template is another symbol to the index, and Change Signature doesn't change it.
- **Constructors** in Change Signature are told apart by how many arguments an object is made with; where another
  constructor takes as many the object is left.  A data member no constructor initialises isn't given an
  initialiser.
- **Safe Delete**: not from Solution Explorer on a file; a member of a template nothing instantiates doesn't say
  its uses may be incomplete; a parameter is refused (it is Change Signature's).
- **Move**: Pull Members Up and Push Members Down don't add the `#include`s the target class then needs, don't
  offer nested types and enums, and don't check a member's use of the source template's parameters; members going
  down to more than one class keep their assembly labels.  Move Class refuses a class defined across several source
  files (`Preprocessor_Linux.cpp` and `Preprocessor_Windows.cpp`) and one that each needs the other with its
  header, and leaves a source file it emptied with only its `#include`s.  Move to File refuses a static function
  and makes no new file.  What is in an unnamed or inline namespace can't be moved.
- **Extract Method**: a variable declared in the selection and used after it is handled only when it is the one
  returned; no name is suggested.
- **Inline Function** refuses a function that returns before its end, and a function template.
- **#includes**: the compiler's own headers aren't followed, so `<>` `#include`s are never touched; a class
  template is never forward declared; the files given an `#include` aren't checked themselves.
- **Convert Macro**: a macro `#define`d more than once or in the compiler's headers; variadic macros (inlined, not
  converted); only the first 30 files that use a macro are checked.
- **Templates**: Extract Template Parameter on a function or class that is a template already, and its inverse for
  a function template (its arguments aren't written at its calls); a class made a template again by it keeps its
  members inline in its header.  Assembly labels for a template argument that is a language type, for a function
  made a template, for overloads of template members and for a type nested in a class template
  (`#!text Outer<T>::Inner`): such labels are left, as unsure.
- **Make Member Function const** of a family has no check input.
- **Rename**: no preview of the non-code places it finds, and files outside the workspace's projects (plans, notes)
  aren't searched.
