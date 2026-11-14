#include "Shape.h"


int main(void)
{
	CShape		cShape;
	CCircle		cCircle;
	EShapeKind	eKind;
	int			iUnusedLocal;

	//UnusedFunction isn't called, and "TSpare" is only in this string's comment.
	cShape.Init();
	eKind = SK_Circle;
	if (eKind == SK_Square)
	{
		cCircle.Init();
	}
	return UsedFunction(cShape.Area());
}
