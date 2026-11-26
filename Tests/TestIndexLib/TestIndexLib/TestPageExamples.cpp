#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildLib/BuildWorkspace.h"
#include "IndexLib/WorkspaceIndex.h"
#include "IndexLib/PageExamples.h"
#include "TestLib/Assert.h"


#define TEST_EXAMPLES_SOLUTION	"Input" _FS_ "Examples" _FS_ "Examples.sln"
#define TEST_EXAMPLES_PAGE		"Input" _FS_ "Examples" _FS_ "Docs" _FS_ "Examples.md"


//////////////////////////////////////////////////////////////////////////
//
// Which fences are checked: cpp, and {.cpp} without .no-check.
//
//////////////////////////////////////////////////////////////////////////
void TestPageExamplesFences(void)
{
	AssertTrue(CPageExamples::IsCheckedFence("cpp", 3));
	AssertTrue(CPageExamples::IsCheckedFence("cpp title=\"a\"", 13));
	AssertTrue(CPageExamples::IsCheckedFence("{.cpp}", 6));
	AssertTrue(CPageExamples::IsCheckedFence(" { .cpp .numbers }", 18));
	AssertFalse(CPageExamples::IsCheckedFence("{.cpp .no-check}", 16));
	AssertFalse(CPageExamples::IsCheckedFence("c++", 3));
	AssertFalse(CPageExamples::IsCheckedFence("cppx", 4));
	AssertFalse(CPageExamples::IsCheckedFence("text", 4));
	AssertFalse(CPageExamples::IsCheckedFence("", 0));
}


//////////////////////////////////////////////////////////////////////////
//
// Which blocks are compiled at file scope: those that start with a word
// that declares, or a function written out; else they are statements.
//
//////////////////////////////////////////////////////////////////////////
void TestPageExamplesDeclaring(void)
{
	AssertTrue(CPageExamples::IsDeclaring("#include <stdint.h>\n"));
	AssertTrue(CPageExamples::IsDeclaring("int Sum(int i)\n{\n\treturn i;\n}\n"));
	AssertTrue(CPageExamples::IsDeclaring("// Before\n\nclass CTank\n{\n};\n"));
	AssertTrue(CPageExamples::IsDeclaring("template<class M>\nclass CTank;\n"));
	AssertFalse(CPageExamples::IsDeclaring("CTank\tcTank;\n\ncTank.Init();\n"));
	AssertFalse(CPageExamples::IsDeclaring("classes = 1;\n"));
	AssertFalse(CPageExamples::IsDeclaring("\tclass CTank;\n"));
	AssertFalse(CPageExamples::IsDeclaring(""));
	AssertTrue(CPageExamples::IsIncluded("--8<-- \"Test.cpp:example\"\n\n--8<-- \"Test.cpp:kill\"\n"));
	AssertFalse(CPageExamples::IsIncluded("--8<-- \"Test.cpp:example\"\nx = 1;\n"));
	AssertFalse(CPageExamples::IsIncluded("\n"));
}


//////////////////////////////////////////////////////////////////////////
//
// The fixture page's hidden comment and its four ```cpp blocks (not its
// no-check, text or snippet blocks), and where each unit's lines are in
// the page.
//
//////////////////////////////////////////////////////////////////////////
void TestPageExamplesScan(void)
{
	CTextFile		cPage;
	CPageExamples	cExamples;
	CChars			szUnit;

	cPage.Init();
	AssertTrue(cPage.Read(TEST_EXAMPLES_PAGE));
	cExamples.Init();
	cExamples.Scan(cPage.Text());

	AssertTrue(cExamples.HasHidden());
	AssertString("#include \"../Shapes.h\"\nCShape\tcShape;\n", cExamples.GetHidden());
	AssertSize(4, cExamples.NumExamples());
	AssertInt(9, cExamples.Get(0)->iFence);
	AssertInt(2, cExamples.Get(0)->iLines);
	AssertFalse(cExamples.Get(0)->bDeclares);
	AssertInt(16, cExamples.Get(1)->iFence);
	AssertTrue(cExamples.Get(1)->bDeclares);
	AssertInt(25, cExamples.Get(2)->iFence);
	AssertTrue(cExamples.Get(2)->bDeclares);
	AssertInt(43, cExamples.Get(3)->iFence);
	AssertFalse(cExamples.Get(3)->bDeclares);

	szUnit.Init();
	cExamples.MakeUnit(0, &szUnit);
	AssertString("#include \"../Shapes.h\"\nCShape\tcShape;\nvoid PageExample(void)\n{\ncShape.Init(EK_Square);\ncShape.GetKind();\n}\n", szUnit.Text());
	szUnit.Kill();

	AssertInt(3, cExamples.GetPageLine(0, 0));
	AssertInt(4, cExamples.GetPageLine(0, 1));
	AssertInt(9, cExamples.GetPageLine(0, 2));
	AssertInt(10, cExamples.GetPageLine(0, 4));
	AssertInt(11, cExamples.GetPageLine(0, 5));
	AssertInt(9, cExamples.GetPageLine(0, 6));
	AssertInt(17, cExamples.GetPageLine(1, 2));

	cExamples.Kill();
	cPage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The fixture page's examples compiled as units of its project: only the
// last has a mistake, a member function CShape hasn't, at its page line.
//
//////////////////////////////////////////////////////////////////////////
void TestPageExamplesCheck(void)
{
	CBuildWorkspace				cWorkspace;
	CWorkspaceIndex				cIndex;
	CTextFile					cPage;
	CPageExamples				cExamples;
	CArrayPageExampleProblems	asProblems;
	CChars						szPage;
	CChars						szPrinted;
	CFileUtil					cFileUtil;
	size						uiProject;

	szPage.Init(TEST_EXAMPLES_PAGE);
	cFileUtil.FullPath(&szPage);
	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_EXAMPLES_SOLUTION));
	cIndex.Init(&cWorkspace);
	uiProject = cIndex.FindItemProject(szPage.Text());
	AssertSize(0, uiProject);
	AssertSize(MAX_SIZE, cIndex.FindItemProject("Nowhere.md"));

	cPage.Init();
	AssertTrue(cPage.Read(TEST_EXAMPLES_PAGE));
	cExamples.Init();
	cExamples.Scan(cPage.Text());
	asProblems.Init();
	AssertTrue(cExamples.Check(&cIndex, uiProject, szPage.Text(), &asProblems));
	AssertSize(1, asProblems.NumElements());
	AssertInt(45, asProblems.Get(0)->iLine);
	AssertString("The class has no member function of that name [cShape]", asProblems.Get(0)->szMessage.Text());

	szPrinted.Init();
	CPageExamples::PrintProblems("Examples.md", &asProblems, &szPrinted);
	AssertString("Examples.md:46:1: error: The class has no member function of that name [cShape]\n", szPrinted.Text());
	szPrinted.Kill();

	CPageExamples::KillProblems(&asProblems);
	cExamples.Kill();
	cPage.Kill();
	cIndex.Kill();
	cWorkspace.Kill();
	szPage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void TestPageExamples(void)
{
	BeginTests();

	TestPageExamplesFences();
	TestPageExamplesDeclaring();
	TestPageExamplesScan();
	TestPageExamplesCheck();

	TestStatistics();
}

