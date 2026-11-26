#include "Vehicle.h"


int Garage::giSpaces;


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CWheel::Inflate(int iPressure)
{
	miPressure = iPressure;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int CWheel::GetPressure(void)
{
	return miPressure;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVehicle::Init(EVehicleKind eKind)
{
	meKind = eKind;
	miSerial = 0;
	miSpeed = 0;
}


int CVehicle::GetSpeed(void) { return miSpeed; }
void CVehicle::SetSpeed(int iSpeed) { miSpeed = iSpeed; }
void CVehicle::SetSpeed(int iSpeed, int iGear) { miSpeed = iSpeed * iGear; }
CWheel* CVehicle::GetWheel(int iIndex) { return &macWheels[iIndex]; }
WheelPointer CVehicle::GetSpare(void) { return &macWheels[0]; }
int CVehicle::Count(void) { return 1; }
void CVehicle::Service(void) { miSpeed = 0; }
void CVehicle::Scrap(void) { miSerial = -1; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTruck::Load(int iWeight)
{
	miLoad = iWeight;

}


int CTruck::GetLoad(void) { return miLoad; }
int Garage::Park(CVehicle* pcVehicle) { return pcVehicle->GetSpeed(); }
CVehicle* Garage::Find(int iSerial) { return 0; }


//////////////////////////////////////////////////////////////////////////
//
// How far the vehicle goes.
//
//////////////////////////////////////////////////////////////////////////
int Drive(CVehicle* pcVehicle, CTruck cTruck, CHolder<CVehicle> cHeld)
{
	int		iDistance;
	CWheel*	pcWheel;

	iDistance = 0;
	pcWheel = pcVehicle->GetWheel(0);

	return iDistance;
}
