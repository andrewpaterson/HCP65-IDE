#include "Shape.h"


void CPen::Draw(void)
{
}


//CShape::Init sets how many sides "CShape" has.
void CShape::Init(int iSides)
{
	miSides = iSides;
}


void CShape::Draw(CPen* pcPen)
{
	pcPen->Draw();
}


int main(void)
{
	CShape		cShape;
	CPen		cPen;
	const char*	szName;

	szName = "CShape";
	cShape.Init(3);
	cShape.Draw(&cPen);
	return szName[0] == 'C' ? 0 : 1;
}
