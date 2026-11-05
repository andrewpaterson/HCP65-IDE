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
#ifndef __IDE_DEBUGGER_H__
#define __IDE_DEBUGGER_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "ThreadLib/Process.h"
#include "ThreadLib/NetworkConnection.h"
#include "ThreadLib/SharedMemoryChannel.h"
#include "EmulatorDebugLib/DebugClient.h"


#define IDE_DEBUG_DISASSEMBLY	24		//The instructions shown from the program address.


enum EIDEDebugState
{
	IDS_Idle,			//Not debugging.
	IDS_Starting,		//The emulator was started; waiting for it to make its channel.
	IDS_Connecting,		//Connected, waiting for the emulator's welcome.
	IDS_Stopped,
	IDS_Running,
	IDS_Stepping,		//A step was asked for and isn't finished.
};


struct SIDEBreakpoint
{
	uint32	uiAddress;
	bool	bEnabled;
};


typedef CArrayTemplate<SIDEBreakpoint>	CArrayIDEBreakpoints;


//The IDE's end of debugging the HCP65 emulator, at the instruction level, over EmulatorDebugLib's protocol.  It
//knows nothing of the processor: the emulator describes its registers and addresses, and disassembles.
//
//Start runs the emulator (DebugLaunch.h's arguments, then SetEmulator's) on a shared memory channel named for this
//IDE, waiting for the debugger, and connects once the emulator has made it.  What it ran is told (TakeMessages), and
//an emulator that isn't SetProcessor's processor is quit (or detached from) as soon as it says what it is.  Attach connects over the network to an emulator already
//running with a port.  The breakpoints are the IDE's, by address: they are kept when not debugging and given to
//the emulator when it is connected.  Poll (each tick while debugging) reads what the emulator has sent and its
//output.  When it stops the instructions there are asked for.
class CIDEDebugger
{
protected:
	CChars					mszEmulator;			//The emulator's executable.
	CChars					mszEmulatorDirectory;	//Where it runs (its resources are found from there).
	CChars					mszEmulatorArguments;	//Given after the debugging ones, split at spaces.
	CChars					mszProcessor;			//The processor the emulator must be, empty for any.
	CArrayChars				maszMessages;			//For the Build tab: what was started, what went wrong.
	CProcess				mcProcess;
	bool					mbLaunched;				//Start ran the emulator; Stop quits it.
	CChars					mszChannel;				//The shared memory channel's name, while starting.
	CSharedMemoryChannel	mcShared;
	CNetworkConnection		mcNetwork;
	CDebugClient			mcClient;
	bool					mbClient;				//mcClient is initialised on one of the channels.
	bool					mbStarting;
	int64					miStartTime;
	bool					mbBreakAtStart;			//Started by stepping into: stay at the first instruction.
	size					muiSeenStops;
	size					muiSeenDisassemblies;
	size					muiSeenErrors;
	CArrayIDEBreakpoints	masBreakpoints;
	CChars					mszStatus;

public:
	void					Init(void);
	void					Kill(void);

	void					SetEmulator(const char* szExecutable, const char* szDirectory, const char* szArguments);
	void					SetProcessor(const char* szProcessor);
	char*					GetEmulator(void);
	void					TakeMessages(CArrayChars* paszDest);

	bool					Start(bool bBreakAtStart);
	bool					Attach(int iPort);
	void					Stop(void);
	bool					Continue(void);
	bool					Break(void);
	bool					StepInto(void);
	bool					StepOver(void);
	bool					StepOut(void);
	bool					Restart(void);

	bool					ToggleBreakpoint(uint32 uiAddress);
	bool					AddBreakpoint(uint32 uiAddress);
	void					DeleteAllBreakpoints(void);
	size					NumBreakpoints(void);
	SIDEBreakpoint*			GetBreakpoint(size uiIndex);
	SIDEBreakpoint*			FindBreakpoint(uint32 uiAddress);

	bool					Poll(CArrayChars* paszOutput);

	EIDEDebugState			GetState(void);
	bool					IsDebugging(void);
	bool					IsStopped(void);
	char*					GetStatus(void);
	CDebugClient*			GetClient(void);
	uint32					GetProgramAddress(void);
	void					AppendAddress(CChars* psz, uint32 uiAddress);

protected:
	bool					Connect(CProcessChannel* pcChannel);
	void					Welcomed(void);
	void					Stopped(void);
	void					SendBreakpoints(void);
	bool					CanMove(void);
	void					End(const char* szStatus);
};


#endif // __IDE_DEBUGGER_H__
