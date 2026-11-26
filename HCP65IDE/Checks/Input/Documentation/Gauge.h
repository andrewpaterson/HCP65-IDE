#ifndef __GAUGE_H__
#define __GAUGE_H__


//The most a gauge reads.
#define GAUGE_MAXIMUM	100


//A gauge shows one value of its machine as a needle on a dial.
//Its machine reads it each tick.
class CGauge
{
protected:
	int		miValue;  //What the needle points at.
	int		miLimit;

public:
	void	Init(int iLimit);
	int		GetValue(void);
	bool	SetValue(int iValue);
	void	Reset(void);
};


int		ClampGauge(int iValue, int iLimit);


#endif // __GAUGE_H__
