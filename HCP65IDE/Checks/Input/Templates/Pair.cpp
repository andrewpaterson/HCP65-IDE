#include "Pair.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int UsePair(void)
{
	CPair<CGauge, int>		cPair;
	CPair<CGauge, float>	cOther;
	CLone<CGauge>			cLone;
	CGauge					cGauge;

	cGauge.miLevel = 1;
	cPair.Set(&cGauge, 2);
	cOther.Set(&cGauge, 2.0f);
	cLone.Hold(&cGauge);
	return cPair.Read() + cOther.Read() + cLone.mp->Read();
}
