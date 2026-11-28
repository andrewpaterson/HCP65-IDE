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
	CArrayChars				aszProjects;
	CCPPCompileOptions		cCompileOptions;

	cFileUtil.MakeDir("Output");
	cConfigurations.Init();
	cConfigurations.Add("Debug|HCP65C816", BT_W65C816, "Debug|Win32");
	cConfigurations.GetConfiguration(0)->SetBreakpointSpace("User");
	cConfigurations.GetConfiguration(0)->SetNoFloats(true);
	cConfigurations.GetConfiguration(0)->SetMathTables(false);
	cConfigurations.GetConfiguration(0)->SetPreferSize(true);
	cConfigurations.GetConfiguration(0)->SetWarnMissingOverride(false);
	cConfigurations.GetConfiguration(0)->SetExceptions(true);
	cConfigurations.GetConfiguration(0)->SetKeepUnusedObjects(true);
	cConfigurations.GetConfiguration(0)->SetWarnStringToPointer(true);
	cConfigurations.GetConfiguration(0)->SetMultiWordCasts(true);
	cConfigurations.GetConfiguration(0)->SetCConversions(true);
	cConfigurations.GetConfiguration(0)->SetDebug(true);
	cConfigurations.GetConfiguration(0)->SetUnoptimised(" Init ,Shell,  ");
	cConfigurations.Add("Fast GCC", BT_GCC, "Release|x64");
	AssertTrue(cConfigurations.Write("Output" _FS_ "Test.hcp65build"));

	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Test.hcp65build"));
	AssertSize(2, cRead.NumConfigurations());
	AssertBuildConfiguration("Debug|HCP65C816", BT_W65C816, "Debug|Win32", cRead.GetConfiguration(0));
	AssertBuildConfiguration("Fast GCC", BT_GCC, "Release|x64", cRead.GetConfiguration(1));

	AssertString("User", cRead.GetConfiguration(0)->GetBreakpointSpace());
	AssertString("", cRead.GetConfiguration(1)->GetBreakpointSpace());

	//NoFloats, MathTables, PreferSize, WarnMissingOverride, Exceptions, KeepUnusedObjects, WarnStringToPointer,
	//MultiWordCasts and CConversions (written only when not the default) are kept.
	AssertTrue(cRead.GetConfiguration(0)->IsNoFloats());
	AssertFalse(cRead.GetConfiguration(0)->IsMathTables());
	AssertTrue(cRead.GetConfiguration(0)->IsPreferSize());
	AssertFalse(cRead.GetConfiguration(1)->IsNoFloats());
	AssertTrue(cRead.GetConfiguration(1)->IsMathTables());
	AssertFalse(cRead.GetConfiguration(1)->IsPreferSize());
	AssertFalse(cRead.GetConfiguration(0)->IsWarnMissingOverride());
	AssertTrue(cRead.GetConfiguration(1)->IsWarnMissingOverride());
	AssertTrue(cRead.GetConfiguration(0)->IsExceptions());
	AssertFalse(cRead.GetConfiguration(1)->IsExceptions());
	AssertTrue(cRead.GetConfiguration(0)->IsKeepUnusedObjects());
	AssertTrue(cRead.GetConfiguration(0)->IsWarnStringToPointer());
	AssertTrue(cRead.GetConfiguration(0)->IsMultiWordCasts());
	AssertTrue(cRead.GetConfiguration(0)->IsCConversions());
	AssertFalse(cRead.GetConfiguration(1)->IsKeepUnusedObjects());
	AssertFalse(cRead.GetConfiguration(1)->IsWarnStringToPointer());
	AssertFalse(cRead.GetConfiguration(1)->IsMultiWordCasts());
	AssertFalse(cRead.GetConfiguration(1)->IsCConversions());

	//Debug and the projects compiled without the optimiser: the projects' names without the spaces round them.
	AssertTrue(cRead.GetConfiguration(0)->IsDebug());
	AssertFalse(cRead.GetConfiguration(1)->IsDebug());
	AssertString(" Init ,Shell,  ", cRead.GetConfiguration(0)->GetUnoptimised());
	AssertString("", cRead.GetConfiguration(1)->GetUnoptimised());
	aszProjects.Init();
	cRead.GetConfiguration(0)->GetUnoptimisedProjects(&aszProjects);
	AssertSize(2, aszProjects.NumElements());
	AssertString("Init", aszProjects.GetText(0));
	AssertString("Shell", aszProjects.GetText(1));
	aszProjects.ReInit();
	cRead.GetConfiguration(1)->GetUnoptimisedProjects(&aszProjects);
	AssertSize(0, aszProjects.NumElements());
	aszProjects.Kill();
	cCompileOptions.Init();
	cRead.GetConfiguration(0)->SetCompileOptions(&cCompileOptions);
	AssertTrue(cCompileOptions.mbDebuggable);
	cRead.GetConfiguration(1)->SetCompileOptions(&cCompileOptions);
	AssertFalse(cCompileOptions.mbDebuggable);
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
	cConfigurations.Add("Debug|HCP65C816", BT_W65C816, "Debug|Win32");
	AssertFalse(cConfigurations.GetConfiguration(0)->HasImage());
	AssertTrue(cConfigurations.Write("Output" _FS_ "NoImage.hcp65build"));
	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "NoImage.hcp65build"));
	AssertFalse(cRead.GetConfiguration(0)->HasImage());

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Release|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Release|Win32\">\n\t\t<Image File=\"Kernal.rom\" Size=\"0x80000\" Limit=\"523264\">\n\t\t\t<Region Start=\"0x0\" End=\"0x40000\" Offset=\"0x0\"/>\n\t\t\t<Region Start=\"0x400000\" End=\"0x440000\" Offset=\"0x40000\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
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
	AssertIntHex(0, pcImage->GetText());
	AssertSize(2, pcImage->NumRegions());

	//An image that runs from elsewhere (Text, where its code starts), written back the same.
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Release|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Release|Win32\">\n\t\t<Image File=\"Boot.rom\" Size=\"0x80000\" Limit=\"0x60000\" Text=\"0x40000\">\n\t\t\t<Region Start=\"0x40000\" End=\"0x60000\" Offset=\"0x40000\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "TextImage.hcp65build"));
	cFile.Kill();
	cConfigurations.Kill();
	cConfigurations.Init();
	AssertTrue(cConfigurations.Read("Output" _FS_ "TextImage.hcp65build"));
	AssertIntHex(0x40000, cConfigurations.GetConfiguration(0)->GetImage()->GetText());
	AssertTrue(cConfigurations.Write("Output" _FS_ "TextImageAgain.hcp65build"));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "TextImageAgain.hcp65build"));
	AssertTrue(strstr(cFile.Text(), "Load=\"0x0\" Text=\"0x40000\">") != NULL);
	cFile.Kill();

	//Line tables to read without loading them: the kernal OS.SYS, in Kernel space (the default) and checked against the
	//card; another in User space, not checked.  Written back the same, the default space left out.
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Release|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Release|Win32\" BreakpointSpace=\"User\">\n\t\t<Executable File=\"p.exe\" Entry=\"UserStart\"/>\n\t\t<Lines File=\"..\\Kernal\\OS.SYS\" CardPath=\"\\OS\\OS.SYS\"/>\n\t\t<Lines File=\"Other.exe\" Space=\"User\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Lines.hcp65build"));
	cFile.Kill();
	cConfigurations.Kill();
	cConfigurations.Init();
	AssertTrue(cConfigurations.Read("Output" _FS_ "Lines.hcp65build"));
	pcConfiguration = cConfigurations.GetConfiguration(0);
	AssertSize(2, pcConfiguration->NumLines());
	AssertSize(0, pcConfiguration->NumLoads());
	AssertString("..\\Kernal\\OS.SYS", pcConfiguration->GetLinesFile(0));
	AssertString("Kernel", pcConfiguration->GetLinesSpace(0));
	AssertString("\\OS\\OS.SYS", pcConfiguration->GetLinesCardPath(0));
	AssertString("Other.exe", pcConfiguration->GetLinesFile(1));
	AssertString("User", pcConfiguration->GetLinesSpace(1));
	AssertString("", pcConfiguration->GetLinesCardPath(1));
	AssertTrue(cConfigurations.Write("Output" _FS_ "LinesAgain.hcp65build"));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "LinesAgain.hcp65build"));
	AssertTrue(strstr(cFile.Text(), "<Lines File=\"..\\Kernal\\OS.SYS\" CardPath=\"\\OS\\OS.SYS\"/>") != NULL);
	AssertTrue(strstr(cFile.Text(), "<Lines File=\"Other.exe\" Space=\"User\"/>") != NULL);
	cFile.Kill();

	//No regions; a region past the image's end; not a number; MSVC.
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadImage.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadImage.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x10\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadImage.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadImage.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"big\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x0\"/>\n\t\t</Image>\n\t</Configuration>\n</BuildConfigurations>\n");
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
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"User\" Card=\"Card.img\" CardPath=\"\\boot.exe\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\" Limit=\"0x200000\">\n\t\t\t<Symbol Name=\"UserHeapSize\" Value=\"0x20000\"/>\n\t\t</Executable>\n\t\t<Load File=\"Kernal.rom\" Address=\"0x0\"/>\n\t\t<Load File=\"Tables.bin\" Address=\"4194304\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
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

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Image File=\"A.rom\" Size=\"0x100\">\n\t\t\t<Region Start=\"0x0\" End=\"0x100\" Offset=\"0x0\"/>\n\t\t</Image>\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadProgram.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" Card=\"Card.img\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
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
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\" BreakpointSpace=\"Kernel\" NoFloats=\"true\" MathTables=\"false\" Card=\"Card.img\" CardPath=\"\\OS\\Drivers\\UART.DRV\">\n\t\t<Module File=\"UART.DRV\" Entry=\"_KernalCreateDriver\" Limit=\"0x20000\"/>\n\t\t<Load File=\"Kernal.rom\" Address=\"0x0\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
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

	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Executable File=\"boot.exe\" Entry=\"UserStart\"/>\n\t\t<Module File=\"UART.DRV\" Entry=\"_KernalCreateDriver\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "BadDriver.hcp65build"));
	cFile.Kill();
	AssertFalse(cRead.Read("Output" _FS_ "BadDriver.hcp65build"));
	cFile.Init("<BuildConfigurations>\n\t<Configuration Name=\"Debug|HCP65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\">\n\t\t<Module File=\"UART.DRV\"/>\n\t</Configuration>\n</BuildConfigurations>\n");
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
// made, where it is read from a .hcp65build file and where it is looked
// for; any other name is itself.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildConfigurationsOldNames(void)
{
	CBuildConfigurations	cConfigurations;
	CBuildConfigurations	cRead;
	CRunConfiguration		cRun;
	CChars					szName;
	CFileUtil				cFileUtil;
	CTextFile				cFile;

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

	cFileUtil.TouchDir("Output" _FS_ "OldNames");
	cFile.Init("<BuildConfigurations>\n"
		"\t<Configuration Name=\"Debug|65C64\" Toolchain=\"65C64\" Solution=\"Debug|Win32\"/>\n"
		"\t<Configuration Name=\"Release|GCC\" Toolchain=\"GCC\" Solution=\"Release|x64\"/>\n"
		"</BuildConfigurations>\n");
	AssertTrue(cFile.Write("Output" _FS_ "OldNames" _FS_ "Old.hcp65build"));
	cFile.Kill();
	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "OldNames" _FS_ "Old.hcp65build"));
	AssertSize(2, cRead.NumConfigurations());
	AssertBuildConfiguration("Debug|HCP65C64", BT_65C64, "Debug|Win32", cRead.GetConfiguration(0));
	AssertBuildConfiguration("Release|GCC-x64", BT_GCC, "Release|x64", cRead.GetConfiguration(1));
	cRead.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "OldNames");

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
	TestBuildConfigurationsOldNames();
	TestBuildConfigurationsListed();
	TestBuildConfigurationsFile();
	TestBuildConfigurationsImage();
	TestBuildConfigurationsExecutable();
	TestBuildConfigurationsModule();
	TestBuildConfigurationsPlatforms();

	TestStatistics();
}

