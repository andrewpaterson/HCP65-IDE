#include "Fault.h"


CFault::CFault(int iCode)
{
	miCode = iCode;
}


int CFault::GetCode(void) const
{
	return miCode;
}


int Check(int iLimit) throw(CFault)
{
	if (iLimit < 0)
	{
		throw CFault(iLimit);
	}
	return iLimit * 2;
}


int Guarded(int iValue)
{
	int	iResult;

	try
	{
		iResult = Check(iValue);
	}
	catch (CFault& cFault)
	{
		iResult = cFault.GetCode() + 1;
		iResult = iResult * 2;
	}
	return iResult;
}


int Choose(int iValue)
{
	if (iValue > 10)
	{
		return iValue - 10;
	}
	throw CFault(iValue);
}


int Wrapped(int iValue)
try
{
	return Check(iValue);
}
catch (...)
{
	return 0;
}


int Promise(int iValue) noexcept
{
	return Check(iValue);
}


int Safe(int iValue) noexcept
{
	return iValue + 1;
}


int Use(int iValue)
{
	return Wrapped(iValue) + Promise(iValue) + Safe(iValue) + Guarded(iValue) + Choose(iValue);
}


int Waited(int iValue)
{
	try
	{
		if (iValue == 0)
		{
			throw CTimeout();
		}
	}
	catch (CTimeout& cTimeout)
	{
		return cTimeout.miAfter;
	}
	return 0;
}


int Relayed(int iValue) noexcept(noexcept(Safe(iValue)))
{
	return Safe(iValue);
}

