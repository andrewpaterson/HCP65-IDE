#include <string.h>
#include "BaseLib/StringHelper.h"
#include "BuildLib/BuildWorkspace.h"
#include "IndexLib/WorkspaceIndex.h"
#include "TestLib/Assert.h"


#define TEST_WORKSPACE_INDEX_SAMPLE		"Input" _FS_ "Sample" _FS_ "Sample.sln"
#define TEST_WORKSPACE_INDEX_TARGET		"Input" _FS_ "Target" _FS_ "Target.sln"
#define TEST_WORKSPACE_INDEX_LIBC65		"Input" _FS_ "Target" _FS_ "Libc65"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool TestWorkspaceIndexEndsWith(const char* szName, const char* szEnd)
{
	size	uiName;
	size	uiEnd;

	uiName = strlen(szName);
	uiEnd = strlen(szEnd);
	return (uiName >= uiEnd) && (StringCompare(&szName[uiName - uiEnd], szEnd) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the index has a place of szKey in a file whose name ends with
// szFile.
//
//////////////////////////////////////////////////////////////////////////
static bool TestWorkspaceIndexHasPlaceIn(CCPPIndex* pcIndex, const char* szKey, const char* szFile)
{
	CCPPIndexSymbol*	pcSymbol;
	size				i;

	pcSymbol = pcIndex->FindSymbol(szKey);
	if (pcSymbol == NULL)
	{
		return false;
	}
	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		if (TestWorkspaceIndexEndsWith(pcIndex->GetFileName(pcSymbol->GetLocation(i)->uiFile), szFile))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The sample solution indexed with its first configuration, Debug|x64:
// both .cpp files (the .asm isn't C++), the header they share gathered
// once, the include path's header found, the project's own define and
// MSVC's for x64 and Debug defined, and a member call through a pointer
// resolved to the member it calls.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspaceIndexSample(void)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CCPPIndex*			pcIndex;
	SCPPIndexLocation*	psDefinition;
	CArrayChars			aszProblems;
	CChars				szUnit;
	size				uiIndexed;
	size				uiToIndex;
	size				uiProject;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_INDEX_SAMPLE));
	AssertSize(1, cWorkspace.NumProjects());

	cIndex.Init(&cWorkspace);
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();

	cIndex.GetIndexingProgress(&uiIndexed, &uiToIndex);
	AssertSize(2, uiToIndex);
	AssertSize(2, uiIndexed);
	AssertFalse(cIndex.IsIndexing());
	AssertSize(1, cIndex.NumIndexings());
	AssertTrue(cIndex.GetIndexingMilliseconds() >= 0);

	aszProblems.Init();
	cIndex.TakeIndexingProblems(&aszProblems);
	AssertSize(0, aszProblems.NumElements());
	aszProblems.Kill();
	AssertSize(0, cIndex.NumIndexingProblemUnits());

	pcIndex = cIndex.GetIndex();
	AssertNotNull(pcIndex->FindSymbol("CShape"));
	AssertNotNull(pcIndex->FindSymbol("CShape::Init(int)"));
	AssertNotNull(pcIndex->FindSymbol("CountSides(CShape*)"));
	AssertNotNull(pcIndex->FindSymbol("SExtra"));
	AssertNotNull(pcIndex->FindSymbol("UseExtra(SExtra*)"));

	//The member call in Main.cpp is a place of CShape::GetSides, which Shape.h declares and Shape.cpp defines.
	AssertTrue(TestWorkspaceIndexHasPlaceIn(pcIndex, "CShape::GetSides()", "Main.cpp"));
	AssertTrue(TestWorkspaceIndexHasPlaceIn(pcIndex, "CShape::GetSides()", "Shape.h"));
	AssertTrue(TestWorkspaceIndexHasPlaceIn(pcIndex, "CShape::GetSides()", "Shape.cpp"));

	//Debug|x64: the project's define, and MSVC's for x64 and Debug.
	AssertNotNull(pcIndex->FindSymbol("ProjectDefined()"));
	AssertNotNull(pcIndex->FindSymbol("Wide()"));
	AssertNotNull(pcIndex->FindSymbol("Debugging()"));
	AssertNull(pcIndex->FindSymbol("Narrow()"));

	//The header both units include (where CShape is defined) was gathered by the first to include it.
	psDefinition = pcIndex->FindSymbol("CShape")->GetDefinition();
	AssertNotNull(psDefinition);
	AssertTrue(TestWorkspaceIndexEndsWith(pcIndex->GetFileName(psDefinition->uiFile), "Shape.h"));
	szUnit.Init();
	AssertTrue(cIndex.GetIncluder(pcIndex->GetFileName(psDefinition->uiFile), &uiProject, &szUnit));
	AssertSize(0, uiProject);
	AssertTrue(szUnit.EndsWith("Main.cpp") || szUnit.EndsWith("Shape.cpp"));
	szUnit.Kill();

	cIndex.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Indexed again as Release|Win32 (a build configuration a solution
// without an .hcp65build file has for each of its own): the project's
// Debug|x64 define is gone, and MSVC's are Win32's and not Debug's.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspaceIndexConfiguration(void)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CCPPIndex*			pcIndex;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_INDEX_SAMPLE));
	cIndex.Init(&cWorkspace);

	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	AssertNotNull(cIndex.GetIndex()->FindSymbol("Wide()"));

	cIndex.SetConfiguration("Release|Win32");
	AssertString("Release|Win32", cIndex.GetConfiguration());
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	AssertSize(2, cIndex.NumIndexings());

	pcIndex = cIndex.GetIndex();
	AssertNotNull(pcIndex->FindSymbol("CountSides(CShape*)"));
	AssertNotNull(pcIndex->FindSymbol("Narrow()"));
	AssertNull(pcIndex->FindSymbol("Wide()"));
	AssertNull(pcIndex->FindSymbol("ProjectDefined()"));
	AssertNull(pcIndex->FindSymbol("Debugging()"));

	cIndex.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace indexed with szConfiguration, with no problems.
//
//////////////////////////////////////////////////////////////////////////
static void TestWorkspaceIndexTargetIndex(CWorkspaceIndex* pcIndex, const char* szConfiguration)
{
	CArrayChars	aszProblems;

	pcIndex->SetConfiguration(szConfiguration);
	pcIndex->StartIndexing();
	pcIndex->WaitForIndexing();
	aszProblems.Init();
	pcIndex->TakeIndexingProblems(&aszProblems);
	AssertSize(0, aszProblems.NumElements());
	aszProblems.Kill();
	AssertNotNull(pcIndex->GetIndex()->FindSymbol("Always()"));
}


//////////////////////////////////////////////////////////////////////////
//
// A solution read with an HCP65 build configuration is indexed as
// hcp65build compiles it: the target's defines and none of MSVC's, and
// Libc65's headers for the system's.  Read with MSVC's it is as before.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspaceIndexTarget(void)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CCPPIndex*			pcIndex;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_INDEX_TARGET));
	cIndex.Init(&cWorkspace);
	cIndex.SetLibc65Directory(TEST_WORKSPACE_INDEX_LIBC65);
	pcIndex = cIndex.GetIndex();

	TestWorkspaceIndexTargetIndex(&cIndex, "Debug|Win32");
	AssertNotNull(pcIndex->FindSymbol("OnHost()"));
	AssertNull(pcIndex->FindSymbol("OnW65C816()"));
	AssertNull(pcIndex->FindSymbol("On65C64()"));
	AssertNull(pcIndex->FindSymbol("SOnly65"));

	TestWorkspaceIndexTargetIndex(&cIndex, "Debug|W65C816");
	AssertNull(pcIndex->FindSymbol("OnHost()"));
	AssertNotNull(pcIndex->FindSymbol("OnW65C816()"));
	AssertNotNull(pcIndex->FindSymbol("WithAssembly()"));
	AssertNull(pcIndex->FindSymbol("On65C64()"));
	AssertNotNull(pcIndex->FindSymbol("SOnly65"));

	TestWorkspaceIndexTargetIndex(&cIndex, "Debug|65C64");
	AssertNull(pcIndex->FindSymbol("OnHost()"));
	AssertNull(pcIndex->FindSymbol("OnW65C816()"));
	AssertNull(pcIndex->FindSymbol("WithAssembly()"));
	AssertNotNull(pcIndex->FindSymbol("On65C64()"));
	AssertNotNull(pcIndex->FindSymbol("SOnly65"));

	cIndex.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Which files are indexed: C and C++, whatever the case of the extension.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspaceIndexSourceFiles(void)
{
	AssertTrue(IsIndexedSourceFile("Main.cpp"));
	AssertTrue(IsIndexedSourceFile("Old.CPP"));
	AssertTrue(IsIndexedSourceFile("a.cc"));
	AssertTrue(IsIndexedSourceFile("a.cxx"));
	AssertTrue(IsIndexedSourceFile("crt.c"));
	AssertFalse(IsIndexedSourceFile("Start.asm"));
	AssertFalse(IsIndexedSourceFile("Shape.h"));
	AssertFalse(IsIndexedSourceFile("Notes.txt"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspaceIndex(void)
{
	BeginTests();

	TestWorkspaceIndexSourceFiles();
	TestWorkspaceIndexSample();
	TestWorkspaceIndexConfiguration();
	TestWorkspaceIndexTarget();

	TestStatistics();
}

