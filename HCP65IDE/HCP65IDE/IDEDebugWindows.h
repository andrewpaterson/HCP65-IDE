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
#ifndef __IDE_DEBUG_WINDOWS_H__
#define __IDE_DEBUG_WINDOWS_H__
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Object.h"
#include "StandardLib/Array.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/DockPane.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/Popup.h"
#include "WindowLib/Dialog.h"
#include "IDEUserFile.h"
#include "IDEEditorTab.h"
#include "IDEDebugStop.h"
#include "IDEValuesPane.h"


#define IDE_DEBUG_WATCHES		4		//Watch 1 to 4.
#define IDE_CALL_STACK_ID		"CallStack"
#define IDE_LOCALS_ID			"Locals"
#define IDE_AUTOS_ID			"Autos"
#define IDE_WATCH_ID			"Watch"		//Then its number, "Watch1".


class CIDEContent;


//The debugger's windows (DebuggerPlan.txt E, decision 2: Visual Studio's), docked tool windows with the Debug tab,
//hidden until they are shown from Debug > Windows (and Call Stack and Locals as debugging starts), each showing the
//stop as it is now (CIDEDebugStop) and empty while the emulator runs:
//
//Call Stack: a row per call, "modplay.exe!Scale(SPoint* psPoint = 0x012345, int iBy = 2) Line 5", the innermost with a
//yellow arrow.  Enter or a double click on one chooses it: its line is shown in the editor, with a green arrow in the
//gutter, and Locals, Autos and the Watches show that call.
//
//Locals: the chosen call's parameters and locals in scope at its line.  Autos: what the line and the one before use, and
//what a call just stepped out of returned.  Watch 1-4: expressions typed in (or added by Add Watch, from the editor's
//menu), kept with the breakpoints in the workspace's .hcp65user.  QuickWatch (Shift+F9) evaluates the expression at the
//caret in a dialog: Reevaluate, Add Watch.  A data tip shows the value of the name the pointer rests on in the editor
//while stopped, opened like a Locals row.  Values are typed into the Value column; Hexadecimal Display shows integers in
//hexadecimal everywhere.
class CIDEDebugWindows : public CObject, public CIDEUserPart
{
CONSTRUCTABLE(CIDEDebugWindows);
DESTRUCTABLE(CIDEDebugWindows);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	CIDEDebugStop			mcStop;
	Ptr<CTreeTable>			mpCallStack;
	Ptr<CDockPane>			mpCallStackTool;
	Ptr<CIDEValuesPane>		mpLocals;
	Ptr<CDockPane>			mpLocalsTool;
	Ptr<CIDEValuesPane>		mpAutos;
	Ptr<CDockPane>			mpAutosTool;
	CArray<CIDEValuesPane>	maWatches;
	CArray<CDockPane>		maWatchTools;
	CArrayChars				maszWatches[IDE_DEBUG_WATCHES];		//Each Watch's expressions.
	Ptr<CImageCel>			mpCurrentArrow;		//The innermost call's (yellow), the chosen call's (green), and none.
	Ptr<CImageCel>			mpChosenArrow;
	Ptr<CImageCel>			mpNoArrow;
	Ptr<CDialog>			mpQuickWatch;		//While it is shown,
	Ptr<CIDEValuesPane>		mpQuickPane;		//its value,
	CChars					mszQuickExpression;	//and what it evaluates.
	bool					mbQuickWatchDue;	//Shift+F9 was pressed: it is shown at the next tick.
	Ptr<CIDEValuesPane>		mpTipPane;			//The data tip's value, in a popup while it shows,
	CChars					mszTipExpression;	//and its expression,
	Ptr<CIDEEditorTab>		mpTipTab;			//where in which editor it is,
	size					muiTipStart;
	size					muiTipEnd;
	SInt32Vec2				msTipPointer;		//Where the pointer was when the tip showed: it stays until it moves.
	size					muiRestPosition;	//The character the pointer rests on, MAX_SIZE for none,
	int64					miRestTime;			//and since when.

public:
	void			Init(CIDEContent* pcContent, CIDEDebugger* pcDebugger);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			CreatePanes(SDockPlace* psPlace);
	void			AddCommands(Ptr<CCommandSet> pCommands);
	void			AddDebugItems(Ptr<CMenuContainer> pMenu);
	void			AddEditorItems(Ptr<CMenuContainer> pMenu);
	void			Poll(int64 iNow);
	void			ShowAll(void);
	void			DebuggingStarted(void);
	void			SteppingOut(void);
	bool			GetChosenLine(CChars* pszFile, uint32* puiLine);
	CIDEDebugStop*	GetStop(void);

	bool			ChooseFrame(size uiFrame);
	bool			AddWatch(size uiWatch, const char* szExpression);
	bool			ShowDataTip(Ptr<CIDEEditorTab> pTab, size uiPosition);
	void			HideDataTip(void);
	bool			ToggleHexadecimal(void);
	bool			IsHexadecimal(void);
	Ptr<CIDEValuesPane>	GetPane(const char* szName);
	void			PrintCallStack(CChars* pszDest);

	void			WriteUser(CXMLWriter* pcWriter) override;
	void			ReadUser(CMarkupTag* pcRoot) override;

	static bool		FindExpressionAt(const char* szText, size uiPosition, size* puiStart, size* puiEnd);

protected:
	void			ShowCallStack(void);
	void			ShowLocals(void);
	void			ShowAutos(void);
	void			ShowWatch(size uiWatch);
	void			ShowQuick(void);
	void			Evaluate(const char* szExpression, Ptr<CIDEValuesPane> pPane);
	bool			GetSourceLines(const char* szFile, uint32 uiLine, CChars* pszDest);
	bool			GetEditorExpression(CChars* pszDest);
	void			PollDataTip(int64 iNow);
	void			ShowQuickWatch(void);
	void			FrameActivated(void);
	void			ValueWritten(void);
	void			WatchRenamed(CIDEValuesPane* pcPane);
	Ptr<CIDEValuesPane>	CreateValuesPane(bool bWatch);
	Ptr<CDockPane>	AddTool(const char* szID, const char* szTitle, Ptr<CComponent> pContent, SDockPlace* psPlace);

	bool			ShowCallStackTool(void);
	bool			ShowLocalsTool(void);
	bool			ShowAutosTool(void);
	bool			ShowWatch1Tool(void);
	bool			ShowWatch2Tool(void);
	bool			ShowWatch3Tool(void);
	bool			ShowWatch4Tool(void);
	bool			QueueQuickWatch(void);
	bool			AddWatchAtCaret(void);
	bool			IsStopped(void);
};


#endif // __IDE_DEBUG_WINDOWS_H__
