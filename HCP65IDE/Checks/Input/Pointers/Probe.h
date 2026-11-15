#ifndef __PROBE_H__
#define __PROBE_H__
#include "Vat.h"


class CProbe
{
public:
	SLevel&	msLevel;
	SLevel*	mpsSpare;
	int		miCount;

	CProbe(SLevel& sLevel);
	int		Read(void);
	void	Set(int iDepth);
};


int		Mark(SLevel* psLevel);
int		Probe(CProbe* pcProbe, SLevel* psLevel);


#endif // __PROBE_H__
