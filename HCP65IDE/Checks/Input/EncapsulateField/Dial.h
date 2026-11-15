#ifndef __DIAL_H__
#define __DIAL_H__


struct SMark
{
	int	iAt;
};


class CDial
{
public:
	int		miValue;
	int		miStep;
	SMark	msMark;
	int		miRange, miMin;

	void	Init(void);
	int		Twice(void) { return miValue * 2; }
};


class CKnob
{
protected:
	int		miTurn;

public:
	int		miGrip;

	void	Init(void) { miTurn = 0; }
};


#endif // __DIAL_H__
