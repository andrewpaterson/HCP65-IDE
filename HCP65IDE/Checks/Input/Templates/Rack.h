#ifndef __RACK_H__
#define __RACK_H__
#include "Mix.h"


//A template nothing makes an instance of.
template<class M>
class CRack
{
public:
	CGauge	mcGauge;
	M*		mp;

	int		Level(void) { return mcGauge.Read(); }
};


#endif // __RACK_H__
