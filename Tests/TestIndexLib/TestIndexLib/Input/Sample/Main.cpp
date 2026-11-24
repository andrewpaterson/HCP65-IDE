#include <Extra.h>
#include "Shape.h"


//A member call through a pointer: the index has it as CShape::GetSides's only because the expression is typed.
int CountSides(CShape* pcShape)
{
	return pcShape->GetSides();
}


int UseExtra(SExtra* psExtra)
{
	return psExtra->iValue;
}


#ifdef SAMPLE_DEFINE
//Only with the project's own define (Debug|x64's PreprocessorDefinitions).
int ProjectDefined(void)
{
	return 1;
}
#endif


#ifdef _WIN64
//Only with MSVC's own defines for x64.
int Wide(void)
{
	return 64;
}
#endif


#ifdef _M_IX86
//Only with MSVC's own defines for Win32.
int Narrow(void)
{
	return 32;
}
#endif


#ifdef _DEBUG
//Only in a Debug configuration.
int Debugging(void)
{
	return 1;
}
#endif
