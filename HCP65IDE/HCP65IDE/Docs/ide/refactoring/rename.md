# Rename

## Rename

**Shift+F6**, or Refactor, Rename...: the symbol at the caret, or the name the selection is in, is renamed
everywhere the index has it.  Any symbol can be: a class, struct, union or enum, an enumerator, a typedef,
a namespace, a function, a member function, a variable, a field, a parameter, a local variable, a template
or a template's parameter.

What it renames with it:

- a class's constructors and destructor, and every place its name is written;
- a virtual function's overrides, and the function it overrides: they must stay one function (the dialog
  says how many: "Rename 'Fill' and 2 overrides to:");
- the members of other classes a template's body calls by the same name (`mp->Init()` in `CHolder<M>`): see
  [families](index.md#templates);
- its labels in hand written assembly (`_CTank__Fill`, and the length before a name in a parameter list,
  `PC5CTank`);
- with **Rename files**, a type's files.

The dialog:

| Field | What it does |
|---|---|
| The new name | Starts as the old one, selected. |
| Search in comments and strings | Also renames the old name as a whole word in comments and strings. |
| Search for text occurrences | Also renames it where the index doesn't have it: code a configuration leaves out, macro bodies, the projects' `.txt` and `.md` files.  What it finds this way is unsure, and is listed first. |
| Rename files | For a type with files of its own (below): they are renamed too. |

The two searches are remembered, separately for types and for everything else.  Nothing is changed if a
file has changed since it was indexed.  The changes are one undo.

```cpp
// Before: Shift+F6 on Fill, renamed Top
class CTank
{
public:
	virtual void	Fill(int iAmount);
};

class CBigTank : public CTank
{
public:
	void	Fill(int iAmount) override;   // renamed too: it overrides it
};

void Start(CTank* pcTank)
{
	pcTank->Fill(2);                     // pcTank->Top(2);
}
```

### A type's files

A type whose name has a one-letter prefix (`CPreprocessor`) has files of its own when they are named by the
rest of its name, its **stem** (`Preprocessor`), in the directory of the file that declares it:

- `Preprocessor.h`, `.hpp`, `.inl`, `.cpp` and `.c`;
- `Preprocessor_<anything>.cpp`, the house style's parts for each platform (`Preprocessor_Linux.cpp`).

The index must have the type declared in one of them, so a file that only shares the name is never taken.
Renamed `CCPPPreprocessor`, its files become `CPPPreprocessor.h`, `CPPPreprocessor_Linux.cpp` and so on, by
[Rename File](#rename-file), with all that changes.  `PreprocessorHeaderCache.cpp` isn't one of its files,
whatever is in it.

## Rename File

Refactor, **Rename File...** (on the editor tab's menu too), or Rename... on a file in Solution Explorer,
renames a file and everything that names it, as IntelliJ's rename of a file does:

1. the file on disk (with `git mv` if git tracks it, so the rename is staged);
2. every `#include` that finds it, as the compiler would find it (beside the includer for `""`, then the
   project's include directories), whether it is written `"Preprocessor.h"`,
   `"CppCompilerLib/Preprocessor.h"` or `"../CppCompilerLib/Preprocessor.h"`.  Only the file's name in it
   changes; its directories stay.  An `#include` of another file of the same name is left alone.  Includes
   in code an `#if` leaves out are changed too;
3. the item in every project that has the file, in the `.vcxproj` and its `.vcxproj.filters`, changing only
   the file's name there, so the rest of the project file stays as Visual Studio wrote it (the Linux CMake
   builds read their sources from them);
4. its editor tab, its place in the undo history, and the index.

The dialog has the new name (its stem selected, not its extension), and:

| Tick | What it does |
|---|---|
| Rename related files | The other files of the same stem in the same directory: renaming `Preprocessor.h` to `CPPPreprocessor.h` offers `Preprocessor.cpp`, `Preprocessor_Linux.cpp` and `Preprocessor_Windows.cpp`, listed under it, each given the new stem with its own ending. |
| Search in comments and strings | The old file name as a word in comments and strings. |
| Search for text occurrences | And in the projects' `.txt` and `.md` files. |

It saves the files it changes, and one undo renames everything back.  The file stays in its directory: to
move a class to other files, see [Move](move.md).

The status line says what it did: "Renamed Preprocessor.h to CPPPreprocessor.h and 3 related files: 14
#includes in 13 files, 2 projects."

It refuses a name that isn't a file name, one taken on disk or in a project, and a file that isn't one of
the solutions'.
