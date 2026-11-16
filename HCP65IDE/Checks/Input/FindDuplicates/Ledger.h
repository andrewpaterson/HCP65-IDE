#ifndef __LEDGER_H__
#define __LEDGER_H__


struct SEntry
{
	int	iAmount;
	int	iCount;
};


class CLedger
{
protected:
	int		miTotal;
	int		miRate;

public:
	void	Init(int iRate);
	int		Add(int iA, int iB);
	int		Tally(SEntry* psEntry, int iTimes);
	int		Distance(int iX, int iY);
	int		Offset(int iA);
};


class CBook
{
protected:
	int		miTotal;
	int		miRate;

public:
	int		Merge(int iP, int iQ);
	int		Recount(SEntry* psItem, int iRepeat);
	int		Scaled(int iX, int iY);
	int		Shift(int iZ);
	int		Kept(int iA);
};


#endif // __LEDGER_H__
