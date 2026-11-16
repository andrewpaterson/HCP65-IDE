#include "Shapes.h"


using namespace Geometry;
using Drawing::Plot;


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	CPoint				cPoint;
	Geometry::CPoint	cOther;
	int					i;

	cPoint.Init(1, 2);
	cOther.Init(3, 4);
	Plot(&cPoint);
	i = Area(&cOther) + Geometry::Detail::Twice(giCount);
	return i;
}
