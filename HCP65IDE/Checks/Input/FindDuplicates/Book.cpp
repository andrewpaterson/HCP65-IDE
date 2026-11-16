#include "Ledger.h"


int CBook::Merge(int iP, int iQ)
{
	int	iJoined;

	iJoined = iP + iQ;
	iJoined = iJoined * miRate;
	miTotal += iJoined;
	return iJoined;
}


int CBook::Recount(SEntry* psItem, int iRepeat)
{
	int	j;
	int	iEvery;

	iEvery = 0;
	for (j = 0; j < iRepeat; j++)
	{
		iEvery += psItem->iAmount * 3;
		psItem->iCount++;
	}
	miTotal += iEvery;
	miRate = 1;
	return iEvery;
}


int CBook::Scaled(int iX, int iY)
{
	int	iLength;

	iLength = iX * iX + iY * iY;
	return iLength * 4;
}


int CBook::Shift(int iZ)
{
	return iZ * 8 + 32;
}


int CBook::Kept(int iA)
{
	return iA * 4 + miRate;
}
