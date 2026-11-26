#include "BaseLib/LeakDetector.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "TestLib/Assert.h"


void TestWorkspaceIndex(void);
void TestSymbolTable(void);
void TestReferences(void);


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

	TestWorkspaceIndex();
	TestSymbolTable();
	TestReferences();

	MemoryKill();
	TypeConverterKill();
	TypesKill();
	iResult = TestTotalStatistics();
	LeakDetection();
	return iResult;
}

