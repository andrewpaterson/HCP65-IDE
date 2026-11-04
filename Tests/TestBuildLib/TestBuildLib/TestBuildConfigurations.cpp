#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildLib/BuildConfigurations.h"
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
	AssertBuildConfiguration("Debug|W65C816", BT_W65C816, "Debug|Win32", cConfigurations.GetConfiguration(2));
	AssertBuildConfiguration("Debug|65C64", BT_65C64, "Debug|Win32", cConfigurations.GetConfiguration(3));
	AssertBuildConfiguration("Debug|GCC", BT_GCC, "Debug|Win32", cConfigurations.GetConfiguration(4));
	AssertBuildConfiguration("Release|W65C816", BT_W65C816, "Release|x64", cConfigurations.GetConfiguration(5));
	AssertBuildConfiguration("Release|65C64", BT_65C64, "Release|x64", cConfigurations.GetConfiguration(6));
	AssertBuildConfiguration("Release|GCC", BT_GCC, "Release|x64", cConfigurations.GetConfiguration(7));

	AssertTrue(cConfigurations.FindConfiguration("Debug|65C64") == cConfigurations.GetConfiguration(3));
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
// Written and read back the same; a file that isn't there, or has a
// configuration that isn't right, gives none.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsFile(void)
{
	CBuildConfigurations	cConfigurations;
	CBuildConfigurations	cRead;
	CFileUtil				cFileUtil;
	CTextFile				cFile;
	CChars					szFileName;

	cFileUtil.MakeDir("Output");
	cConfigurations.Init();
	cConfigurations.Add("Debug|W65C816", BT_W65C816, "Debug|Win32");
	cConfigurations.Add("Fast GCC", BT_GCC, "Release|x64");
	AssertTrue(cConfigurations.Write("Output" _FS_ "Test.hcp65build"));

	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Test.hcp65build"));
	AssertSize(2, cRead.NumConfigurations());
	AssertBuildConfiguration("Debug|W65C816", BT_W65C816, "Debug|Win32", cRead.GetConfiguration(0));
	AssertBuildConfiguration("Fast GCC", BT_GCC, "Release|x64", cRead.GetConfiguration(1));

	AssertFalse(cRead.Read("Output" _FS_ "Missing.hcp65build"));
	AssertSize(0, cRead.NumConfigurations());

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|Z80\" Toolchain=\"Z80\" Solution=\"Debug|Win32\"/>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Bad.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "Bad.hcp65build"));
	AssertSize(0, cRead.NumConfigurations());

	//The toolchain's case doesn't matter.
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|Mine\" Toolchain=\"msvc\" Solution=\"Debug|Win32\"/>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Case.hcp65build"));
	cFile.Kill();
	AssertTrue(cRead.Read("Output" _FS_ "Case.hcp65build"));
	AssertBuildConfiguration("Debug|Mine", BT_MSVC, "Debug|Win32", cRead.GetConfiguration(0));

	szFileName.Init();
	CBuildConfigurations::GetFileName("C:" _FS_ "Code" _FS_ "Game.sln", &szFileName);
	AssertString("C:" _FS_ "Code" _FS_ "Game.hcp65build", szFileName.Text());
	CBuildConfigurations::GetFileName("Lib.vcxproj", &szFileName);
	AssertString("Lib.hcp65build", szFileName.Text());
	szFileName.Kill();

	cRead.Kill();
	cConfigurations.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurations(void)
{
	BeginTests();

	TestBuildConfigurationsDefaults();
	TestBuildConfigurationsFile();

	TestStatistics();
}

