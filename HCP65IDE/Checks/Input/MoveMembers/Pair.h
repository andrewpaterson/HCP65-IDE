#ifndef __PAIR_H__
#define __PAIR_H__


class CRight;


class CLeft
{
public:
	CRight*	mpcRight;

	int		Get(void) { return 1; }
};


class CRight
{
public:
	CLeft	mcLeft;
};


#endif // __PAIR_H__
