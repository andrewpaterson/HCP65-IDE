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
#include "EmulatorDebugLib/DebugLaunch.h"
#include "IDEDebugger.h"


#define IDE_DEBUG_QUIT_WAIT		2000		//Milliseconds the emulator has to quit before it is ended.
#define IDE_DEBUG_START_WAIT	20000		//Milliseconds the emulator has to make its channel.
#define IDE_DEBUG_CHANNEL		"HCP65IDE"	//The start of the shared memory channel's name.


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
	mszImage.Init();
	muiImageLoad = 0;
	mszBreakpointSpace.Init();
	muiBreakpointSpace = DEBUG_SPACE_ANY;
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
	muiSeenStops = 0;
	muiSeenDisassemblies = 0;
	muiSeenErrors = 0;
	masBreakpoints.Init();
	mszStatus.Init("Not debugging.");
}


//////////////////////////////////////////////////////////////////////////
//
// An emulator still being debugged is stopped (one Start ran quits).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Kill(void)
{
	Stop();
	mszStatus.Kill();
	masBreakpoints.Kill();
	mcNetwork.Kill();
	mcShared.Kill();
	mszChannel.Kill();
	mcProcess.Kill();
	maszMessages.Kill();
	mszBreakpointSpace.Kill();
	mszImage.Kill();
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
// The file the next Start loads into the emulator at uiLoad; NULL or ""
// for none (the emulator runs what it has).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetImage(const char* szFile, uint32 uiLoad)
{
	mszImage.Set(szFile ? szFile : "");
	muiImageLoad = uiLoad;
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
	if (mbClient && (mcClient.GetState() != DCS_Connecting))
	{
		mcClient.RemoveBreakpoint(uiAddress, muiBreakpointSpace);
	}
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
	if (mbClient && (mcClient.GetState() != DCS_Connecting))
	{
		mcClient.AddBreakpoint(uiAddress, true, muiBreakpointSpace);
	}
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
	if (mbClient && (mcClient.GetState() != DCS_Connecting))
	{
		mcClient.SetBreakpoints(NULL, 0);
	}
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
	SDebugBreakpoint*	asBreakpoints;
	size				i;

	asBreakpoints = (SDebugBreakpoint*)malloc((masBreakpoints.NumElements() + 1) * sizeof(SDebugBreakpoint));
	for (i = 0; i < masBreakpoints.NumElements(); i++)
	{
		asBreakpoints[i].uiAddress = masBreakpoints.Get(i)->uiAddress;
		asBreakpoints[i].uiEnabled = masBreakpoints.Get(i)->bEnabled;
		asBreakpoints[i].uiSpace = muiBreakpointSpace;
	}
	mcClient.SetBreakpoints(asBreakpoints, masBreakpoints.NumElements());
	free(asBreakpoints);
}


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
	else if (mcClient.NumStops() != muiSeenStops)
	{
		Stopped();
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
// The emulator's welcome and state have arrived.  If it is the processor
// wanted it is given the breakpoints, and if it waited for the debugger it
// is continued.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Welcomed(void)
{
	CChars	sz;

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
	if (mbLaunched && !mszImage.Empty() && !mcClient.IsRunning() && (mcClient.GetStopReason() == DS_Start))
	{
		//The built program goes in before anything runs, and the reset starts it (or stops at its first instruction).
		muiSeenStops = mcClient.NumStops();
		if (!LoadImage() || (mcClient.Reset(!mbBreakAtStart) == 0))
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
// The image (SetImage) sent to the emulator.  False, said in the
// messages, if it can't be read or sent.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::LoadImage(void)
{
	FILE*	pcFile;
	uint8*	puiData;
	long	iSize;
	bool	bResult;
	char	szLine[64];

	pcFile = fopen(mszImage.Text(), "rb");
	if (!pcFile)
	{
		maszMessages.Add("Could not read ")->Append(mszImage)->Append('.');
		mszStatus.Set("Could not read the program to load.");
		return false;
	}
	fseek(pcFile, 0, SEEK_END);
	iSize = ftell(pcFile);
	fseek(pcFile, 0, SEEK_SET);
	bResult = iSize > 0;
	puiData = bResult ? (uint8*)malloc((size)iSize) : NULL;
	bResult = bResult && (fread(puiData, 1, (size)iSize, pcFile) == (size)iSize);
	fclose(pcFile);
	bResult = bResult && (mcClient.Load(muiImageLoad, puiData, (size)iSize) != 0);
	if (puiData)
	{
		free(puiData);
	}
	if (!bResult)
	{
		maszMessages.Add("Could not load ")->Append(mszImage)->Append(" into the emulator.");
		mszStatus.Set("Could not load the program.");
		return false;
	}

	snprintf(szLine, sizeof(szLine), "Loaded %ld bytes at $%06X from ", iSize, (unsigned int)muiImageLoad);
	maszMessages.Add(szLine)->Append(mszImage)->Append('.');
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
