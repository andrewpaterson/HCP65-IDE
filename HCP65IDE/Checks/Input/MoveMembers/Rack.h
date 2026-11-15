#ifndef __RACK_H__
#define __RACK_H__


class CRackBase
{
public:
	int		miSlots;

	void	Init(void) { miSlots = 0; }
	int		Slots(void) { return miSlots; }
};


class CRack : public CRackBase
{
public:
	int		miDepth;

	int		Depth(void) { return miDepth; }
};


#endif // __RACK_H__
