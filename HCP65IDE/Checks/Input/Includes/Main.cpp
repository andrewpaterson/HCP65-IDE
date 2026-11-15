#include "Tank.h"
#include "Pipe.h"
#include "Holder.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	CTank				cTank;
	CGauge				cGauge;
	CSpare				cSpare;
	CHolder<CGauge>		cHolder;

	cGauge.Init();
	cHolder.Start(&cGauge);
	cTank.Init(&cGauge);
	cSpare.miCount = cTank.Level();
	return cSpare.miCount;
}
