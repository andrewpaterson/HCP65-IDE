#ifndef __MACROS_H__
#define __MACROS_H__


#define MAX_UNITS	8	//The most there are.
#define SCALE		(MAX_UNITS * 2)
#define TITLE		"Units"
#define RATE		1.5f
#define LIMIT		4
#define DEBUG_UNITS

#define KIND_NONE	0	//Nothing.
#define KIND_SOLID	1
#define KIND_LIQUID	2

#define BIGGER(a, b)	((a) > (b) ? (a) : (b))
#define SQUARE(x)		((x) * (x))
#define ADD(a, b)		a + b
#define BUMP(x)			((x)++)
#define NAME_OF(x)		#x
#define RESET(p)		do { (p)->miCount = 0; (p)->miKind = KIND_NONE; } while (0)
#define TWICE(x)		((x) + (x))
#define LONGER(a, b) \
	((a) + \
	 (b))


struct SUnit
{
	int		miCount;
	int		miKind;
};


int Next(SUnit* psUnit);


#define CLEAR(p)	do { (p)->miCount = 0; (p)->miKind = KIND_NONE; } while (0)


#endif // __MACROS_H__
