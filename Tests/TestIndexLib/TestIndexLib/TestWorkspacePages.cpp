#include "BaseLib/FileUtil.h"
#include "BuildLib/BuildWorkspace.h"
#include "IndexLib/WorkspaceIndex.h"
#include "TestLib/Assert.h"


#define TEST_PAGES_SOLUTION		"Input" _FS_ "Pages" _FS_ "Pages.sln"
#define TEST_PAGES_PAGE			"Input" _FS_ "Pages" _FS_ "Docs" _FS_ "Shapes.md"
#define TEST_PAGES_SITE_SOLUTION	"Input" _FS_ "Pages" _FS_ "PagesSite.sln"
#define TEST_PAGES_SYMBOLS		"Input" _FS_ "Pages" _FS_ "Site" _FS_ "Web" _FS_ ".symbols" _FS_


//////////////////////////////////////////////////////////////////////////
//
// The place of szKey in szFile at line iLine and column iColumn (0
// based), of usage type CPPUT_Documentation, and its length; -1 if none.
//
//////////////////////////////////////////////////////////////////////////
static int TestPagesPlace(CCPPIndex* pcIndex, const char* szKey, const char* szFile, int iLine, int iColumn)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	size				i;

	pcSymbol = pcIndex->FindSymbol(szKey);
	for (i = 0; (pcSymbol != NULL) && (i < pcSymbol->NumLocations()); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if ((psLocation->uiUsage == CPPUT_Documentation) && (psLocation->iLine == iLine) && (psLocation->iColumn == iColumn) && (StringCompare(pcIndex->GetFileName(psLocation->uiFile), szFile) == 0))
		{
			return psLocation->iLength;
		}
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// How many CPPUT_Documentation places szKey has.
//
//////////////////////////////////////////////////////////////////////////
static int TestPagesNumPlaces(CCPPIndex* pcIndex, const char* szKey)
{
	CCPPIndexSymbol*	pcSymbol;
	size				i;
	int					iPlaces;

	iPlaces = 0;
	pcSymbol = pcIndex->FindSymbol(szKey);
	for (i = 0; (pcSymbol != NULL) && (i < pcSymbol->NumLocations()); i++)
	{
		if (pcSymbol->GetLocation(i)->uiUsage == CPPUT_Documentation)
		{
			iPlaces++;
		}
	}
	return iPlaces;
}


//////////////////////////////////////////////////////////////////////////
//
// The page's references once the C++ is indexed, without the website's
// table: a class, a member function by its parameters and without them,
// an enumerator without its enum and a file link; what the workspace
// hasn't is an error; a plain span naming a class is a warning.  Each
// name in a reference is a place of its symbol, the parameters' types
// too.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspacePagesIndexed(void)
{
	CBuildWorkspace			cWorkspace;
	CWorkspaceIndex			cIndex;
	CArrayPageReferences	asReferences;
	CChars					szPage;
	CFileUtil				cFileUtil;
	CCPPIndex*				pcIndex;
	CCPPIndexSymbol*		pcSymbol;

	szPage.Init(TEST_PAGES_PAGE);
	cFileUtil.FullPath(&szPage);

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_PAGES_SOLUTION));
	cIndex.Init(&cWorkspace);
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	AssertFalse(cIndex.IsIndexing());
	AssertSize(1, cIndex.NumPages());
	AssertTrue(cIndex.IsPage(szPage.Text()));
	AssertFalse(cIndex.HasSiteSymbols());

	asReferences.Init();
	AssertSize(9, cIndex.GetPageReferences(szPage.Text(), &asReferences));
	AssertInt(PRK_Link, asReferences.Get(0)->eKind);
	AssertString("CShape", asReferences.Get(0)->szText.Text());
	AssertTrue(asReferences.Get(0)->szFile.EndsWith("Shapes.h"));
	AssertInt(12, asReferences.Get(0)->iTargetLine);
	AssertInt(PRK_Link, asReferences.Get(1)->eKind);
	AssertInt(18, asReferences.Get(1)->iTargetLine);
	AssertInt(PRK_Link, asReferences.Get(2)->eKind);
	AssertInt(19, asReferences.Get(2)->iTargetLine);
	AssertInt(PRK_Link, asReferences.Get(3)->eKind);
	AssertInt(8, asReferences.Get(3)->iTargetLine);
	AssertInt(PRK_Link, asReferences.Get(4)->eKind);
	AssertString("Shapes.h", asReferences.Get(4)->szText.Text());
	AssertTrue(asReferences.Get(4)->szFile.EndsWith("Pages" _FS_ "Shapes.h"));
	AssertInt(0, asReferences.Get(4)->iTargetLine);
	AssertInt(PRK_Error, asReferences.Get(5)->eKind);
	AssertString("[`SSiteOnly`] names nothing the indexed solutions declare", asReferences.Get(5)->szMessage.Text());
	AssertInt(PRK_Error, asReferences.Get(6)->eKind);
	AssertString("[`Other/Site.h`] names no file in the repositories", asReferences.Get(6)->szMessage.Text());
	AssertInt(PRK_Error, asReferences.Get(7)->eKind);
	AssertInt(4, asReferences.Get(7)->iLine);
	AssertInt(8, asReferences.Get(7)->iColumn);
	AssertInt(PRK_Warning, asReferences.Get(8)->eKind);
	AssertInt(34, asReferences.Get(8)->iColumn);
	CReferenceResolver::KillPageReferences(&asReferences);

	//Line 2: A [`CShape`] is made by [`CShape::Init(EKind)`] and asked [`CShape::GetKind`]; its kind is [`EK_Circle`].
	pcIndex = cIndex.GetIndex();
	AssertInt(6, TestPagesPlace(pcIndex, "CShape", szPage.Text(), 2, 4));
	AssertInt(6, TestPagesPlace(pcIndex, "CShape", szPage.Text(), 2, 26));
	AssertInt(4, TestPagesPlace(pcIndex, "CShape::Init(EKind)", szPage.Text(), 2, 34));
	AssertInt(5, TestPagesPlace(pcIndex, "EKind", szPage.Text(), 2, 39));
	AssertInt(6, TestPagesPlace(pcIndex, "CShape", szPage.Text(), 2, 60));
	AssertInt(7, TestPagesPlace(pcIndex, "CShape::GetKind()", szPage.Text(), 2, 68));
	AssertInt(9, TestPagesPlace(pcIndex, "EKind::EK_Circle", szPage.Text(), 2, 93));
	AssertInt(3, TestPagesNumPlaces(pcIndex, "CShape"));
	pcSymbol = pcIndex->FindSymbolAt(szPage.Text(), 2, 36);
	AssertNotNull(pcSymbol);
	AssertString("CShape::Init(EKind)", pcSymbol->mszKey.Text());

	cIndex.Kill();
	cWorkspace.Kill();
	szPage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// With the website's table, found beside the mkdocs.yml that is an item
// of the workspace's Web project, what the workspace doesn't have links
// to the file the table says (but is no place in the index), and files
// are found in the website's list.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspacePagesSite(void)
{
	CBuildWorkspace			cWorkspace;
	CWorkspaceIndex			cIndex;
	CArrayPageReferences	asReferences;
	CChars					szPage;
	CChars					szSiteFile;
	CChars					szFile;
	CFileUtil				cFileUtil;
	int						iLine;

	szPage.Init(TEST_PAGES_PAGE);
	cFileUtil.FullPath(&szPage);
	szSiteFile.Init("Input" _FS_ "Pages" _FS_ "Site" _FS_ "Other" _FS_ "Site.h");
	cFileUtil.FullPath(&szSiteFile);

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_PAGES_SITE_SOLUTION));
	AssertSize(2, cWorkspace.NumProjects());
	cIndex.Init(&cWorkspace);
	AssertFalse(cIndex.SetSiteSymbols(TEST_PAGES_SYMBOLS "Missing.txt", TEST_PAGES_SYMBOLS "files.txt", "Input"));
	AssertFalse(cIndex.HasSiteSymbols());
	szFile.Init();
	AssertTrue(cIndex.FindDocsSite(&szFile));
	AssertTrue(szFile.EndsWith("Web" _FS_ "mkdocs.yml"));
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	AssertTrue(cIndex.HasSiteSymbols());

	asReferences.Init();
	AssertSize(9, cIndex.GetPageReferences(szPage.Text(), &asReferences));
	AssertInt(PRK_Link, asReferences.Get(4)->eKind);
	AssertTrue(asReferences.Get(4)->szFile.EndsWith("Input" _FS_ "Pages" _FS_ "Shapes.h"));
	AssertInt(PRK_Link, asReferences.Get(5)->eKind);
	AssertString(szSiteFile.Text(), asReferences.Get(5)->szFile.Text());
	AssertInt(3, asReferences.Get(5)->iTargetLine);
	AssertInt(PRK_Link, asReferences.Get(6)->eKind);
	AssertString(szSiteFile.Text(), asReferences.Get(6)->szFile.Text());
	AssertInt(PRK_Error, asReferences.Get(7)->eKind);
	CReferenceResolver::KillPageReferences(&asReferences);

	//Line 3: It is declared in [`Shapes.h`]; the site has [`SSiteOnly`] and [`Other/Site.h`].
	AssertTrue(cIndex.FindPageReference(szPage.Text(), 3, 50, &szFile, &iLine));
	AssertString(szSiteFile.Text(), szFile.Text());
	AssertInt(3, iLine);
	AssertFalse(cIndex.FindPageReference(szPage.Text(), 3, 2, &szFile, &iLine));
	szFile.Kill();
	AssertNull(cIndex.GetIndex()->FindSymbol("SSiteOnly"));

	cIndex.Kill();
	cWorkspace.Kill();
	szSiteFile.Kill();
	szPage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A page resolved again from its editor's text: its places before are
// forgotten.  A file that isn't a page isn't resolved.
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspacePagesEdited(void)
{
	CBuildWorkspace			cWorkspace;
	CWorkspaceIndex			cIndex;
	CArrayPageReferences	asReferences;
	CChars					szPage;
	CFileUtil				cFileUtil;

	szPage.Init(TEST_PAGES_PAGE);
	cFileUtil.FullPath(&szPage);
	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_PAGES_SOLUTION));
	cIndex.Init(&cWorkspace);
	cIndex.StartIndexing();
	cIndex.WaitForIndexing();
	AssertInt(1, TestPagesNumPlaces(cIndex.GetIndex(), "CShape::GetKind()"));

	AssertTrue(cIndex.IndexPage(szPage.Text(), "Only [`CShape::GetKind`] now.\n"));
	AssertInt(1, TestPagesNumPlaces(cIndex.GetIndex(), "CShape::GetKind()"));
	AssertInt(1, TestPagesNumPlaces(cIndex.GetIndex(), "CShape"));
	AssertInt(0, TestPagesNumPlaces(cIndex.GetIndex(), "EKind::EK_Circle"));
	AssertInt(7, TestPagesPlace(cIndex.GetIndex(), "CShape::GetKind()", szPage.Text(), 0, 15));
	asReferences.Init();
	AssertSize(1, cIndex.GetPageReferences(szPage.Text(), &asReferences));
	CReferenceResolver::KillPageReferences(&asReferences);

	AssertFalse(cIndex.IndexPage("Shapes.cpp", "[`CShape`]"));
	AssertTrue(CWorkspaceIndex::IsPageFile("A.md"));
	AssertTrue(CWorkspaceIndex::IsPageFile("README.MD"));
	AssertFalse(CWorkspaceIndex::IsPageFile("Notes.txt"));

	cIndex.Kill();
	cWorkspace.Kill();
	szPage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestWorkspacePages(void)
{
	BeginTests();

	TestWorkspacePagesIndexed();
	TestWorkspacePagesSite();
	TestWorkspacePagesEdited();

	TestStatistics();
}

