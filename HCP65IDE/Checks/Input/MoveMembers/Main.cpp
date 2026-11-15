#include "Drum.h"
#include "Crate.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	CDrum		cDrum;
	CKeg		cKeg;
	CVessel*	pcVessel;
	CCrate<int>	cCrate;

	cDrum.Init(4);
	cDrum.Seal();
	pcVessel = &cKeg;
	pcVessel->Vent();
	pcVessel->Seal();
	return cDrum.Spare() + cCrate.Size();
}
