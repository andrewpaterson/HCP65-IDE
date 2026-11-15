#ifndef __TANK_H__
#define __TANK_H__
#include "Gauge.h"


class CTank
{
public:
	CGauge*	mpcGauge;

	void	Init(CGauge* pcGauge);
	int		Level(void);
};


#endif // __TANK_H__
