# Introduce

The Introduce refactorings give a selected expression (or type) a name: a variable, a field, a constant, a
parameter or a typedef.  Select the expression and press the key.  With nothing selected, Introduce Variable
offers the expressions the caret is in, in a popup under it, the innermost first; the one chosen is selected and
introduced, or, if there is only one, it is taken.

The dialog suggests a name from the expression's last name, without its own prefix or a leading Get, Is, Has,
Find, Calculate, Create, Make or Read, given the prefix of the expression's type as the house style has it (`i`,
`ui`, `b`, `f`, `sz`, `c`, `pc`, `s`, `ps`, `e`, `p`, `pv`): `pcTank->GetLevel()` suggests `iLevel`.  Under
the name it shows the type and the expression, `int  =  pcTank->GetLevel()`.  It refuses a name that is taken.

Where the same expression is written more than once in the function, **Replace all N occurrences** replaces
each (but those that are assigned to).  If something in the expression is changed between the first and the last
of them, or the expression does something (a call, a `++`), that is shown first in Problems Detected, since the
occurrences may not all be the same value.

## Introduce Variable

**Ctrl+Alt+V.**  The expression becomes a local variable.

- Its type is written as its declaration wrote it, where the expression is one thing with a declared type (a
  call's function's return type, a variable's or a field's): `#!text uint32` stays `#!text uint32`.  Otherwise it is the type
  the compiler worked out.  In a template it is in the template's own words (`M*`), and it is refused where only
  the template's arguments decide it.
- As the house style has locals, it is declared with the function's other declarations at the top of its body,
  its name lined up with theirs (they are moved over if its type is longer), and assigned on a line before the
  statement that first uses it.  A statement that is only the expression becomes the assignment.

```{.cpp .no-check}
// Before: Ctrl+Alt+V on pcTank->GetLevel() * 2, named iLevel, both occurrences replaced
int CPump::Rate(CTank* pcTank)
{
	int		iRate;

	iRate = pcTank->GetLevel() * 2;
	return iRate + pcTank->GetLevel() * 2;
}

// After
int CPump::Rate(CTank* pcTank)
{
	int		iRate;
	int		iLevel;

	iLevel = pcTank->GetLevel() * 2;
	iRate = iLevel;
	return iRate + iLevel;
}
```

## Introduce Field

**Ctrl+Alt+F**, in a member function.  The expression becomes a field of the class, declared at the end of the
class's protected part (where its fields are; a struct's public part), lined up with the field above it, and
assigned where the variable would have been.  Its name is suggested with the field's `m` (`miLevel`).

**Assign it in Init** (shown where the class has an `Init` and the expression uses nothing of the function's
own) assigns it at the end of the class's `Init` instead.

## Introduce Constant

**Ctrl+Alt+C.**  An expression that names no variable and calls nothing becomes a `#define`, as the house style
writes constants, after the file's last `#define` or `#include`, in brackets if it is more than one thing.  Its
name is suggested in capitals (`MAX_LEVEL`).

**Replace all N occurrences in the file** (not ticked to begin with) replaces the expression wherever the file
writes it, not only in the function.

It is refused for an expression that names a variable, does something, or is a statement by itself.

## Introduce Parameter

**Ctrl+Alt+P.**  The expression becomes a parameter of its function, added last by [Change
Signature](change-signature.md) (every declaration, definition and override), and every call of the function
passes the expression.  It can't use the function's own local variables, or its class's members, which a call
couldn't give.

It can use the function's **parameters**.  Each call then passes the expression with that call's arguments put
in for them, in brackets if they are more than a name.  A parameter the replaced occurrences were all the uses
of is taken out, with **Remove the parameters only it uses** (ticked to begin with), as IntelliJ does.

```{.cpp .no-check}
// Before: Ctrl+Alt+P on pcCalc->miScale + 7, named iScale
int Area(CCalc* pcCalc, int iWidth)
{
	return (pcCalc->miScale + 7) * iWidth;
}

int Areas(CCalc* pcCalc, int i)
{
	return Area(pcCalc, 3) + Area(pcCalc + 1, i);
}

// After: pcCalc was only used by the expression, so it goes
int Area(int iWidth, int iScale)
{
	return iScale * iWidth;
}

int Areas(CCalc* pcCalc, int i)
{
	return Area(3, pcCalc->miScale + 7) + Area(i, (pcCalc + 1)->miScale + 7);
}
```

It is refused where the function changes such a parameter (no call could give what the expression is there), and
where a call's argument that does something (`Next(&i)`) would be worked out more than once.

## Introduce Typedef

**Ctrl+Alt+K**, as CLion's.  The selected type is given a name, and the other places the same type is written,
token for token, can be given the name too (**Replace all N occurrences**).

- The typedef is written as the house style writes them, `typedef CArrayTemplate<SIDEThing>	CArrayThings;`, two
  blank lines before whatever in the file first uses it (a function or a class, with the comment over it).  It
  goes before the first of the places it replaces, since a place before it couldn't use it.
- A type that names a template's parameter can only be named where the parameter is: in a class template, the
  typedef is a member of the class, at the top of the class with its other typedefs (after its opening brace and
  the `public:` that starts it), and the places are those in the class; in a function template, it is the
  function's own, at the start of its body.
- A typedef written in a header, at the file's level, can be used by every file that includes the header.  **And in
  the files that #include this one** (not ticked to begin with) gives those files' places the name too.  The files
  are listed, with how many places each has, before anything changes, and they are compiled with the name first: a
  place before the `#include`, or one where the same words mean another type, stops it.

Places a macro makes aren't looked at.  It doesn't suggest a name.
