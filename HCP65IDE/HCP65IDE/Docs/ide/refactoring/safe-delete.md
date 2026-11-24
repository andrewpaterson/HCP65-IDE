# Safe Delete

**Alt+Delete**, or Refactor, Safe Delete...: the symbol at the caret is taken out, with every declaration
and definition of it in every file, but only once you know nothing uses it.

What goes with it:

- a class, with its members and their definitions outside it;
- an enum, with its enumerators;
- a namespace, with what is in it;
- a function, with its declarations and its definition.

A whole declaration goes with its lines, the comment lines straight above it, and the blank lines that were
between it and what follows.  One declarator of several (`int a, b;`), or one enumerator, goes with its
comma.

## The dialog

It says what will be deleted, and has:

| Tick | What it does |
|---|---|
| Search in comments and strings | Counts the name in comments and strings as uses. |
| Search for text occurrences | Counts the name as text where the index doesn't have it (code a configuration leaves out, macro bodies, the projects' `.txt` and `.md` files). |
| Delete files (`Keg.h`, `Keg.cpp`) | Shown for a class whose files would have nothing else left in them: the files are deleted too, and taken out of their projects. |

## When it is used

What counts as a use:

- a reference to it that isn't in what is being deleted (a call from a function that is itself deleted
  doesn't count);
- a function in a derived class that overrides it;
- a label of it in hand written assembly;
- a template's body naming it through an instance.

If it has uses, **Usages Detected** says how many ("'Fill' has 3 usages in 2 files."):

| Button | What it does |
|---|---|
| Delete Anyway | Deletes it; the uses are left, and won't compile. |
| View Usages | Shows them in [Find Usages](../navigation.md#find-usages), and deletes nothing. |
| Cancel | Deletes nothing. |

## Refused

It can't be deleted, and the status line says why, when a declaration of it is made by a macro or left out
by the configuration, is in a file that isn't one of the solutions', or when it is a parameter: a
parameter is removed with [Change Signature](change-signature.md), which changes the calls too.
