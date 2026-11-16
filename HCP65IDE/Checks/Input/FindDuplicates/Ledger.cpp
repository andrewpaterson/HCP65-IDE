#include "Ledger.h"


void CLedger::Init(int iRate)
{
	miTotal = 0;
	miRate = iRate;
}


int CLedger::Add(int iA, int iB)
{
	int	iSum;

	iSum = iA + iB;
	iSum = iSum * miRate;
	miTotal += iSum;
	return iSum;
}


int CLedger::Tally(SEntry* psEntry, int iTimes)
{
	int	i;
	int	iAll;

	iAll = 0;
	for (i = 0; i < iTimes; i++)
	{
		iAll += psEntry->iAmount * 3;
		psEntry->iCount++;
	}
	miTotal += iAll;
	return iAll;
}


int CLedger::Distance(int iX, int iY)
{
	return iX * iX + iY * iY;
}


int CLedger::Offset(int iA)
{
	return iA * 4 + 16;
}
