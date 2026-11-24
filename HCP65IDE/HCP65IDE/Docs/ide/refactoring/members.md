# Moving Members

IntelliJ's Pull Members Up and Push Members Down, for the class the caret is in: on one of its members (which is the
one offered, ticked) or on its name.

## The dialog

| Part | What it is |
|---|---|
| To class | Pull Members Up: which of the class's base classes they go to. |
| To classes | Push Members Down: the classes that derive from it, each with a tick. |
| Members | A table of the class's functions and fields, static ones too: **Move** ticks those that go; the third column is **Make abstract** (up) or **Keep abstract** (down). |

A member that can't move says why beside its name, and can't be ticked.

## Pull Members Up

The members move to the base class:

- each declaration, with the comment over it, goes to the end of the base class's part with its access (public,
  protected or private; made if the base class has none);
- a function defined outside the class has its definition renamed for the base class and moved to the file the base
  class's functions are defined in;
- a field goes after the base class's last field;
- **Make abstract**: the base class gets the function pure virtual, and the class keeps it, marked `override`.

## Push Members Down

The members move to the classes that derive from it (all of them, or those ticked): each gets the declaration, and a
copy of a definition outside the class under its own name, unless it has that function already (it overrides it).
**Keep abstract**: the class keeps the function, pure virtual.

## Templates

Where the class or the one a member goes to is a class template, a function defined outside goes with its body into the
class, which is right whatever the template's parameters are called.

## Problems Detected

Found first and shown:

- a member that uses one of the class's that isn't going with it;
- one the class it goes to has already;
- a private one the class's others still use;
- pushed down, one that is used from outside the classes it goes to.  A use through an object of one of those classes
  (`x.Seal()` where `x` is a `CDrum`) isn't a use of the class's: the object's type is asked of the compiler.

Hand written assembly's labels of the members that move (`_CTun__Fill`, and a field's layout constant `CTun_miLevel`)
get the class they went to.  Not where members go down to more than one class, nor where only a declaration goes up.

## Not done

- The `#include`s the class a member goes to then needs aren't added.
- A class's nested types and enums aren't offered.
- A member's use of a class template's parameters isn't checked where it goes to a class that isn't that template's.
