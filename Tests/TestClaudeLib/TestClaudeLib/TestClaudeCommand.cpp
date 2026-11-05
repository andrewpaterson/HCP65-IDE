#include "ClaudeLib/ClaudeCommand.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// The arguments with " | " between them.
//
//////////////////////////////////////////////////////////////////////////
void JoinTestArguments(CArrayChars* paszArguments, CChars* pszDest)
{
	size	i;

	for (i = 0; i < paszArguments->NumElements(); i++)
	{
		if (i > 0)
		{
			pszDest->Append(" | ");
		}
		pszDest->Append(paszArguments->GetText(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The command line: the request straight after -p, the session carried
// on, and the options set.  Claude itself isn't run: each run costs money
// and what it says changes.
//
//////////////////////////////////////////////////////////////////////////
void TestClaudeCommandArguments(void)
{
	CClaudeCommand	cCommand;
	CArrayChars		aszArguments;
	CChars			sz;

	cCommand.Init();

	aszArguments.Init();
	cCommand.GetArguments("Rename Add to Sum", &aszArguments);
	sz.Init();
	JoinTestArguments(&aszArguments, &sz);
	AssertString("-p | Rename Add to Sum | --output-format | stream-json | --verbose | --permission-mode | acceptEdits", sz.Text());
	sz.Kill();
	aszArguments.Kill();

	cCommand.SetSessionId("d844c748-d234");
	cCommand.SetPermissionMode("plan");
	cCommand.SetModel("sonnet");
	cCommand.SetAllowedTools("Bash(git diff:*),Read");
	aszArguments.Init();
	cCommand.GetArguments("-1 is wrong here", &aszArguments);
	sz.Init();
	JoinTestArguments(&aszArguments, &sz);
	AssertString("-p |  -1 is wrong here | --output-format | stream-json | --verbose | --permission-mode | plan | --model | sonnet | --resume | d844c748-d234 | --allowedTools | Bash(git diff:*),Read", sz.Text());
	sz.Kill();
	aszArguments.Kill();

	cCommand.Forget();
	AssertString("", cCommand.GetSessionId());
	aszArguments.Init();
	cCommand.GetArguments("Again", &aszArguments);
	AssertSize(MAX_SIZE, aszArguments.GetIndex("--resume"));
	aszArguments.Kill();

	AssertFalse(cCommand.IsRunning());
	AssertFalse(cCommand.Succeeded());
	cCommand.Kill();

	AssertString("claude", GetClaudeExecutable());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestClaudeCommand(void)
{
	BeginTests();

	TestClaudeCommandArguments();

	TestStatistics();
}

