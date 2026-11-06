/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/Logger.h"
#include "CommandBuild.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CCommandBuild::Init(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot)
{
	CBuild::Init(pcSolution, pcConfiguration, szRoot);
	masCommands.Init();
	muiCommand = 0;
	mcProcess.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// A program still running is stopped.
//
//////////////////////////////////////////////////////////////////////////
void CCommandBuild::Kill(void)
{
	mcProcess.Kill();
	ClearCommands();
	masCommands.Kill();
	CBuild::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CCommandBuild::ClearCommands(void)
{
	size			i;
	SBuildCommand*	psCommand;

	for (i = 0; i < masCommands.NumElements(); i++)
	{
		psCommand = masCommands.Get(i);
		psCommand->szDescription.Kill();
		psCommand->szDirectory.Kill();
		psCommand->aszArguments.Kill();
		psCommand->szExecutable.Kill();
	}
	masCommands.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// A program to run, in szDirectory (NULL for this program's); add its
// arguments to the command's aszArguments.
//
//////////////////////////////////////////////////////////////////////////
SBuildCommand* CCommandBuild::AddCommand(const char* szExecutable, const char* szDirectory, const char* szDescription)
{
	SBuildCommand*	psCommand;

	psCommand = masCommands.Add();
	psCommand->szExecutable.Init(szExecutable);
	psCommand->aszArguments.Init();
	psCommand->szDirectory.Init(szDirectory ? szDirectory : "");
	psCommand->szDescription.Init(szDescription);
	return psCommand;
}


//////////////////////////////////////////////////////////////////////////
//
// Works out what to run and runs the first.  False, with why in
// paszLines, if the build couldn't start.
//
//////////////////////////////////////////////////////////////////////////
bool CCommandBuild::Start(CArrayChars* paszLines)
{
	ClearCommands();
	muiCommand = 0;
	meState = BS_Running;
	if (!AddCommands(paszLines))
	{
		Finish(BS_Failed, paszLines);
		return false;
	}
	if (masCommands.NumElements() == 0)
	{
		paszLines->Add("Nothing to build.");
		Finish(BS_Succeeded, paszLines);
		return true;
	}
	return StartCommand(paszLines);
}


//////////////////////////////////////////////////////////////////////////
//
// Runs command muiCommand, which is the progress.
//
//////////////////////////////////////////////////////////////////////////
bool CCommandBuild::StartCommand(CArrayChars* paszLines)
{
	SBuildCommand*	psCommand;
	CChars			szProgress;
	SLogConfig		sLogConfig;
	bool			bStarted;

	psCommand = masCommands.Get(muiCommand);
	szProgress.Init();
	szProgress.Append((uint32)(muiCommand + 1));
	szProgress.Append(" of ");
	szProgress.Append((uint32)masCommands.NumElements());
	szProgress.Append(": ");
	szProgress.Append(psCommand->szDescription);
	SetProgress(szProgress.Text());
	SetSteps(muiCommand, masCommands.NumElements());
	szProgress.Kill();
	paszLines->Add(psCommand->szDescription.Text());

	//A program that isn't there is the build's failure, said in its output, not the log's error.
	sLogConfig = gcLogger.SetSilent();
	bStarted = mcProcess.Start(psCommand->szExecutable.Text(), &psCommand->aszArguments, psCommand->szDirectory.Empty() ? NULL : psCommand->szDirectory.Text());
	gcLogger.SetConfig(&sLogConfig);
	if (!bStarted)
	{
		szProgress.Init("Could not run ");
		szProgress.Append(psCommand->szExecutable);
		szProgress.Append('.');
		paszLines->Add(szProgress.Text());
		szProgress.Kill();
		Finish(BS_Failed, paszLines);
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Adds what the running program has written.  When it exits the next
// runs, unless it failed.  Returns true while the build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CCommandBuild::Poll(CArrayChars* paszLines)
{
	CChars	szLine;

	if (meState != BS_Running)
	{
		return false;
	}
	if (mcProcess.Poll(paszLines))
	{
		return true;
	}

	if (mcProcess.GetExitCode() != 0)
	{
		szLine.Init(masCommands.Get(muiCommand)->szExecutable);
		szLine.Append(" exited with ");
		szLine.Append(mcProcess.GetExitCode());
		szLine.Append('.');
		paszLines->Add(szLine.Text());
		szLine.Kill();
		Finish(BS_Failed, paszLines);
		return false;
	}

	muiCommand++;
	if (muiCommand == masCommands.NumElements())
	{
		Finish(BS_Succeeded, paszLines);
		return false;
	}
	return StartCommand(paszLines);
}


//////////////////////////////////////////////////////////////////////////
//
// The running program is stopped; the build is cancelled.
//
//////////////////////////////////////////////////////////////////////////
void CCommandBuild::Cancel(void)
{
	CArrayChars	aszLines;

	if (meState != BS_Running)
	{
		return;
	}
	mcProcess.Terminate();
	aszLines.Init();
	mcProcess.Wait(&aszLines);
	aszLines.Kill();
	meState = BS_Cancelled;
	SetProgress("Cancelled.");
}


size CCommandBuild::NumCommands(void) { return masCommands.NumElements(); }
SBuildCommand* CCommandBuild::GetCommand(size uiIndex) { return masCommands.Get(uiIndex); }

