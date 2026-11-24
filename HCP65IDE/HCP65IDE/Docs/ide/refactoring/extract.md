# Extract

## Extract Method

**Ctrl+Alt+M**: the selected statements, or the selected expression, become a function, and a call of it is
written where they were.

### The dialog

| Field | What it does |
|---|---|
| Name | The new function's name. |
| Signature | The function as it will be declared, changing as the ticks change. |
| Pass what it changes by pointer | Ticked to begin with: a variable the statements change, that is used afterwards, is passed by pointer, as the house style does (`int* piLow`), and its uses in the moved statements are rewritten (`*piLow`, `psOther->`).  Unticked, by reference (`int& iLow`).  A selected expression keeps references. |
| Static | Makes a member function static.  Only where it uses nothing of the object; not ticked to begin with. |

### What it is given and gives back

It works out from how the selection uses the function's variables and parameters:

- one the selection reads is a parameter;
- one it changes that is used afterwards is what it returns (`iSum = Sum(iA, iB);`); if there is more than one of
  those, each is passed by pointer (or reference);
- an object (a variable whose type is a class) is always passed by pointer or reference, never copied;
- one only the selection uses becomes the new function's own, its declaration going with it; one the selection
  gives a value first, that nothing reads afterwards, is declared again in the new function.

A variable counts as read by the selection unless a statement of the selection itself (not one inside an `if` or a
loop) assigns it before anything reads it, and as used afterwards if anything after the selection names it, or,
where the selection is in a loop, anything in the loop.

A selection that ends by returning becomes `return Name(...);`.  One that returns part of the way through, or leaves
by `break`, `continue` or `goto`, is refused: it has more than one way out.  A `throw` is no way out of that kind: the
exception leaves the new function as it left the statements, so a selection that returns and ends with a `throw`
(`if (a) return 1; throw E();`) is extracted, and called as `return Name(...);`.  In a handler, the handler's
parameter is given to the new function as a local would be.

### Where it goes

- In a member function the new function is a protected member: declared at the end of the class's protected part
  (made if the class's last part isn't protected) and defined straight after the function it came from, with the
  class's name (and a class template's `template<...>`) written as that function writes them.  It is const if that
  function is.
- From a function of no class it is defined just before that function.
- From a function template it is a template of the same parameters, and the call names them.

### Duplicates

After you give the name, the other places in the file where the same statements are written are found, the
comparison not minding what their variables are called.  **Duplicates Detected** lists them, each ticked to be
replaced by a call; Refactor replaces those ticked.

```cpp
// Before: Ctrl+Alt+M on the two lines, named Total, by pointer
void CPump::Run(void)
{
	int		iSum;
	int		iCount;

	iSum = 0;
	iCount = Count();
	iSum += iCount * 2;
	iCount++;
	Report(iSum, iCount);
}

// After
void CPump::Run(void)
{
	int		iSum;
	int		iCount;

	iSum = 0;
	iCount = Count();
	Total(&iSum, &iCount);
	Report(iSum, iCount);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CPump::Total(int* piSum, int* piCount)
{
	*piSum += *piCount * 2;
	(*piCount)++;
}
```

## Extract Superclass, Interface and Class

IntelliJ's Extract Superclass, Extract Interface and Extract Class (Extract Delegate), for the class the caret is in.
Each makes a new class of members chosen from it, in a header of its own named for the new class without its `C`
(`CGauge` gives `Gauge.h`), in the class's header's directory, and a source file of its own if any member that moves
is defined in one.  The files are written from the [C++ Header and C++ Source templates](../templates.md#the-templates),
added to the projects, staged in git and saved.  The class's header `#include`s the new one.

The dialog has the new class's name (and, for Extract Class, the field's name), and the class's members in a table:
**Extract** ticks the members that go, and, for Extract Superclass, **Make abstract** leaves a function where it is and
only declares it in the new class, pure virtual.

### Extract Superclass

The members move to the new class, and the class derives from it.  Each declaration goes with the comment over it,
under its access; a definition outside the class is renamed for the new class and moves with it.

### Extract Interface

The functions chosen are declared pure virtual in the new class, the class keeps its own, and derives from it.

### Extract Class

The members move to the new class, all public, and the class gets a field of it where the first of them was.  Every
place one of them is named outside what moved gets the field before the name:

```cpp
miLevel            // becomes mcGauge.miLevel
pcTank->Fill()     // becomes pcTank->mcGauge.Fill()
```

The new class gets an empty `Init`, and the class's own `Init`, where it has one, calls it.

### Templates

Where the class is a class template and what moves names its parameters, the new class is a template of the same
parameters, and the class derives from (or has) it with them.  A function defined outside the class then goes with
its body in the new class, as a class template's must.

### Problems Detected

As Pull Members Up's: a member that uses one of the class's that isn't going with it, and the rest.  One undo empties
the new files and puts everything back.
