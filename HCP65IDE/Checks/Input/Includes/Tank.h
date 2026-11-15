#ifndef __TANK_H__
#define __TANK_H__
#include "Gauge.h"
#include "Valve.h"
#include "Spare.h"
#include "Units.h"


class CTank
{
public:
	CGauge*	mpcGauge;
	CValve	mcValve;
	int		maiUnits[MAX_UNITS];

	void	Init(CGauge* pcGauge);
	int		Level(void);
};


#endif // __TANK_H__
