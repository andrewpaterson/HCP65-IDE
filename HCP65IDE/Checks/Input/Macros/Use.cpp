#include "Macros.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int Next(SUnit* psUnit)
{
	psUnit->miCount++;
	return psUnit->miCount;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int Use(SUnit* psUnit, int iCount)
{
	int			aiUnits[MAX_UNITS];
	int			iBig;
	float		fBig;
	float		fRate;
	const char*	szTitle;

	fRate = RATE;
	szTitle = TITLE;
	iBig = BIGGER(iCount, MAX_UNITS);
	fBig = BIGGER(fRate, 2.0f);
	iBig = SQUARE(iCount + 1);
	iBig = SQUARE(Next(psUnit));
	iBig = 2 * ADD(iCount, 1);
	BUMP(iBig);
	szTitle = NAME_OF(iBig);
	RESET(psUnit);
	CLEAR(psUnit);
	psUnit->miKind = KIND_SOLID;
	aiUnits[0] = SCALE + TWICE(TWICE(iCount));
	aiUnits[1] = LONGER(iBig, 2);
	return iBig + aiUnits[0] + (int)fBig + szTitle[0];
}
