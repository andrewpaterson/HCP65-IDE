/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_DEBUG_STOP_H__
#define __IDE_DEBUG_STOP_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayUint32.h"
#include "BaseLib/ArrayTemplate.h"
#include "CppCompilerLib/CPP65DebugWatch.h"
#include "IDEDebugger.h"


#define IDE_DEBUG_MOST_FRAMES	64		//The most calls a stack walk finds.


//The memory a stack walk and the values read: the emulator's, as the debugger has read it since it stopped (its
//client's page cache), kernel and user mode's in the emulator's spaces of those names (CIDEDebugger::GetStackSpace).
//What hasn't come yet can't be read; the client asks for it with whatever else was missed (CDebugClient::FetchMemory).
class CIDEDebugMemory : public CAsm65StackMemory
{
protected:
	CIDEDebugger*	mpcDebugger;

public:
	void	Init(CIDEDebugger* pcDebugger);
	bool	Read(EAsm65StackSpace eSpace, uint32 uiAddress, void* pvDest, size uiBytes) override;
	bool	Write(SCPP65DebugWrite* psWrite);
};


//A value at the top of a debugger window's tree (a local, an auto, a watch): its name, and its value or why it has
//none ("" while not stopped).
struct SIDEDebugRoot
{
	CChars				szName;
	bool				bValue;
	SCPP65DebugValue	sValue;
	CChars				szError;
};


//A window's roots, in the order shown.
class CIDEDebugRoots
{
protected:
	CArrayTemplate<SIDEDebugRoot>	masRoots;

public:
	void			Init(void);
	void			Kill(void);
	void			Clear(void);
	void			Copy(CIDEDebugRoots* pcSource);

	SIDEDebugRoot*	Add(const char* szName);
	size			NumRoots(void) const;
	SIDEDebugRoot*	GetRoot(size uiIndex);
};


//What the debugger knows where the emulator stopped (DebuggerPlan.txt E): the calls it is in, walked from its registers
//through the programs running there (CIDEDebugger::AddStackRecords), each call's source line, the call chosen in the
//Call Stack, and the values the windows show in that call: its Locals, Autos and watch expressions, in Hexadecimal
//Display or not.  Update walks again at each stop, and again whenever memory it was waiting for has come; the windows
//read through the same memory, and Fetch asks for what any of them missed, to be shown when it comes.
//
//Stepped out of a call (SteppingOut, then the stop), Autos shows what the call returned, as Visual Studio does.
class CIDEDebugStop
{
protected:
	CIDEDebugger*			mpcDebugger;
	CIDEDebugMemory			mcMemory;
	CAsm65StackWalker*		mpcWalker;		//The processor's, made when it is first walked (NULL until then).
	CCPP65DebugValues		mcValues;
	CCPP65DebugWatch		mcWatch;
	CArrayAsm65StackFrames	masFrames;
	CArrayChars				maszFiles;		//Each frame's source file, "" if the line tables haven't its code,
	CArrayUint32			mauiLines;		//and its line there.
	size					muiSelected;	//The frame the windows show.
	size					muiStops;		//The client's stops when the frames were walked, 0 when not stopped.
	size					muiReplies;		//Its memory replies then.
	SAsm65StackFrame		msLeaving;		//The call a Step Out is leaving (psFunction NULL when none is),
	SAsm65StackFrame		msReturned;		//and the one it left, at the stop after.
	SAsm65StackRegisters	msResult;		//The registers at that stop: what it returned.
	bool					mbHexadecimal;	//Integers are shown in hexadecimal (Visual Studio's Hexadecimal Display).

public:
	void				Init(CIDEDebugger* pcDebugger);
	void				Kill(void);

	bool				Update(void);
	void				Fetch(void);
	void				SteppingOut(void);

	bool				IsStopped(void) const;
	size				GetStop(void) const;
	size				NumFrames(void) const;
	SAsm65StackFrame*	GetFrame(size uiFrame);
	const char*			GetFrameFile(size uiFrame);
	uint32				GetFrameLine(size uiFrame);
	void				AppendFrame(size uiFrame, CChars* pszDest);
	size				GetSelectedFrame(void) const;
	bool				SelectFrame(size uiFrame);

	void				SetHexadecimal(bool bHexadecimal);
	bool				IsHexadecimal(void);
	CCPP65DebugValues*	GetValues(void);
	CIDEDebugMemory*	GetMemory(void);

	void				GetLocals(CIDEDebugRoots* pcRoots);
	void				GetAutos(const char* szSource, CIDEDebugRoots* pcRoots);
	void				Evaluate(const char* szExpression, SIDEDebugRoot* psRoot);
	bool				Write(SCPP65DebugValue* psValue, const char* szText);

protected:
	void				Clear(void);
	void				Walk(SAsm65StackRegisters* psRegisters);
	bool				UseWalker(EAsm65DebugProcessor eProcessor);
	SAsm65StackFrame*	GetSelected(void);
};


#endif // __IDE_DEBUG_STOP_H__
