#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildLib/BuildCommandLine.h"
#include "TestLib/Assert.h"


bool TestBuildEndsWith(const char* szText, const char* szEnd);


#define TEST_COMMAND_LINE_LIBC65	".." _FS_ ".." _FS_ ".." _FS_ ".." _FS_ "HCP65" _FS_ "Libraries" _FS_ "Libc65"
#define TEST_COMMAND_LINE_OUTPUT	"Output" _FS_ "CommandLine"


//The command line with the lines it writes kept, not printed.
class CTestBuildCommandLine : public CBuildCommandLine
{
public:
	CArrayChars	maszLines;

	void Init(void)
	{
		CBuildCommandLine::Init();
		maszLines.Init();
	}

	void Kill(void) override
	{
		maszLines.Kill();
		CBuildCommandLine::Kill();
	}

	bool HasLine(const char* szText)
	{
		size	i;

		for (i = 0; i < maszLines.NumElements(); i++)
		{
			if (strstr(maszLines.GetText(i), szText))
			{
				return true;
			}
		}
		return false;
	}

	char* GetLastLine(void)
	{
		return (maszLines.NumElements() > 0) ? maszLines.GetText(maszLines.NumElements() - 1) : (char*)"";
	}

protected:
	void WriteLine(const char* szLine) override
	{
		maszLines.Add(szLine);
	}
};


//////////////////////////////////////////////////////////////////////////
//
// Options start with - or --, any case; -config and -libc65 take the
// next argument.  One file, and an unknown option or a missing value is
// refused, saying why.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLineParse(void)
{
	CTestBuildCommandLine	cCommandLine;

	const char* aszGood[] = { "hcp65build", "--CONFIG", "Release|W65C816", "Program.sln", "-card", "-quiet", "-libc65", "Lib" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(8, aszGood));
	AssertString("Program.sln", cCommandLine.GetFile());
	AssertString("Release|W65C816", cCommandLine.GetConfiguration());
	AssertString("Lib", cCommandLine.GetLibc65());
	AssertTrue(cCommandLine.IsCard());
	AssertTrue(cCommandLine.IsQuiet());
	AssertFalse(cCommandLine.IsHelp());
	AssertSize(0, cCommandLine.maszLines.NumElements());
	cCommandLine.Kill();

	const char* aszPlain[] = { "hcp65build", "Program.sln" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(2, aszPlain));
	AssertString("", cCommandLine.GetConfiguration());
	AssertFalse(cCommandLine.IsCard());
	AssertFalse(cCommandLine.IsQuiet());
	cCommandLine.Kill();

	const char* aszHelp[] = { "hcp65build", "-help" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(2, aszHelp));
	AssertTrue(cCommandLine.IsHelp());
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("-config"));
	cCommandLine.Kill();

	const char* aszNone[] = { "hcp65build" };
	cCommandLine.Init();
	AssertFalse(cCommandLine.Parse(1, aszNone));
	AssertString("No solution, project, configurations or workspace to build.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszTwo[] = { "hcp65build", "A.sln", "B.sln" };
	cCommandLine.Init();
	AssertFalse(cCommandLine.Parse(3, aszTwo));
	AssertString("Only one file can be built: A.sln and B.sln.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszUnknown[] = { "hcp65build", "A.sln", "-fast" };
	cCommandLine.Init();
	AssertFalse(cCommandLine.Parse(3, aszUnknown));
	AssertString("Unknown option -fast.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszMissing[] = { "hcp65build", "A.sln", "-config" };
	cCommandLine.Init();
	AssertFalse(cCommandLine.Parse(3, aszMissing));
	AssertString("Option -config needs a value.", cCommandLine.GetLastLine());
	cCommandLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// -quiet leaves out the lines that only say how the build is going.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLineProgress(void)
{
	AssertTrue(CBuildCommandLine::IsProgressLine("Maths.cpp (12/40)"));
	AssertTrue(CBuildCommandLine::IsProgressLine("CalculatorError.cpp (17/781)"));
	AssertTrue(CBuildCommandLine::IsProgressLine("ErrorHandler.cpp (compiled again)"));
	AssertTrue(CBuildCommandLine::IsProgressLine("Tiny: Debug|Win32"));
	AssertTrue(CBuildCommandLine::IsProgressLine("Segment $010000, 512 bytes."));

	AssertFalse(CBuildCommandLine::IsProgressLine("stdlib.cpp (8, 10): Could not include file \"BaseLib/TrueMallocator.h\""));
	AssertFalse(CBuildCommandLine::IsProgressLine("Objects.cpp (606, 26): Warning: Loop counter narrower than its bound. [i]"));
	AssertFalse(CBuildCommandLine::IsProgressLine("Debug|W65C816 build failed."));
	AssertFalse(CBuildCommandLine::IsProgressLine("Wrote 512 bytes to Output\\W65C816\\Debug\\p.exe."));
	AssertFalse(CBuildCommandLine::IsProgressLine("Missing symbol (in Main.cpp)"));
	AssertFalse(CBuildCommandLine::IsProgressLine("Ratio (1/)"));
	AssertFalse(CBuildCommandLine::IsProgressLine(""));
}


//////////////////////////////////////////////////////////////////////////
//
// Libc65 is found in a directory above the one given.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLineFindLibc65(void)
{
	CFileUtil	cFileUtil;
	CChars		szFound;
	CChars		szExpected;

	szFound.Init();
	AssertTrue(CBuildCommandLine::FindLibc65("Input" _FS_ "Tiny", &szFound));
	szExpected.Init(TEST_COMMAND_LINE_LIBC65);
	cFileUtil.FullPath(&szExpected);
	AssertString(szExpected.Text(), szFound.Text());
	szExpected.Kill();
	szFound.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Tiny (copied, so its output goes in Output) built from the command
// line: its first HCP65 configuration (it has no .hcp65build, so Debug|W65C816
// of the defaults) without -config, or the one named; Broken fails; a
// file or configuration that isn't there is a usage error.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLineRun(void)
{
	CTestBuildCommandLine	cCommandLine;
	CFileUtil				cFileUtil;
	CChars					szLibc65;

	cFileUtil.RemoveDir(TEST_COMMAND_LINE_OUTPUT);
	AssertTrue(cFileUtil.TouchDir(TEST_COMMAND_LINE_OUTPUT));
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "Tiny", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny"));
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "Broken", TEST_COMMAND_LINE_OUTPUT _FS_ "Broken"));

	const char* aszDefault[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.vcxproj", "-libc65", TEST_COMMAND_LINE_LIBC65 };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(4, aszDefault));
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertString("Debug|W65C816", cCommandLine.GetConfiguration());
	AssertTrue(cCommandLine.HasLine("Tiny: Debug|Win32"));
	AssertString("Debug|W65C816 build succeeded.", cCommandLine.GetLastLine());
	AssertTrue(cFileUtil.Exists(TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Listing" _FS_ "W65C816" _FS_ "Debug" _FS_ "Maths.lst"));
	cCommandLine.Kill();

	//No -libc65 and a tool directory without Libc65 above it: it is found above the file.
	const char* aszFound[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.vcxproj" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(2, aszFound));
#ifdef _WIN32
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run("C:\\"));
#else
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run("/"));
#endif
	szLibc65.Init(TEST_COMMAND_LINE_LIBC65);
	cFileUtil.FullPath(&szLibc65);
	AssertString(szLibc65.Text(), cCommandLine.GetLibc65());
	szLibc65.Kill();
	cCommandLine.Kill();

	const char* aszQuiet[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.vcxproj", "-config", "Debug|65C64", "-quiet", "-libc65", TEST_COMMAND_LINE_LIBC65 };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(7, aszQuiet));
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertFalse(cCommandLine.HasLine("Tiny: Debug|Win32"));
	AssertString("Debug|65C64 build succeeded.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszBroken[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Broken" _FS_ "Broken.vcxproj", "-quiet", "-libc65", TEST_COMMAND_LINE_LIBC65 };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(5, aszBroken));
	AssertInt(BUILD_EXIT_FAILED, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("Broken.cpp"));
	AssertString("Debug|W65C816 build failed.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszConfiguration[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.vcxproj", "-config", "Fast|W65C816", "-libc65", TEST_COMMAND_LINE_LIBC65 };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(6, aszConfiguration));
	AssertInt(BUILD_EXIT_USAGE, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("No configuration Fast|W65C816. There are: \"Debug|Win32\""));
	cCommandLine.Kill();

	const char* aszMissing[] = { "hcp65build", "Nothing.sln" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(2, aszMissing));
	AssertInt(BUILD_EXIT_USAGE, cCommandLine.Run(NULL));
	AssertString("Could not find Nothing.sln.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszLibc65[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.vcxproj", "-libc65", "NoLibc65" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(4, aszLibc65));
	AssertInt(BUILD_EXIT_USAGE, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("Libc65 isn't at "));
	cCommandLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A .hcp65build is for the .sln of the same name beside it, else the
// .vcxproj.  Tiny (copied by TestBuildCommandLineRun) given one with a
// configuration only it has is built from it; one with neither beside it
// is a usage error.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLineConfigurationsFile(void)
{
	CTestBuildCommandLine	cCommandLine;
	CTextFile				cFile;
	CChars					szFound;

	szFound.Init();
	AssertTrue(CBuildCommandLine::FindSolutionFile("Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.hcp65build", &szFound));
	AssertString("Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln", szFound.Text());
	AssertTrue(CBuildCommandLine::FindSolutionFile(TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.hcp65build", &szFound));
	AssertString(TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.vcxproj", szFound.Text());
	AssertFalse(CBuildCommandLine::FindSolutionFile(TEST_COMMAND_LINE_OUTPUT _FS_ "Alone.hcp65build", &szFound));
	AssertString("", szFound.Text());
	szFound.Kill();

	cFile.Init("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<BuildConfigurations>\n\t<Configuration Name=\"Lean|W65C816\" Toolchain=\"W65C816\" Solution=\"Debug|Win32\"/>\n</BuildConfigurations>\n");
	AssertTrue(cFile.Write(TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.hcp65build"));
	AssertTrue(cFile.Write(TEST_COMMAND_LINE_OUTPUT _FS_ "Alone.hcp65build"));
	cFile.Kill();

	const char* aszTiny[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny" _FS_ "Tiny.hcp65build", "-libc65", TEST_COMMAND_LINE_LIBC65 };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(4, aszTiny));
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertTrue(TestBuildEndsWith(cCommandLine.GetFile(), "Tiny" _FS_ "Tiny.vcxproj"));
	AssertString("Lean|W65C816", cCommandLine.GetConfiguration());
	AssertString("Lean|W65C816 build succeeded.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	const char* aszAlone[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Alone.hcp65build" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(2, aszAlone));
	AssertInt(BUILD_EXIT_USAGE, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("Could not find the .sln or .vcxproj that "));
	AssertTrue(cCommandLine.HasLine("Alone.hcp65build is for (the same name beside it)."));
	cCommandLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the command line given parses, with the last line it wrote if
// it doesn't.
//
//////////////////////////////////////////////////////////////////////////
static bool TestBuildCommandLineParses(int iArgc, const char** aszArgv, CChars* pszLastLine)
{
	CTestBuildCommandLine	cCommandLine;
	bool					bResult;

	cCommandLine.Init();
	bResult = cCommandLine.Parse(iArgc, aszArgv);
	pszLastLine->Set(cCommandLine.GetLastLine());
	cCommandLine.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether two listings are the same but for when they were compiled (the
// .fingerprint lines).
//
//////////////////////////////////////////////////////////////////////////
static bool TestBuildSameListing(const char* szListing1, const char* szListing2)
{
	CTextFile	cFile1;
	CTextFile	cFile2;
	CChars		szText1;
	CChars		szText2;
	CArrayChars	aszLines1;
	CArrayChars	aszLines2;
	size		i;
	size		j;
	bool		bResult;

	cFile1.Init();
	cFile2.Init();
	bResult = cFile1.Read(szListing1) && cFile2.Read(szListing2);
	if (bResult)
	{
		szText1.Init(cFile1.Text());
		szText2.Init(cFile2.Text());
		aszLines1.Init();
		aszLines2.Init();
		szText1.Split(&aszLines1, '\n');
		szText2.Split(&aszLines2, '\n');
		for (i = 0, j = 0; bResult && ((i < aszLines1.NumElements()) || (j < aszLines2.NumElements())); i++, j++)
		{
			while ((i < aszLines1.NumElements()) && strstr(aszLines1.GetText(i), ".fingerprint"))
			{
				i++;
			}
			while ((j < aszLines2.NumElements()) && strstr(aszLines2.GetText(j), ".fingerprint"))
			{
				j++;
			}
			if ((i < aszLines1.NumElements()) != (j < aszLines2.NumElements()))
			{
				bResult = false;
			}
			else if (i < aszLines1.NumElements())
			{
				bResult = strcmp(aszLines1.GetText(i), aszLines2.GetText(j)) == 0;
			}
		}
		aszLines2.Kill();
		aszLines1.Kill();
		szText2.Kill();
		szText1.Kill();
	}
	cFile2.Kill();
	cFile1.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Numbers are decimal, 0x or $ hexadecimal.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLinePlainNumbers(void)
{
	int64	iValue;
	uint32	uiValue;

	AssertTrue(BuildParseNumber("16", &iValue));
	AssertLong(16, iValue);
	AssertTrue(BuildParseNumber("0x180000", &iValue));
	AssertLong(0x180000, iValue);
	AssertTrue(BuildParseNumber("$FF", &iValue));
	AssertLong(0xFF, iValue);
	AssertFalse(BuildParseNumber("", &iValue));
	AssertFalse(BuildParseNumber("$", &iValue));
	AssertFalse(BuildParseNumber("12a", &iValue));
	AssertTrue(BuildParseUint32("$FFFFFFFF", &uiValue));
	AssertIntHex(0xFFFFFFFF, uiValue);
	AssertFalse(BuildParseUint32("-1", &uiValue));
	AssertFalse(BuildParseUint32("0x100000000", &uiValue));
}


//////////////////////////////////////////////////////////////////////////
//
// A plain command line as one PowerShell command: what PowerShell would
// read as something else (@ splatting, a comma's array, a space) is in
// single quotes, a single quote in it doubled.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLinePlainPowerShell(void)
{
	CArrayChars	aszArguments;
	CChars		szLine;

	aszArguments.Init();
	aszArguments.Add("-D");
	aszArguments.Add("A=1");
	aszArguments.Add("-I+");
	aszArguments.Add("..\\Libraries");
	aszArguments.Add("@Base Lib.units");
	aszArguments.Add("0,0x80,0");
	aszArguments.Add("it's");
	szLine.Init();
	CBuildPlain::MakePowerShellLine("D:\\Tools\\hcp65build.exe", &aszArguments, &szLine);
	AssertString("& D:\\Tools\\hcp65build.exe -D A=1 -I+ ..\\Libraries '@Base Lib.units' '0,0x80,0' 'it''s'", szLine.Text());
	CBuildPlain::MakePowerShellLine("C:\\Program Files\\hcp65build.exe", &aszArguments, &szLine);
	AssertTrue(szLine.StartsWith("& 'C:\\Program Files\\hcp65build.exe' -D"));
	szLine.Kill();
	aszArguments.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A plain command line: projects and their sources, include directories
// for every unit or for a project, defines, the program and @files.
// What doesn't go together is refused, saying why.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLinePlainParse(void)
{
	CTestBuildCommandLine	cCommandLine;
	CBuildConfiguration		cConfiguration;
	CHCP65BuildDescription*	pcDescription;
	CHCP65BuildProject*		pcProject;
	CTextFile				cFile;
	CChars					szLine;
	CChars					szError;

	const char* aszPlain[] = { "hcp65build", "-I", "Everyone", "-program", "Main", "-dir", "Main", "-I+", "Inc", "-D", "MAIN=1", "Main.cpp", "Other.asm",
							   "-lib", "Lib", "Lib.cpp", "-exe", "main.exe", "-entry", "Start", "-limit", "$200000", "-symbol", "UserHeapSize=0x20000", "-nofloats" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(25, aszPlain));
	AssertTrue(cCommandLine.IsPlain());
	pcDescription = cCommandLine.GetPlain()->GetDescription();
	AssertSize(1, pcDescription->mcIncludes.NumIncludes());
	AssertString("Everyone", pcDescription->mcIncludes.GetDirectory(0));
	AssertFalse(pcDescription->mcIncludes.HasSubDirectories(0));
	AssertSize(2, pcDescription->NumProjects());
	pcProject = pcDescription->GetProject(0);
	AssertString("Main", pcProject->mszName.Text());
	AssertString("Main", pcProject->mszDirectory.Text());
	AssertFalse(pcProject->mbLibrary);
	AssertSize(1, pcProject->mcIncludes.NumIncludes());
	AssertString("Inc", pcProject->mcIncludes.GetDirectory(0));
	AssertTrue(pcProject->mcIncludes.HasSubDirectories(0));
	AssertSize(1, pcProject->maszDefines.NumElements());
	AssertString("MAIN=1", pcProject->maszDefines.GetText(0));
	AssertSize(2, pcProject->maszSources.NumElements());
	AssertString("Other.asm", pcProject->maszSources.GetText(1));
	pcProject = pcDescription->GetProject(1);
	AssertString("Lib", pcProject->mszName.Text());
	AssertString(".", pcProject->mszDirectory.Text());
	AssertTrue(pcProject->mbLibrary);

	szError.Init();
	AssertTrue(cCommandLine.GetPlain()->MakeConfiguration(&cConfiguration, &szError));
	AssertString("Plain|W65C816", cConfiguration.GetName());
	AssertTrue(cConfiguration.IsNoFloats());
	AssertTrue(cConfiguration.IsMathTables());
	AssertTrue(cConfiguration.HasExecutable());
	AssertFalse(cConfiguration.HasImage());
	AssertString("main.exe", cConfiguration.GetExecutable()->GetFile());
	AssertString("Start", cConfiguration.GetExecutable()->GetEntry());
	AssertIntHex(0x200000, cConfiguration.GetExecutable()->GetLimit());
	AssertSize(1, cConfiguration.GetExecutable()->NumSymbols());
	AssertString("UserHeapSize", cConfiguration.GetExecutable()->GetSymbolName(0));
	AssertLongLongIntHex(0x20000, cConfiguration.GetExecutable()->GetSymbolValue(0));
	cConfiguration.Kill();
	szError.Kill();
	cCommandLine.Kill();

	//An image, for the 65C64.
	const char* aszImage[] = { "hcp65build", "-target", "65C64", "-program", "Rom", "Rom.cpp", "-image", "k.rom", "-size", "0x100", "-fill", "0", "-load", "$8000", "-region", "0,0x80,0", "-region", "0x100,0x180,0x80", "-no-mathtables" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(19, aszImage));
	szError.Init();
	AssertTrue(cCommandLine.GetPlain()->MakeConfiguration(&cConfiguration, &szError));
	AssertString("Plain|65C64", cConfiguration.GetName());
	AssertFalse(cConfiguration.IsMathTables());
	AssertTrue(cConfiguration.HasImage());
	AssertString("k.rom", cConfiguration.GetImage()->GetFile());
	AssertIntHex(0x100, cConfiguration.GetImage()->GetSize());
	AssertIntHex(0, cConfiguration.GetImage()->GetFill());
	AssertIntHex(0x8000, cConfiguration.GetImage()->GetLoad());
	AssertSize(2, cConfiguration.GetImage()->NumRegions());
	AssertIntHex(0x100, cConfiguration.GetImage()->GetRegion(1)->uiStart);
	AssertIntHex(0x80, cConfiguration.GetImage()->GetRegion(1)->uiOffset);
	cConfiguration.Kill();
	szError.Kill();
	cCommandLine.Kill();

	//Arguments from files, which read others; blank lines and # lines are skipped.
	cFile.Init("# The program\n\n-program\nFromFile\n@" TEST_COMMAND_LINE_OUTPUT _FS_ "Sources.args\n");
	AssertTrue(cFile.Write(TEST_COMMAND_LINE_OUTPUT _FS_ "Main.args"));
	cFile.Kill();
	cFile.Init("A.cpp\r\n  B.cpp  \r\n");
	AssertTrue(cFile.Write(TEST_COMMAND_LINE_OUTPUT _FS_ "Sources.args"));
	cFile.Kill();
	const char* aszFile[] = { "hcp65build", "@" TEST_COMMAND_LINE_OUTPUT _FS_ "Main.args", "-quiet" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(3, aszFile));
	AssertTrue(cCommandLine.IsPlain());
	AssertTrue(cCommandLine.IsQuiet());
	pcProject = cCommandLine.GetPlain()->GetDescription()->GetProject(0);
	AssertString("FromFile", pcProject->mszName.Text());
	AssertSize(2, pcProject->maszSources.NumElements());
	AssertString("A.cpp", pcProject->maszSources.GetText(0));
	AssertString("B.cpp", pcProject->maszSources.GetText(1));
	cCommandLine.Kill();

	szLine.Init();
	const char* aszNoFile[] = { "hcp65build", "@Nothing.args" };
	AssertFalse(TestBuildCommandLineParses(2, aszNoFile, &szLine));
	AssertString("Could not read the argument file Nothing.args.", szLine.Text());

	const char* aszSourceFirst[] = { "hcp65build", "A.cpp", "-program", "P" };
	AssertFalse(TestBuildCommandLineParses(4, aszSourceFirst, &szLine));
	AssertString("The source A.cpp must follow a -program or -lib.", szLine.Text());

	const char* aszDefineFirst[] = { "hcp65build", "-D", "X", "-program", "P", "A.cpp" };
	AssertFalse(TestBuildCommandLineParses(6, aszDefineFirst, &szLine));
	AssertString("-D must follow a -program or -lib.", szLine.Text());

	const char* aszEmpty[] = { "hcp65build", "-program", "P", "-lib", "L", "B.cpp" };
	AssertFalse(TestBuildCommandLineParses(6, aszEmpty, &szLine));
	AssertString("-program P has no sources.", szLine.Text());

	const char* aszNoEntry[] = { "hcp65build", "-program", "P", "A.cpp", "-exe", "p.exe" };
	AssertFalse(TestBuildCommandLineParses(6, aszNoEntry, &szLine));
	AssertString("-exe needs its entry symbol (-entry).", szLine.Text());

	const char* aszEntryOnly[] = { "hcp65build", "-program", "P", "A.cpp", "-entry", "Start" };
	AssertFalse(TestBuildCommandLineParses(6, aszEntryOnly, &szLine));
	AssertString("-entry and -symbol are for an executable: they need -exe.", szLine.Text());

	const char* aszBoth[] = { "hcp65build", "-program", "P", "A.cpp", "-exe", "p.exe", "-entry", "S", "-image", "p.rom" };
	AssertFalse(TestBuildCommandLineParses(10, aszBoth, &szLine));
	AssertString("Link an executable (-exe) or an image (-image), not both.", szLine.Text());

	const char* aszNoSize[] = { "hcp65build", "-program", "P", "A.cpp", "-image", "p.rom", "-region", "0,1,0" };
	AssertFalse(TestBuildCommandLineParses(8, aszNoSize, &szLine));
	AssertString("-image needs its size (-size).", szLine.Text());

	const char* aszNoRegion[] = { "hcp65build", "-program", "P", "A.cpp", "-image", "p.rom", "-size", "16" };
	AssertFalse(TestBuildCommandLineParses(8, aszNoRegion, &szLine));
	AssertString("-image needs a -region.", szLine.Text());

	const char* aszBigRegion[] = { "hcp65build", "-program", "P", "A.cpp", "-image", "p.rom", "-size", "16", "-region", "0,32,0" };
	AssertFalse(TestBuildCommandLineParses(10, aszBigRegion, &szLine));
	AssertString("-region 0,32,0 doesn't fit in the image's size.", szLine.Text());

	const char* aszBadRegion[] = { "hcp65build", "-program", "P", "A.cpp", "-region", "0,32" };
	AssertFalse(TestBuildCommandLineParses(6, aszBadRegion, &szLine));
	AssertString("-region is given <start>,<end>,<offset>, not 0,32.", szLine.Text());

	const char* aszBadNumber[] = { "hcp65build", "-program", "P", "A.cpp", "-exe", "p.exe", "-entry", "S", "-limit", "lots" };
	AssertFalse(TestBuildCommandLineParses(10, aszBadNumber, &szLine));
	AssertString("-limit isn't given a number (decimal, 0x or $ hexadecimal): lots.", szLine.Text());

	const char* aszBadFill[] = { "hcp65build", "-program", "P", "A.cpp", "-fill", "256" };
	AssertFalse(TestBuildCommandLineParses(6, aszBadFill, &szLine));
	AssertString("-fill isn't given a number from 0 to 255: 256.", szLine.Text());

	const char* aszBadSymbol[] = { "hcp65build", "-program", "P", "A.cpp", "-symbol", "Heap" };
	AssertFalse(TestBuildCommandLineParses(6, aszBadSymbol, &szLine));
	AssertString("-symbol is given <name>=<number>, not Heap.", szLine.Text());

	const char* aszBadTarget[] = { "hcp65build", "-target", "GCC", "-program", "P", "A.cpp" };
	AssertFalse(TestBuildCommandLineParses(6, aszBadTarget, &szLine));
	AssertString("Unknown target GCC: W65C816 or 65C64.", szLine.Text());

	const char* aszConfig[] = { "hcp65build", "-program", "P", "A.cpp", "-config", "Release|W65C816" };
	AssertFalse(TestBuildCommandLineParses(6, aszConfig, &szLine));
	AssertString("-config is for a solution, not a plain command line (-program, -lib).", szLine.Text());

	const char* aszPlainInSolution[] = { "hcp65build", "Program.sln", "-exe", "p.exe" };
	AssertFalse(TestBuildCommandLineParses(4, aszPlainInSolution, &szLine));
	AssertString("Option -exe is for a plain command line (-program, -lib), not a solution.", szLine.Text());
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Split (Tiny, copied, with a source outside its directory:
// ..\Shared\Extra.cpp, a library of its own, Tiny.Shared) is written as a
// plain command line by -show-plain, and built from it compiles exactly
// what its project does: the same listings but for when they were
// compiled.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLinePlainSameAsProject(void)
{
	CTestBuildCommandLine	cCommandLine;
	CFileUtil				cFileUtil;
	CTextFile				cFile;
	CTextFile				cProject;
	CChars					szProject;

	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "Tiny", TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Tiny"));
	AssertTrue(cFileUtil.TouchDir(TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Shared"));
	cFile.Init("#include \"Maths.h\"\n\n\nint Twice(int a)\n{\n\treturn Triple(a, 0) - a;\n}\n");
	AssertTrue(cFile.Write(TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Shared" _FS_ "Extra.cpp"));
	cFile.Kill();
	cProject.Init();
	AssertTrue(cProject.Read("Input" _FS_ "Tiny" _FS_ "Tiny.vcxproj"));
	szProject.Init(cProject.Text());
	cProject.Kill();
	AssertTrue(szProject.Replace("<ClCompile Include=\"Maths.cpp\" />", "<ClCompile Include=\"Maths.cpp\" />\n    <ClCompile Include=\"..\\Shared\\Extra.cpp\" />") > 0);
	cFile.Init(szProject.Text());
	AssertTrue(cFile.Write(TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Tiny" _FS_ "Tiny.vcxproj"));
	cFile.Kill();
	szProject.Kill();

	//Its project built, then written as a plain command line.
	const char* aszProject[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Tiny" _FS_ "Tiny.vcxproj", "-libc65", TEST_COMMAND_LINE_LIBC65 };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(4, aszProject));
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("Extra.cpp"));
	cCommandLine.Kill();

	const char* aszShow[] = { "hcp65build", TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Tiny" _FS_ "Tiny.vcxproj", "-libc65", TEST_COMMAND_LINE_LIBC65, "-show-plain", TEST_COMMAND_LINE_OUTPUT _FS_ "Split.plain" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(6, aszShow));
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("Tiny.units."));
	AssertTrue(cCommandLine.HasLine("PowerShell, in "));
	AssertTrue(TestBuildEndsWith(cCommandLine.GetLastLine(), "'@" TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny.units'"));
	AssertTrue(strstr(cCommandLine.GetLastLine(), " -lib Tiny -dir ") != NULL);
	AssertTrue(cFileUtil.Exists(TEST_COMMAND_LINE_OUTPUT _FS_ "Split.plain"));
	AssertTrue(cFileUtil.Exists(TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny.units"));
	cCommandLine.Kill();
	cFile.Init();
	AssertTrue(cFile.Read(TEST_COMMAND_LINE_OUTPUT _FS_ "Tiny.units"));
	AssertString("Maths.cpp\n.." _FS_ "Shared" _FS_ "Extra.cpp\n", cFile.Text());
	cFile.Kill();

	//Built from it, into listings of its own.
	const char* aszPlain[] = { "hcp65build", "@" TEST_COMMAND_LINE_OUTPUT _FS_ "Split.plain", "-listings", TEST_COMMAND_LINE_OUTPUT _FS_ "SplitPlain" };
	cCommandLine.Init();
	AssertTrue(cCommandLine.Parse(4, aszPlain));
	AssertTrue(cCommandLine.IsPlain());
	AssertInt(BUILD_EXIT_SUCCEEDED, cCommandLine.Run(NULL));
	AssertTrue(cCommandLine.HasLine("Tiny: Plain|W65C816"));
	AssertString("Plain|W65C816 build succeeded.", cCommandLine.GetLastLine());
	cCommandLine.Kill();

	AssertTrue(TestBuildSameListing(TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Tiny" _FS_ "Listing" _FS_ "W65C816" _FS_ "Debug" _FS_ "Maths.lst", TEST_COMMAND_LINE_OUTPUT _FS_ "SplitPlain" _FS_ "Maths.lst"));
	AssertTrue(TestBuildSameListing(TEST_COMMAND_LINE_OUTPUT _FS_ "Split" _FS_ "Tiny" _FS_ "Listing" _FS_ "W65C816" _FS_ "Debug" _FS_ "Extra.lst", TEST_COMMAND_LINE_OUTPUT _FS_ "SplitPlain" _FS_ "Extra.lst"));

}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuildCommandLine(void)
{
	BeginTests();

	TestBuildCommandLineParse();
	TestBuildCommandLineProgress();
	TestBuildCommandLineFindLibc65();
	TestBuildCommandLineRun();
	TestBuildCommandLineConfigurationsFile();
	TestBuildCommandLinePlainNumbers();
	TestBuildCommandLinePlainPowerShell();
	TestBuildCommandLinePlainParse();
	TestBuildCommandLinePlainSameAsProject();

	TestStatistics();
}

