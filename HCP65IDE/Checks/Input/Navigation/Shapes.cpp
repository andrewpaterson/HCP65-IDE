#include "Shapes.h"


void CCircle::Draw(void)
{
	Move(0, 0);
}


int CCircle::Area(void)
{
	return 3 * miRadius * miRadius;
}


void CSquare::Draw(void)
{
}


int CSquare::Area(void)
{
	return miSide * miSide;
}


void CRoundedSquare::Draw(void)
{
	CSquare::Draw();
}
