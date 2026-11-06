/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ListenSocket.h"
#include "BaseLib/NativeThread.h"
#include "BaseLib/Timer.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "EmulatorDebugLib/DebugLaunch.h"
#include "IDEDebugger.h"


#define IDE_DEBUG_QUIT_WAIT		2000		//Milliseconds the emulator has to quit before it is ended.
#define IDE_DEBUG_START_WAIT	20000		//Milliseconds the emulator has to make its channel.
#define IDE_DEBUG_CHANNEL		"HCP65IDE"	//The start of the shared memory channel's name.
#define IDE_DEBUG_ENTRY_CODE	64			//The most bytes of a function's entry code before its first line.
#define IDE_DEBUG_AUTO_STEPS	64			//The most steps one F11 takes on its own (StepOnInto).


static size	guiIDEDebugChannels = 0;


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Init(void)
{
	mszEmulator.Init();
	mszEmulatorDirectory.Init();
	mszEmulatorArguments.Init();
	mszProcessor.Init();
	maszLoads.Init();
	mauiLoadAddresses.Init();
	mszBreakpointSpace.Init();
	muiBreakpointSpace = DEBUG_SPACE_ANY;
	mszOwnProgram.Init();
	mszCard.Init();
	muiSeenProcessEvents = 0;
	maszMessages.Init();
	mcProcess.Init();
	mbLaunched = false;
	mszChannel.Init();
	mcShared.Init();
	mcNetwork.Init();
	mbClient = false;
	mbStarting = false;
	miStartTime = 0;
	mbBreakAtStart = false;
	mbIntoLine = false;
	mszStepFile.Init();
	muiStepLine = 0;
	muiAutoSteps = 0;
	muiSeenStops = 0;
	muiSeenDisassemblies = 0;
	muiSeenErrors = 0;
	masBreakpoints.Init();
	masSourceBreakpoints.Init();
	for (muiLineTables = 0; muiLineTables < IDE_DEBUG_LINE_TABLES; muiLineTables++)
	{
		macLineTables[muiLineTables].Init();
		masTableTargets[muiLineTables].bOwn = false;
		masTableTargets[muiLineTables].szSpace.Init();
		masTableTargets[muiLineTables].szProcess.Init();
	}
	muiLineTables = 0;
	mszStatus.Init("Not debugging.");
}


//////////////////////////////////////////////////////////////////////////
//
// An emulator still being debugged is stopped (one Start ran quits).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Kill(void)
{
	size	ui;

	Stop();
	mszStepFile.Kill();
	mszStatus.Kill();
	for (ui = 0; ui < IDE_DEBUG_LINE_TABLES; ui++)
	{
		macLineTables[ui].Kill();
	}
	while (masSourceBreakpoints.NumElements() > 0)
	{
		RemoveSourceBreakpointAt(masSourceBreakpoints.NumElements() - 1);
	}
	masSourceBreakpoints.Kill();
	for (ui = 0; ui < IDE_DEBUG_LINE_TABLES; ui++)
	{
		masTableTargets[ui].szProcess.Kill();
		masTableTargets[ui].szSpace.Kill();
	}
	masBreakpoints.Kill();
	mcNetwork.Kill();
	mcShared.Kill();
	mszChannel.Kill();
	mcProcess.Kill();
	maszMessages.Kill();
	mszCard.Kill();
	mszOwnProgram.Kill();
	mszBreakpointSpace.Kill();
	mauiLoadAddresses.Kill();
	maszLoads.Kill();
	mszProcessor.Kill();
	mszEmulatorArguments.Kill();
	mszEmulatorDirectory.Kill();
	mszEmulator.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetEmulator(const char* szExecutable, const char* szDirectory, const char* szArguments)
{
	mszEmulator.Set(szExecutable);
	mszEmulatorDirectory.Set(szDirectory);
	mszEmulatorArguments.Set(szArguments ? szArguments : "");
}


//////////////////////////////////////////////////////////////////////////
//
// The processor the emulator must say it is ("W65C816"); NULL for any.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetProcessor(const char* szProcessor)
{
	mszProcessor.Set(szProcessor ? szProcessor : "");
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing for the next Start to load: the emulator runs what it has.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::ClearLoads(void)
{
	maszLoads.Kill();
	maszLoads.Init();
	mauiLoadAddresses.Kill();
	mauiLoadAddresses.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// A file the next Start loads into the emulator at uiAddress, after those
// added before it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::AddLoad(const char* szFile, uint32 uiAddress)
{
	maszLoads.Add(szFile);
	mauiLoadAddresses.Add(uiAddress);
}


//////////////////////////////////////////////////////////////////////////
//
// The breakpoint space ("Kernel", "User") the breakpoints are in when the
// emulator is next connected to; NULL or "" for every space.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetBreakpointSpace(const char* szSpace)
{
	mszBreakpointSpace.Set(szSpace ? szSpace : "");
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration's own program, as the emulator's kernal names the
// process running it ("\\boot.exe"); NULL or "" if it isn't one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetOwnProgram(const char* szProgram)
{
	mszOwnProgram.Set(szProgram ? szProgram : "");
}


//////////////////////////////////////////////////////////////////////////
//
// The disk image the next Start's emulator boots from (a full path: the
// emulator runs in its own directory); NULL or "" for the emulator's own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetCard(const char* szCard)
{
	mszCard.Set(szCard ? szCard : "");
}


//////////////////////////////////////////////////////////////////////////
//
// The process the own program's breakpoints are for: every one
// (DEBUG_PROCESS_ANY) if it isn't a program a process runs, or there is no
// breakpoint space to keep them in; its ID while it runs.  False while
// it isn't running: its breakpoints wait unbound.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::GetOwnProcess(uint32* puiProcess)
{
	*puiProcess = DEBUG_PROCESS_ANY;
	if (mszOwnProgram.Empty() || (muiBreakpointSpace == DEBUG_SPACE_ANY) || !mcClient.HasProcesses())
	{
		return true;
	}
	return mcClient.FindProcess(mszOwnProgram.Text(), puiProcess);
}


//////////////////////////////////////////////////////////////////////////
//
// The lines told since the last call, for the Build tab.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::TakeMessages(CArrayChars* paszDest)
{
	size	i;

	for (i = 0; i < maszMessages.NumElements(); i++)
	{
		paszDest->Add(maszMessages.GetText(i));
	}
	maszMessages.Kill();
	maszMessages.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the emulator on a shared memory channel of its own, holding before
// its first instruction until it is connected to.  It then runs to the
// first breakpoint, or (bBreakAtStart, as Step Into does when not
// debugging) stays at the first instruction.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Start(bool bBreakAtStart)
{
	CArrayChars	aszArguments;
	CArrayChars	aszExtra;
	CChars*		pszLine;
	size		i;
	bool		bResult;

	if (IsDebugging())
	{
		return false;
	}

	guiIDEDebugChannels++;
	mszChannel.Set(IDE_DEBUG_CHANNEL);
	mszChannel.Append((int)(GetDebugTickCount() % 1000000000));
	mszChannel.Append('-');
	mszChannel.Append((int)guiIDEDebugChannels);

	aszArguments.Init();
	aszArguments.Add(DEBUG_ARGUMENT_SHARED_MEMORY);
	aszArguments.Add(mszChannel.Text());
	aszArguments.Add(DEBUG_ARGUMENT_WAIT);
	aszArguments.Add(DEBUG_ARGUMENT_EXIT_ON_DETACH);
	if (!mszCard.Empty())
	{
		aszArguments.Add(DEBUG_ARGUMENT_CARD);
		aszArguments.Add(mszCard.Text());
	}
	aszExtra.Init();
	aszExtra.Split(mszEmulatorArguments.Text(), ' ');
	for (i = 0; i < aszExtra.NumElements(); i++)
	{
		if (aszExtra.GetText(i)[0] != '\0')
		{
			aszArguments.Add(aszExtra.GetText(i));
		}
	}
	aszExtra.Kill();

	//What is run, as a command line: an argument with a space is quoted.
	pszLine = maszMessages.Add("Debugging: ");
	pszLine->Append(mszEmulator);
	for (i = 0; i < aszArguments.NumElements(); i++)
	{
		pszLine->Append(strchr(aszArguments.GetText(i), ' ') ? " \"" : " ");
		pszLine->Append(aszArguments.GetText(i));
		if (strchr(aszArguments.GetText(i), ' '))
		{
			pszLine->Append('"');
		}
	}
	maszMessages.Add("In: ")->Append(mszEmulatorDirectory);

	mcProcess.Kill();
	mcProcess.Init();
	bResult = mcProcess.Start(mszEmulator.Text(), &aszArguments, mszEmulatorDirectory.Text());
	aszArguments.Kill();
	if (!bResult)
	{
		mszStatus.Set("Could not start the emulator.");
		maszMessages.Add("Could not start the emulator.");
		return false;
	}

	mbLaunched = true;
	mbStarting = true;
	miStartTime = GetDebugTickCount();
	mbBreakAtStart = bBreakAtStart;
	mszStatus.Set("Starting the emulator.");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// To an emulator already running with a network port.  It goes on as it
// was, running or stopped.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Attach(int iPort)
{
	if (IsDebugging())
	{
		return false;
	}

	mbLaunched = false;
	mbBreakAtStart = true;
	maszMessages.Add("Attaching to the emulator on port ")->Append((int32)iPort);
	mcNetwork.Kill();
	mcNetwork.Init();
	if (!mcNetwork.Connect(NETWORK_LOOPBACK_ADDRESS, iPort) || !Connect(&mcNetwork))
	{
		mszStatus.Set("Could not attach to the emulator.");
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Connect(CProcessChannel* pcChannel)
{
	mcClient.Init(pcChannel);
	mbClient = true;
	mbStarting = false;
	muiSeenStops = 0;
	muiSeenProcessEvents = 0;
	muiSeenDisassemblies = 0;
	muiSeenErrors = 0;
	mszStatus.Set("Connecting to the emulator.");
	return mcClient.Hello() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Ends debugging.  An emulator Start ran is quit (and ended if it doesn't
// go); one attached to is detached from and runs on.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Stop(void)
{
	CArrayChars	aszLines;
	int64		iStart;

	if (mbClient)
	{
		if (mbLaunched)
		{
			mcClient.Quit();
		}
		else
		{
			mcClient.Detach();
		}
	}

	if (mbLaunched && mcProcess.IsRunning())
	{
		aszLines.Init();
		iStart = GetDebugTickCount();
		while (mcProcess.Poll(&aszLines) && (GetDebugTickCount() - iStart < IDE_DEBUG_QUIT_WAIT))
		{
			ThreadSleep(10);
		}
		aszLines.Kill();
		mcProcess.Terminate();
	}
	End("Not debugging.");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::End(const char* szStatus)
{
	if (mbClient)
	{
		mcClient.Kill();
		mbClient = false;
	}
	mcShared.Kill();
	mcShared.Init();
	mcNetwork.Kill();
	mcNetwork.Init();
	mbLaunched = false;
	mbStarting = false;
	mszStatus.Set(szStatus);
}


//////////////////////////////////////////////////////////////////////////
//
// After a step into from a source line, stopped at uiAddress: stepping
// goes on, as Visual Studio's F11 does, until it is on a new line.  Code
// with no line just before a line (a function's entry code) is stepped
// over to the line; a function with no lines (a runtime routine) is
// stepped out of; back in the line it stepped from (a call it went over
// returned into it), the rest of the line is stepped.  True if it is
// stepping on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepOnInto(uint32 uiAddress)
{
	SDebugRange	sRange;
	CChars		szFile;
	uint32		uiLine;
	uint32		uiNext;
	size		ui;
	bool		bLine;
	bool		bSameLine;

	if (!mbIntoLine)
	{
		return false;
	}
	mbIntoLine = false;
	muiAutoSteps++;
	if ((mcClient.GetStopReason() != DS_Step) || (muiAutoSteps > IDE_DEBUG_AUTO_STEPS))
	{
		return false;
	}

	szFile.Init();
	bLine = FindSourceLine(uiAddress, &szFile, &uiLine);
	bSameLine = bLine && (uiLine == muiStepLine) && szFile.EqualsIgnoreCase(mszStepFile.Text());
	szFile.Kill();
	if (bSameLine)
	{
		mszStatus.Set("Stepping the rest of the line.");
		return StepLineRanges(false);
	}
	if (bLine)
	{
		return false;
	}

	for (ui = 0; ui < muiLineTables; ui++)
	{
		if (macLineTables[ui].FindNextLine(uiAddress, &uiNext) && (uiNext - uiAddress <= IDE_DEBUG_ENTRY_CODE))
		{
			sRange.uiStart = uiAddress;
			sRange.uiEnd = uiNext;
			mszStatus.Set("Stepping into the function's first line.");
			mbIntoLine = mcClient.StepRange(&sRange, 1, true) != 0;
			return mbIntoLine;
		}
	}
	mszStatus.Set("Stepping over a function with no source lines.");
	mbIntoLine = mcClient.StepOut() != 0;
	return mbIntoLine;
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's answers say when it has done what was asked: nothing more
// is asked of it until then.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::CanMove(void)
{
	return mbClient && !mcClient.IsBusy() && (mcClient.GetState() != DCS_Connecting) && (mcClient.GetState() != DCS_Closed);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Continue(void)
{
	if (!CanMove() || !mcClient.IsStopped())
	{
		return false;
	}
	mszStatus.Set("Running.");
	return mcClient.Continue() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Break All: the emulator stops where it is, even part way through a step.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Break(void)
{
	if (!mbClient || !mcClient.IsRunning())
	{
		return false;
	}
	return mcClient.Pause() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepInto(void)
{
	if (!CanMove() || !mcClient.IsStopped())
	{
		return false;
	}
	return mcClient.Step(1) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction, or the whole of a call (the emulator decides which).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepOver(void)
{
	if (!CanMove() || !mcClient.IsStopped())
	{
		return false;
	}
	return mcClient.StepOver() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The rest of the source line it is stopped on (DM_StepRange over the
// line's code in every place it is): stepping over the calls in it with
// bOver, into the first one otherwise, as Visual Studio's F10 and F11.
// Stepping in, a function with no lines is stepped over (StepOnInto).
// Stopped where no line table has a line, one instruction is stepped.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepLine(bool bOver)
{
	if (!CanMove() || !mcClient.IsStopped())
	{
		return false;
	}
	muiAutoSteps = 0;
	if (!FindSourceLine(GetProgramAddress(), &mszStepFile, &muiStepLine))
	{
		return bOver ? StepOver() : StepInto();
	}
	return StepLineRanges(bOver);
}


//////////////////////////////////////////////////////////////////////////
//
// DM_StepRange over the code of the line the program address is on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepLineRanges(bool bOver)
{
	CArrayUint32	auiRanges;
	SDebugRange*	asRanges;
	size			ui;
	size			uiRanges;
	bool			bResult;

	auiRanges.Init();
	for (ui = 0; (ui < muiLineTables) && (auiRanges.NumElements() == 0); ui++)
	{
		macLineTables[ui].FindLineRanges(GetProgramAddress(), &auiRanges);
	}
	uiRanges = auiRanges.NumElements() / 2;
	if ((uiRanges == 0) || (uiRanges > DEBUG_MAX_LIST))
	{
		auiRanges.Kill();
		return bOver ? StepOver() : StepInto();
	}

	asRanges = (SDebugRange*)malloc(uiRanges * sizeof(SDebugRange));
	for (ui = 0; ui < uiRanges; ui++)
	{
		asRanges[ui].uiStart = auiRanges.GetValue(ui * 2);
		asRanges[ui].uiEnd = auiRanges.GetValue(ui * 2 + 1);
	}
	bResult = mcClient.StepRange(asRanges, uiRanges, bOver) != 0;
	mbIntoLine = bResult && !bOver;
	free(asRanges);
	auiRanges.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Until the function it is in returns.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepOut(void)
{
	if (!CanMove() || !mcClient.IsStopped())
	{
		return false;
	}
	return mcClient.StepOut() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The HCP65 is reset, then runs again (or stops at its first instruction
// if debugging started by stepping into it, or was attached).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Restart(void)
{
	if (!CanMove())
	{
		return false;
	}
	return mcClient.Reset(!mbBreakAtStart) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Adds a breakpoint at uiAddress, or removes the one there.  Returns
// whether there is one now.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::ToggleBreakpoint(uint32 uiAddress)
{
	SIDEBreakpoint*	psBreakpoint;

	psBreakpoint = FindBreakpoint(uiAddress);
	if (psBreakpoint == NULL)
	{
		return AddBreakpoint(uiAddress);
	}

	masBreakpoints.RemoveAt(masBreakpoints.GetIndex(psBreakpoint));
	BreakpointsChanged();
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::AddBreakpoint(uint32 uiAddress)
{
	SIDEBreakpoint*	psBreakpoint;

	if (FindBreakpoint(uiAddress) != NULL)
	{
		return true;
	}
	psBreakpoint = masBreakpoints.Add();
	psBreakpoint->uiAddress = uiAddress;
	psBreakpoint->bEnabled = true;
	BreakpointsChanged();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::DeleteAllBreakpoints(void)
{
	masBreakpoints.Kill();
	masBreakpoints.Init();
	while (masSourceBreakpoints.NumElements() > 0)
	{
		RemoveSourceBreakpointAt(masSourceBreakpoints.NumElements() - 1);
	}
	BreakpointsChanged();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SIDEBreakpoint* CIDEDebugger::FindBreakpoint(uint32 uiAddress)
{
	size			i;
	SIDEBreakpoint*	psBreakpoint;

	for (i = 0; i < masBreakpoints.NumElements(); i++)
	{
		psBreakpoint = masBreakpoints.Get(i);
		if (psBreakpoint->uiAddress == uiAddress)
		{
			return psBreakpoint;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SendBreakpoints(void)
{
	CArrayTemplate<SDebugBreakpoint>	asBreakpoints;
	SDebugBreakpoint*					psBreakpoint;
	SIDESourceBreakpoint*				psSource;
	size								i;
	size								j;
	size								k;
	uint32								uiSpace;
	uint32								uiProcess;
	uint32								uiOwnProcess;
	bool								bOwnRunning;

	//The own program's breakpoints are its process's, and wait while it isn't running.
	bOwnRunning = GetOwnProcess(&uiOwnProcess);
	asBreakpoints.Init();
	for (i = 0; bOwnRunning && (i < masBreakpoints.NumElements()); i++)
	{
		psBreakpoint = asBreakpoints.Add();
		psBreakpoint->uiAddress = masBreakpoints.Get(i)->uiAddress;
		psBreakpoint->uiEnabled = masBreakpoints.Get(i)->bEnabled;
		psBreakpoint->uiSpace = muiBreakpointSpace;
		psBreakpoint->uiProcess = uiOwnProcess;
	}

	//Each place a source breakpoint's line starts, once.
	for (i = 0; i < masSourceBreakpoints.NumElements(); i++)
	{
		psSource = masSourceBreakpoints.Get(i);
		for (j = 0; j < psSource->auiAddresses.NumElements(); j++)
		{
			if (!GetTableTarget(psSource->auiTables.GetValue(j), bOwnRunning, uiOwnProcess, &uiSpace, &uiProcess))
			{
				continue;
			}
			for (k = 0; k < asBreakpoints.NumElements(); k++)
			{
				psBreakpoint = asBreakpoints.Get(k);
				if ((psBreakpoint->uiAddress == psSource->auiAddresses.GetValue(j)) && (psBreakpoint->uiSpace == uiSpace) && (psBreakpoint->uiProcess == uiProcess))
				{
					break;
				}
			}
			if (k == asBreakpoints.NumElements())
			{
				psBreakpoint = asBreakpoints.Add();
				psBreakpoint->uiAddress = psSource->auiAddresses.GetValue(j);
				psBreakpoint->uiEnabled = psSource->bEnabled;
				psBreakpoint->uiSpace = uiSpace;
				psBreakpoint->uiProcess = uiProcess;
			}
			else if (psSource->bEnabled)
			{
				psBreakpoint->uiEnabled = true;
			}
		}
	}
	mcClient.SetBreakpoints(asBreakpoints.GetData(), asBreakpoints.NumElements());
	asBreakpoints.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator, if one is connected, is given all the breakpoints again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::BreakpointsChanged(void)
{
	if (mbClient && (mcClient.GetState() != DCS_Connecting))
	{
		SendBreakpoints();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The line tables the source breakpoints are bound with go; they aren't
// bound until a table is added.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::ClearLineTables(void)
{
	size	ui;

	for (ui = 0; ui < muiLineTables; ui++)
	{
		macLineTables[ui].Clear();
		masTableTargets[ui].bOwn = false;
		masTableTargets[ui].szSpace.Clear();
		masTableTargets[ui].szProcess.Clear();
	}
	muiLineTables = 0;
	BindSourceBreakpoints();
}


//////////////////////////////////////////////////////////////////////////
//
// The line table a build wrote beside szProgram (<program>.lines) binds
// the source breakpoints too.  bOwnSpace: it is the configuration's own
// program, whose breakpoints are in the breakpoint space; otherwise
// (what is loaded with it, a kernal) they are in every space.  False if
// there is no such table or there are too many.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::AddLineTable(const char* szProgram, bool bOwnSpace)
{
	if (!ReadLineTable(szProgram))
	{
		return false;
	}
	masTableTargets[muiLineTables].bOwn = bOwnSpace;
	muiLineTables++;
	BindSourceBreakpoints();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The line table of another program than the configuration's own (one of
// the workspace's other solutions'), which the kernal runs as the process
// szProcess ("\\modplay.exe"): its source breakpoints are in the space
// szSpace ("User"; every space if empty or the emulator hasn't it) and
// stop only in its process, waiting while it isn't running.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::AddProcessLineTable(const char* szProgram, const char* szSpace, const char* szProcess)
{
	if (!ReadLineTable(szProgram))
	{
		return false;
	}
	masTableTargets[muiLineTables].bOwn = false;
	masTableTargets[muiLineTables].szSpace.Set(szSpace ? szSpace : "");
	masTableTargets[muiLineTables].szProcess.Set(szProcess ? szProcess : "");
	muiLineTables++;
	BindSourceBreakpoints();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The next table read from beside szProgram (<program>.lines), not yet
// counted.  False if there is none or there are too many.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::ReadLineTable(const char* szProgram)
{
	CChars	szFile;
	bool	bResult;

	if (muiLineTables >= IDE_DEBUG_LINE_TABLES)
	{
		return false;
	}
	szFile.Init(szProgram);
	szFile.Append(".lines");
	bResult = macLineTables[muiLineTables].ReadFile(szFile.Text());
	szFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The space and process a line table's breakpoints are given in.  False
// while its program isn't running: they wait.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::GetTableTarget(size uiTable, bool bOwnRunning, uint32 uiOwnProcess, uint32* puiSpace, uint32* puiProcess)
{
	SIDELineTableTarget*	psTarget;

	psTarget = &masTableTargets[uiTable];
	if (psTarget->bOwn)
	{
		*puiSpace = muiBreakpointSpace;
		*puiProcess = uiOwnProcess;
		return bOwnRunning;
	}

	*puiSpace = DEBUG_SPACE_ANY;
	*puiProcess = DEBUG_PROCESS_ANY;
	if (psTarget->szSpace.Empty() || !mcClient.FindBreakpointSpace(psTarget->szSpace.Text(), puiSpace))
	{
		*puiSpace = DEBUG_SPACE_ANY;
		return true;
	}
	if (psTarget->szProcess.Empty() || !mcClient.HasProcesses())
	{
		return true;
	}
	return mcClient.FindProcess(psTarget->szProcess.Text(), puiProcess);
}


//////////////////////////////////////////////////////////////////////////
//
// A line table is of the program szProcess runs.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::IsTableProcess(const char* szProcess)
{
	size	i;

	for (i = 0; i < muiLineTables; i++)
	{
		if (!masTableTargets[i].bOwn && !masTableTargets[i].szProcess.Empty() && (StringInsensitiveCompare(masTableTargets[i].szProcess.Text(), szProcess) == 0))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The source line of the code at uiAddress, from the first line table
// that has one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::FindSourceLine(uint32 uiAddress, CChars* pszFile, uint32* puiLine)
{
	size				ui;
	size				uiRank;
	SAsm65LineEntry*	psLine;

	//Programs in different spaces have code at the same addresses: the tables of where it stopped are looked in, the
	//most particular first.
	for (uiRank = 0; uiRank < IDE_TABLE_RANK_NONE; uiRank++)
	{
		for (ui = 0; ui < muiLineTables; ui++)
		{
			if (GetTableRank(ui) != uiRank)
			{
				continue;
			}
			psLine = macLineTables[ui].FindLine(uiAddress);
			if (psLine)
			{
				pszFile->Set(macLineTables[ui].GetFile(psLine->uiFile));
				*puiLine = psLine->uiLine;
				return true;
			}
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// How closely a line table is of where the emulator last stopped (the
// space and process it said): IDE_TABLE_RANK_PROCESS, a program of the
// process it stopped in; IDE_TABLE_RANK_SPACE, of its space;
// IDE_TABLE_RANK_ANY, of every space (a kernal), or when not debugging or
// the emulator doesn't say; IDE_TABLE_RANK_NONE, of another space or
// process (or of a program that isn't running).
//
//////////////////////////////////////////////////////////////////////////
size CIDEDebugger::GetTableRank(size uiTable)
{
	uint32	uiStopSpace;
	uint32	uiStopProcess;
	uint32	uiOwnProcess;
	uint32	uiSpace;
	uint32	uiProcess;
	bool	bOwnRunning;
	bool	bRunning;

	if (!mbClient || (mcClient.NumStops() == 0))
	{
		return IDE_TABLE_RANK_ANY;
	}
	uiStopSpace = mcClient.GetStopSpace();
	uiStopProcess = mcClient.GetStopProcess();
	bOwnRunning = GetOwnProcess(&uiOwnProcess);
	bRunning = GetTableTarget(uiTable, bOwnRunning, uiOwnProcess, &uiSpace, &uiProcess);
	if ((uiStopSpace == DEBUG_SPACE_ANY) || (uiSpace == DEBUG_SPACE_ANY))
	{
		return IDE_TABLE_RANK_ANY;
	}
	if (uiSpace != uiStopSpace)
	{
		return IDE_TABLE_RANK_NONE;
	}
	if (!bRunning)
	{
		return (uiStopProcess == DEBUG_PROCESS_ANY) ? IDE_TABLE_RANK_SPACE : IDE_TABLE_RANK_NONE;
	}
	if ((uiProcess == DEBUG_PROCESS_ANY) || (uiStopProcess == DEBUG_PROCESS_ANY))
	{
		return IDE_TABLE_RANK_SPACE;
	}
	return (uiProcess == uiStopProcess) ? IDE_TABLE_RANK_PROCESS : IDE_TABLE_RANK_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// F9 on line uiLine (1 based) of szFile: the breakpoint there, set on that
// line or bound to it, goes; otherwise one is added.  True if there is
// one there now.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::ToggleSourceBreakpoint(const char* szFile, uint32 uiLine)
{
	SIDESourceBreakpoint*	psBreakpoint;

	psBreakpoint = FindSourceBreakpoint(szFile, uiLine);
	if (psBreakpoint)
	{
		RemoveSourceBreakpointAt(masSourceBreakpoints.GetIndex(psBreakpoint));
		BreakpointsChanged();
		return false;
	}
	AddSourceBreakpoint(szFile, uiLine, true);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A breakpoint on line uiLine (1 based) of szFile, bound if the line
// tables can (as saved ones are given back).
//
//////////////////////////////////////////////////////////////////////////
SIDESourceBreakpoint* CIDEDebugger::AddSourceBreakpoint(const char* szFile, uint32 uiLine, bool bEnabled)
{
	SIDESourceBreakpoint*	psBreakpoint;

	psBreakpoint = masSourceBreakpoints.Add();
	psBreakpoint->szFile.Init(szFile);
	psBreakpoint->uiLine = uiLine;
	psBreakpoint->bEnabled = bEnabled;
	psBreakpoint->uiBoundLine = 0;
	psBreakpoint->auiAddresses.Init();
	psBreakpoint->auiTables.Init();
	BindSourceBreakpoint(psBreakpoint);
	BreakpointsChanged();
	return psBreakpoint;
}


//////////////////////////////////////////////////////////////////////////
//
// The breakpoint of szFile set on line uiLine, or bound to it.
//
//////////////////////////////////////////////////////////////////////////
SIDESourceBreakpoint* CIDEDebugger::FindSourceBreakpoint(const char* szFile, uint32 uiLine)
{
	size					ui;
	SIDESourceBreakpoint*	psBreakpoint;

	for (ui = 0; ui < masSourceBreakpoints.NumElements(); ui++)
	{
		psBreakpoint = masSourceBreakpoints.Get(ui);
		if (psBreakpoint->szFile.EqualsIgnoreCase(szFile) && ((psBreakpoint->uiLine == uiLine) || (psBreakpoint->uiBoundLine == uiLine)))
		{
			return psBreakpoint;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::RemoveSourceBreakpointAt(size uiIndex)
{
	SIDESourceBreakpoint*	psBreakpoint;

	psBreakpoint = masSourceBreakpoints.Get(uiIndex);
	psBreakpoint->auiTables.Kill();
	psBreakpoint->auiAddresses.Kill();
	psBreakpoint->szFile.Kill();
	masSourceBreakpoints.RemoveAt(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// Every source breakpoint bound again with the line tables there are now,
// and given to the emulator if one is connected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::BindSourceBreakpoints(void)
{
	size	ui;

	for (ui = 0; ui < masSourceBreakpoints.NumElements(); ui++)
	{
		BindSourceBreakpoint(masSourceBreakpoints.Get(ui));
	}
	BreakpointsChanged();
}


//////////////////////////////////////////////////////////////////////////
//
// The breakpoint's line's code in every line table that has the file:
// each place it starts, or the next line's with code.  The first table
// with the file says which line that is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::BindSourceBreakpoint(SIDESourceBreakpoint* psBreakpoint)
{
	size	ui;
	size	uiAddresses;
	size	j;
	uint32	uiLine;

	psBreakpoint->auiAddresses.ReInit();
	psBreakpoint->auiTables.ReInit();
	psBreakpoint->uiBoundLine = 0;
	for (ui = 0; ui < muiLineTables; ui++)
	{
		uiAddresses = psBreakpoint->auiAddresses.NumElements();
		uiLine = macLineTables[ui].FindLineAddresses(psBreakpoint->szFile.Text(), (psBreakpoint->uiBoundLine != 0) ? psBreakpoint->uiBoundLine : psBreakpoint->uiLine, &psBreakpoint->auiAddresses);
		if ((uiLine != 0) && (psBreakpoint->uiBoundLine != 0) && (uiLine != psBreakpoint->uiBoundLine))
		{
			//Another table moved it to a different line: only the first table's line is the breakpoint's.
			psBreakpoint->auiAddresses.SetUsedElements(uiAddresses);
			continue;
		}
		if (uiLine != 0)
		{
			psBreakpoint->uiBoundLine = uiLine;
		}
		for (j = uiAddresses; j < psBreakpoint->auiAddresses.NumElements(); j++)
		{
			psBreakpoint->auiTables.Add((uint32)ui);
		}
	}
}


size CIDEDebugger::NumLineTables(void) { return muiLineTables; }
CAsm65LineTable* CIDEDebugger::GetLineTable(size uiIndex) { return (uiIndex < muiLineTables) ? &macLineTables[uiIndex] : NULL; }
size CIDEDebugger::NumSourceBreakpoints(void) { return masSourceBreakpoints.NumElements(); }
SIDESourceBreakpoint* CIDEDebugger::GetSourceBreakpoint(size uiIndex) { return masSourceBreakpoints.SafeGet(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The emulator's output lines go in paszOutput.  True if anything shown
// changed: the state, a stop, the instructions or the status.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Poll(CArrayChars* paszOutput)
{
	EIDEDebugState	eOldState;
	size			uiOldDisassemblies;
	bool			bRunning;

	eOldState = GetState();
	uiOldDisassemblies = muiSeenDisassemblies;
	TakeMessages(paszOutput);

	if (mbLaunched)
	{
		bRunning = mcProcess.Poll(paszOutput);
		if (!bRunning)
		{
			End("The emulator exited.");
			return true;
		}
	}

	if (mbStarting)
	{
		//The emulator makes the channel once it has started.
		if (CSharedMemoryChannel::Exists(mszChannel.Text()))
		{
			if (mcShared.Connect(mszChannel.Text()))
			{
				Connect(&mcShared);
			}
			else
			{
				//Made but not finished: tried again next time.
				mcShared.Kill();
				mcShared.Init();
			}
		}
		else if (GetDebugTickCount() - miStartTime > IDE_DEBUG_START_WAIT)
		{
			Stop();
			mszStatus.Set("The emulator didn't make its debug channel.");
			return true;
		}
		return GetState() != eOldState;
	}

	if (!mbClient)
	{
		return GetState() != eOldState;
	}

	mcClient.Poll(0);
	if (mcClient.GetState() == DCS_Closed)
	{
		End(mbLaunched ? "The emulator closed the connection." : "Detached: the emulator went.");
		return true;
	}

	if ((eOldState == IDS_Connecting) && (mcClient.GetState() != DCS_Connecting))
	{
		Welcomed();
		if (!mbClient)
		{
			//The wrong processor: it was let go.
			TakeMessages(paszOutput);
			return true;
		}
	}
	else
	{
		TellProcesses(paszOutput);
		if (mcClient.NumStops() != muiSeenStops)
		{
			if (mcClient.GetStopReason() == DS_Process)
			{
				//A process started: it has its breakpoints now, and goes on.
				muiSeenStops = mcClient.NumStops();
				Continue();
			}
			else
			{
				Stopped();
			}
		}
	}
	if (mcClient.IsRunning())
	{
		mszStatus.Set(mcClient.IsStepping() ? "Stepping." : "Running.");
	}

	if (mcClient.NumDisassemblies() != muiSeenDisassemblies)
	{
		muiSeenDisassemblies = mcClient.NumDisassemblies();
	}
	if (mcClient.NumErrors() != muiSeenErrors)
	{
		muiSeenErrors = mcClient.NumErrors();
		paszOutput->Add("Debugger: ")->Append(mcClient.GetError());
	}

	return (GetState() != eOldState) || (muiSeenDisassemblies != uiOldDisassemblies);
}


//////////////////////////////////////////////////////////////////////////
//
// Each process the emulator has said started or ended since the last
// time, in the Build tab; the own program's breakpoints are given again
// (to its new process, or unbound) when its process starts or ends.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::TellProcesses(CArrayChars* paszOutput)
{
	SDebugProcessEvent*	psEvent;
	CChars*				pszLine;
	bool				bOwn;
	char				szID[16];

	bOwn = false;
	while (muiSeenProcessEvents < mcClient.NumProcessEvents())
	{
		psEvent = mcClient.GetProcessEvent(muiSeenProcessEvents);
		muiSeenProcessEvents++;
		snprintf(szID, sizeof(szID), "Process $%02X ", (unsigned int)psEvent->sProcess.uiProcess);
		pszLine = paszOutput->Add(szID);
		pszLine->Append(psEvent->bStarted ? "started: " : "ended: ")->Append(psEvent->sProcess.szName);
		if (!psEvent->bStarted)
		{
			pszLine->Append(" (exit code ")->Append((int32)psEvent->sProcess.iExitCode)->Append(')');
		}
		pszLine->Append('.');
		bOwn |= !mszOwnProgram.Empty() && (StringInsensitiveCompare(psEvent->sProcess.szName, mszOwnProgram.Text()) == 0);
		bOwn |= IsTableProcess(psEvent->sProcess.szName);
	}
	if (bOwn)
	{
		SendBreakpoints();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's welcome and state have arrived.  If it is the processor
// wanted it is given the breakpoints, and if it waited for the debugger it
// is continued.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Welcomed(void)
{
	CChars	sz;
	size	i;

	if (!mszProcessor.Empty() && !mszProcessor.Equals(mcClient.GetProcessorName()))
	{
		//The wrong emulator for what is being debugged.
		sz.Init("The emulator is a ");
		sz.Append(mcClient.GetProcessorName());
		sz.Append(" but the configuration is built for a ");
		sz.Append(mszProcessor);
		sz.Append('.');
		maszMessages.Add(sz.Text());
		Stop();
		mszStatus.Set(sz.Text());
		sz.Kill();
		return;
	}

	maszMessages.Add("Debugging a ")->Append(mcClient.GetProcessorName());
	muiBreakpointSpace = DEBUG_SPACE_ANY;
	if (!mszBreakpointSpace.Empty())
	{
		if (mcClient.FindBreakpointSpace(mszBreakpointSpace.Text(), &muiBreakpointSpace))
		{
			maszMessages.Add("Breakpoints are in ")->Append(mcClient.GetBreakpointSpaceName(muiBreakpointSpace - 1))->Append(" mode.");
		}
		else
		{
			muiBreakpointSpace = DEBUG_SPACE_ANY;
			maszMessages.Add("The emulator has no breakpoint space ")->Append(mszBreakpointSpace)->Append(": breakpoints stop in every one.");
		}
	}
	SendBreakpoints();
	if (mbLaunched && (maszLoads.NumElements() != 0) && !mcClient.IsRunning() && (mcClient.GetStopReason() == DS_Start))
	{
		//What is loaded goes in before anything runs, and the reset starts it (or stops at its first instruction).
		muiSeenStops = mcClient.NumStops();
		for (i = 0; i < maszLoads.NumElements(); i++)
		{
			if (!LoadFile(maszLoads.GetText(i), *mauiLoadAddresses.Get(i)))
			{
				Stop();
				return;
			}
		}
		if (mcClient.Reset(!mbBreakAtStart) == 0)
		{
			Stop();
			return;
		}
		mszStatus.Set("Loading.");
		return;
	}
	if (mcClient.IsRunning())
	{
		muiSeenStops = mcClient.NumStops();
		mszStatus.Set("Running.");
		return;
	}

	Stopped();
	if (!mbBreakAtStart && (mcClient.GetStopReason() == DS_Start))
	{
		Continue();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szFile sent to the emulator to load at uiAddress.  False, said in the
// messages, if it can't be read or sent.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::LoadFile(const char* szFile, uint32 uiAddress)
{
	CFileUtil	cFileUtil;
	void*		pvData;
	size		uiSize;
	bool		bResult;
	char		szLine[64];

	pvData = cFileUtil.ReadAll(szFile, &uiSize);
	if (!pvData)
	{
		maszMessages.Add("Could not read ")->Append(szFile)->Append('.');
		mszStatus.Set("Could not read the program to load.");
		return false;
	}
	bResult = (uiSize > 0) && (mcClient.Load(uiAddress, pvData, uiSize) != 0);
	free(pvData);
	if (!bResult)
	{
		maszMessages.Add("Could not load ")->Append(szFile)->Append(" into the emulator.");
		mszStatus.Set("Could not load the program.");
		return false;
	}

	snprintf(szLine, sizeof(szLine), "Loaded %u bytes at $%06X from ", (unsigned int)uiSize, (unsigned int)uiAddress);
	maszMessages.Add(szLine)->Append(szFile)->Append('.');
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A new stop: why, and the instructions there are asked for.  They start
// where the ones shown do while the program address is among them (the
// emulator starts them at it if it isn't).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Stopped(void)
{
	uint32				uiAddress;
	uint32				uiStart;
	SDebugInstruction*	psLast;

	muiSeenStops = mcClient.NumStops();
	uiAddress = mcClient.GetStopAddress();
	if (StepOnInto(uiAddress))
	{
		return;
	}
	switch (mcClient.GetStopReason())
	{
	case DS_Start:		mszStatus.Set("Stopped before the first instruction at ");	break;
	case DS_Breakpoint:	mszStatus.Set("Stopped at a breakpoint at ");				break;
	case DS_Step:		mszStatus.Set("Stepped to ");								break;
	case DS_Pause:		mszStatus.Set("Broke in at ");								break;
	case DS_Stopped:	mszStatus.Set("The processor stopped itself at ");			break;
	case DS_Reset:		mszStatus.Set("Reset, stopped at ");						break;
	}
	mcClient.AppendAddress(&mszStatus, uiAddress);
	mszStatus.Append('.');

	if (!mcClient.CanDecode())
	{
		return;
	}
	uiStart = uiAddress;
	if (mcClient.NumInstructions() != 0)
	{
		//Not the last few, so the instructions after it are shown.
		psLast = mcClient.GetInstruction(mcClient.NumInstructions() - 1);
		if ((uiAddress >= mcClient.GetInstruction(0)->uiAddress) && (uiAddress + IDE_DEBUG_DISASSEMBLY < psLast->uiAddress))
		{
			uiStart = mcClient.GetInstruction(0)->uiAddress;
		}
	}
	mcClient.Disassemble(uiStart, IDE_DEBUG_DISASSEMBLY, uiAddress);
}


//////////////////////////////////////////////////////////////////////////
//
// Idle, starting and connecting are the IDE's; the rest are what the
// emulator has said (stepping while a step it was asked for isn't done).
//
//////////////////////////////////////////////////////////////////////////
EIDEDebugState CIDEDebugger::GetState(void)
{
	if (mbStarting)
	{
		return IDS_Starting;
	}
	if (!mbClient)
	{
		return IDS_Idle;
	}
	if (mcClient.GetState() == DCS_Connecting)
	{
		return IDS_Connecting;
	}
	if (mcClient.IsRunning() && !mcClient.IsStepping())
	{
		return IDS_Running;
	}
	if (mcClient.IsBusy() || mcClient.IsStepping())
	{
		return IDS_Stepping;
	}
	return IDS_Stopped;
}


//////////////////////////////////////////////////////////////////////////
//
// An address as the emulator says to write it, or plain hexadecimal when
// there is no emulator.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::AppendAddress(CChars* psz, uint32 uiAddress)
{
	char	sz[16];

	if (mbClient && (mcClient.GetState() != DCS_Connecting))
	{
		mcClient.AppendAddress(psz, uiAddress);
	}
	else
	{
		sprintf(sz, "$%X", uiAddress);
		psz->Append(sz);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CIDEDebugger::GetProgramAddress(void)
{
	return mbClient ? mcClient.GetStopAddress() : 0;
}


bool CIDEDebugger::IsDebugging(void) { return mbStarting || mbClient; }
bool CIDEDebugger::IsStopped(void) { return GetState() == IDS_Stopped; }
char* CIDEDebugger::GetStatus(void) { return mszStatus.Text(); }
CDebugClient* CIDEDebugger::GetClient(void) { return mbClient ? &mcClient : NULL; }
char* CIDEDebugger::GetEmulator(void) { return mszEmulator.Text(); }
size CIDEDebugger::NumBreakpoints(void) { return masBreakpoints.NumElements(); }
SIDEBreakpoint* CIDEDebugger::GetBreakpoint(size uiIndex) { return masBreakpoints.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The source breakpoints, to the per-user file beside the solution:
//
//	<UserSettings>
//		<Breakpoint File="D:\...\Start.asm" Line="42" Enabled="true"/>
//	</UserSettings>
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::WriteSourceBreakpoints(const char* szFileName)
{
	CXMLWriter				cWriter;
	SIDESourceBreakpoint*	psBreakpoint;
	char					szLine[16];
	size					ui;
	bool					bResult;

	cWriter.Init();
	cWriter.Begin(IDE_USER_SETTINGS_TAG);
	for (ui = 0; ui < masSourceBreakpoints.NumElements(); ui++)
	{
		psBreakpoint = masSourceBreakpoints.Get(ui);
		snprintf(szLine, sizeof(szLine), "%u", (unsigned int)psBreakpoint->uiLine);
		cWriter.Begin(IDE_BREAKPOINT_TAG);
		cWriter.Attribute("File", psBreakpoint->szFile.Text());
		cWriter.Attribute("Line", szLine);
		cWriter.Attribute("Enabled", psBreakpoint->bEnabled ? "true" : "false");
		cWriter.End();
	}
	cWriter.End();
	bResult = cWriter.Write(szFileName);
	cWriter.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The source breakpoints WriteSourceBreakpoints wrote, in place of these,
// bound with the line tables there are.  False, with none, if the file
// isn't there or isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::ReadSourceBreakpoints(const char* szFileName)
{
	CXMLFile		cFile;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szDirectory;
	CMarkupTag*		pcRoot;
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szFile;
	char*			szLine;
	char*			szEnabled;
	bool			bResult;

	while (masSourceBreakpoints.NumElements() > 0)
	{
		RemoveSourceBreakpointAt(masSourceBreakpoints.NumElements() - 1);
	}
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFile.Init();
	bResult = cFileUtil.Exists(szFileName) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = pcRoot && (StringCompare(pcRoot->GetName(), IDE_USER_SETTINGS_TAG) == 0);
	if (bResult)
	{
		for (pcTag = pcRoot->GetTag(&sIterator); pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
		{
			if (StringCompare(pcTag->GetName(), IDE_BREAKPOINT_TAG) != 0)
			{
				continue;
			}
			szFile = pcTag->GetAttribute((char*)"File");
			szLine = pcTag->GetAttribute((char*)"Line");
			szEnabled = pcTag->GetAttribute((char*)"Enabled");
			if (szFile && szLine && (atoi(szLine) > 0))
			{
				AddSourceBreakpoint(szFile, (uint32)atoi(szLine), (szEnabled == NULL) || (StringCompare(szEnabled, "false") != 0));
			}
		}
	}
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	BreakpointsChanged();
	return bResult;
}
