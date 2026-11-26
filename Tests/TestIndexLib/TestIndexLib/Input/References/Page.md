# Shapes

A shape is a [`CShape`], made by [`CShape::Init`] or [`CShape::Init(const char *, int)`] and asked [`CShape::GetSides()`].
Its kind is [`EK_Square`] or [`EKind::EK_Square`]; its header [`Shapes/Shape.h`] is in [`Lib/Shapes/Shape.h`], the plan in [`Notes Plan.txt`].
Broken: [`CShape::Kill`], [`CShape::Init(float)`], [`Shape.h`] and [`Missing.h`].
Plain: `CShape`, `CShape::Init` and `CTank`, `x = 1`, `main`, `X`, `#!text abs`, `abs`, `Shape.h`.
Already links: [`CShape`](shapes.md), [`CShape`][shape], ![`CShape`](shape.png) and \[`CShape`].
[`CShape`]: shapes.md
A colon after a reference isn't a definition: [`CShape`]: a shape.

```cpp
CShape cShape;   // [`Missing`] in a fence is code
```

<!-- [`Missing`] in a comment, `CShape`
still in it --> after it `CShape::GetSides`
