#ifndef __LIMITS_H__
#define __LIMITS_H__


const int	giMostUnits = 8;
const int	giSpare = giMostUnits * 2 + 1;
int			giCount = 0;
const int	giLive = giCount + 1;


class CLimits
{
public:
	static const int	miDepth = 4;

	int		Deepest(void) { return miDepth * 2; }
};


#endif // __LIMITS_H__
