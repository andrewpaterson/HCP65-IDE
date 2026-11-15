#ifndef __DRUM_H__
#define __DRUM_H__
#include "Vessel.h"


class CDrum : public CVessel
{
private:
	int		miSecret;

protected:
	int		miLevel;
	int		miRate, miStep;

public:
	void	Init(int iVolume);
	//How full it is.
	int		GetLevel(void) { return miLevel; }
	void	Fill(int iAmount);
	int		Spare(void);
	void	Roll(void);
	static int	Count(void);
	void	Hide(void);
};


class CKeg : public CVessel
{
public:
	void	Tap(void);
	void	Vent(void) override;
};


#endif // __DRUM_H__
