#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildLib/BuildWorkspace.h"
#include "IndexLib/WorkspaceIndex.h"
#include "IndexLib/SymbolTable.h"
#include "TestLib/Assert.h"


#define TEST_SYMBOL_TABLE_SAMPLE		"Input" _FS_ "Sample" _FS_ "Sample.sln"


//////////////////////////////////////////////////////////////////////////
//
// The sample solution's table, rooted at its folder: a type links to its
// definition, a member function to its declaration in the header (not its
// definition in Shape.cpp), a function only a .cpp file declares to there,
// and the header found through the include path by its folder.  Parameters
// aren't in it.
//
//////////////////////////////////////////////////////////////////////////
void TestSymbolTableSample(void)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CSymbolTable		cTable;
	CChars				szText;
	SSymbolTableEntry*	psEntry;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_SYMBOL_TABLE_SAMPLE));
	cIndex.Init(&cWorkspace);
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();

	cTable.Init("Input" _FS_ "Sample");
	cTable.Gather(cIndex.GetIndex());

	szText.Init();
	cTable.Print(&szText);
	AssertString(
		"CShape\tclass\tShape.h\t5\n"
		"CShape::GetSides()\tfunction\tShape.h\t12\n"
		"CShape::Init(int)\tfunction\tShape.h\t11\n"
		"CShape::miSides\tfield\tShape.h\t8\n"
		"CountSides(CShape*)\tfunction\tMain.cpp\t6\n"
		"Debugging()\tfunction\tMain.cpp\t47\n"
		"ProjectDefined()\tfunction\tMain.cpp\t20\n"
		"SExtra\tclass\tInclude/Extra.h\t6\n"
		"SExtra::iValue\tfield\tInclude/Extra.h\t8\n"
		"UseExtra(SExtra*)\tfunction\tMain.cpp\t12\n"
		"Wide()\tfunction\tMain.cpp\t29\n", szText.Text());
	szText.Kill();

	AssertSize(11, cTable.NumEntries());
	psEntry = cTable.Find("CShape::GetSides()");
	AssertNotNull(psEntry);
	AssertString("Shape.h", psEntry->szFile.Text());
	AssertInt(12, psEntry->iLine);
	AssertInt(CPPSK_Function, psEntry->eKind);
	AssertNull(cTable.Find("CShape::GetSides"));
	AssertNull(cTable.Find("CountSides(CShape*)::pcShape"));

	cTable.Kill();
	cIndex.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Rooted at a folder inside the solution's, only what is declared under
// it is kept; written, the file is the printed table.
//
//////////////////////////////////////////////////////////////////////////
void TestSymbolTableRoot(void)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CSymbolTable		cTable;
	CTextFile			cFile;
	CFileUtil			cFileUtil;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_SYMBOL_TABLE_SAMPLE));
	cIndex.Init(&cWorkspace);
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();

	cTable.Init("Input" _FS_ "Sample" _FS_ "Include" _FS_);
	cTable.Gather(cIndex.GetIndex());
	AssertSize(2, cTable.NumEntries());

	cFileUtil.MakeDir("Output");
	AssertTrue(cTable.Write("Output" _FS_ "Symbols.txt"));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "Symbols.txt"));
	AssertString(
		"SExtra\tclass\tExtra.h\t6\n"
		"SExtra::iValue\tfield\tExtra.h\t8\n", cFile.Text());
	cFile.Kill();

	cTable.Kill();
	cIndex.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestSymbolTable(void)
{
	BeginTests();

	TestSymbolTableSample();
	TestSymbolTableRoot();

	TestStatistics();
}

