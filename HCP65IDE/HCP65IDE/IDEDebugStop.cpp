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
#include "BaseLib/PointerFunctions.h"
#include "IDEDebugStop.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugMemory::Init(CIDEDebugger* pcDebugger)
{
	mpcDebugger = pcDebugger;
}


//////////////////////////////////////////////////////////////////////////
//
// False while the bytes haven't all been read since the stop (or there is
// no emulator): the client asks for them at the next FetchMemory.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugMemory::Read(EAsm65StackSpace eSpace, uint32 uiAddress, void* pvDest, size uiBytes)
{
	CDebugClient*	pcClient;

	pcClient = mpcDebugger->GetClient();
	return pcClient && pcClient->ReadMemory(mpcDebugger->GetStackSpace(eSpace), uiAddress, pvDest, uiBytes);
}


//////////////////////////////////////////////////////////////////////////
//
// Writes what editing a value made, in its space.  The client forgets
// what it had read: it is read again for the windows to show.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugMemory::Write(SCPP65DebugWrite* psWrite)
{
	CDebugClient*	pcClient;

	pcClient = mpcDebugger->GetClient();
	return pcClient && (pcClient->WriteMemory(psWrite->uiAddress, psWrite->auiBytes, psWrite->uiSize, mpcDebugger->GetStackSpace((EAsm65StackSpace)psWrite->uiSpace)) != 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugRoots::Init(void)
{
	masRoots.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugRoots::Kill(void)
{
	Clear();
	masRoots.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugRoots::Clear(void)
{
	size	ui;

	for (ui = 0; ui < masRoots.NumElements(); ui++)
	{
		masRoots.Get(ui)->szError.Kill();
		masRoots.Get(ui)->szName.Kill();
	}
	masRoots.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The roots become pcSource's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugRoots::Copy(CIDEDebugRoots* pcSource)
{
	SIDEDebugRoot*	psSource;
	SIDEDebugRoot*	psRoot;
	size			ui;

	Clear();
	for (ui = 0; ui < pcSource->NumRoots(); ui++)
	{
		psSource = pcSource->GetRoot(ui);
		psRoot = Add(psSource->szName.Text());
		psRoot->bValue = psSource->bValue;
		psRoot->sValue = psSource->sValue;
		psRoot->szError.Set(psSource->szError);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A root named szName with no value yet.
//
//////////////////////////////////////////////////////////////////////////
SIDEDebugRoot* CIDEDebugRoots::Add(const char* szName)
{
	SIDEDebugRoot*	psRoot;

	psRoot = masRoots.Add();
	psRoot->szName.Init(szName);
	psRoot->bValue = false;
	memset(&psRoot->sValue, 0, sizeof(psRoot->sValue));
	psRoot->szError.Init();
	return psRoot;
}


size CIDEDebugRoots::NumRoots(void) const { return masRoots.NumElements(); }
SIDEDebugRoot* CIDEDebugRoots::GetRoot(size uiIndex) { return masRoots.SafeGet(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::Init(CIDEDebugger* pcDebugger)
{
	mpcDebugger = pcDebugger;
	mcMemory.Init(pcDebugger);
	mpcWalker = NULL;
	mcValues.Init(&mcMemory);
	mcWatch.Init(&mcValues, NULL);
	masFrames.Init();
	maszFiles.Init();
	mauiLines.Init();
	muiSelected = 0;
	muiStops = 0;
	muiReplies = 0;
	memset(&msLeaving, 0, sizeof(msLeaving));
	memset(&msReturned, 0, sizeof(msReturned));
	msResult.Init((uint32)0, ASS_Kernel);
	mbHexadecimal = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::Kill(void)
{
	mauiLines.Kill();
	maszFiles.Kill();
	masFrames.Kill();
	mcWatch.Kill();
	mcValues.Kill();
	SafeKill(mpcWalker);
}


//////////////////////////////////////////////////////////////////////////
//
// Called each time the debugger is looked at: true when what the windows
// show has changed, because the emulator stopped, or memory they waited
// for came, or it isn't stopped any more.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugStop::Update(void)
{
	CDebugClient*			pcClient;
	SAsm65StackRegisters	sRegisters;

	pcClient = mpcDebugger->GetClient();
	if (!pcClient || !mpcDebugger->GetStackRegisters(&sRegisters))
	{
		if (muiStops == 0)
		{
			return false;
		}
		Clear();
		return true;
	}
	if ((pcClient->NumStops() == muiStops) && (pcClient->NumMemoryReplies() == muiReplies))
	{
		return false;
	}

	if (pcClient->NumStops() != muiStops)
	{
		//A new stop: the windows show its innermost call, and what a Step Out left returned.
		muiSelected = 0;
		msReturned = msLeaving;
		msLeaving.psFunction = NULL;
		msResult = sRegisters;
	}
	muiStops = pcClient->NumStops();
	muiReplies = pcClient->NumMemoryReplies();
	Walk(&sRegisters);
	if (muiSelected >= masFrames.NumElements())
	{
		muiSelected = 0;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for the memory the walk and the windows couldn't read: they are
// shown again when it comes (Update).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::Fetch(void)
{
	if (mpcDebugger->GetClient() && IsStopped())
	{
		mpcDebugger->GetClient()->FetchMemory();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A Step Out is about to leave the innermost call.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::SteppingOut(void)
{
	msLeaving.psFunction = NULL;
	if (IsStopped() && (masFrames.NumElements() > 0))
	{
		msLeaving = *masFrames.Get(0);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Not stopped: no calls (a Step Out being made is kept).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::Clear(void)
{
	masFrames.ReInit();
	maszFiles.ReInit();
	mauiLines.ReInit();
	muiSelected = 0;
	muiStops = 0;
	muiReplies = 0;
	msReturned.psFunction = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The calls from the registers, through the records of the programs
// running where it stopped, and each one's source line.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::Walk(SAsm65StackRegisters* psRegisters)
{
	SAsm65StackFrame*	psFrame;
	CChars				szFile;
	uint32				uiLine;
	size				ui;

	masFrames.ReInit();
	maszFiles.ReInit();
	mauiLines.ReInit();
	if (!UseWalker(mpcDebugger->GetStackProcessor()))
	{
		return;
	}
	mpcWalker->ClearRecords();
	mpcDebugger->AddStackRecords(mpcWalker);
	mpcWalker->Walk(psRegisters, &masFrames, IDE_DEBUG_MOST_FRAMES);
	szFile.Init();
	for (ui = 0; ui < masFrames.NumElements(); ui++)
	{
		psFrame = masFrames.Get(ui);
		if (!mpcDebugger->FindFrameLine(psFrame, ui == 0, &szFile, &uiLine))
		{
			szFile.Clear();
			uiLine = 0;
		}
		maszFiles.Add(szFile.Text());
		mauiLines.Add(uiLine);
	}
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The walker for eProcessor's stacks, made (and the watches given it) when
// the processor isn't the one walked last.  False for a processor with no
// walker.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugStop::UseWalker(EAsm65DebugProcessor eProcessor)
{
	if (mpcWalker && (mpcWalker->GetProcessor() == eProcessor))
	{
		return true;
	}
	SafeKill(mpcWalker);
	mpcWalker = Asm65NewStackWalker(eProcessor, &mcMemory);
	mcWatch.Kill();
	mcWatch.Init(&mcValues, mpcWalker);
	return mpcWalker != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The Call Stack's text for a call, as Visual Studio's:
// "modplay.exe!Scale(SPoint* psPoint = 0x012345, int iBy = 2) Line 5";
// a kernal's stub by its name alone ("[Kernal call]"); code the records
// don't describe "[External Code]".
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::AppendFrame(size uiFrame, CChars* pszDest)
{
	SAsm65StackFrame*	psFrame;
	const char*			szProgram;

	psFrame = GetFrame(uiFrame);
	if (!psFrame || !psFrame->psFunction)
	{
		pszDest->Append("[External Code]");
		return;
	}
	if (psFrame->psFunction->szName.StartsWith("["))
	{
		pszDest->Append(psFrame->psFunction->szName);
		return;
	}
	szProgram = mpcDebugger->GetProgramName(psFrame->pcRecords);
	if (szProgram[0] != '\0')
	{
		pszDest->Append(szProgram)->Append('!');
	}
	mcValues.AppendCall(mpcWalker, psFrame, pszDest);
	if (GetFrameLine(uiFrame) != 0)
	{
		pszDest->Append(" Line ")->Append(GetFrameLine(uiFrame));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen call's parameters and locals in scope at its line, as
// Locals lists them (CAsm65DebugRecords::FindVisible).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::GetLocals(CIDEDebugRoots* pcRoots)
{
	SAsm65StackFrame*		psFrame;
	SAsm65DebugVariable*	psVariable;
	SIDEDebugRoot*			psRoot;
	CArrayUint32			auiVisible;
	size					ui;

	psFrame = GetSelected();
	if (!psFrame || !psFrame->psFunction)
	{
		return;
	}
	auiVisible.Init();
	psFrame->pcRecords->FindVisible(psFrame->psFunction, GetFrameLine(muiSelected), &auiVisible);
	for (ui = 0; ui < auiVisible.NumElements(); ui++)
	{
		psVariable = psFrame->pcRecords->GetVariable(psFrame->psFunction, auiVisible.GetValue(ui));
		psRoot = pcRoots->Add(psVariable->szName.Text());
		psRoot->bValue = mcValues.Variable(mpcWalker, psFrame, psVariable, &psRoot->sValue);
		if (!psRoot->bValue)
		{
			psRoot->szError.Set("(optimised away)");
		}
	}
	auiVisible.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Autos, as Visual Studio's: what the call stepped out of returned
// ("Scale returned"), then what szSource (the chosen call's line and the
// one before) uses (CCPP65DebugWatch::Autos).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::GetAutos(const char* szSource, CIDEDebugRoots* pcRoots)
{
	SAsm65StackFrame*	psFrame;
	SIDEDebugRoot*		psRoot;
	SCPP65DebugValue	sResult;
	CArrayChars			aszAutos;
	CChars				szName;
	size				ui;

	psFrame = GetSelected();
	if (!psFrame || !psFrame->psFunction)
	{
		return;
	}
	if ((muiSelected == 0) && msReturned.psFunction && mcValues.Result(msReturned.pcRecords, msReturned.psFunction, &msResult, &sResult))
	{
		szName.Init(msReturned.psFunction->szName)->Append(" returned");
		psRoot = pcRoots->Add(szName.Text());
		psRoot->bValue = true;
		psRoot->sValue = sResult;
		szName.Kill();
	}
	aszAutos.Init();
	mcWatch.Autos(psFrame, GetFrameLine(muiSelected), szSource, &aszAutos);
	for (ui = 0; ui < aszAutos.NumElements(); ui++)
	{
		Evaluate(aszAutos.GetText(ui), pcRoots->Add(aszAutos.GetText(ui)));
	}
	aszAutos.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A watch expression in the chosen call, at its line: its value, or why
// it hasn't one ("" while not stopped).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::Evaluate(const char* szExpression, SIDEDebugRoot* psRoot)
{
	SAsm65StackFrame*	psFrame;

	psFrame = GetSelected();
	psRoot->szError.Clear();
	psRoot->bValue = false;
	if (!psFrame)
	{
		return;
	}
	psRoot->bValue = mcWatch.Evaluate(psFrame, GetFrameLine(muiSelected), szExpression, &psRoot->sValue);
	if (!psRoot->bValue)
	{
		psRoot->szError.Set(mcWatch.GetError());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szText typed into the value's Value: written, if it is one it can be.
// The windows show what is read back.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugStop::Write(SCPP65DebugValue* psValue, const char* szText)
{
	SCPP65DebugWrite	sWrite;

	return IsStopped() && mcValues.Edit(psValue, szText, &sWrite) && mcMemory.Write(&sWrite);
}


//////////////////////////////////////////////////////////////////////////
//
// The call the windows show.  False if there is no such call.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugStop::SelectFrame(size uiFrame)
{
	if (uiFrame >= masFrames.NumElements())
	{
		return false;
	}
	muiSelected = uiFrame;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugStop::SetHexadecimal(bool bHexadecimal)
{
	mbHexadecimal = bHexadecimal;
	mcValues.SetHexadecimal(bHexadecimal);
}


bool CIDEDebugStop::IsStopped(void) const { return muiStops != 0; }
size CIDEDebugStop::GetStop(void) const { return muiStops; }
size CIDEDebugStop::NumFrames(void) const { return masFrames.NumElements(); }
SAsm65StackFrame* CIDEDebugStop::GetFrame(size uiFrame) { return masFrames.SafeGet(uiFrame); }
const char* CIDEDebugStop::GetFrameFile(size uiFrame) { return (uiFrame < maszFiles.NumElements()) ? maszFiles.GetText(uiFrame) : ""; }
uint32 CIDEDebugStop::GetFrameLine(size uiFrame) { return (uiFrame < mauiLines.NumElements()) ? mauiLines.GetValue(uiFrame) : 0; }
size CIDEDebugStop::GetSelectedFrame(void) const { return muiSelected; }
SAsm65StackFrame* CIDEDebugStop::GetSelected(void) { return masFrames.SafeGet(muiSelected); }
bool CIDEDebugStop::IsHexadecimal(void) { return mbHexadecimal; }
CCPP65DebugValues* CIDEDebugStop::GetValues(void) { return &mcValues; }
CIDEDebugMemory* CIDEDebugStop::GetMemory(void) { return &mcMemory; }
