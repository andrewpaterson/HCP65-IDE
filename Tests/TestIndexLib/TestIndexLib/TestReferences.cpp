#include "BaseLib/TextFile.h"
#include "IndexLib/SymbolTable.h"
#include "IndexLib/ReferenceResolver.h"
#include "TestLib/Assert.h"


#define TEST_REFERENCES_FOLDER		"Input" _FS_ "References" _FS_


//////////////////////////////////////////////////////////////////////////
//
// The fixture page's references, links, errors and warnings, as
// hcp65index -resolve prints them for the website's build: good
// references (an overload with and without its parameters, an enumerator
// with and without its enum, files), broken ones, plain spans that could
// be references and ones that couldn't (a made-up name, code, a .cpp
// file's main, a register, another language, a file), link texts, an
// escaped bracket, a definition, a fence and an HTML comment.
//
//////////////////////////////////////////////////////////////////////////
void TestReferencesPage(void)
{
	CSymbolTable			cTable;
	CArrayChars				aszFiles;
	CReferenceResolver		cResolver;
	CTextFile				cPage;
	CArrayPageReferences	asReferences;
	CChars					szPrinted;

	cTable.Init(NULL);
	AssertTrue(cTable.Read(TEST_REFERENCES_FOLDER "Symbols.txt"));
	AssertSize(8, cTable.NumEntries());
	aszFiles.Init();
	AssertTrue(CReferenceResolver::ReadFiles(TEST_REFERENCES_FOLDER "Files.txt", &aszFiles));
	AssertSize(5, aszFiles.NumElements());
	cResolver.Init(&cTable, &aszFiles);

	cPage.Init();
	AssertTrue(cPage.Read(TEST_REFERENCES_FOLDER "Page.md"));
	asReferences.Init();
	cResolver.ResolvePage(cPage.Text(), &asReferences);
	szPrinted.Init();
	CReferenceResolver::PrintPageReferences("Page.md", &asReferences, &szPrinted);
	AssertString(
		"Page.md\t3\t14\tlink\t23\tLib/Shapes/Shape.h\t5\n"
		"Page.md\t3\t34\tlink\t49\tLib/Shapes/Shape.h\t11\n"
		"Page.md\t3\t54\tlink\t88\tLib/Shapes/Shape.h\t12\n"
		"Page.md\t3\t100\tlink\t121\tLib/Shapes/Shape.h\t13\n"
		"Page.md\t4\t13\tlink\t25\tLib/Shapes/Shape.h\t20\n"
		"Page.md\t4\t30\tlink\t49\tLib/Shapes/Shape.h\t20\n"
		"Page.md\t4\t63\terror\t[`Shapes/Shape.h`] names 2 files (Lib/Shapes/Shape.h, Other/Shapes/Shape.h): write more of its path\n"
		"Page.md\t4\t88\tlink\t109\tLib/Shapes/Shape.h\t0\n"
		"Page.md\t4\t124\tlink\t141\tLib/Notes Plan.txt\t0\n"
		"Page.md\t5\t9\terror\t[`CShape::Kill`] names nothing the indexed solutions declare\n"
		"Page.md\t5\t27\terror\t[`CShape::Init(float)`] names nothing the indexed solutions declare\n"
		"Page.md\t5\t52\terror\t[`Shape.h`] names 2 files (Lib/Shapes/Shape.h, Other/Shapes/Shape.h): write more of its path\n"
		"Page.md\t5\t68\terror\t[`Missing.h`] names no file in the repositories\n"
		"Page.md\t6\t8\twarning\t`CShape` names class CShape (Lib/Shapes/Shape.h:5): write [`CShape`] to link to it\n"
		"Page.md\t6\t18\twarning\t`CShape::Init` names function CShape::Init(int) (Lib/Shapes/Shape.h:11): write [`CShape::Init`] to link to it\n"
		"Page.md\t6\t82\twarning\t`abs` names function abs(int) (Lib/Shapes/Shape.h:31): write [`abs`] to link to it\n"
		"Page.md\t9\t47\tlink\t56\tLib/Shapes/Shape.h\t5\n"
		"Page.md\t16\t26\twarning\t`CShape::GetSides` names function CShape::GetSides() const (Lib/Shapes/Shape.h:13): write [`CShape::GetSides`] to link to it\n", szPrinted.Text());
	szPrinted.Kill();
	CReferenceResolver::KillPageReferences(&asReferences);
	cPage.Kill();

	cResolver.Kill();
	aszFiles.Kill();
	cTable.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What a name resolves to, and the pieces it is taken apart into.
//
//////////////////////////////////////////////////////////////////////////
void TestReferencesResolve(void)
{
	CSymbolTable		cTable;
	CReferenceResolver	cResolver;
	CArrayChars			aszParameters;
	CChars				szAfter;
	CChars				szName;
	CChars				szRest;

	cTable.Init(NULL);
	AssertTrue(cTable.Read(TEST_REFERENCES_FOLDER "Symbols.txt"));
	cResolver.Init(&cTable, NULL);

	AssertString("CShape::Init(int)", cResolver.ResolveSymbol("CShape::Init")->szKey.Text());
	AssertString("CShape::Init(int)", cResolver.ResolveSymbol("CShape::Init( int )")->szKey.Text());
	AssertString("CShape::Init(char const*, int)", cResolver.ResolveSymbol("CShape::Init(const char *, int)")->szKey.Text());
	AssertString("CShape::GetSides() const", cResolver.ResolveSymbol("CShape::GetSides(void)")->szKey.Text());
	AssertString("CShape::GetSides() const", cResolver.ResolveSymbol("CShape::GetSides() const")->szKey.Text());
	AssertNull(cResolver.ResolveSymbol("CShape::GetSides() volatile"));
	AssertString("EKind::EK_Square", cResolver.ResolveSymbol("EK_Square")->szKey.Text());
	AssertNull(cResolver.ResolveSymbol("Shape"));

	AssertTrue(CReferenceResolver::IsFileReference("BaseLib/Chars.h"));
	AssertTrue(CReferenceResolver::IsFileReference("web.sh"));
	AssertFalse(CReferenceResolver::IsFileReference("CShape::Init"));
	AssertFalse(CReferenceResolver::IsFileReference("CShape"));
	AssertFalse(CReferenceResolver::IsFileReference("x.1"));
	AssertTrue(CReferenceResolver::CouldBeReference("abs"));
	AssertFalse(CReferenceResolver::CouldBeReference("PC"));
	AssertFalse(CReferenceResolver::CouldBeReference("#!text abs"));

	szName.Init();
	szRest.Init();
	CReferenceResolver::SplitName("CFoo::operator()(int) const", &szName, &szRest);
	AssertString("CFoo::operator()", szName.Text());
	AssertString("(int) const", szRest.Text());
	CReferenceResolver::SplitName("CShape", &szName, &szRest);
	AssertString("CShape", szName.Text());
	AssertString("", szRest.Text());
	szRest.Kill();
	szName.Kill();

	aszParameters.Init();
	szAfter.Init();
	AssertTrue(CReferenceResolver::SplitParameters("(const char *, int)", &aszParameters, &szAfter));
	AssertSize(2, aszParameters.NumElements());
	AssertString("charconst*", aszParameters.GetText(0));
	AssertString("int", aszParameters.GetText(1));
	AssertString("", szAfter.Text());
	aszParameters.ReInit();
	AssertTrue(CReferenceResolver::SplitParameters("(CArrayTemplate<M, N>*) const", &aszParameters, &szAfter));
	AssertSize(1, aszParameters.NumElements());
	AssertString("CArrayTemplate<M,N>*", aszParameters.GetText(0));
	AssertString("const", szAfter.Text());
	aszParameters.ReInit();
	AssertTrue(CReferenceResolver::SplitParameters("(void)", &aszParameters, &szAfter));
	AssertSize(0, aszParameters.NumElements());
	AssertFalse(CReferenceResolver::SplitParameters("int", &aszParameters, &szAfter));
	AssertFalse(CReferenceResolver::SplitParameters("(int", &aszParameters, &szAfter));
	szAfter.Kill();
	aszParameters.Kill();

	cResolver.Kill();
	cTable.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A table read is the table written.
//
//////////////////////////////////////////////////////////////////////////
void TestReferencesTable(void)
{
	CSymbolTable	cTable;
	CTextFile		cFile;
	CChars			szPrinted;

	cTable.Init(NULL);
	AssertTrue(cTable.Read(TEST_REFERENCES_FOLDER "Symbols.txt"));
	cFile.Init();
	AssertTrue(cFile.Read(TEST_REFERENCES_FOLDER "Symbols.txt"));
	szPrinted.Init();
	cTable.Print(&szPrinted);
	AssertString(cFile.Text(), szPrinted.Text());
	szPrinted.Kill();
	cFile.Kill();
	AssertFalse(cTable.Read(TEST_REFERENCES_FOLDER "Page.md"));
	cTable.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestReferences(void)
{
	BeginTests();

	TestReferencesTable();
	TestReferencesResolve();
	TestReferencesPage();

	TestStatistics();
}

