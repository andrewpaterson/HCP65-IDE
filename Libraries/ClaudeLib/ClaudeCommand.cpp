/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela ClaudeLib

Codaphela ClaudeLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela ClaudeLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela ClaudeLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "ClaudeCommand.h"


#define CLAUDE_EXECUTABLE_LENGTH	1024


//The program run as Claude Code: "claude" finds it on the path.
char gszClaudeExecutable[CLAUDE_EXECUTABLE_LENGTH] = "claude";


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::Init(void)
{
	mcProcess.Init();
	mcStream.Init();
	mszOutput.Init();
	mszErrors.Init();
	mszPermissionMode.Init("acceptEdits");
	mszAllowedTools.Init();
	mszModel.Init();
	mbStarted = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A request still running is stopped.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::Kill(void)
{
	mszModel.Kill();
	mszAllowedTools.Kill();
	mszPermissionMode.Kill();
	mszErrors.Kill();
	mszOutput.Kill();
	mcStream.Kill();
	mcProcess.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Claude Code's --permission-mode: acceptEdits, default, plan or
// bypassPermissions.  plan only reads and plans, changing nothing.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::SetPermissionMode(const char* szMode)
{
	mszPermissionMode.Clear();
	mszPermissionMode.Append(szMode);
}


//////////////////////////////////////////////////////////////////////////
//
// Tools allowed without asking as well as the mode's, comma separated in
// Claude Code's form, such as "Bash(git diff:*),Bash(msbuild:*)".
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::SetAllowedTools(const char* szTools)
{
	mszAllowedTools.Clear();
	mszAllowedTools.Append(szTools);
}


//////////////////////////////////////////////////////////////////////////
//
// A model's name or alias (opus, sonnet), "" for the user's own.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::SetModel(const char* szModel)
{
	mszModel.Clear();
	mszModel.Append(szModel);
}


//////////////////////////////////////////////////////////////////////////
//
// The arguments after the program: the request first, so an option
// taking a list (--allowedTools) can't take it as one of its own.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::GetArguments(const char* szRequest, CArrayChars* paszArguments)
{
	CChars*	pszRequest;

	paszArguments->Add("-p");

	//A request starting with '-' would be read as an option.
	pszRequest = paszArguments->Add();
	if (szRequest[0] == '-')
	{
		pszRequest->Append(' ');
	}
	pszRequest->Append(szRequest);

	paszArguments->Add("--output-format");
	paszArguments->Add("stream-json");
	paszArguments->Add("--verbose");
	paszArguments->Add("--permission-mode");
	paszArguments->Add(mszPermissionMode.Text());
	if (!mszModel.Empty())
	{
		paszArguments->Add("--model");
		paszArguments->Add(mszModel.Text());
	}
	if (mcStream.GetSessionId()[0] != '\0')
	{
		paszArguments->Add("--resume");
		paszArguments->Add(mcStream.GetSessionId());
	}
	if (!mszAllowedTools.Empty())
	{
		paszArguments->Add("--allowedTools");
		paszArguments->Add(mszAllowedTools.Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Starts Claude Code in szDirectory with szRequest, carrying the
// conversation on if there is one.  The last request's events, changed
// files and errors are cleared.  False if it couldn't be started or the
// last request is still running.
//
//////////////////////////////////////////////////////////////////////////
bool CClaudeCommand::Start(const char* szDirectory, const char* szRequest)
{
	CArrayChars	aszArguments;
	bool		bResult;

	if (mcProcess.IsRunning())
	{
		return false;
	}

	aszArguments.Init();
	GetArguments(szRequest, &aszArguments);

	mszOutput.Clear();
	mszErrors.Clear();
	mcStream.Restart();
	bResult = mcProcess.StartCapture(gszClaudeExecutable, &aszArguments, szDirectory);
	aszArguments.Kill();
	mbStarted = bResult;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Adds what Claude has done since the last Poll to paEvents.  True while
// it is running.
//
//////////////////////////////////////////////////////////////////////////
bool CClaudeCommand::Poll(CArrayClaudeEvents* paEvents)
{
	bool	bRunning;

	if (!mbStarted)
	{
		return false;
	}

	bRunning = mcProcess.PollCapture(&mszOutput, &mszErrors);
	if (!mszOutput.Empty())
	{
		mcStream.Append(mszOutput.Text(), mszOutput.Length(), paEvents);
		mszOutput.Clear();
	}
	if (!bRunning)
	{
		mcStream.End(paEvents);
	}
	return bRunning;
}


//////////////////////////////////////////////////////////////////////////
//
// Stops Claude; Poll finds it has failed.  What it changed before it was
// stopped stays changed.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::Terminate(void)
{
	mcProcess.Terminate();
}


//////////////////////////////////////////////////////////////////////////
//
// The next request starts a new conversation.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeCommand::Forget(void)
{
	mcStream.SetSessionId("");
}


//////////////////////////////////////////////////////////////////////////
//
// True once Claude has finished the request without an error.
//
//////////////////////////////////////////////////////////////////////////
bool CClaudeCommand::Succeeded(void)
{
	return mbStarted && !mcProcess.IsRunning() && (mcProcess.GetExitCode() == 0) && mcStream.Succeeded();
}


//////////////////////////////////////////////////////////////////////////
//
// szExecutable is run as Claude Code, with its folder if it isn't on the
// path.
//
//////////////////////////////////////////////////////////////////////////
void SetClaudeExecutable(const char* szExecutable)
{
	strncpy(gszClaudeExecutable, szExecutable, CLAUDE_EXECUTABLE_LENGTH - 1);
	gszClaudeExecutable[CLAUDE_EXECUTABLE_LENGTH - 1] = '\0';
}


bool CClaudeCommand::IsRunning(void) { return mcProcess.IsRunning(); }
bool CClaudeCommand::IsFinished(void) { return mcStream.IsFinished(); }  //Claude said it had finished the last request, well or not.
int CClaudeCommand::GetExitCode(void) { return mcProcess.GetExitCode(); }
char* CClaudeCommand::GetSessionId(void) { return mcStream.GetSessionId(); }  //"" for a new conversation.
void CClaudeCommand::SetSessionId(const char* szSessionId) { mcStream.SetSessionId(szSessionId); }  //To carry on a conversation from before.
CArrayChars* CClaudeCommand::GetChangedFiles(void) { return mcStream.GetChangedFiles(); }  //By the last request.
CChars* CClaudeCommand::GetErrors(void) { return &mszErrors; }
char* GetClaudeExecutable(void) { return gszClaudeExecutable; }

