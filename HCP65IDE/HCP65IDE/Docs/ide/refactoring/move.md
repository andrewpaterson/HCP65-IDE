# Moving to Other Files

**F6**, or Refactor, Move..., is IntelliJ's Move: what it does depends on what the caret is on.

| On | Refactoring |
|---|---|
| A class | [Move Class to Its Own Files](#move-class-to-its-own-files) |
| A function or variable that is in no class | [Move to File](#move-to-file) |

To move members between classes, see [Moving Members](members.md); to another namespace, [Namespaces](namespaces.md).

## Move Class to Its Own Files

A class that shares a header with other things gets a header of its own and, if any of its members are defined in a
source file, a source file of its own, in the directories of the files it leaves.  The dialog asks for the files'
stem (`Header (.h):` or `Files (.h and .cpp):`), suggested from the class's name without its `C`.

- The new header is written from the [C++ Header template](../templates.md#the-templates): the licence, an include
  guard, the old header's `#include`s, the class with the comment over it, and what the old header defines outside the
  class (a class template's members, inline ones).
- The new source file is written from the C++ Source template: the new header's `#include`, the old source's
  `#include`s, and the members' definitions with the comments over them.
- They go out of the old files.
- The new header is `#include`d where the class is needed: by the old header if what stays there names the class;
  otherwise by each file that names the class or one of its members, beside its `#include` of the old header
  (written as that one is, with its directories) or after its last `#include`.
- If the class names something that stays in the old header (its base class), the new header `#include`s the old one.
  If each needs the other, the class can't be moved until one of them is declared ahead by hand.
- The new files are added to every project the old ones are in, beside them, and to any `CMakeLists.txt` that lists
  the old ones; they are staged in git, and the changed files are saved.

One undo puts the class back and takes the new files away again.  An `#include` that is no longer needed is left: that
is [Optimise #includes](includes.md)' job.

Refused: a class whose members are defined in more than one source file (`Preprocessor_Linux.cpp` and
`Preprocessor_Windows.cpp`), and a class and the rest of its header that each name the other.

## Move to File

A function or a variable that is in no class goes to another header of the solution, named in the dialog's **To
header:** field.  The field completes as you type, matching the solution's headers as Go to File does, each with its
folder beside it; Down and Enter take one.  The **...** button beside it opens a chooser of the solutions' projects and
files.  A header whose name another of the solution's files has is written with its path.

- Its declarations (prototypes, an `extern`) go to that header, after the last function it declares, or before its
  `#endif`.
- Its definition goes to the end of that header's source file; or, if a header defined it (`inline`, a function
  template), to that header.
- The files that name it `#include` the header it has gone to, beside their `#include` of the one it left, and the
  files it goes to `#include` the headers that declare what its text names, where they don't already (each named for
  the thing it declares: include what you use).

**In a namespace**: what is declared in a namespace's block goes into the target's block of the same namespace (or of
the outermost part of it the target has, the rest opened inside it), or a block of its own at the end.  A definition
written with its namespace (`int Geometry::Area(...)`) goes as it is.  A block it leaves empty goes too.

**A variable template** (`template<class M> M gmPi = M(3.14159265);`) goes whole, with its `template<...>`, and its
explicit specialisations (`template<> int gmPi<int> = 3;`) go after it.

A `static` function is refused: only its own file can use it.

## Copy Class

Refactor, Copy Class..., or Alt+Insert on a class: the class under a new name (the dialog's **New name:**), in files of
its own made as Move Class to Its Own Files makes them (`CBarrel` in `Barrel.h` and `Barrel.cpp`), with the class's
name changed wherever the copied text names the class: its constructors and destructor, the class's name before its
members' definitions, and in the copy's comments.  Nothing is taken out of the files it is copied from.  The new header
`#include`s the old one if the copy names something declared there.  What else is in the class's files (another class
in its header) isn't copied.
