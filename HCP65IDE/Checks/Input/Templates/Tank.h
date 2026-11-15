#ifndef __TANK_H__
#define __TANK_H__
#include "Mix.h"


class CTank
{
public:
	CGauge*	mpcGauge;
	CGauge	mcSpare;
	int		miCount;

	CTank(void);
	~CTank(void);
	void	Init(CGauge* pcGauge);
	int		Level(void);
	CTank*	Same(void) { return this; }
};


class CPipe;


class CPipe
{
public:
	CGauge*	mpcGauge;

	CPipe(void) { mpcGauge = 0; }
protected:
	CPipe(CGauge* pcGauge, int iCount = 2) { mpcGauge = pcGauge + iCount; }
};


#endif // __TANK_H__
