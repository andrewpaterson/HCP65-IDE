#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildLib/BuildConfigurations.h"
#include "BuildLib/RunConfiguration.h"
#include "BuildLib/GCCBuild.h"
#include "TestLib/Assert.h"


#define TEST_MINI_SOLUTION	".." _FS_ ".." _FS_ "TestVisualStudioLib" _FS_ "TestVisualStudioLib" _FS_ "Input" _FS_ "MiniSolution" _FS_ "MiniSolution.sln"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void AssertBuildConfiguration(const char* szName, EBuildToolchain eToolchain, const char* szSolution, CBuildConfiguration* pcConfiguration)
{
	AssertString(szName, pcConfiguration->GetName());
	AssertInt(eToolchain, pcConfiguration->GetToolchain());
	AssertString(szSolution, pcConfiguration->GetSolutionConfiguration());
}


//////////////////////////////////////////////////////////////////////////
//
// MiniSolution has Debug|Win32 and Release|x64: each is built with MSVC,
// and Debug and Release with the W65C816, the 65C64 and GCC, from Win32
// and x64 where there are, otherwise the configuration's first.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsDefaults(void)
{
	CVcxSolution			cSolution;
	CBuildConfigurations	cConfigurations;

	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_MINI_SOLUTION, false));
	cConfigurations.Init();
	cConfigurations.AddDefaults(&cSolution);

	AssertSize(8, cConfigurations.NumConfigurations());
	AssertBuildConfiguration("Debug|Win32", BT_MSVC, "Debug|Win32", cConfigurations.GetConfiguration(0));
	AssertBuildConfiguration("Release|x64", BT_MSVC, "Release|x64", cConfigurations.GetConfiguration(1));
	AssertBuildConfiguration("Debug|HCP65C816", BT_W65C816, "Debug|Win32", cConfigurations.GetConfiguration(2));
	AssertBuildConfiguration("Debug|HCP65C64", BT_65C64, "Debug|Win32", cConfigurations.GetConfiguration(3));
	AssertBuildConfiguration("Debug|GCC-x64", BT_GCC, "Debug|Win32", cConfigurations.GetConfiguration(4));
	AssertBuildConfiguration("Release|HCP65C816", BT_W65C816, "Release|x64", cConfigurations.GetConfiguration(5));
	AssertBuildConfiguration("Release|HCP65C64", BT_65C64, "Release|x64", cConfigurations.GetConfiguration(6));
	AssertBuildConfiguration("Release|GCC-x64", BT_GCC, "Release|x64", cConfigurations.GetConfiguration(7));

	AssertTrue(cConfigurations.FindConfiguration("Debug|HCP65C64") == cConfigurations.GetConfiguration(3));
	AssertTrue(cConfigurations.FindConfiguration("Debug|Z80") == NULL);
	AssertTrue(cConfigurations.GetConfiguration(2)->IsHCP65());
	AssertTrue(cConfigurations.GetConfiguration(3)->IsHCP65());
	AssertFalse(cConfigurations.GetConfiguration(0)->IsHCP65());
	AssertFalse(cConfigurations.GetConfiguration(4)->IsHCP65());

	cConfigurations.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A solution whose platforms are names of its own, MSVC-x86 and MSVC-x64
// (ConfigurationsPlan.txt C11-2), its rows naming the projects' Win32 and
// x64: each of its configurations is MSVC's, and a toolchain's starts
// from the one that builds what it is read beside, Win32 for the HCP65's
// and x64 for GCC's, whatever order they are listed in.  A name written
// before the rename (Debug|Win32, in a workspace) finds the one it became;
// in a solution named the old way it finds itself, as before.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsOwnPlatforms(void)
{
	CVcxSolution			cSolution;
	CBuildConfigurations	cConfigurations;
	CFileUtil				cFileUtil;
	CTextFile				cFile;

	cFileUtil.TouchDir("Output" _FS_ "OwnPlatforms");
	cFile.Init("Microsoft Visual Studio Solution File, Format Version 12.00\n"
		"Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"Tiny\", \"..\\..\\Input\\Tiny\\Tiny.vcxproj\", \"{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}\"\n"
		"EndProject\n"
		"Global\n"
		"\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
		"\t\tDebug|MSVC-x64 = Debug|MSVC-x64\n"
		"\t\tDebug|MSVC-x86 = Debug|MSVC-x86\n"
		"\t\tRelease|MSVC-x64 = Release|MSVC-x64\n"
		"\tEndGlobalSection\n"
		"\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|MSVC-x64.ActiveCfg = Debug|x64\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|MSVC-x64.Build.0 = Debug|x64\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|MSVC-x86.ActiveCfg = Debug|Win32\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|MSVC-x86.Build.0 = Debug|Win32\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Release|MSVC-x64.ActiveCfg = Release|x64\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Release|MSVC-x64.Build.0 = Release|x64\n"
		"\tEndGlobalSection\n"
		"EndGlobal\n");
	AssertTrue(cFile.Write("Output" _FS_ "OwnPlatforms" _FS_ "OwnPlatforms.sln"));
	cFile.Kill();

	cSolution.Init();
	AssertTrue(cSolution.Read("Output" _FS_ "OwnPlatforms" _FS_ "OwnPlatforms.sln", false));
	cConfigurations.Init();
	cConfigurations.AddDefaults(&cSolution);
	AssertSize(9, cConfigurations.NumConfigurations());
	AssertBuildConfiguration("Debug|MSVC-x64", BT_MSVC, "Debug|MSVC-x64", cConfigurations.GetConfiguration(0));
	AssertBuildConfiguration("Debug|MSVC-x86", BT_MSVC, "Debug|MSVC-x86", cConfigurations.GetConfiguration(1));
	AssertBuildConfiguration("Release|MSVC-x64", BT_MSVC, "Release|MSVC-x64", cConfigurations.GetConfiguration(2));
	AssertBuildConfiguration("Debug|HCP65C816", BT_W65C816, "Debug|MSVC-x86", cConfigurations.GetConfiguration(3));
	AssertBuildConfiguration("Debug|HCP65C64", BT_65C64, "Debug|MSVC-x86", cConfigurations.GetConfiguration(4));
	AssertBuildConfiguration("Debug|GCC-x64", BT_GCC, "Debug|MSVC-x64", cConfigurations.GetConfiguration(5));
	AssertBuildConfiguration("Release|HCP65C816", BT_W65C816, "Release|MSVC-x64", cConfigurations.GetConfiguration(6));
	AssertBuildConfiguration("Release|GCC-x64", BT_GCC, "Release|MSVC-x64", cConfigurations.GetConfiguration(8));

	AssertTrue(cConfigurations.FindConfiguration("Debug|MSVC-x86") == cConfigurations.GetConfiguration(1));
	AssertTrue(cConfigurations.FindConfiguration("Debug|Win32") == cConfigurations.GetConfiguration(1));
	AssertTrue(cConfigurations.FindConfiguration("Debug|x64") == cConfigurations.GetConfiguration(0));
	AssertTrue(cConfigurations.FindConfiguration("Release|x64") == cConfigurations.GetConfiguration(2));
	AssertTrue(cConfigurations.FindConfiguration("Release|Win32") == NULL);
	cConfigurations.Kill();
	cSolution.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "OwnPlatforms");

	//Named the old way, a name finds itself.
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_MINI_SOLUTION, false));
	cConfigurations.Init();
	cConfigurations.AddDefaults(&cSolution);
	AssertTrue(cConfigurations.FindConfiguration("Debug|Win32") == cConfigurations.GetConfiguration(0));
	AssertTrue(cConfigurations.FindConfiguration("Release|x64") == cConfigurations.GetConfiguration(1));
	AssertTrue(cConfigurations.FindConfiguration("Debug|MSVC-x86") == NULL);
	cConfigurations.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A configuration is chosen by its name's configuration and platform,
// either side of its |; a name without one is all configuration, and its
// platform is its toolchain.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsPlatforms(void)
{
	CBuildConfigurations	cConfigurations;
	CArrayChars				aszNames;
	CChars					szName;

	cConfigurations.Init();
	cConfigurations.Add("Debug|Win32", BT_MSVC, "Debug|Win32");
	cConfigurations.Add("Release|x64", BT_MSVC, "Release|x64");
	cConfigurations.Add("Debug|HCP65C816", BT_W65C816, "Debug|Win32");
	cConfigurations.Add("Release|HCP65C816", BT_W65C816, "Release|x64");
	cConfigurations.Add("Fast GCC", BT_GCC, "Release|x64");

	szName.Init();
	cConfigurations.GetConfiguration(2)->GetConfigurationName(&szName);
	AssertString("Debug", szName.Text());
	szName.Clear();
	cConfigurations.GetConfiguration(2)->GetPlatformName(&szName);
	AssertString("HCP65C816", szName.Text());
	szName.Clear();
	cConfigurations.GetConfiguration(4)->GetConfigurationName(&szName);
	AssertString("Fast GCC", szName.Text());
	szName.Clear();
	cConfigurations.GetConfiguration(4)->GetPlatformName(&szName);
	AssertString("GCC-x64", szName.Text());
	szName.Kill();

	//Each once, in the order they first come.
	aszNames.Init();
	cConfigurations.GetConfigurationNames(&aszNames);
	AssertSize(3, aszNames.NumElements());
	AssertString("Debug", aszNames.GetText(0));
	AssertString("Release", aszNames.GetText(1));
	AssertString("Fast GCC", aszNames.GetText(2));
	aszNames.ReInit();
	cConfigurations.GetPlatformNames(&aszNames);
	AssertSize(4, aszNames.NumElements());
	AssertString("Win32", aszNames.GetText(0));
	AssertString("x64", aszNames.GetText(1));
	AssertString("HCP65C816", aszNames.GetText(2));
	AssertString("GCC-x64", aszNames.GetText(3));
	aszNames.Kill();

	//NULL is any.
	AssertTrue(cConfigurations.FindConfiguration("Release", "HCP65C816") == cConfigurations.GetConfiguration(3));
	AssertTrue(cConfigurations.FindConfiguration("Release", "Win32") == NULL);
	AssertTrue(cConfigurations.FindConfiguration("Release", NULL) == cConfigurations.GetConfiguration(1));
	AssertTrue(cConfigurations.FindConfiguration(NULL, "HCP65C816") == cConfigurations.GetConfiguration(2));
	AssertTrue(cConfigurations.FindConfiguration("Fast GCC", "GCC-x64") == cConfigurations.GetConfiguration(4));

	cConfigurations.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A solution that lists a toolchain's configuration (Debug|GCC-x64, its
// project's row mapped to Debug|Win32 and built; Release|HCP65C816, its row
// not built) has said what it builds: each of its configurations is its
// platform's toolchain's, MSVC's for the rest, and none is made up.  A GCC
// build of Debug|GCC-x64 builds the project its row builds.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsListed(void)
{
	CVcxSolution			cSolution;
	CBuildConfigurations	cConfigurations;
	CGCCBuild				cBuild;
	CArrayBuildProjects		asProjects;
	CFileUtil				cFileUtil;
	CTextFile				cFile;

	cFileUtil.TouchDir("Output" _FS_ "Listed");
	cFile.Init("Microsoft Visual Studio Solution File, Format Version 12.00\n"
		"Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"Tiny\", \"..\\..\\Input\\Tiny\\Tiny.vcxproj\", \"{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}\"\n"
		"EndProject\n"
		"Global\n"
		"\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
		"\t\tDebug|GCC-x64 = Debug|GCC-x64\n"
		"\t\tDebug|Win32 = Debug|Win32\n"
		"\t\tRelease|HCP65C816 = Release|HCP65C816\n"
		"\tEndGlobalSection\n"
		"\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|GCC-x64.ActiveCfg = Debug|Win32\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|GCC-x64.Build.0 = Debug|Win32\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|Win32.ActiveCfg = Debug|Win32\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Debug|Win32.Build.0 = Debug|Win32\n"
		"\t\t{1F3C5A7E-2B4D-4E6F-8A9C-0D1E2F3A4B5C}.Release|HCP65C816.ActiveCfg = Debug|Win32\n"
		"\tEndGlobalSection\n"
		"EndGlobal\n");
	AssertTrue(cFile.Write("Output" _FS_ "Listed" _FS_ "Listed.sln"));
	cFile.Kill();

	cSolution.Init();
	AssertTrue(cSolution.Read("Output" _FS_ "Listed" _FS_ "Listed.sln"));
	cConfigurations.Init();
	cConfigurations.AddDefaults(&cSolution);
	AssertSize(3, cConfigurations.NumConfigurations());
	AssertBuildConfiguration("Debug|GCC-x64", BT_GCC, "Debug|GCC-x64", cConfigurations.GetConfiguration(0));
	AssertBuildConfiguration("Debug|Win32", BT_MSVC, "Debug|Win32", cConfigurations.GetConfiguration(1));
	AssertBuildConfiguration("Release|HCP65C816", BT_W65C816, "Release|HCP65C816", cConfigurations.GetConfiguration(2));

	cBuild.Init(&cSolution, cConfigurations.GetConfiguration(0), "Output" _FS_ "Listed");
	asProjects.Init();
	cBuild.GetProjects(&asProjects);
	AssertSize(1, asProjects.NumElements());
	AssertString("Debug|Win32", asProjects.Get(0)->pcConfiguration->GetName());
	cBuild.Kill();
	cBuild.Init(&cSolution, cConfigurations.GetConfiguration(2), "Output" _FS_ "Listed");
	cBuild.GetProjects(&asProjects);
	AssertSize(0, asProjects.NumElements());
	cBuild.Kill();
	asProjects.Kill();

	cConfigurations.Kill();
	cSolution.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "Listed");
}


//////////////////////////////////////////////////////////////////////////
//
// A configuration named with a toolchain's old platform (Debug|W65C816,
// Debug|65C64, Release|GCC) is read as the name it has now, where it is
// made and where it is looked for; any other name is itself.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsOldNames(void)
{
	CBuildConfigurations	cConfigurations;
	CRunConfiguration		cRun;
	CChars					szName;

	szName.Init();
	CBuildConfiguration::CurrentName("Debug|W65C816", &szName);
	AssertString("Debug|HCP65C816", szName.Text());
	szName.Clear();
	CBuildConfiguration::CurrentName("Debug|65c64", &szName);
	AssertString("Debug|HCP65C64", szName.Text());
	szName.Clear();
	CBuildConfiguration::CurrentName("Release|GCC", &szName);
	AssertString("Release|GCC-x64", szName.Text());
	szName.Clear();
	CBuildConfiguration::CurrentName("Release|GCC-x64", &szName);
	AssertString("Release|GCC-x64", szName.Text());
	szName.Clear();
	CBuildConfiguration::CurrentName("Release|x64", &szName);
	AssertString("Release|x64", szName.Text());
	szName.Clear();
	CBuildConfiguration::CurrentName("Fast GCC", &szName);
	AssertString("Fast GCC", szName.Text());
	szName.Clear();
	CBuildConfiguration::CurrentName(NULL, &szName);
	AssertString("", szName.Text());
	szName.Kill();

	cConfigurations.Init();
	cConfigurations.Add("Release|W65C816", BT_W65C816, "Release|Win32");
	AssertString("Release|HCP65C816", cConfigurations.GetConfiguration(0)->GetName());
	AssertTrue(cConfigurations.FindConfiguration("Release|W65C816") == cConfigurations.GetConfiguration(0));
	AssertTrue(cConfigurations.FindConfiguration("Release|HCP65C816") == cConfigurations.GetConfiguration(0));
	cConfigurations.Kill();

	//A run configuration is named after its build configuration, so its name moves with it.
	cRun.Init("Debug|W65C816", "Debug|W65C816");
	AssertString("Debug|HCP65C816", cRun.GetName());
	AssertString("Debug|HCP65C816", cRun.GetConfiguration());
	cRun.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurations(void)
{
	BeginTests();

	TestBuildConfigurationsDefaults();
	TestBuildConfigurationsOwnPlatforms();
	TestBuildConfigurationsOldNames();
	TestBuildConfigurationsListed();
	TestBuildConfigurationsPlatforms();

	TestStatistics();
}

