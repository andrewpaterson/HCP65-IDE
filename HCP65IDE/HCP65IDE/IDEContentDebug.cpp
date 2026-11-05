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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/Timer.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"


#define IDE_DEBUG_POLL				20		//Milliseconds between looks at the debugger while debugging.
#define IDE_DEBUG_SCRIPT_BREAK		300		//Milliseconds a scripted break lets it run first.
#define IDE_DEBUG_DIALOG_WIDTH		360


//The Debug menu's items, as Visual Studio has them.
//
//	Start Debugging (Continue)	F5				Step Into					F11
//	Stop Debugging				Shift+F5		Step Over					F10
//	Restart						Ctrl+Shift+F5	Step Out					Shift+F11
//	Break All					Ctrl+Break		Toggle Breakpoint			F9
//	Attach to Emulator...						New Breakpoint at Address...
//												Delete All Breakpoints


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEContent::CreateDebugMenu(void)
{
	Ptr<CMenuContainer>	pDebug;

	pDebug = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpStartItem = pDebug->AddItem("Start Debugging (F5)");
	mpStopItem = pDebug->AddItem("Stop Debugging (Shift+F5)");
	mpRestartItem = pDebug->AddItem("Restart (Ctrl+Shift+F5)");
	mpBreakItem = pDebug->AddItem("Break All (Ctrl+Break)");
	mpAttachItem = pDebug->AddItem("Attach to Emulator...");
	mpStepIntoItem = pDebug->AddItem("Step Into (F11)");
	mpStepOverItem = pDebug->AddItem("Step Over (F10)");
	mpStepOutItem = pDebug->AddItem("Step Out (Shift+F11)");
	mpToggleItem = pDebug->AddItem("Toggle Breakpoint (F9)");
	mpNewBreakpointItem = pDebug->AddItem("New Breakpoint at Address...");
	mpDeleteBreakpointsItem = pDebug->AddItem("Delete All Breakpoints");
	return pDebug;
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcItem isn't one of Debug's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::DebugMenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem == &mpStartItem)
	{
		StartDebugging();
	}
	else if (pcItem == &mpStopItem)
	{
		StopDebugging();
	}
	else if (pcItem == &mpRestartItem)
	{
		RestartDebugging();
	}
	else if (pcItem == &mpBreakItem)
	{
		BreakAll();
	}
	else if (pcItem == &mpAttachItem)
	{
		ShowAttachDialog();
	}
	else if (pcItem == &mpStepIntoItem)
	{
		StepInto();
	}
	else if (pcItem == &mpStepOverItem)
	{
		StepOver();
	}
	else if (pcItem == &mpStepOutItem)
	{
		StepOut();
	}
	else if (pcItem == &mpToggleItem)
	{
		ToggleBreakpoint();
	}
	else if (pcItem == &mpNewBreakpointItem)
	{
		ShowNewBreakpointDialog();
	}
	else if (pcItem == &mpDeleteBreakpointsItem)
	{
		mcDebugger.DeleteAllBreakpoints();
		UpdateDebugView(true);
	}
	else
	{
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::DebugCommandAction(EComponentInputCommand eCommand)
{
	switch (eCommand)
	{
	case CIC_StartDebugging:
		StartDebugging();
		break;
	case CIC_StopDebugging:
		StopDebugging();
		break;
	case CIC_RestartDebugging:
		RestartDebugging();
		break;
	case CIC_BreakAll:
		BreakAll();
		break;
	case CIC_ToggleBreakpoint:
		ToggleBreakpoint();
		break;
	case CIC_StepOver:
		StepOver();
		break;
	case CIC_StepInto:
		StepInto();
		break;
	case CIC_StepOut:
		StepOut();
		break;
	default:
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ToolbarButtonClicked(CUnknown* pcButton)
{
	if (pcButton == &mpStartButton)
	{
		StartDebugging();
	}
	else
	{
		Build();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The HCP65 emulator Start Debugging runs, and the folder it runs in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetEmulator(const char* szExecutable, const char* szDirectory)
{
	mcDebugger.SetEmulator(szExecutable, szDirectory);
}


//////////////////////////////////////////////////////////////////////////
//
// F5: starts the emulator under the debugger (it runs to the first
// breakpoint), or continues it from where it stopped.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::StartDebugging(void)
{
	bool	bResult;

	if (mcDebugger.IsStopped())
	{
		bResult = mcDebugger.Continue();
	}
	else if (!mcDebugger.IsDebugging())
	{
		mpBuildPane->Clear();
		bResult = mcDebugger.Start(false);
	}
	else
	{
		return false;
	}
	mpBottomTabs->Select(mpDebugTab);
	UpdateDebugView(false);
	mpWindow->RequestTick(IDE_DEBUG_POLL);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::StopDebugging(void)
{
	if (!mcDebugger.IsDebugging())
	{
		return false;
	}
	mcDebugger.Stop();
	UpdateDebugView(false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::RestartDebugging(void)
{
	bool	bResult;

	bResult = mcDebugger.Restart();
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::BreakAll(void)
{
	return mcDebugger.Break();
}


//////////////////////////////////////////////////////////////////////////
//
// F11.  Not debugging, it starts the emulator stopped at its first
// instruction, as Visual Studio starts at the program's first line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::StepInto(void)
{
	bool	bResult;

	if (!mcDebugger.IsDebugging())
	{
		mpBuildPane->Clear();
		bResult = mcDebugger.Start(true);
		mpBottomTabs->Select(mpDebugTab);
		mpWindow->RequestTick(IDE_DEBUG_POLL);
	}
	else
	{
		bResult = mcDebugger.StepInto();
	}
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// F10.  Not debugging, it starts at the first instruction as F11 does.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::StepOver(void)
{
	bool	bResult;

	if (!mcDebugger.IsDebugging())
	{
		return StepInto();
	}
	bResult = mcDebugger.StepOver();
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::StepOut(void)
{
	bool	bResult;

	bResult = mcDebugger.StepOut();
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// F9: a breakpoint at the disassembly's instruction under the caret.  The
// editor's lines have no addresses until a build links, so only the
// disassembly has breakpoints for now.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ToggleBreakpoint(void)
{
	uint32	uiAddress;

	if (!mpDebugView->GetCaretAddress(&uiAddress))
	{
		return false;
	}
	mcDebugger.ToggleBreakpoint(uiAddress);
	UpdateDebugView(true);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for a number (hexadecimal with bHex; a leading $ is allowed).
// False if cancelled or it isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowAddressDialog(const char* szTitle, const char* szLabel, const char* szDefault, bool bHex, uint32* puiValue)
{
	CTextField*	pcField;
	char*		szText;
	char*		szEnd;
	bool		bResult;

	bResult = false;
	mpDialog = OMalloc<CDialog>(mpWindow, &msDialogLook, szTitle);
	pcField = &mpDialog->AddTextField(szLabel, szDefault);
	if (mpDialog->Show(IDE_DEBUG_DIALOG_WIDTH))
	{
		szText = pcField->GetText();
		while (*szText == ' ')
		{
			szText++;
		}
		if (bHex && (*szText == '$'))
		{
			szText++;
		}
		if (*szText != '\0')
		{
			*puiValue = (uint32)strtoul(szText, &szEnd, bHex ? 16 : 10);
			bResult = (*szEnd == '\0') || (*szEnd == ' ');
		}
	}
	mpDialog = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowNewBreakpointDialog(void)
{
	uint32	uiAddress;

	if (!ShowAddressDialog("New Breakpoint", "Address", "$", true, &uiAddress))
	{
		return false;
	}
	mcDebugger.AddBreakpoint(uiAddress);
	UpdateDebugView(true);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for the port an emulator started with --debug-tcp printed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowAttachDialog(void)
{
	uint32	uiPort;

	if (mcDebugger.IsDebugging() || !ShowAddressDialog("Attach to Emulator", "Port", "", false, &uiPort))
	{
		return false;
	}
	return AttachDebugger((int)uiPort);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::AttachDebugger(int iPort)
{
	bool	bResult;

	bResult = mcDebugger.Attach(iPort);
	mpBottomTabs->Select(mpDebugTab);
	UpdateDebugView(false);
	mpWindow->RequestTick(IDE_DEBUG_POLL);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's output goes in the Build tab.  While debugging it is
// looked at again soon.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::PollDebugger(Ptr<CWindow> pWindow)
{
	CArrayChars	aszOutput;
	bool		bChanged;
	bool		bStopped;

	if (!mcDebugger.IsDebugging())
	{
		RunDebugScript();
		if (maszDebugScript.NumElements() != 0)
		{
			pWindow->RequestTick(IDE_DEBUG_POLL);
		}
		return;
	}

	bStopped = mcDebugger.IsStopped();
	aszOutput.Init();
	bChanged = mcDebugger.Poll(&aszOutput);
	mpBuildPane->AppendLines(&aszOutput);
	aszOutput.Kill();
	if (bChanged)
	{
		UpdateDebugView(false);
		if (!bStopped && mcDebugger.IsStopped())
		{
			mpBottomTabs->Select(mpDebugTab);
		}
	}

	RunDebugScript();
	if (mcDebugger.IsDebugging() || (maszDebugScript.NumElements() != 0))
	{
		pWindow->RequestTick(IDE_DEBUG_POLL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The Debug tab, and the Start button's caption.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::UpdateDebugView(bool bKeepCaret)
{
	mpDebugView->Update(&mcDebugger, bKeepCaret);
	mpStartButton->SetCaption(mcDebugger.IsStopped() ? "Continue" : "Start");
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65_IDE_DEBUG's comma separated commands, for checking the debugger:
// start (F5), into (F11), over (F10), out (Shift+F11), break (Break All,
// once it has run a little), restart, stop, bp:<hex address> and
// toggle (F9 at the disassembly's caret).  Each but bp waits for the
// debugger to be stopped (or not debugging, for start).
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetDebugScript(const char* szCommands)
{
	maszDebugScript.Kill();
	maszDebugScript.Init();
	maszDebugScript.Split(szCommands, ',');
	miDebugScriptTime = GetDebugTickCount();
	mpWindow->RequestTick(IDE_DEBUG_POLL);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::IsDebugScriptRunning(void)
{
	return maszDebugScript.NumElements() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::RunDebugScript(void)
{
	char*	szCommand;
	bool	bDone;

	if (maszDebugScript.NumElements() == 0)
	{
		return;
	}
	if (mcDebugger.GetState() != IDS_Running)
	{
		miDebugScriptTime = GetDebugTickCount();
	}

	szCommand = maszDebugScript.GetText(0);
	bDone = false;
	if (strncmp(szCommand, "bp:", 3) == 0)
	{
		mcDebugger.AddBreakpoint((uint32)strtoul(&szCommand[3], NULL, 16));
		UpdateDebugView(true);
		bDone = true;
	}
	else if (strcmp(szCommand, "break") == 0)
	{
		if ((mcDebugger.GetState() == IDS_Running) && (GetDebugTickCount() - miDebugScriptTime >= IDE_DEBUG_SCRIPT_BREAK))
		{
			bDone = BreakAll();
		}
	}
	else if (strcmp(szCommand, "start") == 0)
	{
		if (!mcDebugger.IsDebugging())
		{
			StartDebugging();
			bDone = true;
		}
	}
	else if (mcDebugger.IsStopped() || (!mcDebugger.IsDebugging() && (strcmp(szCommand, "into") == 0)))
	{
		bDone = true;
		if (strcmp(szCommand, "into") == 0)
		{
			StepInto();
		}
		else if (strcmp(szCommand, "over") == 0)
		{
			StepOver();
		}
		else if (strcmp(szCommand, "out") == 0)
		{
			StepOut();
		}
		else if (strcmp(szCommand, "continue") == 0)
		{
			StartDebugging();
		}
		else if (strcmp(szCommand, "restart") == 0)
		{
			RestartDebugging();
		}
		else if (strcmp(szCommand, "stop") == 0)
		{
			StopDebugging();
		}
		else if (strcmp(szCommand, "toggle") == 0)
		{
			ToggleBreakpoint();
		}
	}

	if (bDone)
	{
		maszDebugScript.Remove((size)0);
	}
}


CIDEDebugger* CIDEContent::GetDebugger(void) { return &mcDebugger; }
Ptr<CIDEDebugView> CIDEContent::GetDebugView(void) { return mpDebugView; }
