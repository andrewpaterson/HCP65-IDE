#include "Shape.h"


void CShape::Init(int iX, int iY)
{
	miX = iX;
	miY = iY;
}


void CShape::Draw(void)
{
}


void CShape::Move(int iX, int iY)
{
	miX += iX;
	miY += iY;
	Draw();
}
