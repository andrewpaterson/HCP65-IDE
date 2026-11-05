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
#include "IDEDebugger.h"


#define IDE_DEBUG_PORT_LINE		"HCP65 debug port "		//What the emulator prints when it listens (HCP65_DEBUG_PORT_LINE).
#define IDE_DEBUG_QUIT_WAIT		2000					//Milliseconds the emulator has to quit before it is ended.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Init(void)
{
	mszEmulator.Init();
	mszEmulatorDirectory.Init();
	mcProcess.Init();
	mbLaunched = false;
	mcConnection.Init();
	mbClient = false;
	meState = IDS_Idle;
	mbBreakAtStart = false;
	mbRestarting = false;
	muiSeenStops = 0;
	muiSeenMemory = 0;
	muiSeenErrors = 0;
	masBreakpoints.Init();
	mszStatus.Init();
	muiMemoryAddress = 0;
	muiMemorySize = 0;
	muiViewStart = 0;
	muiViewSize = 0;
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
	mcConnection.Kill();
	mcProcess.Kill();
	mszEmulatorDirectory.Kill();
	mszEmulator.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetEmulator(const char* szExecutable, const char* szDirectory)
{
	mszEmulator.Set(szExecutable);
	mszEmulatorDirectory.Set(szDirectory);
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the emulator, holding before its first instruction until it is
// connected to.  It then runs to the first breakpoint, or (bBreakAtStart,
// as Step Into does when not debugging) stays at the first instruction.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Start(bool bBreakAtStart)
{
	CArrayChars	aszArguments;
	bool		bResult;

	if (meState != IDS_Idle)
	{
		return false;
	}

	aszArguments.Init();
	aszArguments.Add("--debug-tcp");
	aszArguments.Add("0");
	aszArguments.Add("--debug-wait");
	aszArguments.Add("--exit-on-detach");
	mcProcess.Kill();
	mcProcess.Init();
	bResult = mcProcess.Start(mszEmulator.Text(), &aszArguments, mszEmulatorDirectory.Text());
	aszArguments.Kill();
	if (!bResult)
	{
		SetStatus("Could not start the emulator.", 0, false);
		return false;
	}

	mbLaunched = true;
	mbBreakAtStart = bBreakAtStart;
	meState = IDS_Starting;
	SetStatus("Starting the emulator.", 0, false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// To an emulator already running with --debug-tcp iPort.  It goes on as it
// was, running or stopped.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Attach(int iPort)
{
	if (meState != IDS_Idle)
	{
		return false;
	}

	mbLaunched = false;
	mbBreakAtStart = true;
	if (!Connect(iPort))
	{
		SetStatus("Could not attach to the emulator.", 0, false);
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Connect(int iPort)
{
	mcConnection.Kill();
	mcConnection.Init();
	if (!mcConnection.Connect(NETWORK_LOOPBACK_ADDRESS, iPort))
	{
		return false;
	}

	mcClient.Init(&mcConnection);
	mbClient = true;
	muiSeenStops = 0;
	muiSeenMemory = 0;
	muiSeenErrors = 0;
	muiMemorySize = 0;
	muiViewSize = 0;
	meState = IDS_Connecting;
	SetStatus("Connecting to the emulator.", 0, false);
	return mcClient.Hello();
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
	mcConnection.Kill();
	mcConnection.Init();
	mbLaunched = false;
	mbRestarting = false;
	meState = IDS_Idle;
	muiMemorySize = 0;
	muiViewSize = 0;
	SetStatus(szStatus, 0, false);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Continue(void)
{
	if (!CanStep())
	{
		return false;
	}
	meState = IDS_Running;
	SetStatus("Running.", 0, false);
	return mcClient.Continue();
}


//////////////////////////////////////////////////////////////////////////
//
// Break All: the emulator stops where it is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Break(void)
{
	if ((meState != IDS_Running) && (meState != IDS_Stepping))
	{
		return false;
	}
	return mcClient.Pause();
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepInto(void)
{
	if (!CanStep())
	{
		return false;
	}
	meState = IDS_Stepping;
	return mcClient.Step(1);
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction, or the whole of a call.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepOver(void)
{
	if (!CanStep())
	{
		return false;
	}
	meState = IDS_Stepping;
	return mcClient.StepOver();
}


//////////////////////////////////////////////////////////////////////////
//
// Until the function it is in returns.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::StepOut(void)
{
	if (!CanStep())
	{
		return false;
	}
	meState = IDS_Stepping;
	return mcClient.StepOut();
}


//////////////////////////////////////////////////////////////////////////
//
// The HCP65 is reset, then runs again (or stops at its first instruction
// if debugging started by stepping into it, or was attached).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Restart(void)
{
	if ((meState != IDS_Stopped) && (meState != IDS_Running))
	{
		return false;
	}
	meState = IDS_Stepping;
	mbRestarting = true;
	return mcClient.Reset();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::CanStep(void)
{
	return meState == IDS_Stopped;
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
	if (mbClient && (meState != IDS_Connecting))
	{
		mcClient.RemoveBreakpoint(uiAddress);
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
	if (mbClient && (meState != IDS_Connecting))
	{
		mcClient.AddBreakpoint(uiAddress, true);
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
	if (mbClient && (meState != IDS_Connecting))
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
	}
	mcClient.SetBreakpoints(asBreakpoints, masBreakpoints.NumElements());
	free(asBreakpoints);
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's output lines go in paszOutput.  True if anything shown
// changed: the state, a stop or the memory read at it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugger::Poll(CArrayChars* paszOutput)
{
	EIDEDebugState	eOldState;
	size			uiOldMemory;
	size			i;
	char*			szLine;
	bool			bRunning;

	eOldState = meState;
	uiOldMemory = muiSeenMemory;

	if (mbLaunched)
	{
		i = paszOutput->NumElements();
		bRunning = mcProcess.Poll(paszOutput);
		if (meState == IDS_Starting)
		{
			for (; i < paszOutput->NumElements(); i++)
			{
				szLine = paszOutput->GetText(i);
				if (strncmp(szLine, IDE_DEBUG_PORT_LINE, strlen(IDE_DEBUG_PORT_LINE)) == 0)
				{
					if (!Connect(atoi(&szLine[strlen(IDE_DEBUG_PORT_LINE)])))
					{
						Stop();
						SetStatus("Could not connect to the emulator.", 0, false);
						return true;
					}
					break;
				}
			}
		}
		if (!bRunning)
		{
			End("The emulator exited.");
			return true;
		}
	}

	if (!mbClient)
	{
		return meState != eOldState;
	}

	mcClient.Poll(0);
	if (mcClient.GetState() == DCS_Closed)
	{
		End(mbLaunched ? "The emulator closed the connection." : "Detached: the emulator went.");
		return true;
	}

	if (meState == IDS_Connecting)
	{
		if (mcClient.GetState() != DCS_Connecting)
		{
			Welcomed();
		}
	}
	else if (mcClient.NumStops() != muiSeenStops)
	{
		muiSeenStops = mcClient.NumStops();
		Stopped();
		if (mbRestarting && (mcClient.GetStopReason() == DS_Reset))
		{
			//Restarted: it runs again from the reset, unless it was stepped into from the start.
			mbRestarting = false;
			if (!mbBreakAtStart)
			{
				Continue();
			}
		}
	}

	if (mcClient.NumMemoryReplies() != muiSeenMemory)
	{
		muiSeenMemory = mcClient.NumMemoryReplies();
		muiMemoryAddress = mcClient.GetMemoryAddress();
		muiMemorySize = mcClient.GetMemorySize();
		if (muiMemorySize > IDE_DEBUG_MEMORY_VIEW)
		{
			muiMemorySize = IDE_DEBUG_MEMORY_VIEW;
		}
		memcpy(mauiMemory, mcClient.GetMemory(), muiMemorySize);
	}
	if (mcClient.NumErrors() != muiSeenErrors)
	{
		muiSeenErrors = mcClient.NumErrors();
		paszOutput->Add("Debugger: ")->Append(mcClient.GetError());
	}

	return (meState != eOldState) || (muiSeenMemory != uiOldMemory);
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's welcome and state have arrived: it is given the
// breakpoints, and if it waited for the debugger it is continued.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Welcomed(void)
{
	SendBreakpoints();
	muiSeenStops = mcClient.NumStops();
	if (mcClient.IsRunning())
	{
		meState = IDS_Running;
		SetStatus("Running.", 0, false);
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
// A new stop: why, and the memory there is read to show.  The view stays
// where it was while the program address is inside it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::Stopped(void)
{
	uint32	uiAddress;

	meState = IDS_Stopped;
	uiAddress = mcClient.GetStopAddress();
	switch (mcClient.GetStopReason())
	{
	case DS_Start:
		SetStatus("Stopped before the first instruction at ", uiAddress, true);
		break;
	case DS_Breakpoint:
		SetStatus("Stopped at a breakpoint at ", uiAddress, true);
		break;
	case DS_Step:
		SetStatus("Stepped to ", uiAddress, true);
		break;
	case DS_Pause:
		SetStatus("Broke in at ", uiAddress, true);
		break;
	case DS_Stopped:
		SetStatus("The processor stopped itself at ", uiAddress, true);
		break;
	case DS_Reset:
		SetStatus("Reset, stopped at ", uiAddress, true);
		break;
	}

	if ((muiViewSize == 0) || (uiAddress < muiViewStart) || (uiAddress >= muiViewStart + muiViewSize / 2))
	{
		muiViewStart = uiAddress;
	}
	muiViewSize = IDE_DEBUG_MEMORY_VIEW;
	mcClient.ReadMemory(muiViewStart, IDE_DEBUG_MEMORY_VIEW);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugger::SetStatus(const char* szText, uint32 uiAddress, bool bAddress)
{
	char	sz[32];

	mszStatus.Set(szText);
	if (bAddress)
	{
		sprintf(sz, "$%02X:%04X.", (uiAddress >> 16) & 0xFF, uiAddress & 0xFFFF);
		mszStatus.Append(sz);
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


//////////////////////////////////////////////////////////////////////////
//
// The memory last read to disassemble, NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
uint8* CIDEDebugger::GetMemory(uint32* puiAddress, size* puiSize)
{
	*puiAddress = muiMemoryAddress;
	*puiSize = muiMemorySize;
	return (muiMemorySize != 0) ? mauiMemory : NULL;
}


EIDEDebugState CIDEDebugger::GetState(void) { return meState; }
bool CIDEDebugger::IsDebugging(void) { return meState != IDS_Idle; }
bool CIDEDebugger::IsStopped(void) { return meState == IDS_Stopped; }
char* CIDEDebugger::GetStatus(void) { return mszStatus.Text(); }
CDebugClient* CIDEDebugger::GetClient(void) { return mbClient ? &mcClient : NULL; }
char* CIDEDebugger::GetEmulator(void) { return mszEmulator.Text(); }
size CIDEDebugger::NumBreakpoints(void) { return masBreakpoints.NumElements(); }
SIDEBreakpoint* CIDEDebugger::GetBreakpoint(size uiIndex) { return masBreakpoints.Get(uiIndex); }
