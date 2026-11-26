# Examples

<!-- cpp
#include "../Shapes.h"
CShape	cShape;
-->

Statements, compiled in a function:

```cpp
cShape.Init(EK_Square);
cShape.GetKind();
```

A function written out:

```cpp
EKind Kind(CShape* pcShape)
{
	return pcShape->GetKind();
}
```

A class:

```cpp
class CSquare : public CShape
{
};
```

Not checked:

```{.cpp .no-check}
This isn't C++.
```

```text
Nor is this.
```

A mistake:

```cpp
cShape.Init(EK_Square);
cShape.Draw();
```

Taken from a test, so compiled there:

```cpp
--8<-- "Test.cpp:example"
```
