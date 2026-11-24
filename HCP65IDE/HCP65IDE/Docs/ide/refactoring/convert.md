# Converting

Each of these turns one kind of thing into another, everywhere it is used.  Each checks the files it changes as they
would be, with the compiler, and changes nothing if they would have more errors than they have now.

## Encapsulate Field

Refactor, Encapsulate Field..., on a field: the class gets a getter and a setter for it, and the places that name the
field call those instead.

| Field | What it does |
|---|---|
| Getter, and its name | Whether to write one (`GetLevel`). |
| Setter, and its name | Whether to write one (`SetLevel`). |
| The class's own functions use them too | Otherwise only the places outside the class's own functions are changed. |
| Make the field protected | Which is what it is for. |

They are declared at the end of the class's public part and defined on one line each at the bottom of the class's
`.cpp`, where the one-line functions are, the getter `const`; in the class itself where it has no `.cpp` or is a
template.  Their bodies come from the [Getter Body and Setter Body templates](../templates.md#the-templates).

```cpp
int CTank::GetLevel(void) const { return miLevel; }
void CTank::SetLevel(int iLevel) { miLevel = iLevel; }
```

How each place uses the field is the compiler's answer: a read becomes the getter; all of what an `=` assigns becomes
the setter; a `+=` or `++` that is a statement by itself becomes the setter of the getter's value changed
(`SetLevel(GetLevel() + 2)`).  A field that is an object has a getter that gives a pointer to it and no setter, as
functions here give objects: a member named through it goes through the getter (`GetBox()->Get()`).

A place that uses the field another way (its address taken, a reference bound to it, a `++` inside an expression)
stays as it is and is shown first in Problems Detected: it won't compile if the field is made protected.

## Convert Macro

Refactor, Convert Macro..., on a macro's name at its `#define` or a use.  A macro isn't in the index: its `#define` and
uses are found by its name in the solution's files, as the preprocessor would find them (not in comments or strings).

- **A macro of a value** becomes a constant of the type the compiler gives its value: `const int MAX_UNITS = 8;`; a
  string becomes a `const char` array.  The dialog shows what it becomes ("It becomes: ...").
- **A run of `#define`s of whole numbers** can become one enum's enumerators instead: tick **Make an enum of the N
  #defines here instead** and give **The enum's name**.
- **A macro with parameters** becomes an inline function of the same name.  Its parameters' types and its result's
  are those the compiler gives its arguments and what it makes at each use.  Where they differ between uses it becomes
  a function template, the parameters whose types change together sharing a template parameter.  An argument the
  macro's body uses more than once, or not at all, is worked out once by a function: a use where that argument does
  something (a call, a `++`) is shown first.

Refused: a macro an `#if`, `#ifdef` or `#undef` names, one C code uses, one whose body has `#` or `##`, changes one of its
arguments, or names something that isn't declared where the macro is; a name `#define`d more than once.  To expand a
macro where it is used, see [Inline a macro](inline.md#inline-a-macro).

## Pointers and references

Refactor, **Convert Pointer to Reference** or **Convert Reference to Pointer**, on a parameter or a local variable (its
declaration or any use):

- its declaration's `*` becomes `&` (or `&` a `*`), in every declaration and definition of a parameter's function and of
  the functions that override it or that it overrides;
- where it is used, `p->x` becomes `p.x`, `*p` becomes `p`, and `p` alone (passed on, assigned from) becomes `&p`; the
  other way, `r.x` becomes `r->x`, `&r` becomes `r`, and `r` alone becomes `*r`;
- every call's argument for it changes to match: `&c` becomes `c` and anything else `*p` (the other way, `*p` becomes
  `p` and anything else `&c`); a local variable's initialiser changes the same way.

Refused, to a reference: a pointer that is assigned to, stepped, compared or tested (a reference never changes and is
never `NULL`), used as an array, or deleted; one with a default value; a call that gives it `NULL`; a local variable with
no initialiser.  Either way: a function named without being called, and a use or argument a macro makes.

## Convert Enum to Enum Class

Refactor, Convert Enum to Enum Class..., on an enum or one of its enumerators:

- `enum EName` becomes `enum class EName` where it is defined and wherever it is declared ahead;
- every place one of its enumerators is named gets the enum's name before it: `EN_First` becomes `EName::EN_First`,
  `CTank::EN_First` becomes `CTank::EName::EN_First`, in every file that names one.

An enum class's value isn't an integer.  With **Write casts where its values are used as integers** (ticked to begin
with), each place that uses its value as one is given the cast it needs, found by checking every file that includes the
enum with the enum taken as an enum class:

```cpp
(int)eLevel + 1
aiCounts[(int)eLevel]
if ((int)eMode)
eFlags = (EFlags)((int)FLAG_A | (int)FLAG_B);
eFlags = (EFlags)((int)eFlags | (int)EFlags::FLAG_B);      // was: eFlags |= FLAG_B;
```

The places are shown first.  Unticked, an enum whose values are used as integers is refused, with the places it would
need.  `++` and `--` on its value have no cast, so they refuse it.  Refused too: an enum with no name, one that is an
enum class already, a declaration or use a macro makes, a file that isn't one of the solutions'.

## Make Member Function const

Refactor, Make Member Function const: ` const` after its parameters in every declaration and definition of it, of the
functions that override it and that it overrides, and of the members of other classes a template's body calls with it
by one name (`CHolder<M>::Start` calling `mp->Read()` ties every `M`'s `Read` together).

Each definition's body is checked against what it changes: a field of its object assigned, stepped, given to a
reference that isn't const, its address taken, or a function that isn't const called on it.  Those are shown first in
Problems Detected, since they won't compile in a const function, which is also how to find the next function to make
const.

## Make Member Function static

Refactor, Make Member Function static: `static` before it in its class, and a `const` after its parameters taken away.
Calls that name an object (`cVat.Count()`, `pcVat->Count()`) name its class instead (`CVat::Count()`), where what names
the object does nothing else.  Refused if it is virtual or overrides, or if a body uses its object: a field or member
function that isn't static, or `this`.

## Convert Function to Member Function

On a function whose first parameter is a pointer or reference to a class of the solution: it becomes that class's.

- It is declared in the class, at the end of its public part; its own declarations go.
- Its definition stays where it is as `C::Name`, without that parameter; in its body `p->x` becomes `x` (`this->x` where
  a parameter or local has that name), and `p` alone becomes `this`.  A parameter that points to a const class makes it
  a const function.
- Every call names its object first: `Depth(&cVat, 2)` becomes `cVat.Depth(2)`, `Depth(pcVat, 2)` becomes
  `pcVat->Depth(2)`, and `Depth(this, 2)` becomes `Depth(2)`.
- **In a namespace**: a definition written with its namespace is named with the class's (`Geometry::CPoint::Area`); one
  in a block of its namespace with the class's name from there.  A call's qualifier goes with its first argument
  (`Geometry::Area(&c)` becomes `c.Area()`), and a `using` declaration of it goes, since a member isn't named so.  A
  definition in the block of a namespace its class isn't in is refused: [Move to Namespace](namespaces.md) can move it
  to its class's namespace first.

## Convert Member Function to Function

The other way: its class's declaration of it goes and it is declared after its class, at the class's indent, with a first
parameter that points to its class (`CVat* pcVat`, const for a const function).  In its body what it names of its object
is named through the parameter (`pcVat->miDepth`), and `this` is the parameter; `cVat.Depth(2)` becomes
`Depth(&cVat, 2)`, and a call from its class's own functions `Depth(this, 2)`.

Refused, either way: a function named without being called, a virtual or static one, a constructor, destructor or
operator, one defined in its class (Move Definition out of Class first) or not defined in the solution, one whose body
changes the parameter that is to be its object.  A member function that uses what is private can't be a function: the
compiler check stops it.

The function's label in hand written assembly is changed too.
