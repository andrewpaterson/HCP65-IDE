#include <string.h>
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
	cConfigurations.Add("Debug|W65C816", BT_W65C816, "Debug|Win32")->SetEmulator("Emulators" _FS_ "HCP65.exe", "Emulators", "--fast --mute");
	cConfigurations.GetConfiguration(0)->SetBreakpointSpace("User");
	cConfigurations.GetConfiguration(0)->SetNoFloats(true);
	cConfigurations.GetConfiguration(0)->SetMathTables(false);
	cConfigurations.Add("Fast GCC", BT_GCC, "Release|x64");
	AssertTrue(cConfigurations.Write("Output" _FS_ "Test.hcp65build"));

	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Test.hcp65build"));
	AssertSize(2, cRead.NumConfigurations());
	AssertBuildConfiguration("Debug|W65C816", BT_W65C816, "Debug|Win32", cRead.GetConfiguration(0));
	AssertBuildConfiguration("Fast GCC", BT_GCC, "Release|x64", cRead.GetConfiguration(1));

	//The emulator is kept, and only where it was set.
	AssertTrue(cRead.GetConfiguration(0)->HasEmulator());
	AssertString("Emulators" _FS_ "HCP65.exe", cRead.GetConfiguration(0)->GetEmulator());
	AssertString("Emulators", cRead.GetConfiguration(0)->GetEmulatorDirectory());
	AssertString("--fast --mute", cRead.GetConfiguration(0)->GetEmulatorArguments());
	AssertFalse(cRead.GetConfiguration(1)->HasEmulator());
	AssertString("User", cRead.GetConfiguration(0)->GetBreakpointSpace());
	AssertString("", cRead.GetConfiguration(1)->GetBreakpointSpace());

	//NoFloats and MathTables (written only when not the default) are kept.
	AssertTrue(cRead.GetConfiguration(0)->IsNoFloats());
	AssertFalse(cRead.GetConfiguration(0)->IsMathTables());
	AssertFalse(cRead.GetConfiguration(1)->IsNoFloats());
	AssertTrue(cRead.GetConfiguration(1)->IsMathTables());
	AssertString("W65C816", cRead.GetConfiguration(0)->GetProcessorName());
	AssertTrue(cRead.GetConfiguration(1)->GetProcessorName() == NULL);

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
// An HCP65 configuration's image is written inside it and read back the
// same, numbers in hexadecimal or decimal; one that isn't right, or on a
// toolchain that isn't HCP65's, isn't read.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsImage(void)
{
	CBuildConfigurations	cConfigurations;
	CBuildConfigurations	cRead;
	CBuildConfiguration*	pcConfiguration;
	CAsm65Image*			pcImage;
	CTextFile				cFile;
	CFileUtil				cFileUtil;

	cFileUtil.MakeDir("Output");
	cConfigurations.Init();
	cConfigurations.Add("Debug|W65C816", BT_W65C816, "Debug|Win32");
	AssertFalse(cConfigurations.GetConfiguration(0)->HasImage());
	AssertTrue(cConfigurations.Write("Output" _FS_ "NoImage.hcp65build"));
	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "NoImage.hcp65build"));
	AssertFalse(cRead.GetConfiguration(0)->HasImage());

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Release|W65C816\" Toolchain=\"W65C816\" Solution=\"Release|Win32\">\n\t\t<Image File=\"Kernal.rom\" Size=\"0x80000\" Limit=\"523264\">\n\t\t\t<Region Start=\"0x0\" End=\"0x40000\" Offset=\"0x0\"/>\n\t\t\t<Region Start=\"0x400000\" End=\"0x440000\" Offset=\"0x40000\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Image.hcp65build"));
	cFile.Kill();
	AssertTrue(cRead.Read("Output" _FS_ "Image.hcp65build"));
	pcConfiguration = cRead.GetConfiguration(0);
	AssertTrue(pcConfiguration->HasImage());
	pcImage = pcConfiguration->GetImage();
	AssertString("Kernal.rom", pcImage->GetFile());
	AssertIntHex(0x80000, pcImage->GetSize());
	AssertIntHex(0xFF, pcImage->GetFill());
	AssertIntHex(0x7FC00, pcImage->GetLimit());
	AssertIntHex(0, pcImage->GetLoad());
	AssertSize(2, pcImage->NumRegions());
	AssertIntHex(0x400000, pcImage->GetRegion(1)->uiStart);
	AssertIntHex(0x440000, pcImage->GetRegion(1)->uiEnd);
	AssertIntHex(0x40000, pcImage->GetRegion(1)->uiOffset);

	//Written back, in hexadecimal, and read the same.
	AssertTrue(cRead.Write("Output" _FS_ "ImageAgain.hcp65build"));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "ImageAgain.hcp65build"));
	AssertTrue(strstr(cFile.Text(), "<Image File=\"Kernal.rom\" Size=\"0x80000\" Fill=\"0xFF\" Limit=\"0x7FC00\" Load=\"0x0\">") != NULL);
	AssertTrue(strstr(cFile.Text(), "<Region Start=\"0x400000\" End=\"0x440000\" Offset=\"0x40000\"/>") != NULL);
	cFile.Kill();
	cConfigurations.Kill();
	cConfigurations.Init();
	AssertTrue(cConfigurations.Read("Output" _FS_ "ImageAgain.hcp65build"));
	pcImage = cConfigurations.GetConfiguration(0)->GetImage();
	AssertTrue(cConfigurations.GetConfiguration(0)->HasImage());
	AssertIntHex(0x7FC00, pcImage->GetLimit());
	AssertSize(2, pcImage->NumRegions());

	//No regions; a region past the image's end; not a number; MSVC.
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadImage.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadImage.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x10\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadImage.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadImage.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"big\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadImage.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadImage.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|Win32\" Toolchain=\"MSVC\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadImage.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadImage.hcp65build"));

	cRead.Kill();
	cConfigurations.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An HCP65 program's configuration: its executable (with the symbols it
// is given), the files debugging loads first and the card the program
// goes on, written and read back the same.  An image and an executable
// together, a card without its path, or a load for MSVC aren't read.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsExecutable(void)
{
	CBuildConfigurations	cRead;
	CBuildConfigurations	cAgain;
	CBuildConfiguration*	pcConfiguration;
	CAsm65Executable*		pcExecutable;
	CTextFile				cFile;
	CFileUtil				cFileUtil;

	cFileUtil.MakeDir("Output");
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"User\" Card=\"Card.img\" CardPath=\"\\boot.exe\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\" Limit=\"0x200000\">\n\t\t\t<Symbol Name=\"UserMemorySize\" Value=\"0x220000\"/>\n\t\t</Executable>\n\t\t<Load File=\"Kernal.rom\" Address=\"0x0\"/>\n\t\t<Load File=\"Tables.bin\" Address=\"4194304\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Program.hcp65build"));
	cFile.Kill();

	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Program.hcp65build"));
	pcConfiguration = cRead.GetConfiguration(0);
	AssertFalse(pcConfiguration->HasImage());
	AssertTrue(pcConfiguration->HasExecutable());
	AssertTrue(pcConfiguration->IsLinked());
	pcExecutable = pcConfiguration->GetExecutable();
	AssertString("boot.exe", pcExecutable->GetFile());
	AssertString("UserStart", pcExecutable->GetEntry());
	AssertIntHex(0x200000, pcExecutable->GetLimit());
	AssertSize(1, pcExecutable->NumSymbols());
	AssertString("UserMemorySize", pcExecutable->GetSymbolName(0));
	AssertLongLongIntHex(0x220000, pcExecutable->GetSymbolValue(0));
	AssertSize(2, pcConfiguration->NumLoads());
	AssertString("Kernal.rom", pcConfiguration->GetLoadFile(0));
	AssertIntHex(0, pcConfiguration->GetLoadAddress(0));
	AssertIntHex(0x400000, pcConfiguration->GetLoadAddress(1));
	AssertTrue(pcConfiguration->HasCard());
	AssertString("Card.img", pcConfiguration->GetCard());
	AssertString("\\boot.exe", pcConfiguration->GetCardPath());
	AssertString("User", pcConfiguration->GetBreakpointSpace());

	//Written back and read the same.
	AssertTrue(cRead.Write("Output" _FS_ "ProgramAgain.hcp65build"));
	cAgain.Init();
	AssertTrue(cAgain.Read("Output" _FS_ "ProgramAgain.hcp65build"));
	pcConfiguration = cAgain.GetConfiguration(0);
	AssertTrue(pcConfiguration->HasExecutable());
	AssertLongLongIntHex(0x220000, pcConfiguration->GetExecutable()->GetSymbolValue(0));
	AssertSize(2, pcConfiguration->NumLoads());
	AssertIntHex(0x400000, pcConfiguration->GetLoadAddress(1));
	AssertString("\\boot.exe", pcConfiguration->GetCardPath());
	cAgain.Kill();

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x0\"/>\n\t\t</Image>\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" Card=\"Card.img\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|Win32\" Toolchain=\"MSVC\" Solution=\"Debug|Win32\">\n\t\t<Load File=\"Kernal.rom\" Address=\"0x0\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadProgram.hcp65build"));

	cRead.Kill();
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
	cConfigurations.Add("Debug|W65C816", BT_W65C816, "Debug|Win32");
	cConfigurations.Add("Release|W65C816", BT_W65C816, "Release|x64");
	cConfigurations.Add("Fast GCC", BT_GCC, "Release|x64");

	szName.Init();
	cConfigurations.GetConfiguration(2)->GetConfigurationName(&szName);
	AssertString("Debug", szName.Text());
	szName.Clear();
	cConfigurations.GetConfiguration(2)->GetPlatformName(&szName);
	AssertString("W65C816", szName.Text());
	szName.Clear();
	cConfigurations.GetConfiguration(4)->GetConfigurationName(&szName);
	AssertString("Fast GCC", szName.Text());
	szName.Clear();
	cConfigurations.GetConfiguration(4)->GetPlatformName(&szName);
	AssertString("GCC", szName.Text());
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
	AssertString("W65C816", aszNames.GetText(2));
	AssertString("GCC", aszNames.GetText(3));
	aszNames.Kill();

	//NULL is any.
	AssertTrue(cConfigurations.FindConfiguration("Release", "W65C816") == cConfigurations.GetConfiguration(3));
	AssertTrue(cConfigurations.FindConfiguration("Release", "Win32") == NULL);
	AssertTrue(cConfigurations.FindConfiguration("Release", NULL) == cConfigurations.GetConfiguration(1));
	AssertTrue(cConfigurations.FindConfiguration(NULL, "W65C816") == cConfigurations.GetConfiguration(2));
	AssertTrue(cConfigurations.FindConfiguration("Fast GCC", "GCC") == cConfigurations.GetConfiguration(4));

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
	TestBuildConfigurationsImage();
	TestBuildConfigurationsExecutable();
	TestBuildConfigurationsPlatforms();

	TestStatistics();
}

