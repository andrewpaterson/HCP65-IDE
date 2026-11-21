#ifndef __BASES_H__
#define __BASES_H__


//Classes with more than one base class, for the hierarchies.
class CNamed
{
public:
	virtual const char*	Name(void) = 0;
};


class CSwitchable
{
public:
	virtual void	Switch(bool bOn);
};


class CCounted
{
public:
	int		miCount;
};


class CThing : public CCounted
{
};


class CWidget : public CThing, public CNamed
{
public:
	const char*	Name(void) override;
};


class CDial : public CWidget
{
};


class CLamp : public CDial, public CSwitchable, public CNamed
{
public:
	void	Switch(bool bOn) override;
};


#endif // __BASES_H__
