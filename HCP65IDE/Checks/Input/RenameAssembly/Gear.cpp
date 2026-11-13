#include "Gear.h"


void NGear::CGear::Turn(int iSteps)
{
	miTeeth += iSteps;
}


void NGear::Spin(CGear* pcGear)
{
	pcGear->Turn(1);
}


void Pack(CBox<NGear::CGear>* pcBox)
{
	NGear::CGear	cGear;

	cGear.miTeeth = 0;
	pcBox->Fill(cGear);
}


void Load(CBox<int>* pcBox)
{
	pcBox->Fill(1);
}


int NGear::GearCount(int iGears)
{
	return iGears;
}
