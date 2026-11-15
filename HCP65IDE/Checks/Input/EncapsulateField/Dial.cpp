#include "Dial.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CDial::Init(void)
{
	miValue = 0;
	miStep = 1;
	msMark.iAt = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int UseDial(CDial* pcDial, CKnob* pcKnob)
{
	CDial	cOther;
	SMark*	psMark;
	int*	piStep;
	int		iRead;

	cOther.Init();
	cOther.miValue = pcDial->miValue + 1;
	pcDial->miValue += 2;
	pcDial->miValue++;
	iRead = cOther.miValue * pcDial->miValue;
	piStep = &pcDial->miStep;
	pcDial->msMark.iAt = iRead;
	psMark = &cOther.msMark;
	pcKnob->miGrip = *piStep + psMark->iAt;
	return iRead + cOther.Twice();
}
