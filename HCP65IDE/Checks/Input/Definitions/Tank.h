#ifndef __TANK_H__
#define __TANK_H__


class CTank
{
protected:
	int		miLevel;
	int		miSize;

public:
			CTank(int iSize);
	void	Init(int iSize);
	void	Fill(int iAmount, int iRate = 1);
	int		GetLevel(void) const { return miLevel; }
	int		Drain(int iAmount)
	{
		int	iTaken;

		iTaken = (iAmount < miLevel) ? iAmount : miLevel;
		miLevel -= iTaken;
		return iTaken;
	}
	bool	IsFull(void) const;
	void	Empty(void);
	virtual void	Vent(void) =0;
};


template<class M>
class CStore
{
protected:
	M	mcItem;

public:
	void	Init(void);
	M*		Get(void);
	void	Set(M* pcItem) { mcItem = *pcItem; }
};


template<class M>
void CStore<M>::Init(void)
{
}


int		MeasureTank(CTank* pcTank);


#endif // __TANK_H__
