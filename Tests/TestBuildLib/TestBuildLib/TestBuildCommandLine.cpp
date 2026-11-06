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

	TestStatistics();
}

