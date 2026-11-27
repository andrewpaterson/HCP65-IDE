#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/NativeThread.h"
#include "CppCompilerLib/CPP65ListingIndex.h"
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
// its listing is in Listing\HCP65C816\Debug, indexed by Maths.cpp's lines.
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
	cConfiguration.Init("Debug|HCP65C816", BT_W65C816, "Debug|Win32");
	cBuild.Init();
	aszLines.Init();
	AssertInt(BS_NotStarted, cBuild.GetState());
	AssertInt(BS_Succeeded, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "HCP65", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "Tiny: Debug|Win32"));
	AssertString("Debug|HCP65C816 build succeeded.", aszLines.GetText(aszLines.NumElements() - 1));
	AssertString("Debug|HCP65C816 build succeeded.", cBuild.GetProgress());
	AssertFalse(cBuild.IsRunning());

	szListing.Init();
	cBuild.GetBuild()->GetListingDirectory(&szListing);
	AssertString("Output" _FS_ "HCP65" _FS_ "Listing" _FS_ "HCP65C816" _FS_ "Debug", szListing.Text());
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
	AssertString("Debug|HCP65C816 build failed.", aszLines.GetText(aszLines.NumElements() - 1));
	cSolution.Kill();

	aszLines.Kill();
	cBuild.Kill();
	cConfiguration.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Tiny built by MSBuild, as Visual Studio does; Broken fails.  MSBuild is
// only on Windows.
//
//////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
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
#endif // _WIN32


//////////////////////////////////////////////////////////////////////////
//
// A GCC-x64 configuration is Linux's: anywhere else its build doesn't
// start, and says so.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildGCCNotLinux(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Tiny" _FS_ "Tiny.vcxproj"));
	cConfiguration.Init("Debug|GCC-x64", BT_GCC, "Debug|Win32");
	cBuild.Init();
	aszLines.Init();
	AssertInt(BS_Failed, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "GCC", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "A GCC-x64 configuration is built on Linux."));
	AssertSize(0, ((CCommandBuild*)cBuild.GetBuild())->NumCommands());
	aszLines.Kill();
	cBuild.Kill();
	cSolution.Kill();
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
	cConfiguration.Init("Debug|GCC-x64", BT_GCC, "Debug|Win32");
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
	//Tiny has no GCC-x64 configuration: it is built as its Win32 one, with that one's defines and no options (they
	//are a GCC-x64 configuration's: TestBuildGCCConfiguration).
	AssertTrue(TestBuildHasLine(&aszLines, "No GCC-x64 configuration, built as Win32: Tiny."));
	AssertFalse(psCompile->aszArguments.Contains("-DLINUX_GNU"));
	AssertFalse(psCompile->aszArguments.Contains("-m64"));
	AssertFalse(psCompile->aszArguments.Contains("-g"));
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
// Tools with GCC in Release: two applications linked, the library
// archived.  Its projects have no GCC-x64 configuration, so no compile or
// link has an option of the build's own (ConfigurationsPlan.txt C7-4).
//
//////////////////////////////////////////////////////////////////////////
void TestBuildGCCRelease(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CCommandBuild*			pcCommands;
	SBuildCommand*			psCommand;
	EBuildState				eState;
	size					i;
	int						iCompiles;
	int						iLinks;
	CFileUtil				cFileUtil;

	//An application is linked where an older build put its objects' folder.
	cFileUtil.RemoveDir("Output" _FS_ "GCCRelease");
	cSolution.Init();
	AssertTrue(cSolution.Read("Input" _FS_ "Workspace" _FS_ "Tools" _FS_ "Tools.sln"));
	cConfiguration.Init("Release|GCC-x64", BT_GCC, "Release|Win32");
	cBuild.Init();
	aszLines.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "GCCRelease", &aszLines);

	pcCommands = (CCommandBuild*)cBuild.GetBuild();
	iCompiles = 0;
	iLinks = 0;
	for (i = 0; i < pcCommands->NumCommands(); i++)
	{
		psCommand = pcCommands->GetCommand(i);
		if (psCommand->aszArguments.Contains("-c"))
		{
			iCompiles++;
			AssertFalse(psCommand->aszArguments.Contains("-DLINUX_GNU"));
			AssertFalse(psCommand->aszArguments.Contains("-O3"));
		}
		else if (psCommand->szDescription.StartsWith("Linking "))
		{
			iLinks++;
			AssertFalse(psCommand->aszArguments.Contains("-m64"));
			AssertFalse(psCommand->aszArguments.Contains("-Wl,--gc-sections"));
		}
	}
	AssertInt(3, iCompiles);
	AssertInt(2, iLinks);

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
// A project with a GCC-x64 configuration of its own is built with it,
// whichever solution configuration the build names (Debug|x64 here): its
// defines and its ClCompile AdditionalOptions, and none of the options a
// project without one is given; its type and include directory are its
// x64 configuration's.  Its link has its Link AdditionalOptions and its
// AdditionalDependencies, a bare name as -l<name>.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildGCCConfiguration(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CCommandBuild*			pcCommands;
	SBuildCommand*			psCommand;
	EBuildState				eState;
	CFileUtil				cFileUtil;
	CTextFile				cFile;
	size					uiDefine;

	cFileUtil.RemoveDir("Output" _FS_ "Grouped");
	cFileUtil.TouchDir("Output" _FS_ "Grouped" _FS_ "Include");
	cFile.Init("<Project>\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\n"
		"    <ProjectConfiguration Include=\"Debug|x64\"/>\n"
		"  </ItemGroup>\n"
		"  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
		"    <ConfigurationType>Application</ConfigurationType>\n"
		"    <IncludePath>$(ProjectDir)Include;$(IncludePath)</IncludePath>\n"
		"  </PropertyGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
		"    <ClCompile><PreprocessorDefinitions>GROUPED_MSVC</PreprocessorDefinitions></ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|GCC-x64'\">\n"
		"    <ClCompile>\n"
		"      <PreprocessorDefinitions>GROUPED_GCC=7;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
		"      <AdditionalOptions>-O1 -fno-strict-aliasing %(AdditionalOptions)</AdditionalOptions>\n"
		"    </ClCompile>\n"
		"    <Link>\n"
		"      <AdditionalOptions>-s</AdditionalOptions>\n"
		"      <AdditionalDependencies>m;%(AdditionalDependencies)</AdditionalDependencies>\n"
		"    </Link>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemGroup>\n"
		"    <ClCompile Include=\"Main.cpp\"/>\n"
		"  </ItemGroup>\n"
		"</Project>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Grouped" _FS_ "Grouped.vcxproj"));
	cFile.Kill();
	cFile.Init("#include \"Grouped.h\"\n#include <math.h>\n\nint main(int argc, char* argv[])\n{\n\treturn (int)floor((double)argc) + GROUPED_GCC - GROUPED_HEADER - 1;\n}\n");
	AssertTrue(cFile.Write("Output" _FS_ "Grouped" _FS_ "Main.cpp"));
	cFile.Kill();
	cFile.Init("#define GROUPED_HEADER 7\n");
	AssertTrue(cFile.Write("Output" _FS_ "Grouped" _FS_ "Include" _FS_ "Grouped.h"));
	cFile.Kill();

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Output" _FS_ "Grouped" _FS_ "Grouped.vcxproj"));
	cConfiguration.Init("Debug|GCC-x64", BT_GCC, "Debug|x64");
	cBuild.Init();
	aszLines.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Grouped" _FS_ "Build", &aszLines);
	AssertFalse(TestBuildHasLine(&aszLines, "No GCC-x64 configuration, built as x64: Grouped."));

	pcCommands = (CCommandBuild*)cBuild.GetBuild();
	AssertSize(2, pcCommands->NumCommands());
	psCommand = pcCommands->GetCommand(0);
	AssertString("Main.cpp", psCommand->szDescription.Text());
	uiDefine = psCommand->aszArguments.GetIndex("-DGROUPED_GCC=7");
	AssertTrue(uiDefine != ARRAY_ELEMENT_NOT_FOUND);
	AssertString("-O1", psCommand->aszArguments.GetText(uiDefine + 1));
	AssertString("-fno-strict-aliasing", psCommand->aszArguments.GetText(uiDefine + 2));
	AssertFalse(psCommand->aszArguments.Contains("-DGROUPED_MSVC"));
	AssertFalse(psCommand->aszArguments.Contains("-DLINUX_GNU"));
	AssertFalse(psCommand->aszArguments.Contains("-g"));
	AssertFalse(psCommand->aszArguments.Contains("-march=haswell"));
	AssertTrue(TestBuildEndsWith(psCommand->aszArguments.GetText(2), "Include"));

	psCommand = pcCommands->GetCommand(1);
	AssertString("Linking Grouped", psCommand->szDescription.Text());
	AssertString("-s", psCommand->aszArguments.GetText(0));
	AssertString("-o", psCommand->aszArguments.GetText(1));
	AssertString("-lm", psCommand->aszArguments.GetText(psCommand->aszArguments.NumElements() - 1));
	AssertFalse(psCommand->aszArguments.Contains("-m64"));
	AssertTrue((eState == BS_Succeeded) || TestBuildHasLine(&aszLines, "Could not run g++."));

	aszLines.Kill();
	cBuild.Kill();
	cConfiguration.Kill();
	cSolution.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "Grouped");
}


//////////////////////////////////////////////////////////////////////////
//
// Excluded has three sources: Kept.cpp, NotInDebug.cpp (ExcludedFromBuild
// for Debug|Win32) and Never.cpp (for every configuration); the two stop a
// build that compiles them where they are excluded.  GCC compiles one in
// Debug and two in Release, and the HCP65 build of Debug succeeds.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildExcludedFromBuild(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CCommandBuild*			pcCommands;
	EBuildState				eState;
	CFileUtil				cFileUtil;

	cFileUtil.RemoveDir("Output" _FS_ "Excluded");
	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Excluded" _FS_ "Excluded.vcxproj"));
	aszLines.Init();

	cConfiguration.Init("Debug|GCC-x64", BT_GCC, "Debug|Win32");
	cBuild.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Excluded", &aszLines);
	pcCommands = (CCommandBuild*)cBuild.GetBuild();
	AssertSize(2, pcCommands->NumCommands());
	AssertString("Kept.cpp", pcCommands->GetCommand(0)->szDescription.Text());
	AssertString("ar", pcCommands->GetCommand(1)->szExecutable.Text());
	AssertSize(3, pcCommands->GetCommand(1)->aszArguments.NumElements());
	AssertTrue((eState == BS_Succeeded) || TestBuildHasLine(&aszLines, "Could not run g++."));
	cBuild.Kill();
	cConfiguration.Kill();

	aszLines.ReInit();
	cConfiguration.Init("Release|GCC-x64", BT_GCC, "Release|Win32");
	cBuild.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Excluded", &aszLines);
	pcCommands = (CCommandBuild*)cBuild.GetBuild();
	AssertSize(3, pcCommands->NumCommands());
	AssertString("Kept.cpp", pcCommands->GetCommand(0)->szDescription.Text());
	AssertString("NotInDebug.cpp", pcCommands->GetCommand(1)->szDescription.Text());
	AssertSize(4, pcCommands->GetCommand(2)->aszArguments.NumElements());
	AssertTrue((eState == BS_Succeeded) || TestBuildHasLine(&aszLines, "Could not run g++."));
	cBuild.Kill();
	cConfiguration.Kill();

	aszLines.ReInit();
	cConfiguration.Init("Debug|HCP65C816", BT_W65C816, "Debug|Win32");
	cBuild.Init();
	AssertInt(BS_Succeeded, TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Excluded", &aszLines));
	AssertTrue(TestBuildHasLine(&aszLines, "Kept.cpp (1/1)"));
	cBuild.Kill();
	cConfiguration.Kill();

	aszLines.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A Makefile project with GCC: its build command, run by the shell in its
// folder, and its output the build's.  One without a command is built as
// before (it has nothing to compile); one whose command fails fails the
// build.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildMakefile(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CCommandBuild*			pcCommands;
	SBuildCommand*			psCommand;
	EBuildState				eState;

	cConfiguration.Init("Debug|GCC-x64", BT_GCC, "Debug|Win32");

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Makefile" _FS_ "Made.vcxproj"));
	cBuild.Init();
	aszLines.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Makefile", &aszLines);
	pcCommands = (CCommandBuild*)cBuild.GetBuild();
	AssertSize(1, pcCommands->NumCommands());
	psCommand = pcCommands->GetCommand(0);
	AssertString("Building Made: echo Page.md:1:1: warning: made in the project folder", psCommand->szDescription.Text());
	AssertTrue(TestBuildEndsWith(psCommand->szDirectory.Text(), "Makefile"));
	AssertString("echo Page.md:1:1: warning: made in the project folder", psCommand->aszArguments.GetText(psCommand->aszArguments.NumElements() - 1));
	AssertInt(BS_Succeeded, eState);
	AssertTrue(TestBuildHasLine(&aszLines, "Page.md:1:1: warning: made in the project folder"));
	aszLines.Kill();
	cBuild.Kill();
	cSolution.Kill();

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Makefile" _FS_ "Unmade.vcxproj"));
	cBuild.Init();
	aszLines.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Makefile", &aszLines);
	AssertSize(0, ((CCommandBuild*)cBuild.GetBuild())->NumCommands());
	AssertInt(BS_Succeeded, eState);
	AssertTrue(TestBuildHasLine(&aszLines, "Nothing to build."));
	aszLines.Kill();
	cBuild.Kill();
	cSolution.Kill();

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Makefile" _FS_ "Failing.vcxproj"));
	cBuild.Init();
	aszLines.Init();
	eState = TestBuildProject(&cBuild, &cSolution, &cConfiguration, "Makefile", &aszLines);
	AssertSize(1, ((CCommandBuild*)cBuild.GetBuild())->NumCommands());
	AssertInt(BS_Failed, eState);
	aszLines.Kill();
	cBuild.Kill();
	cSolution.Kill();

	cConfiguration.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Tools has two applications and a library.  An HCP65 build, which links
// the solution into one program, builds the startup project and the
// library and leaves the other application out; told of none it builds
// them all.  GCC links each application: the startup project's is what a
// run configuration without a tool runs.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildStartupProject(void)
{
	CVcxSolution			cSolution;
	CBuildConfiguration		cConfiguration;
	CBuildConfiguration		cGCC;
	CSolutionBuild			cBuild;
	CArrayChars				aszLines;
	CArrayBuildProjects		asProjects;
	CChars					szPath;
	CChars					szExpected;
	CFileUtil				cFileUtil;
	int						i;

	cSolution.Init();
	AssertTrue(cSolution.Read("Input" _FS_ "Workspace" _FS_ "Tools" _FS_ "Tools.sln"));
	cConfiguration.Init("Release|HCP65C816", BT_W65C816, "Release|Win32");
	cBuild.Init();
	aszLines.Init();

	cBuild.Start(&cSolution, &cConfiguration, "Output" _FS_ "StartupSecond", TEST_BUILD_LIBC65, &aszLines, "Second");
	for (i = 0; (i < 60000) && cBuild.Poll(&aszLines); i++)
	{
		ThreadSleep(10);
	}
	AssertInt(BS_Succeeded, cBuild.GetState());
	AssertString("Second", cBuild.GetBuild()->GetStartupProject());
	AssertTrue(TestBuildHasLine(&aszLines, "Second: Release|Win32"));
	AssertTrue(TestBuildHasLine(&aszLines, "Shared: Release|Win32"));
	AssertFalse(TestBuildHasLine(&aszLines, "First: Release|Win32"));
	AssertFalse(TestBuildHasLine(&aszLines, "First.cpp"));

	//Every project it builds, and those of them that go into one program.
	asProjects.Init();
	cBuild.GetBuild()->GetProjects(&asProjects);
	AssertSize(3, asProjects.NumElements());
	AssertFalse(asProjects.Get(0)->bLibrary);
	AssertTrue(asProjects.Get(2)->bLibrary);
	cBuild.GetBuild()->GetProjects(&asProjects, true);
	AssertSize(2, asProjects.NumElements());
	AssertString("Second", asProjects.Get(0)->pcProject->GetName());
	AssertString("Shared", asProjects.Get(1)->pcProject->GetName());
	asProjects.Kill();

	//Told of no startup project it builds both applications, as it always did.
	aszLines.ReInit();
	cBuild.Start(&cSolution, &cConfiguration, "Output" _FS_ "StartupNone", TEST_BUILD_LIBC65, &aszLines);
	for (i = 0; (i < 60000) && cBuild.Poll(&aszLines); i++)
	{
		ThreadSleep(10);
	}
	AssertString("", cBuild.GetBuild()->GetStartupProject());
	AssertTrue(TestBuildHasLine(&aszLines, "First: Release|Win32"));
	AssertTrue(TestBuildHasLine(&aszLines, "Second: Release|Win32"));

	//Where GCC links an application.
	cGCC.Init("Release|GCC-x64", BT_GCC, "Release|Win32");
	szExpected.Init();
	CBuild::GetDirectory("Output" _FS_ "Root", &cGCC, "Output", &szExpected);
	cFileUtil.FullPath(&szExpected);
	cFileUtil.AppendToPath(&szExpected, "Second");
	szPath.Init();
	CGCCBuild::GetExecutablePath("Output" _FS_ "Root", &cGCC, "Second", &szPath);
	AssertString(szExpected.Text(), szPath.Text());
	AssertTrue(strstr(szPath.Text(), "GCC") != NULL);
	szPath.Kill();
	szExpected.Kill();
	cGCC.Kill();

	aszLines.Kill();
	cBuild.Kill();
	cConfiguration.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuild(void)
{
	BeginTests();

	TestBuildHCP65();
	TestBuildStartupProject();
#ifdef _WIN32
	TestBuildMSVC();
#endif // _WIN32
#ifdef LINUX_GNU
	TestBuildGCC();
	TestBuildGCCRelease();
	TestBuildGCCConfiguration();
	TestBuildExcludedFromBuild();
	TestBuildMakefile();
#else
	TestBuildGCCNotLinux();
#endif // LINUX_GNU

	TestStatistics();
}

