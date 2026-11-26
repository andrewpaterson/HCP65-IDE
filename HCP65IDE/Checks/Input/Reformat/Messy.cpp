#include "Shapes.h"
void CCircle::Draw(void) {
    int i;
    int *pi;
  for(i=0;i<3;i++)
	{
		Move(i,0);  
	}
}


int CCircle::Area(void)
{
	return 3 * miRadius * miRadius;
}


//Draws nothing.
void CSquare::Draw(void)
{
}


int CSquare::Area(void) { return miSide*miSide; }
