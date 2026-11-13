#include "65CoreCycles.h"


int main(void)
{
	C65Core			cCore;
	C65CoreCycles	cCycles;

	cCore.Init();
	cCycles.mpcCore = &cCore;
	return 0;
}
