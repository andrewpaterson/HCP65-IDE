//What the debugger's checks stop in (IDEChecks.py's debug-* checks): a class, a recursive function, an array, a char*,
//an enum and a float, run by the kernal as the card's start up program.  The line numbers are the checks'.
#include "HCP65User.h"


enum EColour { EC_Red, EC_Green = 5, EC_Blue };


class CCounter
{
public:
	int		miCount;
	EColour	meColour;
	float	mfScale;

	void	Add(int i);
};


int			gaiValues[4] = { 3, 1, 4, 1 };
const char*	gszName = "Debugging";
float		gfScale = 1.5f;


void CCounter::Add(int i)
{
	miCount = miCount + i;
}


int Sum(CCounter* pcCounter, int iDepth)
{
	int	iBelow;

	if (iDepth == 0)
	{
		return 0;
	}
	iBelow = Sum(pcCounter, iDepth - 1);
	pcCounter->Add(gaiValues[iDepth - 1]);
	return iBelow + gaiValues[iDepth - 1];
}


int main(void)
{
	CCounter	cCounter;
	int			iTotal;

	cCounter.miCount = 0;
	cCounter.meColour = EC_Green;
	cCounter.mfScale = gfScale;
	iTotal = Sum(&cCounter, 4);
	return iTotal + gszName[0] - 'D';
}


//What the index-target check looks for: the code model indexes this file as the configuration's compiler compiles
//it, so only the W65C816's or the 65C64's is there.  Nothing calls them.
#ifdef _WIN32
int OnlyOnWindows(void) { return 1; }
#endif


#ifdef __W65C816__
int OnlyOnW65C816(void) { return 2; }
#endif


#ifdef __65C64__
int OnlyOn65C64(void) { return 3; }
#endif
