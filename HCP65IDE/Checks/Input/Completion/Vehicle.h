#ifndef __VEHICLE_H__
#define __VEHICLE_H__
#include "Parts/Wheel.h"


#define VEHICLE_WHEELS	4


enum EVehicleKind
{
	VK_Car,
	VK_Truck,
};


typedef CWheel*	WheelPointer;


//Holds one of something, as a smart pointer does.
template<class M>
class CHolder
{
public:
	M*	mpHeld;

	M*	operator->(void) { return mpHeld; }
	M*	Get(void) { return mpHeld; }
};


class CVehicle
{
private:
	int				miSerial;

protected:
	CWheel			macWheels[VEHICLE_WHEELS];
	int				miSpeed;

public:
	EVehicleKind	meKind;

	void			Init(EVehicleKind eKind);
	int				GetSpeed(void);
	void			SetSpeed(int iSpeed);
	void			SetSpeed(int iSpeed, int iGear);
	CWheel*			GetWheel(int iIndex);
	WheelPointer	GetSpare(void);
	static int		Count(void);

protected:
	void			Service(void);

private:
	void			Scrap(void);
};


class CTruck : public CVehicle
{
public:
	int		miLoad;

	void	Load(int iWeight);
	int		GetLoad(void);
};


namespace Garage
{
	int			Park(CVehicle* pcVehicle);
	CVehicle*	Find(int iSerial);
	extern int	giSpaces;
}


#endif // __VEHICLE_H__
