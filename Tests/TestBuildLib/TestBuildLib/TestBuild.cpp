#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/NativeThread.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "BuildLib/SolutionBuild.h"
#include "TestLib/Assert.h"


#define TEST_BUILD_LIBC65	".." _FS_ ".." _FS_ ".." _FS_ ".." _FS_ "HCP65" _FS_ "Libraries" _FS_ "Libc65"


//////////////////////////////////////////////////////////////////////////
//
// Builds a project (a solution of its own) in a configuration, its output
// in Output\<szRoot>, polling until it has finished.  The lines it wrote
// are added to paszLines.
//
//////////////////////////////////////////////////////////////////////////
EBuildState TestBuildProject(CSolutionBuild* pcBuild, CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot, CArrayChars* paszLines)
{
	CChars	szDirectory;
	int		i;

	szDirectory.Init("Output" _FS_);
	szDirectory.Append(szRoot);
	pcBuild->Start(pcSolution, pcConfiguration, szDirectory.Text(), TEST_BUILD_LIBC65, paszLines);
	szDirectory.Kill();
	for (i = 0; (i < 60000) && pcBuild->Poll(paszLines); i++)
	{
		ThreadSleep(10);
	}
	return pcBuild->GetState();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a line of paszLines contains szText.
//
//////////////////////////////////////////////////////////////////////////
bool TestBuildHasLine(CArrayChars* paszLines, const char* szText)
{
	size	i;

	for (i = 0; i < paszLines->NumElements(); i++)
	{
		if (strstr(paszLines->GetText(i), szText))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool TestBuildEndsWith(const char* szText, const char* szEnd)
{
	size	uiLength;
	size	uiEnd;

	uiLength = strlen(szText);
	uiEnd = strlen(szEnd);
	return (uiLength >= uiEnd) && (strcmp(szText + uiLength - uiEnd, szEnd) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Tiny compiled for the W65C816 with its include directory and define:
// its listing is in Listing\W65C816\Debug, indexed by Maths.cpp's lines.
// Broken doesn't compile.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildHCP65(void)
{
	CVcxSolution				cSolution;
	CBuildConfiguration			cConfiguration;
	CSolutionBuild				cBuild;
	CArrayChars					aszLines;
	CFileUtil					cFileUtil;
	CChars						szListing;
	CChars						szSource;
	CCPP65ListingIndex			cIndex;
	CArrayCPP65ListingPlaces	asPlaces;

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Tiny" _FS_ "Tiny.vcxproj"));
	cConfiguration.Init("Debug|W65C816", BT_W65C816, "Debug|Win32");
	cBuild.Init();
	aszLines.Init();
	AssertInt(BS_NotStarted, cBuild.GetState());
	AssertInt(BS_Succeeded, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "HCP65", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "Tiny: Debug|Win32"));
	AssertString("Debug|W65C816 build succeeded.", aszLines.GetText(aszLines.NumElements() - 1));
	AssertString("Debug|W65C816 build succeeded.", cBuild.GetProgress());
	AssertFalse(cBuild.IsRunning());

	szListing.Init();
	cBuild.GetBuild()->GetListingDirectory(&szListing);
	AssertString("Output" _FS_ "HCP65" _FS_ "Listing" _FS_ "W65C816" _FS_ "Debug", szListing.Text());
	cFileUtil.AppendToPath(&szListing, "Maths.lst");
	cIndex.Init();
	AssertTrue(cIndex.ReadListing(szListing.Text()));
	szSource.Init("Input" _FS_ "Tiny" _FS_ "Maths.cpp");
	cFileUtil.FullPath(&szSource);
	asPlaces.Init();
	cIndex.GetPlaces(szSource.Text(), 5, &asPlaces);
	AssertSize(1, asPlaces.NumElements());
	asPlaces.Kill();
	szSource.Kill();
	cIndex.Kill();
	szListing.Kill();
	cSolution.Kill();

	aszLines.ReInit();
	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Broken" _FS_ "Broken.vcxproj"));
	AssertInt(BS_Failed, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "HCP65Broken", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "Broken.cpp"));
	AssertString("Debug|W65C816 build failed.", aszLines.GetText(aszLines.NumElements() - 1));
	cSolution.Kill();

	aszLines.Kill();
	cBuild.Kill();
	cConfiguration.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Tiny built by MSBuild, as Visual Studio does; Broken fails.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildMSVC(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CChars					szMSBuild;

	szMSBuild.Init();
	AssertTrue(CMSVCBuild::FindMSBuild(&szMSBuild));
	AssertTrue(szMSBuild.EndsWithIgnoreCase("MSBuild.exe"));
	szMSBuild.Kill();

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Tiny" _FS_ "Tiny.vcxproj"));
	cConfiguration.Init("Debug|Win32", BT_MSVC, "Debug|Win32");
	cBuild.Init();
	aszLines.Init();
	AssertInt(BS_Succeeded, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "MSVC", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "MSBuild Debug|Win32"));
	AssertTrue(TestBuildHasLine(&aszLines, "Tiny.lib"));
	AssertString("Debug|Win32 build succeeded.", aszLines.GetText(aszLines.NumElements() - 1));
	cSolution.Kill();

	aszLines.ReInit();
	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Broken" _FS_ "Broken.vcxproj"));
	AssertInt(BS_Failed, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "MSVCBroken", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "Broken.cpp"));
	AssertTrue(TestBuildHasLine(&aszLines, "exited with"));
	cSolution.Kill();

	aszLines.Kill();
	cBuild.Kill();
	cConfiguration.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Tiny with GCC: g++ -c with its include directory and define, then ar.
// Without g++ on the path the build fails saying so.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildGCC(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CCommandBuild*			pcCommands;
	SBuildCommand*			psCompile;
	EBuildState				eState;

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Tiny" _FS_ "Tiny.vcxproj"));
	cConfiguration.Init("Debug|GCC", BT_GCC, "Debug|Win32");
	cBuild.Init();
	aszLines.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "GCC", &aszLines);

	pcCommands = (CCommandBuild*)cBuild.GetBuild();
	AssertSize(2, pcCommands->NumCommands());
	psCompile = pcCommands->GetCommand(0);
	AssertString("g++", psCompile->szExecutable.Text());
	AssertString("Maths.cpp", psCompile->szDescription.Text());
	AssertString("-c", psCompile->aszArguments.GetText(0));
	AssertTrue(psCompile->aszArguments.Contains("-DTINY_DEBUG"));
	AssertTrue(TestBuildEndsWith(psCompile->aszArguments.GetText(psCompile->aszArguments.NumElements() - 1), "Maths.o"));
	AssertString("ar", pcCommands->GetCommand(1)->szExecutable.Text());
	AssertTrue(TestBuildEndsWith(pcCommands->GetCommand(1)->aszArguments.GetText(1), "libTiny.a"));

	if (eState == BS_Failed)
	{
		//No GCC here.
		AssertTrue(TestBuildHasLine(&aszLines, "Could not run g++."));
	}
	else
	{
		AssertInt(BS_Succeeded, eState);
	}

	aszLines.Kill();
	cBuild.Kill();
	cSolution.Kill();
	cConfiguration.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuild(void)
{
	BeginTests();

	TestBuildHCP65();
	TestBuildMSVC();
	TestBuildGCC();

	TestStatistics();
}

