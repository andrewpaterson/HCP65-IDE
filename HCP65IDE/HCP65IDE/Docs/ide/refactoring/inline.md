# Inline

**Ctrl+Alt+N**, or Refactor, Inline..., is one command for whatever the caret is on: a local variable, a
constant, a function or a macro.  Each is replaced by what it stands for, the inverse of Introduce and Extract.

## Inline a variable

A function's own variable that has one value is taken out, and its value written wherever it was used.  Its value
is what its declaration gives it (`int iCount = Count();`) or, as the house style declares locals at the top of a
function, what the one statement that assigns it gives it (`iCount = Count();`).  The declaration and that
statement go.  The value gets brackets where the expression it goes into would otherwise take it apart: `iSum * 2`
for an `iSum` of `iA + iB` becomes `(iA + iB) * 2`.

Refused:

- a variable that is assigned more than once, or changed some other way: `++`, `+=`, its address taken, bound to a
  reference that isn't const, or a function that isn't const called on it (the compiler says which);
- one declared with others in one declaration;
- one used before it has its value, or assigned in a block its uses are outside.

Problems Detected first: a value that does something (a call, an assignment) and would be done more than once, or
not at all; a variable the value reads that is changed between the value and a use of it.

## Inline a constant

A `const` variable of a file or a class, whose value is a constant, is written where it is used (not an
enumerator; a `#define` is a [macro](#inline-a-macro)).  The dialog asks:

| Inline | What happens |
|---|---|
| All uses, and take the constant out | Every use gets the value, and the constant goes. |
| This use only | Only the use at the caret. |

## Inline a function

The function's body is written where it is called, at every call or at the one at the caret.  The dialog asks:

| Inline | What happens |
|---|---|
| All calls, and take the function out | Every call is inlined, and the function's declarations and definition go. |
| All calls, and keep the function | Every call is inlined; the function stays. |
| This call only | Only the call at the caret (offered when the caret is on a call). |

- A function whose body is `return <expression>;` can go anywhere it is called: the call becomes the expression, in
  brackets where what it goes into would take it apart.
- A function with statements is written where its call is a statement by itself, or all of what a statement
  assigns or returns: its statements go there at that statement's indent, and what it returned is assigned,
  returned or dropped.  Its own variables are declared with the declarations of the function it goes into.
- A call of a function with statements that is part of a bigger expression is taken out of it: the statements go
  before the statement the call is in, what it returned goes in a variable (named as Introduce Variable would
  name it), and the variable is written where the call was.  Only where the statement always makes the call, and
  once: an expression statement, a `return`, an `if`'s condition, in braces and on a line of its own; not after
  `&&` or `||`, in the arms of `?:`, after a comma, or in a loop's condition.
- A parameter the body changes (passed by value) gets a variable of its own where it is inlined, given the
  argument before the statements; it is numbered if the name is taken.  One passed by reference is the argument
  itself.
- In the body, each parameter becomes the call's argument, in brackets where it is more than one thing and is an
  operand.  A member named alone gets the call's object before it (`cTank.miLevel`, `pcTank->Fill()`), `this`
  becomes the object, and a type or constant of the class gets the class's name.  A class template's member
  inlined outside the template has the template's parameters replaced by the instance's arguments.

```cpp
// Before: Ctrl+Alt+N on Scaled, all calls
int CPump::Scaled(int i)
{
	return miLimit * (i + 1);
}

int CPump::Total(int i)
{
	return miCount + Scaled(i);
}

// After
int CPump::Total(int i)
{
	return miCount + (miLimit * (i + 1));
}
```

Refused: a virtual function, a recursive one, one that returns part of the way through, a constructor, destructor
or operator, a function template.  In a program built with exceptions also: a function whose body is a
function-try-block (its handlers can't be written where it is called), and one with an exception specification
(`noexcept`, `throw()`, `throw(A, B)`) whose body may throw, by a call, a `new` or a `throw`, since inlined the
exception would no longer stop at it.

Problems Detected first: a call that is part of an expression it can't be taken out of (it stays a call, and the
function isn't taken out); an argument that does something and would be written more than once, or not at all; a
member that isn't public named where the call is outside the class; one of its variables' names that the function
it goes into already uses; and, where it is to be taken out, a place it is named without being called.

## Inline a macro

On a macro's name, at its `#define` or a use: the use at the caret, or every use, is replaced by what the
preprocessor would make of it (its arguments put in its body, `#` and `##` done).

| Inline | What happens |
|---|---|
| All uses, and take the #define out | Every use is expanded, and the `#define` goes. |
| This use only | Only the use at the caret. |

Refused for a macro an `#if`, `#ifdef` or `#undef` names, or one defined more than once.  To turn a macro into a
constant or a function instead, see [Convert Macro](convert.md#convert-macro).
