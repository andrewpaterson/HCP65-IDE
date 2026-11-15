#include "Rig.h"
#include "High/Frame.h"
#include <string.h>
#include "Low/Nut.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int Build(void)
{
	SFrame	sFrame;
	SNut	sNut;
	SRig	sRig;
	SWasher	sWasher;

	sFrame.sBolt.iSize = 1;
	sNut.iSize = 2;
	sRig.iCount = 3;
	sWasher.iSize = 4;
	return sFrame.sBolt.iSize + sNut.iSize + sRig.iCount + sWasher.iSize;
}
