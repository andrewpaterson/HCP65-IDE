#include <string.h>
#include "GitLib/GitLineChanges.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void AssertGitLineChange(EGitLineChange eType, size uiStartLine, size uiEndLine, size uiBaseStartLine, size uiBaseEndLine, SGitLineChange* psChange)
{
	AssertNotNull(psChange);
	AssertInt(eType, psChange->eType);
	AssertSize(uiStartLine, psChange->uiStartLine);
	AssertSize(uiEndLine, psChange->uiEndLine);
	AssertSize(uiBaseStartLine, psChange->uiBaseStartLine);
	AssertSize(uiBaseEndLine, psChange->uiBaseEndLine);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CompareGitLines(CGitLineChanges* pcChanges, const char* szBase, const char* szText)
{
	pcChanges->Compare(szBase, strlen(szBase), szText, strlen(szText));
}


//////////////////////////////////////////////////////////////////////////
//
// Lines modified, added after the last, and removed (above the line now
// after them), each numbered in the text as it is now.
//
//////////////////////////////////////////////////////////////////////////
void TestGitLineChangesKinds(void)
{
	CGitLineChanges	cChanges;

	cChanges.Init();

	CompareGitLines(&cChanges, "a\nb\nc\n", "a\nb\nc\n");
	AssertSize(0, cChanges.NumChanges());

	CompareGitLines(&cChanges, "a\nb\nc\n", "a\nB\nc\nd\n");
	AssertSize(2, cChanges.NumChanges());
	AssertGitLineChange(GLC_Modified, 1, 2, 1, 2, cChanges.GetChange(0));
	AssertGitLineChange(GLC_Added, 3, 4, 3, 3, cChanges.GetChange(1));
	AssertNull(cChanges.GetChange(2));

	CompareGitLines(&cChanges, "a\nb\nc\nd\n", "a\nd\n");
	AssertSize(1, cChanges.NumChanges());
	AssertGitLineChange(GLC_Removed, 1, 1, 1, 3, cChanges.GetChange(0));

	//Removed from the top and the end.
	CompareGitLines(&cChanges, "a\nb\nc\n", "b\n");
	AssertSize(2, cChanges.NumChanges());
	AssertGitLineChange(GLC_Removed, 0, 0, 0, 1, cChanges.GetChange(0));
	AssertGitLineChange(GLC_Removed, 1, 1, 2, 3, cChanges.GetChange(1));

	cChanges.Clear();
	AssertSize(0, cChanges.NumChanges());

	cChanges.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A new file is all added and an emptied one all removed.  The texts
// needn't end with a '\0', and whitespace counts.
//
//////////////////////////////////////////////////////////////////////////
void TestGitLineChangesEdges(void)
{
	CGitLineChanges	cChanges;

	cChanges.Init();

	CompareGitLines(&cChanges, "", "x\ny\n");
	AssertSize(1, cChanges.NumChanges());
	AssertGitLineChange(GLC_Added, 0, 2, 0, 0, cChanges.GetChange(0));

	CompareGitLines(&cChanges, "x\ny\n", "");
	AssertSize(1, cChanges.NumChanges());
	AssertGitLineChange(GLC_Removed, 0, 0, 0, 2, cChanges.GetChange(0));

	cChanges.Compare("a\nb\nXX", 4, "a\nc\nYY", 4);
	AssertSize(1, cChanges.NumChanges());
	AssertGitLineChange(GLC_Modified, 1, 2, 1, 2, cChanges.GetChange(0));

	CompareGitLines(&cChanges, "int a;\n", "int  a;\n");
	AssertSize(1, cChanges.NumChanges());
	AssertGitLineChange(GLC_Modified, 0, 1, 0, 1, cChanges.GetChange(0));

	cChanges.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestGitLineChanges(void)
{
	BeginTests();

	TestGitLineChangesKinds();
	TestGitLineChangesEdges();

	TestStatistics();
}

