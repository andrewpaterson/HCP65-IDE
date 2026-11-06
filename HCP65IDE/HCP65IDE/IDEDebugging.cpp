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
#include "BaseLib/Timer.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/FatImageFile.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "WindowLib/JewelColours.h"
#include "IDEContent.h"
#include "IDEDebugging.h"


#define IDE_DEBUG_POLL				20		//Milliseconds between looks at the debugger while debugging.
#define IDE_DEBUG_SCRIPT_BREAK		300		//Milliseconds a scripted break lets it run first.
#define IDE_DEBUG_DIALOG_WIDTH		360
#define IDE_USER_SETTINGS_EXTENSION	".hcp65user"	//The per-user file beside the solution: its breakpoints.
#define IDE_BREAKPOINT_COLOUR		Set32BitColour((uint8)0xE5, (uint8)0x51, (uint8)0x4F)	//Visual Studio's breakpoint red.
#define IDE_STOPPED_LINE_COLOUR		Set32BitColour((uint8)0xFF, (uint8)0xD8, (uint8)0x00)	//Its yellow arrow at the current line.


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
void CIDEDebugging::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mcDebugger.Init();
	mszDefaultEmulator.Init();
	mszDefaultEmulatorDirectory.Init();
	mszStoppedFile.Init();
	muiStoppedLine = 0;
	maszDebugScript.Init();
	miDebugScriptTime = 0;
	miDebugScriptWait = 0;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDebugView);
	M_Pointer(mpDebugTool);
	M_Pointer(mpStartButton);
	U_Unknown(CIDEDebugger, mcDebugger);
	U_String(mszDefaultEmulator);
	U_String(mszDefaultEmulatorDirectory);
	U_String(mszStoppedFile);
	U_UInt32(muiStoppedLine);
	M_Pointer(mpStartItem);
	M_Pointer(mpStepIntoItem);
	M_Pointer(mpStepOverItem);
	M_Pointer(mpStepOutItem);
	M_Pointer(mpBreakItem);
	M_Pointer(mpStopItem);
	M_Pointer(mpRestartItem);
	M_Pointer(mpToggleItem);
	M_Pointer(mpNewBreakpointItem);
	M_Pointer(mpDeleteBreakpointsItem);
	M_Pointer(mpAttachItem);
	U_Unknown(CArrayChars, maszDebugScript);
	U_Int64(miDebugScriptTime);
	U_Int64(miDebugScriptWait);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::Free(void)
{
	if (mpStartButton.IsNotNull())
	{
		mpStartButton->GetClickEvent()->Remove(this);
	}
	//An emulator being debugged is quit (or detached from).
	mszStoppedFile.Kill();
	mcDebugger.Kill();
	mszDefaultEmulatorDirectory.Kill();
	mszDefaultEmulator.Kill();
	maszDebugScript.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDebugging::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEDebugging::CreateDebugMenu(void)
{
	Ptr<CMenuContainer>	pDebug;

	pDebug = OMalloc<CMenuContainer>(mpcContent->mpWindow, mpcContent->mpTheme);
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
bool CIDEDebugging::DebugMenuItemClicked(CMenuItem* pcItem)
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
void CIDEDebugging::DebugCommandAction(EComponentInputCommand eCommand)
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
// A path from the configurations' file, relative to that file's folder.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::ResolveEmulatorPath(const char* szPath, CChars* pszDest)
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
	CBuildConfigurations::GetFileName(mpcContent->mpSolutions->GetStartupFile(), &szFile);
	pszDest->Set("");
	cFileUtil.SplitPath(szFile.Text(), &szName, pszDest);
	cFileUtil.AppendToPath(pszDest, szPath);
	cFileUtil.FullPath(pszDest);
	szName.Kill();
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What debugging pcConfiguration loads into the emulator it starts: its
// <Load>s (a kernal), then its built image; the card it boots from (its
// Card); and its built executable is written onto that card for the
// kernal to run.  What was done goes in
// paszLines.  False, with why in pszMessage, if a file isn't there (it
// must have been built) or the card can't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::ChooseDebugLoads(CBuildConfiguration* pcConfiguration, CChars* pszMessage, CArrayChars* paszLines)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	CChars		szCard;
	CChars*		pszLine;
	size		i;
	bool		bResult;

	//The card the emulator boots from, if the configuration says.
	szCard.Init();
	if (pcConfiguration->HasCard())
	{
		ResolveEmulatorPath(pcConfiguration->GetCard(), &szCard);
	}
	mcDebugger.SetCard(szCard.Text());

	szFile.Init();
	for (i = 0; i < pcConfiguration->NumLoads(); i++)
	{
		ResolveEmulatorPath(pcConfiguration->GetLoadFile(i), &szFile);
		if (!cFileUtil.Exists(szFile.Text()))
		{
			pszMessage->Set("Could not find ");
			pszMessage->Append(szFile);
			pszMessage->Append(" to load: build what makes it first.");
			szFile.Kill();
			szCard.Kill();
			return false;
		}
		mcDebugger.AddLoad(szFile.Text(), pcConfiguration->GetLoadAddress(i));
	}

	if (CBuild::GetProgramPath(mpcContent->mcWorkspace.GetStartupSolution()->mcSolution.GetDirectory(), pcConfiguration, &szFile) && !cFileUtil.Exists(szFile.Text()))
	{
		pszMessage->Set("Build ");
		pszMessage->Append(pcConfiguration->GetName());
		pszMessage->Append(" first: ");
		pszMessage->Append(szFile);
		pszMessage->Append(" isn't there.");
		szFile.Kill();
		szCard.Kill();
		return false;
	}

	bResult = true;
	if (pcConfiguration->HasImage())
	{
		mcDebugger.AddLoad(szFile.Text(), pcConfiguration->GetImage()->GetLoad());
	}
	else if (pcConfiguration->HasExecutable() && pcConfiguration->HasCard())
	{
		//The program goes on the card the emulator boots from, where the kernal finds it.
		bResult = FatPutDiskFileOnImage(szCard.Text(), pcConfiguration->GetCardPath(), szFile.Text());
		if (bResult)
		{
			pszLine = paszLines->Add("Wrote ");
			pszLine->Append(szFile)->Append(" onto ")->Append(szCard)->Append(" as ")->Append(pcConfiguration->GetCardPath())->Append('.');
		}
		else
		{
			pszMessage->Set("Could not write ");
			pszMessage->Append(szFile);
			pszMessage->Append(" onto the card ");
			pszMessage->Append(szCard);
			pszMessage->Append(" (an SD card image with a FAT volume).");
		}
	}
	szCard.Kill();
	szFile.Kill();

	//What is loaded may have been built since the line tables were read.
	if (bResult)
	{
		ReadLineTables();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Gives the debugger the chosen configuration's processor and (bLaunch)
// emulator: the one its configuration names, or for a W65C816 the default.
// False, told in the Build tab, if it has no processor (MSVC, GCC) or no
// emulator.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::ChooseEmulator(bool bLaunch)
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

	pcConfiguration = mpcContent->mpBuildConfigurations->GetConfiguration();
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
		mcDebugger.ClearLoads();
		mcDebugger.SetBreakpointSpace(pcConfiguration->GetBreakpointSpace());
		mcDebugger.SetOwnProgram((pcConfiguration->HasExecutable() && pcConfiguration->HasCard()) ? pcConfiguration->GetCardPath() : NULL);
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
			CBuildConfigurations::GetFileName(mpcContent->mpSolutions->GetStartupFile(), &szFile);
			szMessage.Set("No emulator is set for ");
			szMessage.Append(pcConfiguration->GetName());
			szMessage.Append(": give its configuration an Emulator in ");
			szMessage.Append(szFile);
			szMessage.Append('.');
			szFile.Kill();
			bResult = false;
		}

		//What the configuration builds goes into the emulator it starts (or onto its card); it must have been built.
		aszLines.Init();
		if (bResult && bLaunch)
		{
			bResult = ChooseDebugLoads(pcConfiguration, &szMessage, &aszLines);
		}
		mpcContent->mpBuilding->GetBuildPane()->AppendLines(&aszLines);
		aszLines.Kill();
	}

	if (!bResult)
	{
		aszLines.Init();
		aszLines.Add(szMessage.Text());
		mpcContent->mpBuilding->GetBuildPane()->AppendLines(&aszLines);
		aszLines.Kill();
		mpcContent->mpDock->Show(mpcContent->mpBuilding->GetBuildTool());
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
void CIDEDebugging::ShowDebugMessages(void)
{
	CArrayChars	aszLines;

	aszLines.Init();
	mcDebugger.TakeMessages(&aszLines);
	mpcContent->mpBuilding->GetBuildPane()->AppendLines(&aszLines);
	aszLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// F5: starts the emulator under the debugger (it runs to the first
// breakpoint), or continues it from where it stopped.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::StartDebugging(void)
{
	bool	bResult;

	if (mcDebugger.IsStopped())
	{
		bResult = mcDebugger.Continue();
	}
	else if (!mcDebugger.IsDebugging())
	{
		mpcContent->mpBuilding->GetBuildPane()->Clear();
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
	mpcContent->mpDock->Show(mpDebugTool);
	UpdateDebugView(false);
	mpcContent->mpWindow->RequestTick(IDE_DEBUG_POLL);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::StopDebugging(void)
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
bool CIDEDebugging::RestartDebugging(void)
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
bool CIDEDebugging::BreakAll(void)
{
	return mcDebugger.Break();
}


//////////////////////////////////////////////////////////////////////////
//
// F11.  Not debugging, it starts the emulator stopped at its first
// instruction, as Visual Studio starts at the program's first line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::StepInto(void)
{
	bool	bResult;

	if (!mcDebugger.IsDebugging())
	{
		mpcContent->mpBuilding->GetBuildPane()->Clear();
		if (!ChooseEmulator(true))
		{
			return false;
		}
		bResult = mcDebugger.Start(true);
		ShowDebugMessages();
		mpcContent->mpDock->Show(mpDebugTool);
		mpcContent->mpWindow->RequestTick(IDE_DEBUG_POLL);
	}
	else if (IsDisassemblyFocussed())
	{
		bResult = mcDebugger.StepInto();
	}
	else
	{
		bResult = mcDebugger.StepLine(false);
	}
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// F10.  Not debugging, it starts at the first instruction as F11 does.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::StepOver(void)
{
	bool	bResult;

	if (!mcDebugger.IsDebugging())
	{
		return StepInto();
	}
	bResult = IsDisassemblyFocussed() ? mcDebugger.StepOver() : mcDebugger.StepLine(true);
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Steps are by instruction in the disassembly, by source line elsewhere.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::IsDisassemblyFocussed(void)
{
	return mpcContent->mpWindow->GetFocus()->GetFocussedComponent() == mpDebugView->GetDisassembly();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::StepOut(void)
{
	bool	bResult;

	bResult = mcDebugger.StepOut();
	UpdateDebugView(false);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// F9: in the code, a breakpoint on the caret's line; otherwise one at the
// disassembly's instruction under the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::ToggleBreakpoint(void)
{
	uint32				uiAddress;
	Ptr<CIDEEditorTab>	pTab;
	int					iLine;
	int					iColumn;

	//In the code, the caret's line: a source breakpoint, as Visual Studio's F9.
	pTab = mpcContent->mpDocuments->GetActiveTab();
	if (pTab.IsNotNull() && (mpcContent->mpWindow->GetFocus()->GetFocussedComponent() == pTab->GetEditor()))
	{
		pTab->GetCaretPlace(&iLine, &iColumn);
		return ToggleSourceBreakpoint(pTab, (size)iLine);
	}

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
bool CIDEDebugging::ShowAddressDialog(const char* szTitle, const char* szLabel, const char* szDefault, bool bHex, uint32* puiValue)
{
	CTextField*	pcField;
	char*		szText;
	char*		szEnd;
	bool		bResult;

	bResult = false;
	mpcContent->mpDialog = OMalloc<CDialog>(mpcContent->mpWindow, &mpcContent->msDialogLook, szTitle);
	pcField = &mpcContent->mpDialog->AddTextField(szLabel, szDefault);
	if (mpcContent->mpDialog->Show(IDE_DEBUG_DIALOG_WIDTH))
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
	mpcContent->mpDialog = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::ShowNewBreakpointDialog(void)
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
bool CIDEDebugging::ShowAttachDialog(void)
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
bool CIDEDebugging::AttachDebugger(int iPort)
{
	bool	bResult;

	mpcContent->mpBuilding->GetBuildPane()->Clear();
	if (!ChooseEmulator(false))
	{
		return false;
	}
	bResult = mcDebugger.Attach(iPort);
	ShowDebugMessages();
	mpcContent->mpDock->Show(mpDebugTool);
	UpdateDebugView(false);
	mpcContent->mpWindow->RequestTick(IDE_DEBUG_POLL);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's output goes in the Build tab.  While debugging it is
// looked at again soon.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::PollDebugger(Ptr<CWindow> pWindow)
{
	CArrayChars	aszOutput;
	bool		bChanged;
	bool		bStopped;

	if (!mcDebugger.IsDebugging())
	{
		ShowStoppedLine(false);
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
	mpcContent->mpBuilding->GetBuildPane()->AppendLines(&aszOutput);
	aszOutput.Kill();
	if (bChanged)
	{
		UpdateDebugView(false);
		if (!bStopped && mcDebugger.IsStopped())
		{
			mpcContent->mpDock->Show(mpDebugTool);
		}
		ShowStoppedLine(bStopped);
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
void CIDEDebugging::UpdateDebugView(bool bKeepCaret)
{
	mpDebugView->Update(&mcDebugger, bKeepCaret);
	mpStartButton->SetCaption(mcDebugger.IsStopped() ? "Continue" : "Start");
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65_IDE_DEBUG's comma separated commands, for checking the debugger:
// start (F5), into (F11), over (F10), out (Shift+F11), break (Break All,
// once it has run a little), restart, stop, bp:<hex address> and
// toggle (F9 at the disassembly's caret), buildtab (the Build tab shown),
// line:<line> (a source breakpoint toggled on the open file's line, 1
// based), built (until HCP65_IDE_BUILD's build has finished) and
// wait:<milliseconds>.  Each but bp, buildtab, line, built and wait waits
// for the debugger to be stopped (or not debugging, for start).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::SetDebugScript(const char* szCommands)
{
	maszDebugScript.Kill();
	maszDebugScript.Init();
	maszDebugScript.Split(szCommands, ',');
	miDebugScriptTime = GetDebugTickCount();
	mpcContent->mpWindow->RequestTick(IDE_DEBUG_POLL);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::IsDebugScriptRunning(void)
{
	return maszDebugScript.NumElements() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::RunDebugScript(void)
{
	char*	szCommand;
	bool	bDone;
	CChars	szScriptCommand;

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

	//The IDE's own commands (not the debugger's) are the script's; they're done at once, and may show a dialog, so
	//they're taken off the list first.
	if (mpcContent->mpScript->IsCommand(szCommand))
	{
		szScriptCommand.Init(szCommand);
		maszDebugScript.Remove((size)0);
		mpcContent->mpScript->RunCommand(szScriptCommand.Text());
		szScriptCommand.Kill();
		return;
	}
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
		mpcContent->mpDock->Show(mpcContent->mpBuilding->GetBuildTool());
		bDone = true;
	}
	else if (strncmp(szCommand, "line:", 5) == 0)
	{
		if (mpcContent->mpDocuments->GetActiveTab().IsNotNull())
		{
			ToggleSourceBreakpoint(mpcContent->mpDocuments->GetActiveTab(), (size)(atoi(&szCommand[5]) - 1));
		}
		bDone = true;
	}
	else if (strcmp(szCommand, "built") == 0)
	{
		bDone = !mpcContent->mpBuilding->IsBuilding();
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


CIDEDebugger* CIDEDebugging::GetDebugger(void) { return &mcDebugger; }
Ptr<CIDEDebugView> CIDEDebugging::GetDebugView(void) { return mpDebugView; }
Ptr<CDockPane> CIDEDebugging::GetTool(void) { return mpDebugTool; }


//////////////////////////////////////////////////////////////////////////
//
// The Debug tab, the emulator's disassembly and registers, put at
// psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::CreatePane(SDockPlace* psPlace)
{
	mpDebugView = OMalloc<CIDEDebugView>(mpcContent->mpWindow, mpcContent->mpTheme, mpcContent->mpCodeFamily, JEWEL_TEXT);
	mpDebugTool = mpcContent->mpDock->AddPane(IDE_DEBUG_ID, "Debug", NULL, mpDebugView->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's Start button: Continue while stopped.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDEDebugging::CreateStartButton(void)
{
	mpStartButton = OMalloc<CButton>(mpcContent->mpWindow, mpcContent->mpTheme, "Start");
	mpStartButton->SetAlignment(CSH_Left, CSV_Center);
	mpStartButton->GetClickEvent()->Add(this, &CIDEDebugging::StartClicked);
	return mpStartButton;
}


//////////////////////////////////////////////////////////////////////////
//
// A source breakpoint on line uiLine (0 based) of pTab's file goes, or one
// is added: shown in its glyph column and saved with the solution.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugging::ToggleSourceBreakpoint(Ptr<CIDEEditorTab> pTab, size uiLine)
{
	mcDebugger.ToggleSourceBreakpoint(pTab->GetFileName(), (uint32)(uiLine + 1));
	ShowBreakpoints(pTab);
	SaveBreakpoints();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A click in an editor's glyph column toggles its line's breakpoint.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::GlyphClicked(CCodeEditor* pcEditor, size uiLine)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->mpDocuments->FindTab(pcEditor);
	if (pTab.IsNotNull())
	{
		ToggleSourceBreakpoint(pTab, uiLine);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The line tables beside what the chosen configuration builds and loads
// (<program>.lines) bind the source breakpoints: its own program's in its
// breakpoint space, a load's (a kernal) in every space.  The programs of
// the workspace's other solutions that the kernal runs (on a card) bind
// theirs too, each in its own space and process.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::ReadLineTables(void)
{
	CBuildConfiguration*	pcConfiguration;
	CBuildConfiguration*	pcOther;
	CChars					szFile;
	CArrayInt				aiOthers;
	size					uiOther;
	size					i;

	mcDebugger.ClearLineTables();
	pcConfiguration = mpcContent->mpBuildConfigurations->GetConfiguration();
	if ((pcConfiguration != NULL) && pcConfiguration->IsHCP65())
	{
		szFile.Init();
		for (i = 0; i < pcConfiguration->NumLoads(); i++)
		{
			ResolveEmulatorPath(pcConfiguration->GetLoadFile(i), &szFile);
			mcDebugger.AddLineTable(szFile.Text(), false);
		}
		if (CBuild::GetProgramPath(mpcContent->mcWorkspace.GetStartupSolution()->mcSolution.GetDirectory(), pcConfiguration, &szFile))
		{
			mcDebugger.AddLineTable(szFile.Text(), true);
		}

		aiOthers.Init();
		mpcContent->mcWorkspace.GetProcessPrograms(mpcContent->mszConfiguration.Text(), &aiOthers);
		for (i = 0; i < aiOthers.NumElements(); i++)
		{
			uiOther = (size)aiOthers.GetValue(i);
			pcOther = mpcContent->mcWorkspace.FindConfiguration(uiOther, mpcContent->mszConfiguration.Text());
			if (pcOther->IsHCP65() && CBuild::GetProgramPath(mpcContent->mcWorkspace.GetSolution(uiOther)->mcSolution.GetDirectory(), pcOther, &szFile))
			{
				mcDebugger.AddProcessLineTable(szFile.Text(), pcOther->GetBreakpointSpace(), pcOther->GetCardPath());
			}
		}
		aiOthers.Kill();
		szFile.Kill();
	}
	ShowAllBreakpoints();
}


//////////////////////////////////////////////////////////////////////////
//
// pTab's file's source breakpoints in its glyph column, on the lines they
// are bound to: a red cell, or a red 'o' while unbound (no built program
// has the line's code yet) or disabled.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::ShowBreakpoints(Ptr<CIDEEditorTab> pTab)
{
	Ptr<CCodeEditor>		pEditor;
	SIDESourceBreakpoint*	psBreakpoint;
	size					i;

	pEditor = pTab->GetCodeEditor();
	pEditor->ClearLineGlyphs();
	for (i = 0; i < mcDebugger.NumSourceBreakpoints(); i++)
	{
		psBreakpoint = mcDebugger.GetSourceBreakpoint(i);
		if (!psBreakpoint->szFile.EqualsIgnoreCase(pTab->GetFileName()))
		{
			continue;
		}
		if ((psBreakpoint->uiBoundLine != 0) && psBreakpoint->bEnabled)
		{
			pEditor->SetLineGlyph(psBreakpoint->uiBoundLine - 1, ' ', IDE_BREAKPOINT_COLOUR, IDE_BREAKPOINT_COLOUR);
		}
		else
		{
			pEditor->SetLineGlyph(psBreakpoint->uiLine - 1, 'o', IDE_BREAKPOINT_COLOUR);
		}
	}

	//Where the debugger is stopped: a yellow '>', on the breakpoint's red if it has one.
	if ((muiStoppedLine != 0) && mszStoppedFile.EqualsIgnoreCase(pTab->GetFileName()))
	{
		if (pEditor->GetLineGlyph(muiStoppedLine - 1) && pEditor->GetLineGlyph(muiStoppedLine - 1)->bBackground)
		{
			pEditor->SetLineGlyph(muiStoppedLine - 1, '>', IDE_STOPPED_LINE_COLOUR, IDE_BREAKPOINT_COLOUR);
		}
		else
		{
			pEditor->SetLineGlyph(muiStoppedLine - 1, '>', IDE_STOPPED_LINE_COLOUR);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::ShowAllBreakpoints(void)
{
	size	i;

	for (i = 0; i < mpcContent->mpDocuments->NumTabs(); i++)
	{
		ShowBreakpoints(mpcContent->mpDocuments->GetTab(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The per-user file beside the workspace file (or the solution if there
// isn't one): <workspace>.hcp65user.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::GetUserSettingsFileName(CChars* pszDest)
{
	CFileUtil	cFileUtil;

	if (!mpcContent->mpSolutions->GetSettingsBaseFile(pszDest))
	{
		pszDest->Set("");
		return;
	}
	cFileUtil.RemoveExtension(pszDest);
	pszDest->Append(IDE_USER_SETTINGS_EXTENSION);
}


//////////////////////////////////////////////////////////////////////////
//
// The source breakpoints to the per-user file, when a solution is open.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::SaveBreakpoints(void)
{
	CChars	szFile;

	if (!mpcContent->mpSolutions->IsSolutionOpen())
	{
		return;
	}
	szFile.Init();
	GetUserSettingsFileName(&szFile);
	mcDebugger.WriteSourceBreakpoints(szFile.Text());
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Stopped (and not stopped before, bWasStopped) at code the line tables
// have a line for: its file is shown, the caret on that line, and the
// line marked.  Running, or not debugging, nothing is marked.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::ShowStoppedLine(bool bWasStopped)
{
	Ptr<CIDEEditorTab>	pTab;
	uint32				uiLine;
	bool				bShown;

	if (!mcDebugger.IsStopped())
	{
		if (muiStoppedLine != 0)
		{
			muiStoppedLine = 0;
			ShowAllBreakpoints();
		}
		return;
	}
	if (bWasStopped)
	{
		return;
	}

	muiStoppedLine = 0;
	bShown = mcDebugger.FindSourceLine(mcDebugger.GetProgramAddress(), &mszStoppedFile, &uiLine);
	if (bShown && mpcContent->mpDocuments->OpenFile(mszStoppedFile.Text()))
	{
		muiStoppedLine = uiLine;
		pTab = mpcContent->mpDocuments->FindTab(mszStoppedFile.Text());
		if (pTab.IsNotNull())
		{
			pTab->SetCaretPlace((int)(uiLine - 1), 0);
		}
	}
	ShowAllBreakpoints();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::StartClicked(void) { StartDebugging(); }  //The toolbar's Start button, Continue while stopped.


//////////////////////////////////////////////////////////////////////////
//
// The emulator a W65C816 configuration is debugged in when it doesn't say
// (its Emulator in the .hcp65build), and the folder it runs in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugging::SetDefaultEmulator(const char* szExecutable, const char* szDirectory)
{
	mszDefaultEmulator.Set(szExecutable);
	mszDefaultEmulatorDirectory.Set(szDirectory);
}
