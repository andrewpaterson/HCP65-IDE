#include "Gauge.h"


//////////////////////////////////////////////////////////////////////////
//
// The gauge reads nothing, and its needle stops at iLimit.
//
//////////////////////////////////////////////////////////////////////////
void CGauge::Init(int iLimit)
{
	miValue = 0;
	miLimit = iLimit;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int CGauge::GetValue(void)
{
	return miValue;
}


//////////////////////////////////////////////////////////////////////////
//
// The needle goes to iValue, or to the limit if that is less.
//
// False if it was past the limit.
// A value under nothing is nothing.
// The machine is not told.
//
//////////////////////////////////////////////////////////////////////////
bool CGauge::SetValue(int iValue)
{
	miValue = ClampGauge(iValue, miLimit);
	return miValue == iValue;
}


//////////////////////////////////////////////////////////////////////////
//
// The needle goes back to nothing, a step at a time.
//
// Step 1: the needle is moved a little nearer to nothing.
// Step 2: the needle is moved a little nearer to nothing.
// Step 3: the needle is moved a little nearer to nothing.
// Step 4: the needle is moved a little nearer to nothing.
// Step 5: the needle is moved a little nearer to nothing.
// Step 6: the needle is moved a little nearer to nothing.
// Step 7: the needle is moved a little nearer to nothing.
// Step 8: the needle is moved a little nearer to nothing.
// Step 9: the needle is moved a little nearer to nothing.
// Step 10: the needle is moved a little nearer to nothing.
// Step 11: the needle is moved a little nearer to nothing.
// Step 12: the needle is moved a little nearer to nothing.
// Step 13: the needle is moved a little nearer to nothing.
// Step 14: the needle is moved a little nearer to nothing.
// Step 15: the needle is moved a little nearer to nothing.
// Step 16: the needle is moved a little nearer to nothing.
// Step 17: the needle is moved a little nearer to nothing.
// Step 18: the needle is moved a little nearer to nothing.
// Step 19: the needle is moved a little nearer to nothing.
// Step 20: the needle is moved a little nearer to nothing.
//
//////////////////////////////////////////////////////////////////////////
void CGauge::Reset(void)
{
	miValue = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// iValue, kept from nothing to iLimit.
//
//////////////////////////////////////////////////////////////////////////
int ClampGauge(int iValue, int iLimit)
{
	if (iValue < 0)
	{
		return 0;
	}
	return (iValue > iLimit) ? iLimit : iValue;
}


int ReadGauges(CGauge* pcGauge)
{
	int		iTotal;  //Of every value read: "one // two" is a string.
	char*	szName = (char*)"one // two";

	pcGauge->Init(GAUGE_MAXIMUM);
	pcGauge->SetValue(40);
	iTotal = pcGauge->GetValue();
	pcGauge->Reset();
	return ClampGauge(iTotal, GAUGE_MAXIMUM) + szName[0];
}
