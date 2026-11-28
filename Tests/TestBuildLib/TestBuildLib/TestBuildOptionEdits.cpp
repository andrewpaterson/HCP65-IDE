#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BuildLib/BuildOptionEdits.h"
#include "VisualStudioLib/VcxSolutionRows.h"
#include "TestLib/Assert.h"
#include "TestLib/AssertFile.h"


#define TEST_OPTIONS_BEFORE		".." _FS_ ".." _FS_ "TestVisualStudioLib" _FS_ "TestVisualStudioLib" _FS_ "Input" _FS_ "Configurations" _FS_ "Before"
#define TEST_OPTIONS_ROWS		".." _FS_ ".." _FS_ "TestVisualStudioLib" _FS_ "TestVisualStudioLib" _FS_ "Input" _FS_ "Configurations" _FS_ "Expected" _FS_ "Program.sln"
#define TEST_OPTIONS_EXPECTED	"Input" _FS_ "Options" _FS_ "Expected" _FS_
#define TEST_OPTIONS_DIRECTORY	"Output" _FS_ "Options"
#define TEST_OPTIONS_SOLUTION	TEST_OPTIONS_DIRECTORY _FS_ "Program.sln"
#define TEST_OPTIONS_PROGRAM	TEST_OPTIONS_DIRECTORY _FS_ "Program.vcxproj"
#define TEST_OPTIONS_LIBRARY	TEST_OPTIONS_DIRECTORY _FS_ "Library.vcxproj"
#define TEST_OPTIONS_SHEET		TEST_OPTIONS_DIRECTORY _FS_ "Shared.props"
#define TEST_OPTIONS_GCC			"Input" _FS_ "OptionsGCC"


//////////////////////////////////////////////////////////////////////////
//
// A copy of the solution the options are edited in: a program and a
// library that share a sheet (TestVisualStudioLib's
// Input/Configurations/Before).
//
//////////////////////////////////////////////////////////////////////////
static void CopyOptionsSolution(void)
{
	CFileUtil	cFileUtil;

	cFileUtil.RemoveDir(TEST_OPTIONS_DIRECTORY);
	AssertTrue(cFileUtil.TouchDir(TEST_OPTIONS_DIRECTORY));
	AssertTrue(cFileUtil.CopyDir(TEST_OPTIONS_BEFORE, TEST_OPTIONS_DIRECTORY));
}


//The kind of the options the helpers below find by name: the toolchains each have a ClCompile.PreprocessorDefinitions.
static EBuildOptionKind	geTestOptionKind = BOK_HCP65;


static void AssertPlaceText(const char* szExpected, CBuildOptionEdits* pcEdits, size uiConfiguration, size uiProject, const char* szOption, EBuildOptionPlace ePlace)
{
	CChars	szText;

	szText.Init();
	pcEdits->GetPlaceText(uiConfiguration, uiProject, FindBuildOption(szOption, geTestOptionKind), ePlace, &szText);
	AssertString(szExpected, szText.Text());
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where each option's value is from, as the projects and their sheet say
// it: the project's own for one configuration or its platform, the sheet,
// the program (a library that says nothing has what the startup project
// has) or the default; and the places it could be from.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEditsRead(void)
{
	CVcxSolution		cSolution;
	CBuildOptionEdits	cEdits;
	EBuildOptionPlace	aePlaces[BOP_NumPlaces];
	size				uiOption;

	CopyOptionsSolution();
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	AssertSize(0, cEdits.Add("Release|HCP65C816", "Release|HCP65C816"));
	AssertSize(1, cEdits.Add("Debug|HCP65C816", "Debug|HCP65C816"));
	AssertSize(2, cEdits.Add("Release|Win32", "Release|Win32"));
	AssertSize(2, cEdits.NumProjects(0));
	AssertSize(2, cEdits.NumProjects(2));	//An MSVC configuration, which has options of its own kind.
	AssertSize(0, cEdits.GetStartupProject(0));
	AssertString("Program", cEdits.GetProject(0, 0)->mszName.Text());
	AssertString("Library", cEdits.GetProject(0, 1)->mszName.Text());
	AssertString("Release", cEdits.GetProject(0, 0)->mszConfiguration.Text());
	AssertString("Release|Win32", cEdits.GetProject(0, 0)->mszRow.Text());

	//The program's own for the platform, over the sheet's.
	uiOption = FindBuildOption("NoFloats");
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiOption));
	AssertString("true", cEdits.GetValue(0, 0, uiOption));
	AssertSize(3, cEdits.GetPlaces(0, 0, uiOption, aePlaces));
	AssertInt(BOP_Configuration, aePlaces[0]);
	AssertInt(BOP_Platform, aePlaces[1]);
	AssertInt(BOP_Sheet, aePlaces[2]);
	AssertPlaceText("This project, Release only", &cEdits, 0, 0, "NoFloats", BOP_Configuration);
	AssertPlaceText("This project, every HCP65C816 configuration", &cEdits, 0, 0, "NoFloats", BOP_Platform);
	AssertPlaceText("Shared.props, 2 projects, every HCP65C816 configuration", &cEdits, 0, 0, "NoFloats", BOP_Sheet);

	//Its own for the one configuration, which Debug hasn't; and what nothing says.
	uiOption = FindBuildOption("PreferSize");
	AssertInt(BOP_Configuration, cEdits.GetPlace(0, 0, uiOption));
	AssertString("true", cEdits.GetValue(0, 0, uiOption));
	AssertInt(BOP_Default, cEdits.GetPlace(1, 0, uiOption));
	AssertString("false", cEdits.GetValue(1, 0, uiOption));
	AssertSize(3, cEdits.GetPlaces(0, 0, uiOption, aePlaces));
	AssertInt(BOP_Default, aePlaces[2]);
	AssertPlaceText("Default", &cEdits, 0, 0, "PreferSize", BOP_Default);
	AssertInt(BOP_Default, cEdits.GetPlace(0, 0, FindBuildOption("MathTables")));
	AssertString("true", cEdits.GetValue(0, 0, FindBuildOption("MathTables")));
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, FindBuildOption("Executable")));
	AssertString("program.exe", cEdits.GetValue(0, 0, FindBuildOption("Executable")));

	//The sheet's, for one configuration.
	uiOption = FindBuildOption("Debug");
	AssertInt(BOP_Sheet, cEdits.GetPlace(1, 0, uiOption));
	AssertString("true", cEdits.GetValue(1, 0, uiOption));
	AssertPlaceText("Shared.props, 2 projects, Debug only", &cEdits, 1, 0, "Debug", BOP_Sheet);
	AssertInt(BOP_Default, cEdits.GetPlace(0, 0, uiOption));
	AssertString("false", cEdits.GetValue(0, 0, uiOption));

	//The library: the sheet's, and of its own code what the program has; an option of the whole program is no library's.
	AssertInt(BOP_Sheet, cEdits.GetPlace(0, 1, FindBuildOption("WarnStringToPointer")));
	AssertString("true", cEdits.GetValue(0, 1, FindBuildOption("WarnStringToPointer")));
	uiOption = FindBuildOption("WarnMissingOverride");
	AssertInt(BOP_Program, cEdits.GetPlace(0, 1, uiOption));
	AssertString("true", cEdits.GetValue(0, 1, uiOption));
	AssertSize(3, cEdits.GetPlaces(0, 1, uiOption, aePlaces));
	AssertInt(BOP_Program, aePlaces[2]);
	AssertPlaceText("The program's (Program)", &cEdits, 0, 1, "WarnMissingOverride", BOP_Program);
	AssertInt(BOP_Sheet, cEdits.GetPlace(1, 1, FindBuildOption("Debug")));
	AssertInt(BOP_Default, cEdits.GetPlace(0, 1, FindBuildOption("MathTables")));
	AssertFalse(cEdits.IsEdited());

	cEdits.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Options edited and written: a value changed where it is from (the
// project's line, the sheet's, which is the other project's too), one made
// the platform's from one configuration's and one configuration's from the
// default, and one made the project's own from the program's.  What an
// edit changes for the other configuration shows there before anything is
// written.  The files are as they were but for those lines
// (Input/Options/Expected).
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEditsApply(void)
{
	CVcxSolution		cSolution;
	CBuildOptionEdits	cEdits;
	CChars				szNote;
	size				uiOption;

	CopyOptionsSolution();
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	cEdits.Add("Release|HCP65C816", "Release|HCP65C816");
	cEdits.Add("Debug|HCP65C816", "Debug|HCP65C816");
	cEdits.Add("Debug|Win32", "Debug|Win32");
	cEdits.Add("Release|Win32", "Release|Win32");

	//The platform's line is Debug's too.
	uiOption = FindBuildOption("NoFloats");
	cEdits.SetValue(0, 0, uiOption, "false");
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiOption));
	AssertString("false", cEdits.GetValue(1, 0, uiOption));
	AssertTrue(cEdits.IsEdited());

	//Release's own made the platform's: Debug has it then.
	uiOption = FindBuildOption("PreferSize");
	cEdits.SetPlace(0, 0, uiOption, BOP_Platform);
	AssertString("true", cEdits.GetValue(0, 0, uiOption));
	AssertInt(BOP_Platform, cEdits.GetPlace(1, 0, uiOption));
	AssertString("true", cEdits.GetValue(1, 0, uiOption));

	//The default made Release's own, which Debug hasn't.
	uiOption = FindBuildOption("MathTables");
	cEdits.SetPlace(0, 0, uiOption, BOP_Configuration);
	AssertString("true", cEdits.GetValue(0, 0, uiOption));
	cEdits.SetValue(0, 0, uiOption, "false");
	AssertInt(BOP_Configuration, cEdits.GetPlace(0, 0, uiOption));
	AssertInt(BOP_Default, cEdits.GetPlace(1, 0, uiOption));
	AssertString("true", cEdits.GetValue(1, 0, uiOption));

	//The sheet's changed from the library is the program's too, in both configurations.
	uiOption = FindBuildOption("WarnStringToPointer");
	cEdits.SetValue(0, 1, uiOption, "false");
	AssertInt(BOP_Sheet, cEdits.GetPlace(0, 1, uiOption));
	AssertString("false", cEdits.GetValue(0, 0, uiOption));
	AssertString("false", cEdits.GetValue(1, 0, uiOption));
	AssertString("false", cEdits.GetValue(1, 1, uiOption));

	//The program's is no line to change: the library's value is then its own, for the platform.
	uiOption = FindBuildOption("WarnMissingOverride");
	cEdits.SetValue(0, 1, uiOption, "false");
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 1, uiOption));
	AssertString("true", cEdits.GetValue(0, 0, uiOption));
	AssertString("false", cEdits.GetValue(1, 1, uiOption));

	//Made its own and given back to the sheet: nothing is written for it.
	uiOption = FindBuildOption("Debug");
	cEdits.SetPlace(1, 0, uiOption, BOP_Configuration);
	AssertString("true", cEdits.GetValue(1, 0, uiOption));
	cEdits.SetValue(1, 0, uiOption, "false");
	cEdits.SetPlace(1, 0, uiOption, BOP_Sheet);
	AssertString("true", cEdits.GetValue(1, 0, uiOption));

	//Every project is built as its configuration of the solution configuration's name: none is given another.
	szNote.Init();
	AssertFalse(cEdits.NeedsOwnConfiguration(0, 0));
	cEdits.GetNote(0, &szNote);
	AssertString("", szNote.Text());
	AssertTrue(cEdits.Check(0, &szNote));
	AssertTrue(cEdits.Apply(TEST_OPTIONS_SOLUTION, &szNote));
	AssertString("", szNote.Text());
	szNote.Kill();
	AssertFile(TEST_OPTIONS_EXPECTED "Program.vcxproj", TEST_OPTIONS_PROGRAM);
	AssertFile(TEST_OPTIONS_EXPECTED "Library.vcxproj", TEST_OPTIONS_LIBRARY);
	AssertFile(TEST_OPTIONS_EXPECTED "Shared.props", TEST_OPTIONS_SHEET);
	AssertFile(TEST_OPTIONS_BEFORE _FS_ "Program.sln", TEST_OPTIONS_SOLUTION);
	cEdits.Kill();
	cSolution.Kill();

	//Read again, each is where it was put.
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	cEdits.Add("Release|HCP65C816", "Release|HCP65C816");
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, FindBuildOption("PreferSize")));
	AssertInt(BOP_Configuration, cEdits.GetPlace(0, 0, FindBuildOption("MathTables")));
	AssertString("false", cEdits.GetValue(0, 0, FindBuildOption("MathTables")));
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 1, FindBuildOption("WarnMissingOverride")));
	AssertString("false", cEdits.GetValue(0, 1, FindBuildOption("WarnStringToPointer")));
	AssertFalse(cEdits.IsEdited());
	cEdits.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A solution configuration that builds a project as a configuration of
// another name (a new one, a copy of Release's rows): an edit that is the
// one configuration's gives the project a configuration of that name, a
// copy of the one it was built as, and the solution's row names it, as
// the note says before and after.  One that is the platform's doesn't.
// Release is left as it was.  The linking is checked as a build reads it.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEditsOwnConfiguration(void)
{
	CVcxSolution		cSolution;
	CBuildOptionEdits	cEdits;
	CVcxProject			cProject;
	CChars				szNote;
	size				uiOption;

	CopyOptionsSolution();
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	cEdits.Add("Fast|HCP65C816", "Release|HCP65C816");
	cEdits.Add("Release|HCP65C816", "Release|HCP65C816");
	cEdits.Add("Debug|HCP65C816", "Debug|HCP65C816");
	cEdits.Add("Debug|Win32", "Debug|Win32");
	cEdits.Add("Release|Win32", "Release|Win32");
	AssertString("Release", cEdits.GetProject(0, 0)->mszConfiguration.Text());
	szNote.Init();

	//The platform's: no configuration is needed for that.
	cEdits.SetPlace(0, 1, FindBuildOption("WarnStringToPointer"), BOP_Platform);
	AssertFalse(cEdits.NeedsOwnConfiguration(0, 1));
	cEdits.GetNote(0, &szNote);
	AssertString("", szNote.Text());

	//Release's own line changed for Fast.
	uiOption = FindBuildOption("PreferSize");
	AssertInt(BOP_Configuration, cEdits.GetPlace(0, 0, uiOption));
	AssertPlaceText("This project, Release only, which Fast is built as", &cEdits, 0, 0, "PreferSize", BOP_Configuration);
	AssertPlaceText("This project, Fast only", &cEdits, 0, 0, "MathTables", BOP_Configuration);
	cEdits.SetValue(0, 0, uiOption, "false");
	AssertPlaceText("This project, Fast only", &cEdits, 0, 0, "PreferSize", BOP_Configuration);
	AssertTrue(cEdits.NeedsOwnConfiguration(0, 0));
	cEdits.GetNote(0, &szNote);
	AssertString("OK gives Program a Fast configuration of its own, a copy of its Release, and the solution's Fast|HCP65C816 builds it as that.", szNote.Text());

	//Two files named to link is refused, in the build's words.
	cEdits.SetValue(0, 0, FindBuildOption("Module"), "program.drv");
	AssertFalse(cEdits.Check(0, &szNote));
	AssertString("HCP65Link names more than one of Executable, Module and Image.", szNote.Text());
	cEdits.SetValue(0, 0, FindBuildOption("Module"), "");
	AssertTrue(cEdits.Check(0, &szNote));

	//The solution is given the new configuration's rows, as Release's, and then the program's names its own.
	szNote.Set("");
	AssertTrue(cEdits.Apply(TEST_OPTIONS_SOLUTION, &szNote));
	AssertString("Program has a Fast configuration of its own now, a copy of its Release, which Fast|HCP65C816 builds it as.", szNote.Text());
	szNote.Kill();
	cEdits.Kill();
	cSolution.Kill();
	AssertFile(TEST_OPTIONS_ROWS, TEST_OPTIONS_SOLUTION);

	cProject.Init();
	AssertTrue(cProject.Read(TEST_OPTIONS_PROGRAM));
	AssertString("false", cProject.FindConfiguration("Fast|HCP65C816")->GetProperty("HCP65Compile.PreferSize"));
	AssertString("true", cProject.FindConfiguration("Release|HCP65C816")->GetProperty("HCP65Compile.PreferSize"));
	AssertString("true", cProject.FindConfiguration("Fast|HCP65C816")->GetProperty("HCP65Compile.NoFloats"));
	AssertNull(cProject.FindConfiguration("Fast|HCP65C816")->GetProperty("HCP65Link.Module"));
	AssertNotNull(cProject.FindConfiguration("Fast|Win32"));
	cProject.Kill();
	cProject.Init();
	AssertTrue(cProject.Read(TEST_OPTIONS_LIBRARY));
	AssertNull(cProject.FindConfiguration("Fast|Win32"));
	AssertString("true", cProject.FindConfiguration("Release|HCP65C816")->GetProperty("HCP65Compile.WarnStringToPointer"));
	cProject.Kill();

	//Read again, Fast is built as the program's Fast, where its line is.
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	cEdits.Add("Fast|HCP65C816", "Fast|HCP65C816");
	AssertString("Fast", cEdits.GetProject(0, 0)->mszConfiguration.Text());
	AssertInt(BOP_Configuration, cEdits.GetPlace(0, 0, uiOption));
	AssertString("false", cEdits.GetValue(0, 0, uiOption));
	AssertString("Release", cEdits.GetProject(0, 1)->mszConfiguration.Text());
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 1, FindBuildOption("WarnStringToPointer")));
	cEdits.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The solution's configurations as the edits have them: two that swap
// names each keep the rows they were read from, one is removed and its
// name taken by a copy of another, and the projects aren't touched.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEditsConfigurations(void)
{
	CVcxSolution		cSolution;
	CBuildOptionEdits	cEdits;
	CChars				szNote;

	CopyOptionsSolution();
	//So the two HCP65 configurations' rows can be told apart: the program isn't built in Debug.
	AssertTrue(SetVcxSolutionProjectConfiguration(TEST_OPTIONS_SOLUTION, "{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}", "Debug|HCP65C816", "Other|Win32"));
	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	cEdits.Add("Release|HCP65C816", "Debug|HCP65C816");		//Swapped...
	cEdits.Add("Debug|HCP65C816", "Release|HCP65C816");		//...with this.
	cEdits.Add("Release|Win32", "Debug|Win32");				//Release|Win32 removed, and Debug|Win32 copied under its name.
	cEdits.Add("Debug|Win32", "Debug|Win32");
	szNote.Init();
	AssertTrue(cEdits.Apply(TEST_OPTIONS_SOLUTION, &szNote));
	AssertString("", szNote.Text());
	szNote.Kill();
	cEdits.Kill();
	cSolution.Kill();

	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	AssertSize(4, cSolution.NumConfigurations());
	AssertString("Debug|HCP65C816", cSolution.GetConfiguration(0));
	AssertString("Debug|Win32", cSolution.GetConfiguration(1));
	AssertString("Release|HCP65C816", cSolution.GetConfiguration(2));
	AssertString("Release|Win32", cSolution.GetConfiguration(3));
	AssertString("Other|Win32", cSolution.FindProjectByName("Library")->FindConfiguration("Release|HCP65C816")->GetProjectConfiguration());
	AssertString("Release|Win32", cSolution.FindProjectByName("Library")->FindConfiguration("Debug|HCP65C816")->GetProjectConfiguration());
	AssertString("Debug|Win32", cSolution.FindProjectByName("Program")->FindConfiguration("Release|Win32")->GetProjectConfiguration());
	AssertFalse(cSolution.FindProjectByName("Program")->FindConfiguration("Release|Win32")->IsBuild());
	AssertTrue(cSolution.FindProjectByName("Library")->FindConfiguration("Release|Win32")->IsBuild());
	cSolution.Kill();
	AssertFile(TEST_OPTIONS_BEFORE _FS_ "Program.vcxproj", TEST_OPTIONS_PROGRAM);
	AssertFile(TEST_OPTIONS_BEFORE _FS_ "Library.vcxproj", TEST_OPTIONS_LIBRARY);
}


static void AssertEffective(const char* szExpected, CBuildOptionEdits* pcEdits, size uiConfiguration, size uiProject, const char* szOption)
{
	CChars	szText;

	szText.Init();
	pcEdits->GetEffective(uiConfiguration, uiProject, FindBuildOption(szOption, geTestOptionKind), &szText);
	AssertString(szExpected, szText.Text());
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A GCC configuration has g++'s options (ConfigurationsPlan.txt C9, stage
// 1), whose lines add up: each place has its own, a sheet may have more
// than one, and the option comes to all of them.  The line shown is one
// place's; choosing another place shows that one's and moves nothing.  A
// line of the project's own is written naming the setting itself, so it
// adds; an empty one is taken away; a sheet's changes for every project
// that has it.  An MSVC configuration has no options here.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEditsGCC(void)
{
	CVcxSolution		cSolution;
	CBuildOptionEdits	cEdits;
	CFileUtil			cFileUtil;
	CChars				szNote;
	EBuildOptionPlace	aePlaces[BOP_NumPlaces];
	EBuildOptionKind	eKind;
	size				uiDefines;
	size				uiOptions;
	size				uiLibraries;
	size				uiPass;

	cFileUtil.RemoveDir(TEST_OPTIONS_DIRECTORY);
	AssertTrue(cFileUtil.TouchDir(TEST_OPTIONS_DIRECTORY));
	AssertTrue(cFileUtil.CopyDir(TEST_OPTIONS_GCC, TEST_OPTIONS_DIRECTORY));

	AssertTrue(GetBuildOptionKind("GCC-x64", &eKind));
	AssertInt(BOK_GCC, eKind);
	AssertTrue(GetBuildOptionKind("HCP65C816", &eKind));
	AssertInt(BOK_HCP65, eKind);
	AssertTrue(GetBuildOptionKind("x64", &eKind));
	AssertInt(BOK_MSVC, eKind);
	AssertFalse(GetBuildOptionKind("", &eKind));
	geTestOptionKind = BOK_GCC;
	uiDefines = FindBuildOption("ClCompile.PreprocessorDefinitions", BOK_GCC);
	uiOptions = FindBuildOption("ClCompile.AdditionalOptions", BOK_GCC);
	uiLibraries = FindBuildOption("Link.AdditionalDependencies", BOK_GCC);
	AssertTrue((uiDefines != MAX_SIZE) && (uiOptions != MAX_SIZE) && (uiLibraries != MAX_SIZE));
	AssertTrue(uiOptions != FindBuildOption("Link.AdditionalOptions", BOK_GCC));
	AssertTrue(uiDefines != FindBuildOption("ClCompile.PreprocessorDefinitions", BOK_MSVC));
	AssertTrue(uiDefines != FindBuildOption("ClCompile.PreprocessorDefinitions", BOK_HCP65));
	AssertSize(MAX_SIZE, FindBuildOption("ClCompile.NoSuch", BOK_GCC));
	AssertSize(MAX_SIZE, FindBuildOption("NoFloats", BOK_GCC));

	//Read, then read again after the edits are written: uiPass 1 has what uiPass 0 left.
	for (uiPass = 0; uiPass < 2; uiPass++)
	{
		cSolution.Init();
		AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
		cEdits.Init(&cSolution, "Program");
		cEdits.Add("Debug|GCC-x64", "Debug|GCC-x64");
		cEdits.Add("Release|GCC-x64", "Release|GCC-x64");
		cEdits.Add("Debug|x64", "Debug|x64");
		AssertSize(2, cEdits.NumProjects(0));
		AssertSize(2, cEdits.NumProjects(2));
		AssertSize(0, cEdits.GetStartupProject(0));

		if (uiPass == 0)
		{
			//The library's defines: its own line for Debug, and the sheet's two, the last read first.
			AssertInt(BOP_Configuration, cEdits.GetPlace(0, 1, uiDefines));
			AssertString("LIB_DEBUG;%(PreprocessorDefinitions)", cEdits.GetValue(0, 1, uiDefines));
			AssertSize(4, cEdits.GetPlaces(0, 1, uiDefines, aePlaces));
			AssertInt(BOP_Configuration, aePlaces[0]);
			AssertInt(BOP_Platform, aePlaces[1]);
			AssertInt(BOP_Sheet, aePlaces[2]);
			AssertInt(BOP_Sheet2, aePlaces[3]);
			AssertPlaceText("This project, Debug only", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions", BOP_Configuration);
			AssertPlaceText("Shared.props, 2 projects, every GCC-x64 configuration", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions", BOP_Sheet);
			AssertPlaceText("Shared.props, 2 projects, Debug only", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions", BOP_Sheet2);
			AssertEffective("LIB_DEBUG;LINUX_GNU;_DEBUG", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions");
			AssertEffective("LINUX_GNU", &cEdits, 1, 1, "ClCompile.PreprocessorDefinitions");

			//The program's: the sheet's alone, so its own line for the platform shows, empty; the sheet's is Debug's,
			//the last read.  And what it is linked with, its own added.
			AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiOptions));
			AssertString("", cEdits.GetValue(0, 0, uiOptions));
			cEdits.SetPlace(0, 0, uiOptions, BOP_Sheet);
			AssertString("%(AdditionalOptions) -g", cEdits.GetValue(0, 0, uiOptions));
			AssertPlaceText("Shared.props, 2 projects, Debug only", &cEdits, 0, 0, "ClCompile.AdditionalOptions", BOP_Sheet);
			AssertEffective("-m64 -fno-exceptions -g", &cEdits, 0, 0, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions", &cEdits, 1, 0, "ClCompile.AdditionalOptions");
			AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiLibraries));
			AssertEffective("m;pthread;dl", &cEdits, 0, 0, "Link.AdditionalDependencies");
			AssertEffective("-m64", &cEdits, 0, 0, "Link.AdditionalOptions");
			AssertFalse(cEdits.IsEdited());

			//Looking at another place moves nothing and is no edit.
			cEdits.SetPlace(0, 1, uiDefines, BOP_Sheet);
			AssertString("LINUX_GNU;%(PreprocessorDefinitions)", cEdits.GetValue(0, 1, uiDefines));
			AssertEffective("LIB_DEBUG;LINUX_GNU;_DEBUG", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions");
			cEdits.SetPlace(0, 1, uiDefines, BOP_Platform);
			AssertString("", cEdits.GetValue(0, 1, uiDefines));
			AssertFalse(cEdits.IsEdited());

			//The library's own line for Debug changed: it is kept as typed, and still adds, as it is written to.
			cEdits.SetPlace(0, 1, uiDefines, BOP_Configuration);
			cEdits.SetValue(0, 1, uiDefines, "LIB_DEBUG;EXTRA");
			AssertString("LIB_DEBUG;EXTRA", cEdits.GetValue(0, 1, uiDefines));
			AssertEffective("LIB_DEBUG;EXTRA;LINUX_GNU;_DEBUG", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions");
			AssertEffective("LINUX_GNU", &cEdits, 1, 1, "ClCompile.PreprocessorDefinitions");

			//The program given an option of its own for the platform: both its configurations have it.
			cEdits.SetPlace(0, 0, uiOptions, BOP_Platform);
			cEdits.SetValue(0, 0, uiOptions, "-Wshadow");
			AssertString("-Wshadow", cEdits.GetValue(0, 0, uiOptions));
			AssertEffective("-m64 -fno-exceptions -g -Wshadow", &cEdits, 0, 0, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions -Wshadow", &cEdits, 1, 0, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions -g", &cEdits, 0, 1, "ClCompile.AdditionalOptions");

			//The sheet's line for the platform changed from the library: the program has it too.
			cEdits.SetPlace(0, 1, uiOptions, BOP_Sheet2);
			AssertString("-m64 -fno-exceptions %(AdditionalOptions)", cEdits.GetValue(0, 1, uiOptions));
			cEdits.SetValue(0, 1, uiOptions, "-m64 -fno-exceptions -Wall %(AdditionalOptions)");
			AssertEffective("-m64 -fno-exceptions -Wall -g", &cEdits, 0, 1, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions -Wall -g -Wshadow", &cEdits, 0, 0, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions -Wall", &cEdits, 1, 1, "ClCompile.AdditionalOptions");

			//The program's own libraries emptied: the line goes, and the sheet's are what it links.
			cEdits.SetValue(0, 0, uiLibraries, "");
			AssertEffective("pthread;dl", &cEdits, 0, 0, "Link.AdditionalDependencies");
			AssertEffective("pthread;dl", &cEdits, 1, 0, "Link.AdditionalDependencies");

			AssertTrue(cEdits.IsEdited());
			AssertFalse(cEdits.NeedsOwnConfiguration(0, 0));
			AssertFalse(cEdits.NeedsOwnConfiguration(0, 1));
			szNote.Init();
			AssertTrue(cEdits.Check(0, &szNote));
			AssertTrue(cEdits.Apply(TEST_OPTIONS_SOLUTION, &szNote));
			AssertString("", szNote.Text());
			szNote.Kill();
		}
		else
		{
			AssertString("LIB_DEBUG;EXTRA;%(PreprocessorDefinitions)", cEdits.GetValue(0, 1, uiDefines));
			AssertEffective("LIB_DEBUG;EXTRA;LINUX_GNU;_DEBUG", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions");
			AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiOptions));
			AssertString("%(AdditionalOptions) -Wshadow", cEdits.GetValue(0, 0, uiOptions));
			AssertEffective("-m64 -fno-exceptions -Wall -g -Wshadow", &cEdits, 0, 0, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions -Wall -Wshadow", &cEdits, 1, 0, "ClCompile.AdditionalOptions");
			AssertEffective("-m64 -fno-exceptions -Wall", &cEdits, 1, 1, "ClCompile.AdditionalOptions");
			AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiLibraries));
			AssertString("", cEdits.GetValue(0, 0, uiLibraries));
			AssertEffective("pthread;dl", &cEdits, 0, 0, "Link.AdditionalDependencies");
			AssertEffective("-m64", &cEdits, 0, 0, "Link.AdditionalOptions");
			AssertFalse(cEdits.IsEdited());
		}
		cEdits.Kill();
		cSolution.Kill();
	}
	geTestOptionKind = BOK_HCP65;
}


//////////////////////////////////////////////////////////////////////////
//
// An MSVC configuration has the settings the projects and sheets set
// (ConfigurationsPlan.txt C9, stage 2).  One with values to choose from
// has one value, the most particular place's, as the HCP65 options have:
// changed it is written where it is from, made the project's own it is
// the project's, and given no value its line is taken away.  Its lists add
// up as g++'s do.  Nothing of g++'s is among them.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEditsMSVC(void)
{
	CVcxSolution		cSolution;
	CBuildOptionEdits	cEdits;
	CVcxProject			cProject;
	CFileUtil			cFileUtil;
	CChars				szNote;
	size				uiOptimisation;
	size				uiRuntime;
	size				uiSubSystem;
	size				uiDefines;

	cFileUtil.RemoveDir(TEST_OPTIONS_DIRECTORY);
	AssertTrue(cFileUtil.TouchDir(TEST_OPTIONS_DIRECTORY));
	AssertTrue(cFileUtil.CopyDir(TEST_OPTIONS_GCC, TEST_OPTIONS_DIRECTORY));
	geTestOptionKind = BOK_MSVC;
	uiOptimisation = FindBuildOption("ClCompile.Optimization", BOK_MSVC);
	uiRuntime = FindBuildOption("ClCompile.RuntimeLibrary", BOK_MSVC);
	uiSubSystem = FindBuildOption("Link.SubSystem", BOK_MSVC);
	uiDefines = FindBuildOption("ClCompile.PreprocessorDefinitions", BOK_MSVC);
	AssertTrue((uiOptimisation != MAX_SIZE) && (uiRuntime != MAX_SIZE) && (uiSubSystem != MAX_SIZE) && (uiDefines != MAX_SIZE));
	AssertString("Disabled;MinSpace;MaxSpeed;Full", GetBuildOption(uiOptimisation)->szChoices);
	AssertString("Optimisation", GetBuildOption(uiOptimisation)->szSection);
	AssertNull(GetBuildOption(uiDefines)->szChoices);

	cSolution.Init();
	AssertTrue(cSolution.Read(TEST_OPTIONS_SOLUTION));
	cEdits.Init(&cSolution, "Program");
	cEdits.Add("Release|x64", "Release|x64");
	cEdits.Add("Debug|x64", "Debug|x64");
	AssertSize(2, cEdits.NumProjects(0));

	//The sheet's, by each group's condition; the program's own; and one that nothing says.
	AssertString("MaxSpeed", cEdits.GetValue(0, 0, uiOptimisation));
	AssertInt(BOP_Sheet, cEdits.GetPlace(0, 0, uiOptimisation));
	AssertPlaceText("Shared.props, 2 projects, every configuration but Debug", &cEdits, 0, 0, "ClCompile.Optimization", BOP_Sheet);
	AssertString("Disabled", cEdits.GetValue(1, 0, uiOptimisation));
	AssertPlaceText("Shared.props, 2 projects, Debug only", &cEdits, 1, 0, "ClCompile.Optimization", BOP_Sheet);
	AssertString("Level3", cEdits.GetValue(0, 1, FindBuildOption("ClCompile.WarningLevel", BOK_MSVC)));
	AssertPlaceText("Shared.props, 2 projects, every configuration", &cEdits, 0, 1, "ClCompile.WarningLevel", BOP_Sheet);
	AssertString("Console", cEdits.GetValue(0, 0, uiSubSystem));
	AssertInt(BOP_Configuration, cEdits.GetPlace(0, 0, uiSubSystem));
	AssertString("", cEdits.GetValue(1, 0, uiSubSystem));
	AssertString("", cEdits.GetValue(0, 0, uiRuntime));
	AssertInt(BOP_Default, cEdits.GetPlace(0, 0, uiRuntime));

	//Its defines add up, and g++'s lines aren't among them.
	AssertString("PROGRAM;%(PreprocessorDefinitions)", cEdits.GetValue(0, 0, uiDefines));
	AssertEffective("PROGRAM", &cEdits, 0, 0, "ClCompile.PreprocessorDefinitions");
	AssertEffective("_DEBUG", &cEdits, 1, 1, "ClCompile.PreprocessorDefinitions");
	AssertEffective("", &cEdits, 0, 1, "ClCompile.PreprocessorDefinitions");

	//The optimisation made the program's own for Release; a runtime library chosen, which is its own for the platform;
	//and the subsystem given no value.
	cEdits.SetPlace(0, 0, uiOptimisation, BOP_Configuration);
	cEdits.SetValue(0, 0, uiOptimisation, "Full");
	cEdits.SetValue(0, 0, uiRuntime, "MultiThreadedDLL");
	AssertInt(BOP_Platform, cEdits.GetPlace(0, 0, uiRuntime));
	AssertString("MultiThreadedDLL", cEdits.GetValue(1, 0, uiRuntime));
	cEdits.SetValue(0, 0, uiSubSystem, "");
	AssertString("MaxSpeed", cEdits.GetValue(0, 1, uiOptimisation));
	szNote.Init();
	AssertTrue(cEdits.Check(0, &szNote));
	AssertTrue(cEdits.Apply(TEST_OPTIONS_SOLUTION, &szNote));
	AssertString("", szNote.Text());
	szNote.Kill();
	cEdits.Kill();
	cSolution.Kill();

	cProject.Init();
	AssertTrue(cProject.Read(TEST_OPTIONS_PROGRAM));
	AssertString("Full", cProject.FindConfiguration("Release|x64")->GetProperty("ClCompile.Optimization"));
	AssertString("Disabled", cProject.FindConfiguration("Debug|x64")->GetProperty("ClCompile.Optimization"));
	AssertString("MultiThreadedDLL", cProject.FindConfiguration("Release|x64")->GetProperty("ClCompile.RuntimeLibrary"));
	AssertString("MultiThreadedDLL", cProject.FindConfiguration("Debug|x64")->GetProperty("ClCompile.RuntimeLibrary"));
	AssertNull(cProject.FindConfiguration("Release|x64")->GetProperty("Link.SubSystem"));
	cProject.Kill();
	cProject.Init();
	AssertTrue(cProject.Read(TEST_OPTIONS_LIBRARY));
	AssertString("MaxSpeed", cProject.FindConfiguration("Release|x64")->GetProperty("ClCompile.Optimization"));
	AssertNull(cProject.FindConfiguration("Release|x64")->GetProperty("ClCompile.RuntimeLibrary"));
	cProject.Kill();
	geTestOptionKind = BOK_HCP65;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEdits(void)
{
	BeginTests();

	TestBuildOptionEditsRead();
	TestBuildOptionEditsApply();
	TestBuildOptionEditsOwnConfiguration();
	TestBuildOptionEditsConfigurations();
	TestBuildOptionEditsGCC();
	TestBuildOptionEditsMSVC();

	TestStatistics();
}
