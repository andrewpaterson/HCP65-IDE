#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BuildLib/BuildWorkspace.h"
#include "IndexLib/WorkspaceIndex.h"
#include "TestLib/Assert.h"


#define TEST_WORKSPACE_INDEX_SAMPLE		"Input" _FS_ "Sample" _FS_ "Sample.sln"
#define TEST_WORKSPACE_INDEX_TARGET		"Input" _FS_ "Target" _FS_ "Target.sln"
#define TEST_WORKSPACE_INDEX_LIBC65		"Input" _FS_ "Target" _FS_ "Libc65"
#define TEST_WORKSPACE_INDEX_RUNTIME		"Input" _FS_ "Runtime" _FS_ "Runtime.sln"
#define TEST_WORKSPACE_INDEX_NO_RUNTIME	"Input" _FS_ "Runtime" _FS_ "NoRuntime.sln"


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
// that lists no toolchain's has for each of its own): the project's
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
// Libc65's headers for the system's.  Read with a GCC one it has GCC's
// defines for Linux and those of the project's GCC-x64 configuration
// (ConfigurationsPlan.txt C5).  Read with MSVC's it is as before.
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
	AssertNull(pcIndex->FindSymbol("OnLinux()"));
	AssertNull(pcIndex->FindSymbol("WithLinuxDefine()"));

	TestWorkspaceIndexTargetIndex(&cIndex, "Debug|GCC-x64");
	AssertNull(pcIndex->FindSymbol("OnHost()"));
	AssertNotNull(pcIndex->FindSymbol("OnLinux()"));
	AssertNotNull(pcIndex->FindSymbol("WithLinuxDefine()"));
	AssertNull(pcIndex->FindSymbol("OnW65C816()"));
	AssertNull(pcIndex->FindSymbol("SOnly65"));

	TestWorkspaceIndexTargetIndex(&cIndex, "Debug|HCP65C816");
	AssertNull(pcIndex->FindSymbol("OnHost()"));
	AssertNotNull(pcIndex->FindSymbol("OnW65C816()"));
	AssertNotNull(pcIndex->FindSymbol("WithAssembly()"));
	AssertNull(pcIndex->FindSymbol("On65C64()"));
	AssertNotNull(pcIndex->FindSymbol("SOnly65"));
	AssertNull(pcIndex->FindSymbol("OnLinux()"));
	AssertNull(pcIndex->FindSymbol("WithLinuxDefine()"));

	TestWorkspaceIndexTargetIndex(&cIndex, "Debug|HCP65C64");
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
// Whether the workspace's unit szFile is checked with the compiler's
// warning of a string given to a pointer: one of the options a project
// may have of its own.
//
//////////////////////////////////////////////////////////////////////////
static bool TestWorkspaceIndexWarnsStringToPointer(CWorkspaceIndex* pcIndex, const char* szFile)
{
	CParsedUnit	cUnit;
	CChars		szFileName;
	CChars		szErrors;
	CFileUtil	cFileUtil;
	size		uiProject;
	bool		bWarns;

	szFileName.Init("Input" _FS_ "Runtime" _FS_);
	szFileName.Append(szFile);
	cFileUtil.FullPath(&szFileName);
	szErrors.Init();
	cUnit.Init();
	uiProject = pcIndex->FindItemProject(szFileName.Text());
	bWarns = (uiProject != MAX_SIZE) && pcIndex->ParseUnit(uiProject, szFileName.Text(), NULL, &cUnit, &szErrors) && cUnit.mcOptions.mbWarnStringToPointer;
	cUnit.Kill();
	szErrors.Kill();
	szFileName.Kill();
	return bWarns;
}


//////////////////////////////////////////////////////////////////////////
//
// A solution read with an HCP65 configuration has the system headers its
// build has (ConfigurationsPlan.txt C4-7, C5): those of the project
// called Libc65 that the configuration's rows build, and that project's
// include directories, with no directory handed to the index.  One whose
// rows build no Libc65 has the directory handed, or none.  And a unit is
// checked with its project's own compile options where it has any, not
// only the program's.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspaceIndexRuntime(void)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CArrayChars			aszProblems;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_INDEX_RUNTIME));
	cIndex.Init(&cWorkspace);
	cIndex.SetConfiguration("Release|HCP65C816");
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	aszProblems.Init();
	cIndex.TakeIndexingProblems(&aszProblems);
	AssertSize(0, aszProblems.NumElements());
	aszProblems.Kill();
	AssertNotNull(cIndex.GetIndex()->FindSymbol("SFromRows"));
	AssertNotNull(cIndex.GetIndex()->FindSymbol("SSharedByRuntime"));
	AssertNotNull(cIndex.GetIndex()->FindSymbol("UsesRows(SFromRows*, SSharedByRuntime*)"));

	AssertFalse(TestWorkspaceIndexWarnsStringToPointer(&cIndex, "Program.cpp"));
	AssertTrue(TestWorkspaceIndexWarnsStringToPointer(&cIndex, "Own.cpp"));
	cIndex.Kill();
	cWorkspace.Kill();

	//Its rows build no Libc65: nothing says where the system headers are.
	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_INDEX_NO_RUNTIME));
	cIndex.Init(&cWorkspace);
	cIndex.SetConfiguration("Release|HCP65C816");
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	aszProblems.Init();
	cIndex.TakeIndexingProblems(&aszProblems);
	AssertTrue(aszProblems.NumElements() > 0);
	aszProblems.Kill();
	AssertNull(cIndex.GetIndex()->FindSymbol("SFromRows"));
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
	TestWorkspaceIndexRuntime();

	TestStatistics();
}

