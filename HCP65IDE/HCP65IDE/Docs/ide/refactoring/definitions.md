# Declarations and Definitions

These keep a function's declaration and its definition in step, and move a definition between its class and its
`.cpp`, as CLion's do.  Each is one undo, and the status line says what was done, or why it can't be.

## Generate Definition

**Ctrl+Shift+D**, on a function that is declared and not defined: it is defined, with an empty body, in the file
its class's other functions are defined in, in the class's order (after the definition of the function declared
before it), under a [banner](../templates.md#the-templates) like theirs, two blank lines apart.  If none of the
class's functions is defined outside it, it goes at the end of its header's `.cpp`.  A class template's member goes
under its `template<...>` in the header, before its `#endif`.

```cpp
// Tank.h: Ctrl+Shift+D on Drain
class CTank
{
public:
	int		GetLevel(void) const;
	int		Drain(int iAmount);
};

// Tank.cpp: after GetLevel's definition
//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int CTank::Drain(int iAmount)
{
}
```

## Generate Declaration

On a function that is defined and not declared: it is declared in its class, after its other functions and lined up
with them, or, for a function of no class, in the header of its source file.

## Bringing them back into step

Change a member's definition (give it another parameter, say) and its class's declaration no longer matches it: to
the compiler they are two functions, one declared and not defined.  The same two commands put them back in step,
whichever side you changed:

- **Generate Declaration** on the edited definition changes the class's declaration of that name to match it, its
  default values and `override` kept;
- **Generate Definition** on the edited declaration changes the definition to match it, the names its body uses kept
  where a parameter's type is as it was.

What the caret is on is what is right.  Where the class has more than one undefined overload of the name, it can't
tell which was meant, and makes a new one.

## Move Definition out of Class

On a function defined in its class: it is left declared there and defined outside, where Generate Definition would
put it, with its body.  A function of no class that is defined in a header (inline) is defined in the header's
source file instead, without its `inline`, and declared in the header.

## Move Definition into Class

The other way: the definition goes into the class.  One statement of 60 characters or fewer stays on the
declaration's line, as the house style writes a class's short functions
(`int GetLevel(void) const { return miLevel; }`); more go under it as a block.  A function of no class that a header
declares is defined in that header, `inline`, and stays declared where it was.
