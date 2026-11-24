# Templates

The refactorings understand templates (see [How they all behave](index.md#templates)).  These three are about templates
themselves.

## Extract Template Parameter

Refactor, Extract Template Parameter..., on a type: the type becomes a template parameter.  The dialog asks **The
parameter's name** (`M`) and, for a class, **The class template's name**.

### In a function

On a type in a function's declaration or its body, the function becomes a function template:

- `template<class M>` goes over every declaration and definition of the function, and the type becomes `M` everywhere
  they name it (`typename M::X` where they name a type of its own); a type written with its namespace
  (`Geometry::CPoint`) is `M` as a whole;
- a template's definition must be where its uses are compiled: one in a source file whose function a header declares
  goes to the header in place of the declaration (a member function's after its class, with its banner).  A function
  declared in a namespace's block keeps its declaration there, the template's, and its definition goes after the
  namespace's outermost block;
- the calls stay as they are where the type is among the function's parameters (the compiler works `M` out from the
  arguments); otherwise each names it: `Make<CGauge>()`.

Refused: a virtual function, one overridden or called by a template with other classes' functions, one that is a
template already or a class template's, one named without being called (a `using` declaration is allowed), a
constructor or a destructor.

### In a class

On the type of a field, the class becomes a class template, as "Refactoring Towards Seams in C++" has it:

```cpp
// Before: on CGauge, the template named CTankTemplate
class CTank
{
	CGauge*	mpcGauge;
	void	Fill(int iAmount);
};

// After
template<class M>
class CTankTemplate
{
	M*		mpcGauge;
	void	Fill(int iAmount);
};

template<class M>
void CTankTemplate<M>::Fill(int iAmount) { ... }

class CTank : public CTankTemplate<CGauge>
{
};
```

- the class gets the new name and `template<class M>`, and the type is `M` throughout it and its members;
- its members' definitions go to its header after it, as a template's must be, as
  `template<class M> ... CTankTemplate<M>::Fill(...)`;
- the old name is a class deriving from the template with the type it had, with nothing of its own but the class's
  constructors, each passing what it is given to the template's.  So the code that uses `CTank` compiles as it did, and
  `CTank` can still be declared ahead of its definition (a typedef couldn't be).  The compiler warns where
  `CTankTemplate<CGauge>` is written out under it;
- its functions' labels in hand written assembly become the template instance's (`_CTank__Level` becomes
  `_CTankTemplate__T_C6CGauge__Level`).

Refused: a class in a class or a namespace, one with a static field defined outside it, a constructor with a parameter
that has no name, and whatever doesn't compile with its name another type than the template's (a member that returns a
pointer to its own class, given to a pointer to the name).

## Remove Template Parameter

Refactor, Remove Template Parameter, on a class template's parameter: if every place the template is used gives the
parameter one argument, the argument takes its place throughout the template, and the parameter goes from each
`template<...>` and every use.  A class that only named the template's instance (`class CTank : public
CTankTemplate<CGauge>`) gives the class its name back.  The labels in hand written assembly lose the argument again.

A function template's parameter can't be removed: its arguments aren't written at its calls.

## Write Template's Interface

Refactor, Write Template's Interface...: a template says nowhere what its argument must have; its body just uses it.
For the class or function template the caret is in, the members its body needs of each type parameter are found
(`mp->Init()`, `cItem.miCount`, `this->mp->Start(4)`, `M::Count`), in the template and in its functions defined outside
it whatever they call the parameter.  Each is shown with the declaration it has in a class the template is used with,
where the index knows one.

**Write as:**

| Choice | Written |
|---|---|
| A comment over the template | `//M must have:` and each member's declaration (the [Template Interface Comment template](../templates.md#the-templates)). |
| An abstract class before it | Each function pure virtual: what an argument can derive from, to be held to it.  Named by **Class's name**.  Only where every member is a function with a known declaration, for one parameter. |

**Make the classes it is used with derive from it** (not ticked to begin with): with the abstract class, the classes the
template is used with derive from it, their functions marked `override`, each `#include`ing the template's header where
it doesn't reach it.  Each such class has a virtual table then.  The files are checked first.

Only a member named through a name declared with the parameter's type is found, not one reached through what another
member returns.
