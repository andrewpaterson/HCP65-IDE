#include "Shapes.h"


void CShape::Init(EKind eKind)
{
	meKind = eKind;
}


EKind CShape::GetKind(void)
{
	return meKind;
}
