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
	cConfigurations.Add("Debug|W65C816", BT_W65C816, "Debug|Win32");
	cConfigurations.GetConfiguration(0)->SetBreakpointSpace("User");
	cConfigurations.GetConfiguration(0)->SetNoFloats(true);
	cConfigurations.GetConfiguration(0)->SetMathTables(false);
	cConfigurations.GetConfiguration(0)->SetPreferSize(true);
	cConfigurations.Add("Fast GCC", BT_GCC, "Release|x64");
	AssertTrue(cConfigurations.Write("Output" _FS_ "Test.hcp65build"));

	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Test.hcp65build"));
	AssertSize(2, cRead.NumConfigurations());
	AssertBuildConfiguration("Debug|W65C816", BT_W65C816, "Debug|Win32", cRead.GetConfiguration(0));
	AssertBuildConfiguration("Fast GCC", BT_GCC, "Release|x64", cRead.GetConfiguration(1));

	AssertString("User", cRead.GetConfiguration(0)->GetBreakpointSpace());
	AssertString("", cRead.GetConfiguration(1)->GetBreakpointSpace());

	//NoFloats, MathTables and PreferSize (written only when not the default) are kept.
	AssertTrue(cRead.GetConfiguration(0)->IsNoFloats());
	AssertFalse(cRead.GetConfiguration(0)->IsMathTables());
	AssertTrue(cRead.GetConfiguration(0)->IsPreferSize());
	AssertFalse(cRead.GetConfiguration(1)->IsNoFloats());
	AssertTrue(cRead.GetConfiguration(1)->IsMathTables());
	AssertFalse(cRead.GetConfiguration(1)->IsPreferSize());
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
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"User\" Card=\"Card.img\" CardPath=\"\\boot.exe\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\" Limit=\"0x200000\">\n\t\t\t<Symbol Name=\"UserHeapSize\" Value=\"0x20000\"/>\n\t\t</Executable>\n\t\t<Load File=\"Kernal.rom\" Address=\"0x0\"/>\n\t\t<Load File=\"Tables.bin\" Address=\"4194304\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
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
	AssertString("UserHeapSize", pcExecutable->GetSymbolName(0));
	AssertLongLongIntHex(0x20000, pcExecutable->GetSymbolValue(0));
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
	AssertLongLongIntHex(0x20000, pcConfiguration->GetExecutable()->GetSymbolValue(0));
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
// A driver's configuration: its module, linked into a file the kernal
// loads from the card, written and read back the same and put on its
// card.  A module beside an executable isn't read.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsModule(void)
{
	CBuildConfigurations	cRead;
	CBuildConfigurations	cAgain;
	CBuildConfiguration*	pcConfiguration;
	CTextFile				cFile;
	CFileUtil				cFileUtil;

	cFileUtil.MakeDir("Output");
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"Kernel\" NoFloats=\"true\" MathTables=\"false\" Card=\"Card.img\" CardPath=\"\\OS\\Drivers\\UART.DRV\">\n\t\t<Module File=\"UART.DRV\" Entry=\"_KernalCreateDriver\" Limit=\"0x20000\"/>\n\t\t<Load File=\"Kernal.rom\" Address=\"0x0\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Driver.hcp65build"));
	cFile.Kill();

	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Driver.hcp65build"));
	pcConfiguration = cRead.GetConfiguration(0);
	AssertTrue(pcConfiguration->HasModule());
	AssertFalse(pcConfiguration->HasExecutable());
	AssertFalse(pcConfiguration->HasImage());
	AssertTrue(pcConfiguration->IsLinked());
	AssertTrue(pcConfiguration->IsOnCard());
	AssertString("UART.DRV", pcConfiguration->GetModule()->GetFile());
	AssertString("_KernalCreateDriver", pcConfiguration->GetModule()->GetEntry());
	AssertIntHex(0x20000, pcConfiguration->GetModule()->GetLimit());
	AssertString("UART.DRV", pcConfiguration->GetProgramFile());
	AssertIntHex(0x20000, pcConfiguration->GetProgramLimit());
	AssertString("\\OS\\Drivers\\UART.DRV", pcConfiguration->GetCardPath());

	//Written back and read the same.
	AssertTrue(cRead.Write("Output" _FS_ "DriverAgain.hcp65build"));
	cAgain.Init();
	AssertTrue(cAgain.Read("Output" _FS_ "DriverAgain.hcp65build"));
	pcConfiguration = cAgain.GetConfiguration(0);
	AssertTrue(pcConfiguration->HasModule());
	AssertString("_KernalCreateDriver", pcConfiguration->GetModule()->GetEntry());
	AssertIntHex(0x20000, pcConfiguration->GetModule()->GetLimit());
	cAgain.Kill();

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\"/>\n\t\t<Module File=\"UART.DRV\" Entry=\"_KernalCreateDriver\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadDriver.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadDriver.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Module File=\"UART.DRV\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadDriver.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadDriver.hcp65build"));

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


#define TEST_EDIT_FILE		"Output" _FS_ "Edit.hcp65build"


//////////////////////////////////////////////////////////////////////////
//
// szText written to the file and read; the edits for it as it is, kept.
//
//////////////////////////////////////////////////////////////////////////
static void ReadEditFile(const char* szText, CBuildConfigurations* pcConfigurations, CArrayBuildConfigurationEdits* pasEdits)
{
	CTextFile	cFile;
	size		i;

	cFile.Init(szText);
	AssertTrue(cFile.Write(TEST_EDIT_FILE));
	cFile.Kill();
	AssertTrue(pcConfigurations->Read(TEST_EDIT_FILE));
	for (i = 0; i < pcConfigurations->NumConfigurations(); i++)
	{
		pasEdits->Add()->Init(pcConfigurations->GetConfiguration(i), i);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The edits go; the file is szExpected to the byte, and no temporary file
// is left.
//
//////////////////////////////////////////////////////////////////////////
static void KillEdits(CArrayBuildConfigurationEdits* pasEdits)
{
	size	i;

	for (i = 0; i < pasEdits->NumElements(); i++)
	{
		pasEdits->Get(i)->Kill();
	}
	pasEdits->ReInit();
}


static void AssertEditFileHas(const char* szExpected, CArrayBuildConfigurationEdits* pasEdits)
{
	CTextFile	cFile;

	cFile.Init();
	AssertTrue(cFile.Read(TEST_EDIT_FILE));
	AssertTrue(strstr(cFile.Text(), szExpected) != NULL);
	cFile.Kill();
	KillEdits(pasEdits);
}


static void AssertEditFile(const char* szExpected, CArrayBuildConfigurationEdits* pasEdits)
{
	CTextFile	cFile;
	CFileUtil	cFileUtil;

	cFile.Init();
	AssertTrue(cFile.Read(TEST_EDIT_FILE));
	AssertString(szExpected, cFile.Text());
	cFile.Kill();
	AssertFalse(cFileUtil.Exists(TEST_EDIT_FILE ".tmp"));
	KillEdits(pasEdits);
}


#define TEST_EDIT_HEADER	"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!-- A comment about <Configuration Name=\"Old\"/> to keep. -->\n<BuildConfigurations>\n"
#define TEST_EDIT_KERNEL	"\t<!-- The first. -->\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"Kernel\" NoFloats=\"true\">\n\t\t<Image File=\"Kernal.rom\" Size=\"0x80000\" Fill=\"0xFF\" Limit=\"0x7FC00\" Load=\"0x0\">\n\t\t\t<!-- Inside. -->\n\t\t\t<Region Start=\"0x0\" End=\"0x40000\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n"
#define TEST_EDIT_GCC		"\t<Configuration Toolchain=\"GCC\" Name=\"Fast\" Solution=\"Release|x64\"/>\n"
#define TEST_EDIT_FOOTER	"</BuildConfigurations>\n"
#define TEST_EDIT_TEXT		TEST_EDIT_HEADER TEST_EDIT_KERNEL TEST_EDIT_GCC TEST_EDIT_FOOTER


//////////////////////////////////////////////////////////////////////////
//
// Only what is edited changes in the file: comments, the order of the
// attributes, how numbers are written and what is inside a configuration
// stay to the byte.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsEdits(void)
{
	CBuildConfigurations			cConfigurations;
	CArrayBuildConfigurationEdits	asEdits;
	CBuildConfigurationEdit*		pcEdit;
	CFileUtil						cFileUtil;
	CTextFile						cFile;
	SLogConfig						sLogConfig;

	cFileUtil.MakeDir("Output");
	cConfigurations.Init();
	asEdits.Init();

	//Nothing edited changes nothing.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile(TEST_EDIT_TEXT, &asEdits);

	//An attribute changed where it is, one added at the end of the tag and one taken out.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	pcEdit = asEdits.Get(1);
	pcEdit->mszName.Set("Faster");
	pcEdit->mszBreakpointSpace.Set("A&B");
	pcEdit = asEdits.Get(0);
	pcEdit->mbNoFloats = false;
	pcEdit->mbMathTables = false;
	pcEdit->mszBreakpointSpace.Set("");
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile(TEST_EDIT_HEADER "\t<!-- The first. -->\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" MathTables=\"false\">\n\t\t<Image File=\"Kernal.rom\" Size=\"0x80000\" Fill=\"0xFF\" Limit=\"0x7FC00\" Load=\"0x0\">\n\t\t\t<!-- Inside. -->\n\t\t\t<Region Start=\"0x0\" End=\"0x40000\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n\t<Configuration Toolchain=\"GCC\" Name=\"Faster\" Solution=\"Release|x64\" BreakpointSpace=\"A&amp;B\"/>\n" TEST_EDIT_FOOTER, &asEdits);

	//And it is what is read back.
	AssertString("Faster", cConfigurations.GetConfiguration(1)->GetName());
	AssertString("A&B", cConfigurations.GetConfiguration(1)->GetBreakpointSpace());
	AssertTrue(cConfigurations.GetConfiguration(0)->HasImage());
	AssertFalse(cConfigurations.GetConfiguration(0)->IsMathTables());

	//A card needs its path; both go in, and both go out.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	asEdits.Get(0)->mszCard.Set("Card.img");
	asEdits.Get(0)->mszCardPath.Set("\\boot.exe");
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile(TEST_EDIT_HEADER "\t<!-- The first. -->\n\t<Configuration Name=\"Debug|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"Kernel\" NoFloats=\"true\" Card=\"Card.img\" CardPath=\"\\boot.exe\">\n\t\t<Image File=\"Kernal.rom\" Size=\"0x80000\" Fill=\"0xFF\" Limit=\"0x7FC00\" Load=\"0x0\">\n\t\t\t<!-- Inside. -->\n\t\t\t<Region Start=\"0x0\" End=\"0x40000\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n" TEST_EDIT_GCC TEST_EDIT_FOOTER, &asEdits);
	AssertString("Card.img", cConfigurations.GetConfiguration(0)->GetCard());

	//One removed goes with its line; the comment before it isn't its.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	asEdits.Get(0)->Kill();
	asEdits.RemoveAt(0);
	asEdits.Get(0)->muiSource = 1;
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile(TEST_EDIT_HEADER "\t<!-- The first. -->\n" TEST_EDIT_GCC TEST_EDIT_FOOTER, &asEdits);
	AssertSize(1, cConfigurations.NumConfigurations());

	//A copy has all that is inside the one it copies; a blank one has only what it says.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	pcEdit = asEdits.Add();
	pcEdit->Init(asEdits.Get(0), "Copy");
	pcEdit = asEdits.Add();
	pcEdit->Init("Blank", BT_W65C816, "Debug|Win32");
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile(TEST_EDIT_HEADER TEST_EDIT_KERNEL TEST_EDIT_GCC "\t<Configuration Name=\"Copy\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"Kernel\" NoFloats=\"true\">\n\t\t<Image File=\"Kernal.rom\" Size=\"0x80000\" Fill=\"0xFF\" Limit=\"0x7FC00\" Load=\"0x0\">\n\t\t\t<!-- Inside. -->\n\t\t\t<Region Start=\"0x0\" End=\"0x40000\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n\t<Configuration Name=\"Blank\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\"/>\n" TEST_EDIT_FOOTER, &asEdits);
	AssertSize(4, cConfigurations.NumConfigurations());
	AssertTrue(cConfigurations.GetConfiguration(2)->HasImage());
	AssertFalse(cConfigurations.GetConfiguration(3)->HasImage());

	//Windows' line ends are kept, and new lines have them.
	ReadEditFile("<BuildConfigurations>\r\n  <Configuration Name=\"A\" Toolchain=\"GCC\" Solution=\"Release|x64\"/>\r\n</BuildConfigurations>\r\n", &cConfigurations, &asEdits);
	asEdits.Add()->Init("B", BT_GCC, "Release|x64");
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile("<BuildConfigurations>\r\n  <Configuration Name=\"A\" Toolchain=\"GCC\" Solution=\"Release|x64\"/>\r\n  <Configuration Name=\"B\" Toolchain=\"GCC\" Solution=\"Release|x64\"/>\r\n</BuildConfigurations>\r\n", &asEdits);

	//An edit the file can't have (an image for GCC) leaves the file, and the configurations, as they were.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	asEdits.Get(0)->meToolchain = BT_GCC;
	sLogConfig = gcLogger.SetSilent();
	AssertFalse(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	gcLogger.SetConfig(&sLogConfig);
	AssertEditFile(TEST_EDIT_TEXT, &asEdits);
	AssertSize(2, cConfigurations.NumConfigurations());
	AssertTrue(cConfigurations.GetConfiguration(0)->HasImage());
	AssertTrue(BT_W65C816 == cConfigurations.GetConfiguration(0)->GetToolchain());

	//A file changed since it was read isn't edited: what is edited wouldn't be what is there.
	ReadEditFile(TEST_EDIT_TEXT, &cConfigurations, &asEdits);
	cFile.Init(TEST_EDIT_HEADER TEST_EDIT_GCC TEST_EDIT_KERNEL TEST_EDIT_FOOTER);
	AssertTrue(cFile.Write(TEST_EDIT_FILE));
	cFile.Kill();
	asEdits.Get(0)->mszBreakpointSpace.Set("User");
	AssertFalse(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile(TEST_EDIT_HEADER TEST_EDIT_GCC TEST_EDIT_KERNEL TEST_EDIT_FOOTER, &asEdits);

	//With no file yet, it is written from the configurations there are, with the edits.
	cFileUtil.Delete(TEST_EDIT_FILE);
	cConfigurations.Kill();
	cConfigurations.Init();
	cConfigurations.Add("Only", BT_GCC, "Release|x64");
	asEdits.Add()->Init(cConfigurations.GetConfiguration(0), 0);
	asEdits.Get(0)->mbNoFloats = true;
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertTrue(cFileUtil.Exists(TEST_EDIT_FILE));
	AssertEditFileHas("NoFloats=\"true\"", &asEdits);
	AssertTrue(cConfigurations.GetConfiguration(0)->IsNoFloats());

	//A file with no configurations at all can be added to, however its root is written.
	cConfigurations.Kill();
	cConfigurations.Init();
	ReadEditFile("<BuildConfigurations/>\n", &cConfigurations, &asEdits);
	AssertSize(0, cConfigurations.NumConfigurations());
	asEdits.Add()->Init("First", BT_GCC, "Release|x64");
	AssertTrue(cConfigurations.ApplyEdits(TEST_EDIT_FILE, &asEdits));
	AssertEditFile("<BuildConfigurations>\n\t<Configuration Name=\"First\" Toolchain=\"GCC\" Solution=\"Release|x64\"/>\n</BuildConfigurations>\n", &asEdits);

	asEdits.Kill();
	cConfigurations.Kill();
	cFileUtil.Delete(TEST_EDIT_FILE);
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
	TestBuildConfigurationsModule();
	TestBuildConfigurationsPlatforms();
	TestBuildConfigurationsEdits();

	TestStatistics();
}

