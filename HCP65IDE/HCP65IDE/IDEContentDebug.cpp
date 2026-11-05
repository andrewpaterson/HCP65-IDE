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
#include "BaseLib/FileUtil.h"
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
void CIDEContent::BuildClicked(void) { Build(); }  //The toolbar's Build button.
void CIDEContent::StartClicked(void) { StartDebugging(); }  //The toolbar's Start button, Continue while stopped.


//////////////////////////////////////////////////////////////////////////
//
// The emulator a W65C816 configuration is debugged in when it doesn't say
// (its Emulator in the .hcp65build), and the folder it runs in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetDefaultEmulator(const char* szExecutable, const char* szDirectory)
{
	mszDefaultEmulator.Set(szExecutable);
	mszDefaultEmulatorDirectory.Set(szDirectory);
}


//////////////////////////////////////////////////////////////////////////
//
// A path from the configurations' file, relative to that file's folder.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ResolveEmulatorPath(const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	CChars		szName;

	if (cFileUtil.IsAbsolutePath(szPath))
	{
		pszDest->Set(szPath);
		return;
	}
	szFile.Init();
	szName.Init();
	CBuildConfigurations::GetFileName(mszSolutionFile.Text(), &szFile);
	pszDest->Set("");
	cFileUtil.SplitPath(szFile.Text(), &szName, pszDest);
	cFileUtil.AppendToPath(pszDest, szPath);
	cFileUtil.FullPath(pszDest);
	szName.Kill();
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Gives the debugger the chosen configuration's processor and (bLaunch)
// emulator: the one its configuration names, or for a W65C816 the default.
// False, told in the Build tab, if it has no processor (MSVC, GCC) or no
// emulator.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ChooseEmulator(bool bLaunch)
{
	CBuildConfiguration*	pcConfiguration;
	const char*				szProcessor;
	CChars					szExecutable;
	CChars					szDirectory;
	CChars					szFile;
	CChars					szMessage;
	CArrayChars				aszLines;
	CFileUtil				cFileUtil;
	bool					bResult;

	pcConfiguration = GetConfiguration();
	szMessage.Init();
	szExecutable.Init();
	szDirectory.Init();
	bResult = false;
	if (pcConfiguration == NULL)
	{
		szMessage.Set("Choose a configuration to debug.");
	}
	else if ((szProcessor = pcConfiguration->GetProcessorName()) == NULL)
	{
		szMessage.Set(pcConfiguration->GetName());
		szMessage.Append(" is built for this computer (");
		szMessage.Append(CBuildConfiguration::GetToolchainName(pcConfiguration->GetToolchain()));
		szMessage.Append("): there is no emulator to debug it in.");
	}
	else
	{
		mcDebugger.SetProcessor(szProcessor);
		mcDebugger.SetImage(NULL, 0);
		bResult = true;
		if (!bLaunch)
		{
			//Attached to: the emulator is whatever is running, checked when it says what it is.
		}
		else if (pcConfiguration->HasEmulator())
		{
			ResolveEmulatorPath(pcConfiguration->GetEmulator(), &szExecutable);
			if (pcConfiguration->GetEmulatorDirectory()[0] != '\0')
			{
				ResolveEmulatorPath(pcConfiguration->GetEmulatorDirectory(), &szDirectory);
			}
			else
			{
				szFile.Init();
				cFileUtil.SplitPath(szExecutable.Text(), &szFile, &szDirectory);
				szFile.Kill();
			}
			mcDebugger.SetEmulator(szExecutable.Text(), szDirectory.Text(), pcConfiguration->GetEmulatorArguments());
		}
		else if ((pcConfiguration->GetToolchain() == BT_W65C816) && !mszDefaultEmulator.Empty())
		{
			mcDebugger.SetEmulator(mszDefaultEmulator.Text(), mszDefaultEmulatorDirectory.Text(), pcConfiguration->GetEmulatorArguments());
		}
		else
		{
			szFile.Init();
			CBuildConfigurations::GetFileName(mszSolutionFile.Text(), &szFile);
			szMessage.Set("No emulator is set for ");
			szMessage.Append(pcConfiguration->GetName());
			szMessage.Append(": give its configuration an Emulator in ");
			szMessage.Append(szFile);
			szMessage.Append('.');
			szFile.Kill();
			bResult = false;
		}

		//What the configuration builds is loaded into the emulator it starts; it must have been built.
		szFile.Init();
		if (bResult && bLaunch && CBuild::GetImagePath(mcSolution.GetDirectory(), pcConfiguration, &szFile))
		{
			if (cFileUtil.Exists(szFile.Text()))
			{
				mcDebugger.SetImage(szFile.Text(), pcConfiguration->GetImage()->GetLoad());
			}
			else
			{
				szMessage.Set("Build ");
				szMessage.Append(pcConfiguration->GetName());
				szMessage.Append(" first: ");
				szMessage.Append(szFile);
				szMessage.Append(" isn't there.");
				bResult = false;
			}
		}
		szFile.Kill();
	}

	if (!bResult)
	{
		aszLines.Init();
		aszLines.Add(szMessage.Text());
		mpBuildPane->AppendLines(&aszLines);
		mpBuildPane->SetProgress(szMessage.Text());
		aszLines.Kill();
		mpBottomTabs->Select(mpBuildTab);
	}
	szDirectory.Kill();
	szExecutable.Kill();
	szMessage.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What the debugger has told since (what it started), in the Build tab.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowDebugMessages(void)
{
	CArrayChars	aszLines;

	aszLines.Init();
	mcDebugger.TakeMessages(&aszLines);
	mpBuildPane->AppendLines(&aszLines);
	aszLines.Kill();
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
		if (!ChooseEmulator(true))
		{
			return false;
		}
		bResult = mcDebugger.Start(false);
		ShowDebugMessages();
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
		if (!ChooseEmulator(true))
		{
			return false;
		}
		bResult = mcDebugger.Start(true);
		ShowDebugMessages();
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

	mpBuildPane->Clear();
	if (!ChooseEmulator(false))
	{
		return false;
	}
	bResult = mcDebugger.Attach(iPort);
	ShowDebugMessages();
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
// toggle (F9 at the disassembly's caret), buildtab (the Build tab shown)
// and wait:<milliseconds>.  Each but bp, buildtab and wait waits for the
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
	else if (strncmp(szCommand, "wait:", 5) == 0)
	{
		//The debugger is polled meanwhile.
		if (miDebugScriptWait == 0)
		{
			miDebugScriptWait = GetDebugTickCount();
		}
		if (GetDebugTickCount() - miDebugScriptWait >= atoi(&szCommand[5]))
		{
			miDebugScriptWait = 0;
			bDone = true;
		}
	}
	else if (strcmp(szCommand, "buildtab") == 0)
	{
		mpBottomTabs->Select(mpBuildTab);
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
