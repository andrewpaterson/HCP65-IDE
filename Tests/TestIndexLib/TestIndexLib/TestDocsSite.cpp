#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "IndexLib/DocsSite.h"
#include "IndexLib/MarkdownSpans.h"
#include "TestLib/Assert.h"


#define TEST_DOCS_SITE_FOLDER	"Input" _FS_ "Pages" _FS_
#define TEST_DOCS_SITE_MKDOCS	TEST_DOCS_SITE_FOLDER "Site" _FS_ "Web" _FS_ "mkdocs.yml"
#define TEST_DOCS_SITE_HOME		TEST_DOCS_SITE_FOLDER "Site" _FS_ "Web" _FS_ "docs" _FS_ "index.md"
#define TEST_DOCS_SITE_SHAPES	TEST_DOCS_SITE_FOLDER "Docs" _FS_ "Shapes.md"


//////////////////////////////////////////////////////////////////////////
//
// szRelative's full path.
//
//////////////////////////////////////////////////////////////////////////
static void TestDocsSiteFull(const char* szRelative, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	pszDest->Set(szRelative);
	cFileUtil.FullPath(pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The fixture's website: two folders, one landing in the site's top, the
// other in its shapes folder, and the snippets' folder.  A link between
// pages is between their places in the site.
//
//////////////////////////////////////////////////////////////////////////
void TestDocsSiteMap(void)
{
	CDocsSite	cSite;
	CChars		szResult;
	CChars		szExpected;

	szResult.Init();
	szExpected.Init();
	cSite.Init();
	AssertTrue(cSite.Read(TEST_DOCS_SITE_MKDOCS));
	AssertTrue(cSite.IsRead());
	AssertSize(2, cSite.NumSources());

	AssertTrue(cSite.GetSitePath(TEST_DOCS_SITE_SHAPES, &szResult));
	AssertString("shapes/Shapes.md", szResult.Text());
	AssertTrue(cSite.GetSitePath(TEST_DOCS_SITE_HOME, &szResult));
	AssertString("index.md", szResult.Text());
	AssertFalse(cSite.GetSitePath(TEST_DOCS_SITE_FOLDER "Shapes.h", &szResult));

	AssertTrue(cSite.ResolveLink(TEST_DOCS_SITE_HOME, "shapes/Shapes.md#kinds", &szResult));
	TestDocsSiteFull(TEST_DOCS_SITE_SHAPES, &szExpected);
	AssertString(szExpected.Text(), szResult.Text());
	AssertTrue(cSite.ResolveLink(TEST_DOCS_SITE_SHAPES, "../index.md", &szResult));
	TestDocsSiteFull(TEST_DOCS_SITE_HOME, &szExpected);
	AssertString(szExpected.Text(), szResult.Text());
	AssertFalse(cSite.ResolveLink(TEST_DOCS_SITE_HOME, "missing.png", &szResult));
	AssertFalse(cSite.ResolveLink(TEST_DOCS_SITE_HOME, "https://example.com", &szResult));
	AssertFalse(cSite.ResolveLink(TEST_DOCS_SITE_HOME, "#kinds", &szResult));
	AssertFalse(cSite.ResolveLink(TEST_DOCS_SITE_SHAPES, "../../index.md", &szResult));

	AssertTrue(cSite.ResolveSnippet(TEST_DOCS_SITE_HOME, "Snippet.txt", &szResult));
	TestDocsSiteFull(TEST_DOCS_SITE_FOLDER "Site" _FS_ "Other" _FS_ "Snippet.txt", &szExpected);
	AssertString(szExpected.Text(), szResult.Text());
	AssertFalse(cSite.ResolveSnippet(TEST_DOCS_SITE_HOME, "Missing.txt", &szResult));
	cSite.Kill();

	//Without a map a link is relative to its page's folder.
	cSite.Init();
	AssertFalse(cSite.Read(TEST_DOCS_SITE_FOLDER "Missing.yml"));
	AssertTrue(cSite.ResolveLink(TEST_DOCS_SITE_SHAPES, "../Shapes.h", &szResult));
	TestDocsSiteFull(TEST_DOCS_SITE_FOLDER "Shapes.h", &szExpected);
	AssertString(szExpected.Text(), szResult.Text());
	AssertFalse(cSite.ResolveSnippet(TEST_DOCS_SITE_HOME, "Snippet.txt", &szResult));
	cSite.Kill();

	szExpected.Kill();
	szResult.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The files a page names: its links' and images' targets and its
// snippets' paths, where each starts.
//
//////////////////////////////////////////////////////////////////////////
void TestDocsSiteLinks(void)
{
	CTextFile		cPage;
	CMarkdownSpans	cSpans;

	cPage.Init();
	AssertTrue(cPage.Read(TEST_DOCS_SITE_HOME));
	cSpans.Init();
	cSpans.Scan(cPage.Text());
	AssertSize(4, cSpans.NumLinks());
	AssertString("shapes/Shapes.md#kinds", cSpans.GetLink(0)->szTarget.Text());
	AssertInt(2, cSpans.GetLink(0)->iLine);
	AssertInt(17, cSpans.GetLink(0)->iColumn);
	AssertFalse(cSpans.GetLink(0)->bSnippet);
	AssertString("missing.png", cSpans.GetLink(1)->szTarget.Text());
	AssertInt(55, cSpans.GetLink(1)->iColumn);
	AssertString("https://example.com", cSpans.GetLink(2)->szTarget.Text());
	AssertInt(87, cSpans.GetLink(2)->iColumn);
	AssertString("Snippet.txt", cSpans.GetLink(3)->szTarget.Text());
	AssertInt(5, cSpans.GetLink(3)->iLine);
	AssertInt(8, cSpans.GetLink(3)->iColumn);
	AssertTrue(cSpans.GetLink(3)->bSnippet);
	cSpans.Kill();
	cPage.Kill();

	//Not in code: a span's text isn't a link, nor a fence's.
	cSpans.Init();
	cSpans.Scan("Code `[a](b.md)` and [c](d.md).\n```\n[e](f.md)\n```\n");
	AssertSize(1, cSpans.NumLinks());
	AssertString("d.md", cSpans.GetLink(0)->szTarget.Text());
	cSpans.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestDocsSite(void)
{
	BeginTests();

	TestDocsSiteMap();
	TestDocsSiteLinks();

	TestStatistics();
}

