#ifndef __VALVE_H__
#define __VALVE_H__


class CValve
{
protected:
	int		miFlow;

public:
	void			Init(void);
	virtual int		Open(int iAmount, int iSeconds = 5) const;
	void			Close(int iDelay);
	int				Turn(int iAngle, bool bFast) const;
};


#endif // __VALVE_H__
