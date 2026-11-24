# Namespaces

## Move to Namespace

Refactor, **Move to Namespace...** (and Alt+Enter; it has no key, as IntelliJ's has none): what the caret is on goes to
another namespace, a new one, or the global namespace.  It takes a class, struct, union or enum, a typedef or alias, a
function, a variable, a class, function, alias or variable template, or a namespace itself, at namespace scope.

### The dialog

| Field | What it does |
|---|---|
| Namespace (empty for the global one) | The namespace to go to, `A::B`, starting as the one it is in, selected.  It completes over the solution's namespaces as you type.  Empty, or `::`, is the global namespace. |
| Add using-declarations in source files instead of qualifying | Where a source file's uses would have to be qualified, it gets one `using New::X;` instead, and its uses are left as they are. |
| Functions that take it | For a class: its old namespace's functions and operators that take it, ticked, to move with it (below). |

### Its declarations

Its declarations stay where they are in their files; the namespaces round them change:

- in an old namespace's block, the block is closed before it and opened again after it, and the new namespace's is
  opened round it (`namespace A` then `namespace B` for `A::B`, each a block of its own);
- a block it is alone in goes; one it starts or ends goes before or after it;
- next to a block of the new namespace it goes into that block instead;
- a new block is written as the file's other blocks are, or as the house style has it: `namespace Name`, the brace
  on a line of its own, the body a tab in;
- what moves is indented as its new block is;
- a definition written outside its namespace (`int Old::F(void) { }`) only has its qualifier changed;
- a class's members' definitions in the old namespace's blocks move with it.

### Its uses

Every use is made to name it from where it is (the index's uses, as Rename finds them):

- unqualified in the new namespace, in a namespace inside it, or under a `using namespace` of it;
- otherwise qualified with as much of the new namespace as is needed, from the namespaces the use and it share, or from
  a namespace a `using namespace` opens there, whichever is shorter (the global namespace's `::Name` where something
  nearer would hide it);
- a qualifier that named the old namespace is replaced; `Old::X::Member` keeps its `::X::Member`;
- a `using Old::X;` names the new namespace, or goes where it isn't needed any more.

What moves names things of the namespace it leaves without their namespace (it found them by being in it): those are
qualified, unless the new namespace is inside the old one.

```cpp
// Before: Move to Namespace on Area, to Shapes
namespace Geometry
{
	class CPoint { ... };
	TLength	Area(CPoint* pcPoint);
	extern int	giCount;
}

Geometry::TLength Geometry::Area(CPoint* pcPoint)
{
	return pcPoint->Sum() * giCount;
}

// After
namespace Geometry
{
	class CPoint { ... };
}


namespace Shapes
{
	Geometry::TLength	Area(Geometry::CPoint* pcPoint);
}


namespace Geometry
{
	extern int	giCount;
}

Geometry::TLength Shapes::Area(Geometry::CPoint* pcPoint)
{
	return pcPoint->Sum() * Geometry::giCount;
}

// Main.cpp, which had "using namespace Geometry;":   Area(&cOther)  becomes  Shapes::Area(&cOther)
```

### The functions that take a class

A call like `Area(&cPoint)` can find `Geometry::Area` through its argument's namespace (argument dependent lookup), so a
class moved out of `Geometry` leaves such functions harder to find.  The dialog lists the old namespace's functions and
operators that take the class, each ticked.  Those ticked go with it, as one move: their declarations into the same new
blocks, their uses named from where they are, their labels changed.  Those left are warned of in Problems Detected.

### Moving a namespace

A namespace itself can move: `Geometry::Detail` to the global namespace, into `Drawing`, or a namespace of the file's
into another.  Each of its blocks moves as a declaration does, every name written with it (`Geometry::Detail::Twice`,
`using namespace`, a qualified definition) is written from where it is, and what its blocks name of the namespace round
them is qualified.  A namespace can't go into itself or a namespace inside it, and an inline one isn't moved.

### Problems Detected

- The new namespace has something of the name already (for a namespace: the two will be one namespace, and what they
  declare mustn't clash).
- Functions of the old namespace that take the class, left behind: calls that found them through the class's namespace
  won't any more.

### Hand written assembly

A namespace is part of a label: `_Geometry__Area__FPC16Geometry__CPoint` becomes `_Shapes__Area__FPC16Geometry__CPoint`.
A class's scoped name changes in every label that names it, its members' and the length-prefixed names in parameter
lists (`C16Geometry__CPoint` becomes `C14Shapes__CPoint`).  A function inside `extern "C"` is labelled by its name alone,
so nothing changes.

### Refused

What is declared in an unnamed or inline namespace, a class's member, and a function's own declarations.

## Move to File in a namespace

[Move to File](move.md#move-to-file) takes a function or variable in a namespace: it goes into the target header's block of
its namespace.
