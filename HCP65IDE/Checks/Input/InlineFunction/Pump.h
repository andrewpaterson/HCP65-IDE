#ifndef __PUMP_H__
#define __PUMP_H__


template<class M>
class CSlot
{
public:
	M		mcItem;
	int		miUsed;

	int		Bytes(void) { return miUsed * sizeof(M); }
};


class CPump
{
protected:
	int		miRate;
	int		miCount;

public:
	int		miLimit;

	void		Init(int iRate);
	int			GetRate(void) { return miRate; }
	int			Scaled(int iBy) { return miLimit * iBy; }
	void		Step(int iBy);
	int			Run(int iTimes);
	virtual int	Kind(void);
	int			Loop(int iDepth);
	int			Pick(int iA);
	int			Bump(int iA);
};


int		Twice(int iValue);
void	Report(CPump* pcPump, int iWhat);


#endif // __PUMP_H__
