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


static void AssertPlaceText(const char* szExpected, CBuildOptionEdits* pcEdits, size uiConfiguration, size uiProject, const char* szOption, EBuildOptionPlace ePlace)
{
	CChars	szText;

	szText.Init();
	pcEdits->GetPlaceText(uiConfiguration, uiProject, FindBuildOption(szOption), ePlace, &szText);
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
	AssertSize(0, cEdits.NumProjects(2));	//Not an HCP65 configuration: the options aren't its.
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
	cEdits.SetValue(0, 0, uiOption, "false");
	AssertTrue(cEdits.NeedsOwnConfiguration(0, 0));
	cEdits.GetNote(0, &szNote);
	AssertString("OK gives Program a Fast configuration of its own, a copy of its Release, and the solution's Fast|HCP65C816 builds it as that.", szNote.Text());

	//Two files named to link is refused, in the build's words.
	cEdits.SetValue(0, 0, FindBuildOption("Module"), "program.drv");
	AssertFalse(cEdits.Check(0, &szNote));
	AssertString("HCP65Link names more than one of Executable, Module and Image.", szNote.Text());
	cEdits.SetValue(0, 0, FindBuildOption("Module"), "");
	AssertTrue(cEdits.Check(0, &szNote));

	//The solution's own rows are the caller's to add, before the edits are written.
	szNote.Set("");
	AssertTrue(AddVcxSolutionConfiguration(TEST_OPTIONS_SOLUTION, "Fast|HCP65C816", "Release|HCP65C816"));
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
//
//////////////////////////////////////////////////////////////////////////
void TestBuildOptionEdits(void)
{
	BeginTests();

	TestBuildOptionEditsRead();
	TestBuildOptionEditsApply();
	TestBuildOptionEditsOwnConfiguration();

	TestStatistics();
}
