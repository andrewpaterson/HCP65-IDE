#ifdef LINUX_GNU
#include "Shape.h"


void CShape::InitPlatform(void)
{
	CPen	cPen;

	Draw(&cPen);
	cPen.Draw();
}


#endif // LINUX_GNU
