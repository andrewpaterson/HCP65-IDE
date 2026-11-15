#ifndef __VESSEL_H__
#define __VESSEL_H__


class CVessel
{
protected:
	int		miVolume;

public:
	void			Init(int iVolume);
	int				GetVolume(void) { return miVolume; }
	virtual void	Vent(void);
	void			Seal(void);
};


#endif // __VESSEL_H__
