#include "Shapes.h"


int TotalArea(CShape* pcFirst, CShape* pcSecond)
{
	return pcFirst->Area() + pcSecond->Area();
}


void DrawAll(CShape* pcShape, CCircle* pcCircle)
{
	pcShape->Draw();
	pcCircle->Draw();
}


int main(void)
{
	CCircle			cCircle;
	CRoundedSquare	cSquare;

	cCircle.Init(1, 2);
	cSquare.Init(3, 4);
	DrawAll(&cCircle, &cCircle);
	return TotalArea(&cCircle, &cSquare);
}
