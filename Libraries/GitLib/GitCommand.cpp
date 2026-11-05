/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela GitLib

Codaphela GitLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela GitLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela GitLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "GitCommand.h"


#define GIT_EXECUTABLE_LENGTH	1024


//The program run as git: "git" finds it on the path.
char gszGitExecutable[GIT_EXECUTABLE_LENGTH] = "git";

//Before the command's own arguments.  -C (and the repository) goes first.
const char* gaszGitOptions[] =
{
	"--no-pager",
	"--no-optional-locks",
	"-c", "core.quotepath=off",
	"-c", "color.ui=false",
	NULL
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitCommand::Init(void)
{
	mcProcess.Init();
	mszOutput.Init();
	mszErrors.Init();
	mbStarted = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A command still running is stopped.
//
//////////////////////////////////////////////////////////////////////////
void CGitCommand::Kill(void)
{
	mszErrors.Kill();
	mszOutput.Kill();
	mcProcess.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Starts git in szRepository (any folder in it) with paszArguments, the
// command and its own arguments, after the options it is always given.
// The last command's output and errors are cleared.  False if git
// couldn't be started or the last command is still running.
//
//////////////////////////////////////////////////////////////////////////
bool CGitCommand::Start(const char* szRepository, CArrayChars* paszArguments)
{
	CArrayChars	aszArguments;
	size		i;
	bool		bResult;

	if (mcProcess.IsRunning())
	{
		return false;
	}

	aszArguments.Init();
	aszArguments.Add("-C");
	aszArguments.Add(szRepository);
	for (i = 0; gaszGitOptions[i] != NULL; i++)
	{
		aszArguments.Add(gaszGitOptions[i]);
	}
	for (i = 0; i < paszArguments->NumElements(); i++)
	{
		aszArguments.Add(paszArguments->GetText(i));
	}

	mszOutput.Clear();
	mszErrors.Clear();
	bResult = mcProcess.StartCapture(gszGitExecutable, &aszArguments, NULL);
	aszArguments.Kill();
	mbStarted = bResult;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads what git has written so far.  True while it is running.
//
//////////////////////////////////////////////////////////////////////////
bool CGitCommand::Poll(void)
{
	return mcProcess.PollCapture(&mszOutput, &mszErrors);
}


//////////////////////////////////////////////////////////////////////////
//
// Starts git and waits for it.  True if it succeeded (exited with 0).
//
//////////////////////////////////////////////////////////////////////////
bool CGitCommand::Run(const char* szRepository, CArrayChars* paszArguments)
{
	if (!Start(szRepository, paszArguments))
	{
		return false;
	}
	mcProcess.WaitCapture(&mszOutput, &mszErrors);
	return Succeeded();
}


//////////////////////////////////////////////////////////////////////////
//
// Stops git; Poll finds it has failed.
//
//////////////////////////////////////////////////////////////////////////
void CGitCommand::Terminate(void)
{
	mcProcess.Terminate();
}


//////////////////////////////////////////////////////////////////////////
//
// True once git has exited with 0.  On Linux a git that couldn't be run
// exits with 127.
//
//////////////////////////////////////////////////////////////////////////
bool CGitCommand::Succeeded(void)
{
	return mbStarted && !mcProcess.IsRunning() && (mcProcess.GetExitCode() == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// szExecutable is run as git, with its folder if it isn't on the path.
//
//////////////////////////////////////////////////////////////////////////
void SetGitExecutable(const char* szExecutable)
{
	strncpy(gszGitExecutable, szExecutable, GIT_EXECUTABLE_LENGTH - 1);
	gszGitExecutable[GIT_EXECUTABLE_LENGTH - 1] = '\0';
}


bool CGitCommand::IsRunning(void) { return mcProcess.IsRunning(); }
int CGitCommand::GetExitCode(void) { return mcProcess.GetExitCode(); }
CChars* CGitCommand::GetOutput(void) { return &mszOutput; }  //Byte for byte: its Length counts '\0's in it.
CChars* CGitCommand::GetErrors(void) { return &mszErrors; }
char* GetGitExecutable(void) { return gszGitExecutable; }

