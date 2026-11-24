# #includes

## Optimise #includes

**Ctrl+Alt+O**, Refactor, Optimise #includes (IntelliJ's Optimize Imports), for the file in the editor, puts its
`#include`s right:

- **An `#include` nothing the file names comes from is removed.**  What the file names, and which file declares each thing,
  is the index's answer.  A thing declared in a header an `#include` only reaches through the headers it includes counts
  for that `#include`, if the file doesn't include the header itself.  One whose macros the file uses is kept (the macros
  are read from the texts).
- **An `#include` only classes come from, each named only as a pointer or a reference, becomes their forward
  declarations** (`class CGauge;`), written two lines below the `#include`s, with any the file has there already.  This is
  done in `.cpp` files too, not only headers.
- **A header that declares something the file names, and that none of its `#include`s reaches, is given an `#include`.**
  (The file compiled only because whoever included it had the header first.)  Only headers of the solution.
- **A name the file's check doesn't know, that one header of the solution declares, gets that header's `#include`**: type
  the name, then Ctrl+Alt+O.

A header's `#include` that goes may be what brought a name to the files that include the header: each of those that names
something it brought, and no longer reaches its header, is given its own `#include` of that header.  Those files are shown
first, with what each will get (Problems Detected), and Cancel changes nothing.

Then the file is checked as it would be.  If it has more errors than it had, the `#include`s that went are tried back one
at a time, and those it needs are kept (a header that needs another before it).

### The order

The `#include`s are put in order too (there is no command to sort them alone):

1. the `<>` ones first, as they are;
2. then each library's (`"BaseLib/Chars.h"`: the directory a name is written with is its library), the lowest library
   first (one that another's headers reach comes before it), each library's own in the order they are, a new one after
   its last;
3. then the headers of the file's own directory, as they are, a new one last.

The file's own header stays where it is.  The lines sorted are the run the file's last `#include` is in, with nothing
between them.  If the file has more errors with them in order (a header needs another before it), they are left in the
order they are, and a new one goes after the last.

### Left alone

`<>` `#include`s, `#include`s inside an `#if`, and a file's `#include` of its own header.

### Not done

The compiler's own headers aren't followed, so `<>` `#include`s are never touched, and a header's `#include` that brings one
to its includers stays.  A class template is never forward declared.  The files given an `#include` aren't checked
themselves (only the file's own unit is).
