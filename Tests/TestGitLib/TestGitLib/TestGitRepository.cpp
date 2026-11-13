#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/NativeThread.h"
#include "GitLib/GitRepositories.h"
#include "TestLib/Assert.h"


#define TEST_GIT_REPOSITORY		"Output" _FS_ "GitRepository"


//////////////////////////////////////////////////////////////////////////
//
// Runs git in szRepository with the arguments that follow, up to a NULL.
// The commit's author is given, and it isn't signed, whatever the user's
// settings are.
//
//////////////////////////////////////////////////////////////////////////
bool RunTestGit(const char* szRepository, ...)
{
	va_list		vaArguments;
	const char*	szArgument;
	CArrayChars	aszArguments;
	CGitCommand	cCommand;
	bool		bResult;

	aszArguments.Init();
	aszArguments.Add("-c");
	aszArguments.Add("user.name=Test");
	aszArguments.Add("-c");
	aszArguments.Add("user.email=test@example.com");
	aszArguments.Add("-c");
	aszArguments.Add("commit.gpgsign=false");
	va_start(vaArguments, szRepository);
	for (szArgument = va_arg(vaArguments, const char*); szArgument != NULL; szArgument = va_arg(vaArguments, const char*))
	{
		aszArguments.Add(szArgument);
	}
	va_end(vaArguments);

	cCommand.Init();
	bResult = cCommand.Run(szRepository, &aszArguments);
	if (!bResult)
	{
		printf("git failed: %s\n", cCommand.GetErrors()->Text());
	}
	cCommand.Kill();
	aszArguments.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Writes szText, as it is, to szName in szFolder.
//
//////////////////////////////////////////////////////////////////////////
void WriteTestGitFile(const char* szFolder, const char* szName, const char* szText)
{
	CChars	szFileName;
	FILE*	pFile;

	szFileName.Init(szFolder);
	szFileName.Append('/');
	szFileName.Append(szName);
	pFile = fopen(szFileName.Text(), "wb");
	AssertNotNull(pFile);
	if (pFile)
	{
		fwrite(szText, 1, strlen(szText), pFile);
		fclose(pFile);
	}
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// pszDest is szName's full path in szFolder.
//
//////////////////////////////////////////////////////////////////////////
char* TestGitPath(CChars* pszDest, const char* szFolder, const char* szName)
{
	pszDest->Clear();
	pszDest->Append(szFolder);
	pszDest->Append('/');
	pszDest->Append(szName);
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// Paths are normalised to '/'s without one at the end, and a path is in a
// folder only at a separator.
//
//////////////////////////////////////////////////////////////////////////
void TestGitRepositoryPaths(void)
{
	CChars	sz;

	sz.Init("C:\\Code\\Project\\");
	NormaliseGitPath(&sz);
	AssertString("C:/Code/Project", sz.Text());
	sz.Clear();
	sz.Append("C:\\");
	NormaliseGitPath(&sz);
	AssertString("C:/", sz.Text());
	sz.Clear();
	sz.Append("/");
	NormaliseGitPath(&sz);
	AssertString("/", sz.Text());
	sz.Kill();

	AssertTrue(IsGitPathIn("/a/b/c.cpp", "/a/b"));
	AssertTrue(IsGitPathIn("/a/b", "/a/b"));
	AssertFalse(IsGitPathIn("/a/bc/d.cpp", "/a/b"));
	AssertFalse(IsGitPathIn("/a", "/a/b"));
	AssertTrue(IsGitPathIn("/x.cpp", "/"));
	AssertTrue(IsGitPathIn("C:/x.cpp", "C:/"));
}


//////////////////////////////////////////////////////////////////////////
//
// A repository with a commit is found from a file in it, its files are
// named relative to it, and its status says what has changed since:
// modified, added to the index, untracked, ignored (in an ignored folder)
// and unmodified.  The last commit's text is read as it would be checked
// out, with the line ends its attributes ask for.
//
//////////////////////////////////////////////////////////////////////////
void TestGitRepositoryStatus(void)
{
	CFileUtil			cFileUtil;
	CChars				szRoot;
	CChars				szFound;
	CChars				szPath;
	CChars				szText;
	CGitRepository		cRepository;
	CGitStatus*			pcStatus;
	CArrayChars			aszBranches;

	szRoot.Init(TEST_GIT_REPOSITORY);
	cFileUtil.FullPath(&szRoot);
	NormaliseGitPath(&szRoot);
	cFileUtil.RemoveDir(szRoot.Text());
	AssertTrue(cFileUtil.MakeDirs(false, szRoot.Text(), NULL));
	szPath.Init();
	AssertTrue(cFileUtil.MakeDir(TestGitPath(&szPath, szRoot.Text(), "Sub")));

	AssertTrue(RunTestGit(szRoot.Text(), "init", "-q", "--initial-branch=master", NULL));
	AssertTrue(RunTestGit(szRoot.Text(), "config", "core.autocrlf", "false", NULL));
	WriteTestGitFile(szRoot.Text(), "Main.cpp", "a\nb\nc\n");
	WriteTestGitFile(szRoot.Text(), "Sub/Util.h", "x\n");
	WriteTestGitFile(szRoot.Text(), "Windows.crlf", "p\r\nq\r\n");
	WriteTestGitFile(szRoot.Text(), ".gitignore", "Build/\n");
	WriteTestGitFile(szRoot.Text(), ".gitattributes", "*.crlf text eol=crlf\n");
	AssertTrue(RunTestGit(szRoot.Text(), "add", "-A", NULL));
	AssertTrue(RunTestGit(szRoot.Text(), "commit", "-q", "-m", "First", NULL));

	//Found from a file in a folder of it.
	szFound.Init();
	AssertTrue(FindGitRoot(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h"), &szFound));
	AssertString(szRoot.Text(), szFound.Text());
	szFound.Clear();
	AssertTrue(FindGitRoot(szRoot.Text(), &szFound));
	AssertString(szRoot.Text(), szFound.Text());
	AssertFalse(FindGitRoot("/no-such-folder-for-git/Main.cpp", &szFound));

	//Relative to the current folder, through "..".
	szFound.Clear();
	AssertTrue(FindGitRoot("Output/GitRepository/Sub/../Main.cpp", &szFound));
	AssertString(szRoot.Text(), szFound.Text());
	szFound.Clear();
	szFound.Append("Output/GitRepository/Sub/../Main.cpp");
	MakeFullGitPath(&szFound);
	AssertString(TestGitPath(&szPath, szRoot.Text(), "Main.cpp"), szFound.Text());
	szFound.Kill();

	cRepository.Init(szRoot.Text());
	AssertString(szRoot.Text(), cRepository.GetRoot());
	szText.Init();
	AssertTrue(cRepository.GetRelativePath(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h"), &szText));
	AssertString("Sub/Util.h", szText.Text());
	szText.Clear();
	AssertTrue(cRepository.GetRelativePath(szRoot.Text(), &szText));
	AssertString("", szText.Text());
	AssertFalse(cRepository.GetRelativePath("/no-such-folder-for-git/Main.cpp", &szText));
	szText.Clear();
	AssertTrue(cRepository.GetRelativePath("Output/GitRepository/Sub/../Sub/Util.h", &szText));
	AssertString("Sub/Util.h", szText.Text());

	//Changes since the commit.
	WriteTestGitFile(szRoot.Text(), "Main.cpp", "a\nB\nc\nd\n");
	WriteTestGitFile(szRoot.Text(), "New.txt", "new\n");
	WriteTestGitFile(szRoot.Text(), "Added.h", "added\n");
	AssertTrue(cFileUtil.MakeDir(TestGitPath(&szPath, szRoot.Text(), "Build")));
	WriteTestGitFile(szRoot.Text(), "Build/Main.o", "object\n");
	AssertTrue(RunTestGit(szRoot.Text(), "add", "Added.h", NULL));

	AssertSize(0, cRepository.GetStatusVersion());
	AssertTrue(cRepository.ReadStatus());
	AssertSize(1, cRepository.GetStatusVersion());
	pcStatus = cRepository.GetStatus();
	AssertString("master", pcStatus->GetBranch());
	AssertSize(40, strlen(pcStatus->GetHead()));
	AssertInt(GFS_Modified, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Main.cpp")));
	AssertInt(GFS_Untracked, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "New.txt")));
	AssertInt(GFS_Added, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Added.h")));
	AssertInt(GFS_Ignored, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Build/Main.o")));
	AssertInt(GFS_Unmodified, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h")));
	AssertInt(GFS_Unmodified, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Windows.crlf")));
	AssertInt(GFS_Unmodified, cRepository.GetFileState("/no-such-folder-for-git/Main.cpp"));

	//The last commit's text, not the file's.
	szText.Clear();
	AssertTrue(cRepository.ReadHeadText(TestGitPath(&szPath, szRoot.Text(), "Main.cpp"), &szText));
	AssertString("a\nb\nc\n", szText.Text());
	AssertTrue(cRepository.ReadHeadText(TestGitPath(&szPath, szRoot.Text(), "Windows.crlf"), &szText));
	AssertString("p\r\nq\r\n", szText.Text());
	AssertFalse(cRepository.ReadHeadText(TestGitPath(&szPath, szRoot.Text(), "New.txt"), &szText));
	AssertFalse(cRepository.ReadHeadText(TestGitPath(&szPath, szRoot.Text(), "Added.h"), &szText));
	AssertFalse(cRepository.ReadHeadText("/no-such-folder-for-git/Main.cpp", &szText));

	//Any revision: a branch, or the first commit's parent, which there isn't.
	szText.Clear();
	AssertTrue(cRepository.ReadRevisionText(TestGitPath(&szPath, szRoot.Text(), "Main.cpp"), "master", &szText));
	AssertString("a\nb\nc\n", szText.Text());
	AssertFalse(cRepository.ReadRevisionText(TestGitPath(&szPath, szRoot.Text(), "Main.cpp"), "HEAD~1", &szText));

	//The branches, sorted.
	AssertTrue(RunTestGit(szRoot.Text(), "branch", "feature", NULL));
	aszBranches.Init();
	AssertTrue(cRepository.ReadBranches(&aszBranches));
	AssertSize(2, aszBranches.NumElements());
	AssertString("feature", aszBranches.GetText(0));
	AssertString("master", aszBranches.GetText(1));
	aszBranches.Kill();

	//Read without waiting.
	AssertTrue(cRepository.StartStatus());
	AssertTrue(cRepository.IsReadingStatus());
	AssertFalse(cRepository.StartStatus());
	while (cRepository.PollStatus())
	{
		ThreadSleep(2);
	}
	AssertFalse(cRepository.IsReadingStatus());
	AssertSize(2, cRepository.GetStatusVersion());
	AssertInt(GFS_Modified, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Main.cpp")));

	//A file changed while the status is read: it is read again after.
	AssertTrue(cRepository.StartStatus());
	WriteTestGitFile(szRoot.Text(), "Sub/Util.h", "y\n");
	cRepository.RefreshStatus();
	while (cRepository.PollStatus())
	{
		ThreadSleep(2);
	}
	AssertSize(4, cRepository.GetStatusVersion());
	AssertInt(GFS_Modified, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h")));

	//Not being read, it is read at once.
	WriteTestGitFile(szRoot.Text(), "Sub/Util.h", "x\n");
	cRepository.RefreshStatus();
	AssertTrue(cRepository.IsReadingStatus());
	while (cRepository.PollStatus())
	{
		ThreadSleep(2);
	}
	AssertSize(5, cRepository.GetStatusVersion());
	AssertInt(GFS_Unmodified, cRepository.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h")));

	cRepository.Kill();
	szText.Kill();
	szPath.Kill();
	cFileUtil.RemoveDir(szRoot.Text());
	szRoot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A repository is added the first time a file in it is asked about, once.
// A file in no repository has none.  The statuses are read together.
//
//////////////////////////////////////////////////////////////////////////
void TestGitRepositories(void)
{
	CFileUtil			cFileUtil;
	CChars				szRoot;
	CChars				szPath;
	CGitRepositories	cRepositories;
	CGitRepository*		pcRepository;
	size				uiVersion;

	szRoot.Init(TEST_GIT_REPOSITORY);
	cFileUtil.FullPath(&szRoot);
	NormaliseGitPath(&szRoot);
	cFileUtil.RemoveDir(szRoot.Text());
	AssertTrue(cFileUtil.MakeDirs(false, szRoot.Text(), NULL));
	AssertTrue(RunTestGit(szRoot.Text(), "init", "-q", "--initial-branch=master", NULL));
	WriteTestGitFile(szRoot.Text(), "Main.cpp", "a\n");
	AssertTrue(RunTestGit(szRoot.Text(), "add", "-A", NULL));
	AssertTrue(RunTestGit(szRoot.Text(), "commit", "-q", "-m", "First", NULL));
	WriteTestGitFile(szRoot.Text(), "Main.cpp", "b\n");
	szPath.Init();

	cRepositories.Init();
	AssertSize(0, cRepositories.GetStatusVersion());
	pcRepository = cRepositories.FindRepository(TestGitPath(&szPath, szRoot.Text(), "Main.cpp"));
	AssertNotNull(pcRepository);
	AssertString(szRoot.Text(), pcRepository->GetRoot());
	AssertTrue(pcRepository == cRepositories.FindRepository(TestGitPath(&szPath, szRoot.Text(), "Other.cpp")));
	AssertSize(1, cRepositories.NumRepositories());
	AssertTrue(pcRepository == cRepositories.GetRepository(0));
	AssertNull(cRepositories.GetRepository(1));
	AssertNull(cRepositories.FindRepository("/no-such-folder-for-git/Main.cpp"));
	AssertNull(cRepositories.FindRepository("/no-such-folder-for-git/Other.cpp"));
	AssertSize(1, cRepositories.NumRepositories());

	uiVersion = cRepositories.GetStatusVersion();
	cRepositories.StartStatus();
	AssertTrue(cRepositories.IsReadingStatus());
	while (cRepositories.PollStatus())
	{
		ThreadSleep(2);
	}
	AssertFalse(cRepositories.IsReadingStatus());
	AssertTrue(cRepositories.GetStatusVersion() != uiVersion);
	AssertInt(GFS_Modified, cRepositories.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Main.cpp")));
	AssertInt(GFS_Unmodified, cRepositories.GetFileState("/no-such-folder-for-git/Main.cpp"));

	//Saving a file reads its repository's status again.
	WriteTestGitFile(szRoot.Text(), "Main.cpp", "a\n");
	AssertTrue(cRepositories.RefreshStatus(TestGitPath(&szPath, szRoot.Text(), "Main.cpp")));
	AssertFalse(cRepositories.RefreshStatus("/no-such-folder-for-git/Main.cpp"));
	while (cRepositories.PollStatus())
	{
		ThreadSleep(2);
	}
	AssertInt(GFS_Unmodified, cRepositories.GetFileState(TestGitPath(&szPath, szRoot.Text(), "Main.cpp")));

	cRepositories.RemoveAll();
	AssertSize(0, cRepositories.NumRepositories());
	cRepositories.Kill();

	szPath.Kill();
	cFileUtil.RemoveDir(szRoot.Text());
	szRoot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// git's output is kept byte for byte and its errors apart; a command that
// fails says so.
//
//////////////////////////////////////////////////////////////////////////
void TestGitCommand(void)
{
	CGitCommand	cCommand;
	CArrayChars	aszArguments;

	aszArguments.Init();
	aszArguments.Add("--version");
	cCommand.Init();
	AssertFalse(cCommand.Succeeded());
	AssertTrue(cCommand.Run(".", &aszArguments));
	AssertTrue(cCommand.Succeeded());
	AssertInt(0, cCommand.GetExitCode());
	AssertTrue(cCommand.GetOutput()->StartsWith("git version "));
	AssertString("", cCommand.GetErrors()->Text());

	aszArguments.ReInit();
	aszArguments.Add("no-such-command-for-git");
	AssertFalse(cCommand.Run(".", &aszArguments));
	AssertFalse(cCommand.Succeeded());
	AssertTrue(cCommand.GetExitCode() != 0);
	AssertString("", cCommand.GetOutput()->Text());
	AssertTrue(cCommand.GetErrors()->Length() > 0);
	cCommand.Kill();
	aszArguments.Kill();

	AssertString("git", GetGitExecutable());
	SetGitExecutable("no-such-git-for-test");
	AssertString("no-such-git-for-test", GetGitExecutable());
	SetGitExecutable("git");
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is tracked, and a tracked file renamed with git mv: the
// rename staged, the file moved.  An untracked file, or a name taken,
// isn't moved.
//
//////////////////////////////////////////////////////////////////////////
void TestGitRepositoryMove(void)
{
	CFileUtil		cFileUtil;
	CChars			szRoot;
	CChars			szPath;
	CChars			szNew;
	CGitRepository	cRepository;
	CGitCommand		cCommand;
	CArrayChars		aszArguments;

	szRoot.Init(TEST_GIT_REPOSITORY "Move");
	cFileUtil.FullPath(&szRoot);
	NormaliseGitPath(&szRoot);
	cFileUtil.RemoveDir(szRoot.Text());
	AssertTrue(cFileUtil.MakeDirs(false, szRoot.Text(), NULL));
	szPath.Init();
	szNew.Init();
	AssertTrue(cFileUtil.MakeDir(TestGitPath(&szPath, szRoot.Text(), "Sub")));
	AssertTrue(RunTestGit(szRoot.Text(), "init", "-q", "--initial-branch=master", NULL));
	WriteTestGitFile(szRoot.Text(), "Engine.h", "e\n");
	WriteTestGitFile(szRoot.Text(), "Sub/Util.h", "u\n");
	AssertTrue(RunTestGit(szRoot.Text(), "add", "-A", NULL));
	AssertTrue(RunTestGit(szRoot.Text(), "commit", "-q", "-m", "First", NULL));
	WriteTestGitFile(szRoot.Text(), "New.h", "n\n");

	cRepository.Init(szRoot.Text());
	AssertTrue(cRepository.IsTracked(TestGitPath(&szPath, szRoot.Text(), "Engine.h")));
	AssertTrue(cRepository.IsTracked(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h")));
	AssertFalse(cRepository.IsTracked(TestGitPath(&szPath, szRoot.Text(), "New.h")));
	AssertFalse(cRepository.IsTracked(TestGitPath(&szPath, szRoot.Text(), "Missing.h")));
	AssertFalse(cRepository.IsTracked("/no-such-folder-for-git/Engine.h"));

	//Tracked: moved, and staged as a rename.
	AssertTrue(cRepository.Move(TestGitPath(&szPath, szRoot.Text(), "Engine.h"), TestGitPath(&szNew, szRoot.Text(), "CPPEngine.h")));
	AssertFalse(cFileUtil.Exists(TestGitPath(&szPath, szRoot.Text(), "Engine.h")));
	AssertTrue(cFileUtil.Exists(TestGitPath(&szPath, szRoot.Text(), "CPPEngine.h")));
	AssertTrue(cRepository.Move(TestGitPath(&szPath, szRoot.Text(), "Sub/Util.h"), TestGitPath(&szNew, szRoot.Text(), "Sub/Utility.h")));
	aszArguments.Init();
	aszArguments.Add("status");
	aszArguments.Add("--porcelain");
	aszArguments.Add("--untracked-files=no");
	cCommand.Init();
	AssertTrue(cCommand.Run(szRoot.Text(), &aszArguments));
	AssertString("R  Engine.h -> CPPEngine.h\nR  Sub/Util.h -> Sub/Utility.h\n", cCommand.GetOutput()->Text());
	cCommand.Kill();
	aszArguments.Kill();

	//Untracked, or onto a file that is there: nothing moves.
	AssertFalse(cRepository.Move(TestGitPath(&szPath, szRoot.Text(), "New.h"), TestGitPath(&szNew, szRoot.Text(), "Newer.h")));
	AssertTrue(cFileUtil.Exists(TestGitPath(&szPath, szRoot.Text(), "New.h")));
	AssertFalse(cRepository.Move(TestGitPath(&szPath, szRoot.Text(), "CPPEngine.h"), TestGitPath(&szNew, szRoot.Text(), "Sub/Utility.h")));
	AssertTrue(cFileUtil.Exists(TestGitPath(&szPath, szRoot.Text(), "CPPEngine.h")));

	cRepository.Kill();
	szNew.Kill();
	szPath.Kill();
	cFileUtil.RemoveDir(szRoot.Text());
	szRoot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestGitRepository(void)
{
	BeginTests();

	TestGitCommand();
	TestGitRepositoryPaths();
	TestGitRepositoryStatus();
	TestGitRepositories();
	TestGitRepositoryMove();

	TestStatistics();
}

