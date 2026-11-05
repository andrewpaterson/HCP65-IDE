#include "BaseLib/Chars.h"
#include "GitLib/GitStatus.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// Parses a status from szOutput, uiLength bytes with '\0's in it.
//
//////////////////////////////////////////////////////////////////////////
bool ParseTestGitStatus(CGitStatus* pcStatus, const char* szOutput, size uiLength)
{
	CChars	szCopy;
	bool	bResult;

	szCopy.Init();
	szCopy.Append(szOutput, uiLength);
	bResult = pcStatus->Parse(szCopy.Text(), szCopy.Length());
	szCopy.Kill();
	return bResult;
}


#define PARSE_TEST_GIT_STATUS(status, literal)	ParseTestGitStatus(status, literal, sizeof(literal) - 1)


//////////////////////////////////////////////////////////////////////////
//
// The branch headers and every kind of entry: changed in the work tree
// and in the index, a path with a space, deleted, renamed (followed by
// its original path), unmerged, untracked and ignored (files and
// folders).
//
//////////////////////////////////////////////////////////////////////////
void TestGitStatusParse(void)
{
	CGitStatus		cStatus;
	SGitFileStatus*	psFile;

	cStatus.Init();
	AssertTrue(PARSE_TEST_GIT_STATUS(&cStatus,
		"# branch.oid 0123456789abcdef0123456789abcdef01234567\0"
		"# branch.head master\0"
		"# branch.upstream origin/master\0"
		"# branch.ab +2 -1\0"
		"1 .M N... 100644 100644 100644 aaaa aaaa Source/Main.cpp\0"
		"1 AM N... 000000 100644 100644 0000 cccc New File.h\0"
		"1 .D N... 100644 100644 000000 aaaa aaaa Gone.cpp\0"
		"2 R. N... 100644 100644 100644 aaaa aaaa R100 Renamed.cpp\0Old.cpp\0"
		"u UU N... 100644 100644 100644 100644 h1 h2 h3 Both.cpp\0"
		"? Notes.txt\0"
		"? Scratch/\0"
		"! Output/\0"));

	AssertString("master", cStatus.GetBranch());
	AssertString("0123456789abcdef0123456789abcdef01234567", cStatus.GetHead());
	AssertString("origin/master", cStatus.GetUpstream());
	AssertInt(2, cStatus.GetAhead());
	AssertInt(1, cStatus.GetBehind());
	AssertFalse(cStatus.IsDetached());
	AssertSize(8, cStatus.NumFiles());

	psFile = cStatus.GetFile(0);
	AssertString("Source/Main.cpp", cStatus.GetPath(psFile));
	AssertInt(GFS_Modified, psFile->eState);
	AssertChar('.', psFile->cIndex);
	AssertChar('M', psFile->cWorkTree);
	AssertNull(cStatus.GetOriginalPath(psFile));

	psFile = cStatus.FindFile("Renamed.cpp");
	AssertNotNull(psFile);
	AssertInt(GFS_Renamed, psFile->eState);
	AssertString("Old.cpp", cStatus.GetOriginalPath(psFile));
	AssertNull(cStatus.FindFile("Old.cpp"));
	AssertNull(cStatus.GetFile(8));

	AssertInt(GFS_Modified, cStatus.GetFileState("Source/Main.cpp"));
	AssertInt(GFS_Added, cStatus.GetFileState("New File.h"));
	AssertInt(GFS_Deleted, cStatus.GetFileState("Gone.cpp"));
	AssertInt(GFS_Renamed, cStatus.GetFileState("Renamed.cpp"));
	AssertInt(GFS_Conflicted, cStatus.GetFileState("Both.cpp"));
	AssertInt(GFS_Untracked, cStatus.GetFileState("Notes.txt"));
	AssertInt(GFS_Untracked, cStatus.GetFileState("Scratch/Deeper/Test.cpp"));
	AssertInt(GFS_Ignored, cStatus.GetFileState("Output/Frame0.png"));
	AssertInt(GFS_Unmodified, cStatus.GetFileState("Source/Other.cpp"));
	AssertInt(GFS_Unmodified, cStatus.GetFileState("Scratch"));

	//Read again, the last status is gone.
	AssertTrue(PARSE_TEST_GIT_STATUS(&cStatus, "# branch.oid (initial)\0# branch.head main\0? a.cpp\0"));
	AssertString("main", cStatus.GetBranch());
	AssertString("", cStatus.GetHead());
	AssertString("", cStatus.GetUpstream());
	AssertInt(0, cStatus.GetAhead());
	AssertFalse(cStatus.IsDetached());
	AssertSize(1, cStatus.NumFiles());
	AssertInt(GFS_Unmodified, cStatus.GetFileState("Source/Main.cpp"));

	cStatus.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A detached head, no output, entries of kinds it doesn't know (skipped)
// and entries cut short (the status is cleared and false returned).
//
//////////////////////////////////////////////////////////////////////////
void TestGitStatusUnusual(void)
{
	CGitStatus	cStatus;

	cStatus.Init();
	AssertTrue(PARSE_TEST_GIT_STATUS(&cStatus, "# branch.oid abcdef\0# branch.head (detached)\0"));
	AssertString("", cStatus.GetBranch());
	AssertString("abcdef", cStatus.GetHead());
	AssertTrue(cStatus.IsDetached());

	AssertTrue(cStatus.Parse("", 0));
	AssertSize(0, cStatus.NumFiles());
	AssertString("", cStatus.GetHead());

	AssertTrue(PARSE_TEST_GIT_STATUS(&cStatus, "# stash 3\0x something new\0? a.cpp\0"));
	AssertSize(1, cStatus.NumFiles());
	AssertInt(GFS_Untracked, cStatus.GetFileState("a.cpp"));

	//The last entry needn't end with a '\0'.
	AssertTrue(PARSE_TEST_GIT_STATUS(&cStatus, "? a.cpp\0? b.cpp"));
	AssertSize(2, cStatus.NumFiles());
	AssertInt(GFS_Untracked, cStatus.GetFileState("b.cpp"));

	AssertFalse(PARSE_TEST_GIT_STATUS(&cStatus, "? a.cpp\0" "1 .M N... 100644\0"));
	AssertSize(0, cStatus.NumFiles());
	AssertFalse(PARSE_TEST_GIT_STATUS(&cStatus, "2 R. N... 100644 100644 100644 aaaa aaaa R100 New.cpp\0"));
	AssertFalse(PARSE_TEST_GIT_STATUS(&cStatus, "?\0"));
	AssertFalse(PARSE_TEST_GIT_STATUS(&cStatus, "# branch.ab +1\0"));
	AssertSize(0, cStatus.NumFiles());

	cStatus.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A changed file's state from its kind and XY.
//
//////////////////////////////////////////////////////////////////////////
void TestGitStatusStates(void)
{
	AssertInt(GFS_Modified, GetGitChangedState('1', 'M', '.'));
	AssertInt(GFS_Modified, GetGitChangedState('1', '.', 'T'));
	AssertInt(GFS_Modified, GetGitChangedState('1', 'M', 'M'));
	AssertInt(GFS_Added, GetGitChangedState('1', 'A', 'M'));
	AssertInt(GFS_Added, GetGitChangedState('2', 'C', '.'));
	AssertInt(GFS_Deleted, GetGitChangedState('1', 'D', '.'));
	AssertInt(GFS_Deleted, GetGitChangedState('1', 'M', 'D'));
	AssertInt(GFS_Renamed, GetGitChangedState('2', 'R', 'M'));
	AssertInt(GFS_Conflicted, GetGitChangedState('u', 'A', 'A'));

	AssertString("Modified", GetGitFileStateName(GFS_Modified));
	AssertString("Conflicted", GetGitFileStateName(GFS_Conflicted));
	AssertString("Unknown", GetGitFileStateName(NUM_GIT_FILE_STATES));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestGitStatus(void)
{
	BeginTests();

	TestGitStatusParse();
	TestGitStatusUnusual();
	TestGitStatusStates();

	TestStatistics();
}

