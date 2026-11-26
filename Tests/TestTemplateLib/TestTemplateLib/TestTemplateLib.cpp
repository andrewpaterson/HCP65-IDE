#include "BaseLib/LeakDetector.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "TestLib/Assert.h"


void TestTemplateText(void);
void TestCodeFormatter(void);


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int __cdecl main(void)
{
	int iResult;

	BreakOnAllocation(0);
	InitTotalStatistics();
	TypesInit();
	TypeConverterInit();
	MemoryInit();

	TestTemplateText();
	TestCodeFormatter();

	MemoryKill();
	TypeConverterKill();
	TypesKill();
	iResult = TestTotalStatistics();
	LeakDetection();
	return iResult;
}

