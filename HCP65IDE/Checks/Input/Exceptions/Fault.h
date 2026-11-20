#ifndef __FAULT_H__
#define __FAULT_H__


class CFault
{
public:
	int		miCode;

			CFault(int iCode);
	int		GetCode(void) const;
};


int	Check(int iLimit) throw(CFault);
int	Guarded(int iValue);
int	Choose(int iValue);
int	Wrapped(int iValue);
int	Promise(int iValue) noexcept;
int	Safe(int iValue) noexcept;
int	Use(int iValue);
int	Waited(int iValue);


class CTimeout
{
public:
	int		miAfter;
};


#endif // __FAULT_H__

