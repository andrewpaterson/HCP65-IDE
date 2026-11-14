#include "Calc.h"


int main(void)
{
	CCalc			cCalc;
	CStack<CCalc>	cStack;

	cCalc.Init();
	cStack.mpTop = &cCalc;
	cStack.miDepth = 1;
	return cCalc.Total(2) + cStack.Depth();
}
