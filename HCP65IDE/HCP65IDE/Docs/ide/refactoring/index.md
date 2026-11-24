# Refactoring

The IDE refactors C++ the way IntelliJ IDEA refactors Java: a change of structure (a rename, a move, an
extracted function) is made everywhere it has to be made, across every file of the workspace, as one step
you can undo.  It works from the code model's index of every name and from the compiler's own type checking,
not from text, so `Init` the member of `CTank` is renamed without touching `Init` the member of `CPipe`.
The type checking is done with the active build configuration's compiler options, the same ones the build uses,
so a program built with exceptions (or multi word casts, or C conversions) is read as it is built.

## How to start one

- **Alt+Enter, Show Context Actions.**  A popup under the caret lists the refactorings that make sense for
  what the caret is on, or what is selected: on a member function, Make Member Function static; on a
  pointer, Convert Pointer to Reference; on selected statements, Extract Method; Rename wherever there is
  something to rename.  Choose one and it runs as its own command does.  This is the quickest way to find
  what can be done.
- **The Refactor menu**, in the editor's right-click menu.
- **A key**, for those that have one (below).

Each works on the symbol at the caret (the name the caret is in), or on what is selected.

## The Refactor menu

| Refactoring | Keys | Page |
|---|---|---|
| Rename... | Shift+F6 | [Rename](rename.md) |
| Rename File... | | [Rename](rename.md#rename-file) |
| Safe Delete... | Alt+Delete | [Safe Delete](safe-delete.md) |
| Introduce Variable... | Ctrl+Alt+V | [Introduce](introduce.md#introduce-variable) |
| Introduce Field... | Ctrl+Alt+F | [Introduce](introduce.md#introduce-field) |
| Introduce Constant... | Ctrl+Alt+C | [Introduce](introduce.md#introduce-constant) |
| Introduce Parameter... | Ctrl+Alt+P | [Introduce](introduce.md#introduce-parameter) |
| Introduce Typedef... | Ctrl+Alt+K | [Introduce](introduce.md#introduce-typedef) |
| Inline... | Ctrl+Alt+N | [Inline](inline.md) |
| Change Signature... | Ctrl+F6 | [Change Signature](change-signature.md) |
| Unify Parameter Names | | [Change Signature](change-signature.md#unify-parameter-names) |
| Generate Definition | Ctrl+Shift+D | [Declarations and Definitions](definitions.md#generate-definition) |
| Generate Declaration | | [Declarations and Definitions](definitions.md#generate-declaration) |
| Move Definition out of Class | | [Declarations and Definitions](definitions.md#move-definition-out-of-class) |
| Move Definition into Class | | [Declarations and Definitions](definitions.md#move-definition-into-class) |
| Pull Members Up... | | [Moving Members](members.md#pull-members-up) |
| Push Members Down... | | [Moving Members](members.md#push-members-down) |
| Move... | F6 | [Moving to Other Files](move.md) |
| Copy Class... | | [Moving to Other Files](move.md#copy-class) |
| Move to Namespace... | | [Namespaces](namespaces.md) |
| Extract Method... | Ctrl+Alt+M | [Extract](extract.md#extract-method) |
| Extract Superclass... | | [Extract](extract.md#extract-superclass) |
| Extract Interface... | | [Extract](extract.md#extract-interface) |
| Extract Class... | | [Extract](extract.md#extract-class) |
| Encapsulate Field... | | [Converting](convert.md#encapsulate-field) |
| Optimise #includes | Ctrl+Alt+O | [#includes](includes.md) |
| Convert Macro... | | [Converting](convert.md#convert-macro) |
| Convert Pointer to Reference | | [Converting](convert.md#pointers-and-references) |
| Convert Reference to Pointer | | [Converting](convert.md#pointers-and-references) |
| Convert Enum to Enum Class... | | [Converting](convert.md#convert-enum-to-enum-class) |
| Write Template's Interface... | | [Templates](templates.md#write-templates-interface) |
| Make Member Function const | | [Converting](convert.md#make-member-function-const) |
| Make Member Function static | | [Converting](convert.md#make-member-function-static) |
| Convert Function to Member Function | | [Converting](convert.md#convert-function-to-member-function) |
| Convert Member Function to Function | | [Converting](convert.md#convert-member-function-to-function) |
| Extract Template Parameter... | | [Templates](templates.md#extract-template-parameter) |
| Remove Template Parameter | | [Templates](templates.md#remove-template-parameter) |

An item is greyed out where it can't be used (Extract Method with nothing selected).

## How they all behave

**One undo.**  Everything a refactoring changes, in however many files, is one step of the undo history:
one Ctrl+Z puts it all back, files it made taken away and files it renamed renamed back.  See [Undo and
redo](../editor.md#undo-and-redo).

**The changes are made in the editors.**  Each changed file is opened in an editor (not necessarily shown) and
changed there, so you can look before saving; Ctrl+Shift+S saves them.  Those that make or rename files (New,
Rename File, Move Class, Copy Class, Extract Superclass, Interface and Class, and Rename of a class whose files
are renamed with it) save what they change, since the files on disk move.

**A dialog first, where there is a choice.**  The name, the options, the members to move.  Its OK button is
**Refactor**, and it isn't allowed while what is typed can't be used: the reason is shown under the fields
("Another function of the class has this name").

**Problems Detected.**  What could go wrong is found before anything changes, and listed: a name that would
clash, a use that would stop compiling, an argument that would be worked out twice.  **Continue** goes ahead
anyway; **Cancel** changes nothing.

**Checked by the compiler.**  Many refactorings check the files they would change as they would be, with the
compiler's own type checking, before changing them.  If the files would have more errors afterwards than they
have now, nothing is changed and the status line says why.

**Refused, with why.**  A refactoring that can't be done says so on the status line ("Can't introduce it: the
expression uses its class's members, which a call of the function names another way.") and changes nothing.

**The status line says what was done**: "Moved Geometry::CPoint and Geometry::Area to Shapes: 9 uses; 5 labels
in assembly changed."

**The house style.**  What a refactoring writes is laid out as the code round it is: declarations at the top of
a function, names lined up with tabs, the banner of slashes over each definition, one-line functions at the
bottom of a `.cpp`.  The pieces it writes come from the [Code templates](../templates.md#the-templates), which
can be changed.

## Templates

C++ templates are a large part of this code, so the refactorings understand them:

- **Every instance is a use.**  A name in a template's body (`mp->Init()` in `CHolder<M>`) is a use of whatever it
  is in each instance: Find Usages and Rename of `CTank::Init` include `mp->Init()` in `CHolder<M>` where some
  `CHolder<CTank>` is made.
- **Families.**  That one `mp->Init()` ties together `CTank::Init`, `CPipe::Init` and every other class `CHolder`
  is made with.  Renaming `CTank::Init` alone would break `CHolder<CPipe>`, so Rename, Change Signature and Make
  Member Function const change the whole family together, and say which.
- **The template as itself.**  The code model checks a template's body as itself, its parameters standing for
  themselves, so a refactoring inside a template writes types in the template's own words (`M*`, not `CTank*`).
- **Where a template's code must go.**  A member of a class template defined outside it is
  `template<class M> void CArray<M>::Add(...)`, in the header and never a `.cpp`; what a refactoring writes there
  is written so.

## Hand written assembly

HCP65 programs have hand written assembly that calls compiled C++ by the labels the compiler gives it
(`_Geometry__CPoint__Sum`, `_Pack__FPC12NGear__CGear`).  A refactoring that changes a label changes it in the
solutions' `.asm` and `.inc` files too:

| Refactoring | Labels it changes |
|---|---|
| Rename | The symbol's name in every label that has it, including its length where it is in a parameter list. |
| Change Signature | A function's label, which has its parameters' types in it, asked of the compiler before and after for each processor. |
| Pull Members Up, Push Members Down, Extract Superclass, Extract Class | The labels of the members that move, which have their class in them. |
| Convert Function to Member Function, and back | The function's label. |
| Extract Template Parameter, Remove Template Parameter | A class's functions' labels, which have the template's arguments in them. |
| Move to Namespace | The labels of what moves, which have its namespace in them. |
| Safe Delete | Counts a label as a use. |

An overloaded function's labels are told apart by their parameters; a label that could be more than one
overload's is left alone, and the status line says how many were.

## What limits them

- **Macros.**  Code a macro makes has no source of its own: a refactoring refuses where its selection or a use
  is made by a macro, and says so.
- **Configurations.**  Code that an `#if` leaves out in the chosen configuration isn't in the index.  Rename can
  search for the name as text as well, and shows what it finds that way as unsure.
- **Files outside the solutions** are only seen as text.
