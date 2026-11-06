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
#ifndef __IDE_DEBUGGING_H__
#define __IDE_DEBUGGING_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "BaseLib/ArrayChars.h"
#include "WindowLib/Window.h"
#include "WindowLib/MenuContainer.h"
#include "BuildLib/BuildConfiguration.h"
#include "IDEDebugger.h"
#include "IDEDebugView.h"
#include "WindowLib/Button.h"
#include "WindowLib/DockManager.h"
#include "IDEEditorTab.h"


class CIDEContent;


//The Debug menu (and Visual Studio's keys) debugging the HCP65 emulator at the instruction level, see CIDEDebugger:
//the Debug tab under the editor shows its disassembly and registers, and breakpoints are set in the editors'
//gutters.  It also runs HCP65_IDE_DEBUG's script (CIDEScript's commands), which waits for the debugger.
class CIDEDebugging : public CObject
{
friend class CIDEContent;
friend class CIDEScript;
CONSTRUCTABLE(CIDEDebugging);
DESTRUCTABLE(CIDEDebugging);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>	mpDialog;  //The address or port asked for, while it's shown.
	Ptr<CIDEDebugView>	mpDebugView;  //The Debug tab,
	Ptr<CDockPane>		mpDebugTool;  //its tool window,
	Ptr<CButton>		mpStartButton;  //and the toolbar's Start, or Continue while stopped.
	CIDEDebugger				mcDebugger;
	CChars						mszDefaultEmulator;	//A W65C816 configuration's emulator when it doesn't say.
	CChars						mszDefaultEmulatorDirectory;
	CChars						mszStoppedFile;		//The source file the debugger is stopped in, and its line
	uint32						muiStoppedLine;		//(1 based), 0 if it isn't stopped in one.
	Ptr<CMenuItem>				mpStartItem;		//Debug's items.
	Ptr<CMenuItem>				mpStepIntoItem;
	Ptr<CMenuItem>				mpStepOverItem;
	Ptr<CMenuItem>				mpStepOutItem;
	Ptr<CMenuItem>				mpBreakItem;
	Ptr<CMenuItem>				mpStopItem;
	Ptr<CMenuItem>				mpRestartItem;
	Ptr<CMenuItem>				mpToggleItem;
	Ptr<CMenuItem>				mpNewBreakpointItem;
	Ptr<CMenuItem>				mpDeleteBreakpointsItem;
	Ptr<CMenuItem>				mpAttachItem;
	CArrayChars					maszDebugScript;	//HCP65_IDE_DEBUG's commands still to do, for checking.
	int64						miDebugScriptTime;	//When the debugger was last seen not running, for a scripted break.
	int64						miDebugScriptWait;	//When a scripted wait started, 0 if none is.
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				StartClicked(void);
	void				SetDefaultEmulator(const char* szExecutable, const char* szDirectory);
	bool				StartDebugging(void);
	bool				StopDebugging(void);
	bool				RestartDebugging(void);
	bool				BreakAll(void);
	bool				StepInto(void);
	bool				StepOver(void);
	bool				StepOut(void);
	bool				ToggleBreakpoint(void);
	bool				ToggleSourceBreakpoint(Ptr<CIDEEditorTab> pTab, size uiLine);
	void				GlyphClicked(CCodeEditor* pcEditor, size uiLine);
	void				ReadLineTables(void);
	void				ShowBreakpoints(Ptr<CIDEEditorTab> pTab);
	void				ShowAllBreakpoints(void);
	void				ShowStoppedLine(bool bWasStopped);
	bool				IsDisassemblyFocussed(void);
	bool				ShowNewBreakpointDialog(void);
	bool				ShowAttachDialog(void);
	bool				AttachDebugger(int iPort);
	void				SetDebugScript(const char* szCommands);
	bool				IsDebugScriptRunning(void);
	CIDEDebugger*		GetDebugger(void);
	Ptr<CIDEDebugView>	GetDebugView(void);
	Ptr<CDockPane>		GetTool(void);
	void				CreatePane(SDockPlace* psPlace);
	Ptr<CButton>		CreateStartButton(void);
	void				GetUserSettingsFileName(CChars* pszDest);

protected:
	Ptr<CMenuContainer>	CreateDebugMenu(void);
	bool				DebugMenuItemClicked(CMenuItem* pcItem);
	void				DebugCommandAction(EComponentInputCommand eCommand);
	void				PollDebugger(Ptr<CWindow> pWindow);
	void				UpdateDebugView(bool bKeepCaret);
	void				RunDebugScript(void);
	bool				ChooseEmulator(bool bLaunch);
	bool				ChooseDebugLoads(CBuildConfiguration* pcConfiguration, CChars* pszMessage, CArrayChars* paszLines);
	void				SaveBreakpoints(void);
	void				ShowDebugMessages(void);
	void				ResolveEmulatorPath(const char* szPath, CChars* pszDest);
	bool				ShowAddressDialog(const char* szTitle, const char* szLabel, const char* szDefault, bool bHex, uint32* puiValue);
};


#endif // __IDE_DEBUGGING_H__

