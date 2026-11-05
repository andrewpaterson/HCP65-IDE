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
#include "BaseLib/ArrayUint32.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "ThreadLib/Process.h"
#include "ThreadLib/NetworkConnection.h"
#include "ThreadLib/SharedMemoryChannel.h"
#include "EmulatorDebugLib/DebugClient.h"
#include "Assembler65Lib/Asm65LineTable.h"


#define IDE_USER_SETTINGS_TAG	"UserSettings"	//The root of the per-user file beside the solution (<solution>.hcp65user).
#define IDE_BREAKPOINT_TAG		"Breakpoint"


#define IDE_DEBUG_DISASSEMBLY	24		//The instructions shown from the program address.
#define IDE_DEBUG_LINE_TABLES	8		//The most programs' line tables read (a kernal and a program).


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


//A breakpoint on a line of a source file (F9 in the editor), bound to where the line's code starts in the programs'
//line tables: each place, or the next line's with code.  Until it is bound it has no addresses (uiBoundLine is 0).
struct SIDESourceBreakpoint
{
	CChars			szFile;
	uint32			uiLine;			//1 based, where it was set.
	bool			bEnabled;
	uint32			uiBoundLine;	//The line its addresses are for, 0 while it isn't bound.
	CArrayUint32	auiAddresses;
	CArrayUint32	auiTables;		//The line table each address is from.
};


typedef CArrayTemplate<SIDESourceBreakpoint>	CArrayIDESourceBreakpoints;


//The IDE's end of debugging the HCP65 emulator, at the instruction level, over EmulatorDebugLib's protocol.  It
//knows nothing of the processor: the emulator describes its registers and addresses, and disassembles.
//
//Start runs the emulator (DebugLaunch.h's arguments, then SetEmulator's) on a shared memory channel named for this
//IDE, waiting for the debugger, and connects once the emulator has made it.  What it ran is told (TakeMessages), and
//an emulator that isn't SetProcessor's processor is quit (or detached from) as soon as it says what it is.  Attach connects over the network to an emulator already
//running with a port.  The breakpoints are the IDE's, by address: they are kept when not debugging and given to
//the emulator when it is connected.  Poll (each tick while debugging) reads what the emulator has sent and its
//output.  When it stops the instructions there are asked for.
//
//A Start with files to load (AddLoad: a kernal, the configuration's built image) loads them into the emulator while
//it waits at its first instruction (DM_Load at each one's address), then resets it so it starts from what was loaded.
//
//The breakpoints are in SetBreakpointSpace's space ("Kernel", "User"), as the emulator names its spaces; in every
//space if it is empty or the emulator has none of that name.
//
//Source breakpoints are by file and line (ToggleSourceBreakpoint).  The programs' line tables (AddLineTable, the
//<program>.lines a build writes beside it) bind them to addresses: a table of the configuration's own program puts
//them in the breakpoint space, one of what is loaded with it (a kernal) in every space.  Both kinds of breakpoint are
//kept when not debugging, and all of them are given to the emulator whenever one changes.
//
//A configuration's own program can be one the emulator's kernal runs as a process (SetOwnProgram: its file there,
//"\\boot.exe").  The emulator tells of each process started and ended; the program's ID is the process of that name.
//The own program's breakpoints (the disassembly's, and its line table's) stop only in its process, and wait unbound
//while it isn't running.  A process started stops the emulator before it runs: its breakpoints are given and it goes
//on (the stop isn't shown).
class CIDEDebugger
{
protected:
	CChars					mszEmulator;			//The emulator's executable.
	CChars					mszEmulatorDirectory;	//Where it runs (its resources are found from there).
	CChars					mszEmulatorArguments;	//Given after the debugging ones, split at spaces.
	CChars					mszProcessor;			//The processor the emulator must be, empty for any.
	CArrayChars				maszLoads;				//What is loaded into the emulator Start runs, in order.
	CArrayUint32			mauiLoadAddresses;		//Where each is loaded.
	CChars					mszBreakpointSpace;		//The breakpoints' space by name, empty for every space.
	CChars					mszOwnProgram;			//The program a process runs ("\\boot.exe"), empty if it isn't one.
	CChars					mszCard;				//The disk image the emulator boots from (DEBUG_ARGUMENT_CARD), empty for its own.
	size					muiSeenProcessEvents;
	uint32					muiBreakpointSpace;		//Its DEBUG_SPACE_ANY or index plus one, once welcomed.
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
	bool					mbIntoLine;				//A StepLine into a call: on until on a new line (StepOnInto).
	CChars					mszStepFile;			//The line a StepLine started from.
	uint32					muiStepLine;
	size					muiAutoSteps;			//The steps StepOnInto has taken on its own.
	size					muiSeenStops;
	size					muiSeenDisassemblies;
	size					muiSeenErrors;
	CArrayIDEBreakpoints	masBreakpoints;
	CArrayIDESourceBreakpoints	masSourceBreakpoints;
	CAsm65LineTable			macLineTables[IDE_DEBUG_LINE_TABLES];
	bool					mabOwnSpace[IDE_DEBUG_LINE_TABLES];	//The table is the configuration's own program's.
	size					muiLineTables;
	CChars					mszStatus;

public:
	void					Init(void);
	void					Kill(void);

	void					SetEmulator(const char* szExecutable, const char* szDirectory, const char* szArguments);
	void					SetProcessor(const char* szProcessor);
	void					ClearLoads(void);
	void					AddLoad(const char* szFile, uint32 uiAddress);
	void					SetBreakpointSpace(const char* szSpace);
	void					SetOwnProgram(const char* szProgram);
	void					SetCard(const char* szCard);
	bool					GetOwnProcess(uint32* puiProcess);
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
	bool					StepLine(bool bOver);
	bool					Restart(void);

	bool					ToggleBreakpoint(uint32 uiAddress);
	bool					AddBreakpoint(uint32 uiAddress);
	void					DeleteAllBreakpoints(void);
	size					NumBreakpoints(void);
	SIDEBreakpoint*			GetBreakpoint(size uiIndex);
	SIDEBreakpoint*			FindBreakpoint(uint32 uiAddress);

	void					ClearLineTables(void);
	bool					AddLineTable(const char* szProgram, bool bOwnSpace);
	size					NumLineTables(void);
	CAsm65LineTable*		GetLineTable(size uiIndex);
	bool					FindSourceLine(uint32 uiAddress, CChars* pszFile, uint32* puiLine);
	bool					ToggleSourceBreakpoint(const char* szFile, uint32 uiLine);
	SIDESourceBreakpoint*	AddSourceBreakpoint(const char* szFile, uint32 uiLine, bool bEnabled);
	SIDESourceBreakpoint*	FindSourceBreakpoint(const char* szFile, uint32 uiLine);
	size					NumSourceBreakpoints(void);
	SIDESourceBreakpoint*	GetSourceBreakpoint(size uiIndex);
	bool					WriteSourceBreakpoints(const char* szFileName);
	bool					ReadSourceBreakpoints(const char* szFileName);

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
	bool					LoadFile(const char* szFile, uint32 uiAddress);
	void					Stopped(void);
	void					SendBreakpoints(void);
	void					TellProcesses(CArrayChars* paszOutput);
	void					BreakpointsChanged(void);
	void					BindSourceBreakpoints(void);
	void					BindSourceBreakpoint(SIDESourceBreakpoint* psBreakpoint);
	void					RemoveSourceBreakpointAt(size uiIndex);
	bool					CanMove(void);
	bool					StepOnInto(uint32 uiAddress);
	bool					StepLineRanges(bool bOver);
	void					End(const char* szStatus);
};


#endif // __IDE_DEBUGGER_H__
