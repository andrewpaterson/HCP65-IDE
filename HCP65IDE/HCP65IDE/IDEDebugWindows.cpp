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
#include <ctype.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEIcons.h"
#include "IDESymbolInfo.h"
#include "IDEDebugWindows.h"


#define IDE_DEBUG_WINDOWS_WIDTH		520		//The debugger's windows' group, beside the editors on the right.
#define IDE_CALL_STACK_WIDTH		800
#define IDE_QUICK_WATCH_WIDTH		600
#define IDE_QUICK_WATCH_HEIGHT		360
#define IDE_DATA_TIP_REST			500		//Milliseconds the pointer rests on a name before its data tip shows.
#define IDE_WATCH_TAG				"Watch"
#define IDE_CURRENT_ARROW_COLOUR	Set32BitColour((uint8)0xFF, (uint8)0xD8, (uint8)0x00)	//Visual Studio's yellow,
#define IDE_CHOSEN_ARROW_COLOUR		Set32BitColour((uint8)0x6A, (uint8)0xBF, (uint8)0x5A)	//and its green for the call chosen.
#define IDE_CHANGED_VALUE_COLOUR	Set32BitColour((uint8)0xF0, (uint8)0x50, (uint8)0x50)	//A value changed since the last stop.


//Debug > Windows' items, in order (Visual Studio's), then the Watches'.
static const char*	gaszIDEDebugWindows[] = { "Debug.CallStack", "Debug.Locals", "Debug.Autos", NULL };
static const char*	gaszIDEWatchCommands[IDE_DEBUG_WATCHES] = { "Debug.Watch1", "Debug.Watch2", "Debug.Watch3", "Debug.Watch4" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::Init(CIDEContent* pcContent, CIDEDebugger* pcDebugger)
{
	CColourFormatHelper	cFormat;
	size				ui;

	PreInit();
	mpcContent = pcContent;
	mcStop.Init(pcDebugger);
	maWatches.Init();
	maWatchTools.Init();
	for (ui = 0; ui < IDE_DEBUG_WATCHES; ui++)
	{
		maszWatches[ui].Init();
	}
	pcContent->GetWindow()->GetColourFormat(&cFormat);
	mpCurrentArrow = CIDEIcons::CreateToolIcon(ITI_Replace, IDE_CURRENT_ARROW_COLOUR, &cFormat);
	mpChosenArrow = CIDEIcons::CreateToolIcon(ITI_Replace, IDE_CHOSEN_ARROW_COLOUR, &cFormat);
	mpNoArrow = CIDEIcons::CreateToolIcon(ITI_Replace, Set32BitColour((uint8)0, (uint8)0, (uint8)0, (uint8)0), &cFormat);
	mszQuickExpression.Init();
	mbQuickWatchDue = false;
	mszTipExpression.Init();
	muiTipStart = 0;
	muiTipEnd = 0;
	msTipPointer.Init(0, 0);
	muiRestPosition = MAX_SIZE;
	miRestTime = 0;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CIDEDebugStop, mcStop);
	M_Pointer(mpCallStack);
	M_Pointer(mpCallStackTool);
	M_Pointer(mpLocals);
	M_Pointer(mpLocalsTool);
	M_Pointer(mpAutos);
	M_Pointer(mpAutosTool);
	M_Embedded(maWatches);
	M_Embedded(maWatchTools);
	U_Unknown(CArrayChars, maszWatches[0]);
	U_Unknown(CArrayChars, maszWatches[1]);
	U_Unknown(CArrayChars, maszWatches[2]);
	U_Unknown(CArrayChars, maszWatches[3]);
	M_Pointer(mpCurrentArrow);
	M_Pointer(mpChosenArrow);
	M_Pointer(mpNoArrow);
	M_Pointer(mpQuickWatch);
	M_Pointer(mpQuickPane);
	U_String(mszQuickExpression);
	U_Bool(mbQuickWatchDue);
	M_Pointer(mpTipPane);
	U_String(mszTipExpression);
	M_Pointer(mpTipTab);
	U_Size(muiTipStart);
	U_Size(muiTipEnd);
	U_Unknown(SInt32Vec2, msTipPointer);
	U_Size(muiRestPosition);
	U_Int64(miRestTime);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::Free(void)
{
	size	ui;

	if (mpCallStack.IsNotNull())
	{
		mpCallStack->GetTree()->GetActivationEvent()->Remove(this);
	}
	if (mpLocals.IsNotNull())
	{
		mpLocals->GetWrittenEvent()->Remove(this);
		mpAutos->GetWrittenEvent()->Remove(this);
	}
	for (ui = 0; ui < maWatches.NumElements(); ui++)
	{
		maWatches.Get(ui)->GetWrittenEvent()->Remove(this);
		maWatches.Get(ui)->GetRenamedEvent()->Remove(this);
	}
	mszTipExpression.Kill();
	mszQuickExpression.Kill();
	for (ui = 0; ui < IDE_DEBUG_WATCHES; ui++)
	{
		maszWatches[ui].Kill();
	}
	mcStop.Kill();
}


bool CIDEDebugWindows::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDebugWindows::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The windows, hidden, together in a group beside the editors on the
// right when they are shown (psPlace, the Debug tab's, isn't theirs: it
// is in front at every stop).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::CreatePanes(SDockPlace* psPlace)
{
	SDockPlace	sPlace;
	CChars		szID;
	CChars		szTitle;
	size		ui;

	sPlace.BesideWell(DS_Right, IDE_DEBUG_WINDOWS_WIDTH);
	mpLocals = CreateValuesPane(false);
	mpLocalsTool = AddTool(IDE_LOCALS_ID, "Locals", mpLocals->GetComponent(), &sPlace);

	sPlace.WithPane(IDE_LOCALS_ID);
	mpCallStack = OMalloc<CTreeTable>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpCallStack->AddColumn("Name", IDE_CALL_STACK_WIDTH);
	mpCallStack->GetTree()->GetActivationEvent()->Add(this, &CIDEDebugWindows::FrameActivated);
	mpCallStackTool = AddTool(IDE_CALL_STACK_ID, "Call Stack", mpCallStack, &sPlace);
	mpAutos = CreateValuesPane(false);
	mpAutosTool = AddTool(IDE_AUTOS_ID, "Autos", mpAutos->GetComponent(), &sPlace);
	szID.Init();
	szTitle.Init();
	for (ui = 0; ui < IDE_DEBUG_WATCHES; ui++)
	{
		szID.Clear();
		szID.Append(IDE_WATCH_ID)->Append((int32)(ui + 1));
		szTitle.Clear();
		szTitle.Append("Watch ")->Append((int32)(ui + 1));
		maWatches.Add(CreateValuesPane(true));
		maWatchTools.Add(AddTool(szID.Text(), szTitle.Text(), maWatches.Get(ui)->GetComponent(), &sPlace));
		maWatches.Get(ui)->GetRenamedEvent()->Add(this, &CIDEDebugWindows::WatchRenamed);
	}
	szTitle.Kill();
	szID.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A window of values; a value written in it shows every window again.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEValuesPane> CIDEDebugWindows::CreateValuesPane(bool bWatch)
{
	Ptr<CIDEValuesPane>	pPane;

	pPane = OMalloc<CIDEValuesPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetFontColours()->GetFont(IDE_CHANGED_VALUE_COLOUR), &mcStop, bWatch);
	pPane->GetWrittenEvent()->Add(this, &CIDEDebugWindows::ValueWritten);
	return pPane;
}


//////////////////////////////////////////////////////////////////////////
//
// A tool pane, hidden until it is shown.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CDockPane> CIDEDebugWindows::AddTool(const char* szID, const char* szTitle, Ptr<CComponent> pContent, SDockPlace* psPlace)
{
	return mpcContent->GetDock()->AddPane(szID, szTitle, NULL, pContent, DPK_Tool, psPlace, false);
}


//////////////////////////////////////////////////////////////////////////
//
// Visual Studio's commands; QuickWatch with its keys (Call Stack's
// Control+Alt+C is Introduce Constant here).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::AddCommands(Ptr<CCommandSet> pCommands)
{
	Ptr<CImageCel>	pNoIcon;

	pCommands->Add("Debug.CallStack", "Call Stack", "", pNoIcon, this, &CIDEDebugWindows::ShowCallStackTool);
	pCommands->Add("Debug.Locals", "Locals", "", pNoIcon, this, &CIDEDebugWindows::ShowLocalsTool);
	pCommands->Add("Debug.Autos", "Autos", "", pNoIcon, this, &CIDEDebugWindows::ShowAutosTool);
	pCommands->Add("Debug.Watch1", "Watch 1", "", pNoIcon, this, &CIDEDebugWindows::ShowWatch1Tool);
	pCommands->Add("Debug.Watch2", "Watch 2", "", pNoIcon, this, &CIDEDebugWindows::ShowWatch2Tool);
	pCommands->Add("Debug.Watch3", "Watch 3", "", pNoIcon, this, &CIDEDebugWindows::ShowWatch3Tool);
	pCommands->Add("Debug.Watch4", "Watch 4", "", pNoIcon, this, &CIDEDebugWindows::ShowWatch4Tool);
	pCommands->Add("Debug.Hexadecimal", "Hexadecimal Display", "", pNoIcon, this, &CIDEDebugWindows::ToggleHexadecimal)->SetCheckedCheck(this, &CIDEDebugWindows::IsHexadecimal);
	pCommands->Add("Debug.QuickWatch", "QuickWatch...", "Shift+F9", pNoIcon, this, &CIDEDebugWindows::QueueQuickWatch)->SetEnabledCheck(this, &CIDEDebugWindows::IsStopped);
	pCommands->Add("Debug.AddWatch", "Add Watch", "", pNoIcon, this, &CIDEDebugWindows::AddWatchAtCaret);
}


//////////////////////////////////////////////////////////////////////////
//
// Debug's Windows (Call Stack, Locals, Autos, Watch 1-4 and Hexadecimal
// Display) and QuickWatch.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::AddDebugItems(Ptr<CMenuContainer> pMenu)
{
	Ptr<CMenuContainer>	pWindows;
	Ptr<CMenuContainer>	pWatch;
	Ptr<CCommandSet>	pCommands;
	size				ui;

	pCommands = mpcContent->GetCommands();
	pWindows = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	for (ui = 0; gaszIDEDebugWindows[ui]; ui++)
	{
		pWindows->AddItem(pCommands->Find(gaszIDEDebugWindows[ui]));
	}
	pWatch = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	for (ui = 0; ui < IDE_DEBUG_WATCHES; ui++)
	{
		pWatch->AddItem(pCommands->Find(gaszIDEWatchCommands[ui]));
	}
	pWindows->AddItem("Watch", NULL, pWatch);
	pWindows->AddSeparator();
	pWindows->AddItem(pCommands->Find("Debug.Hexadecimal"));
	pMenu->AddItem("Windows", NULL, pWindows);
	pMenu->AddItem(pCommands->Find("Debug.QuickWatch"));
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' menu: Add Watch and QuickWatch, of the expression at the
// caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::AddEditorItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Debug.AddWatch"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Debug.QuickWatch"));
}


//////////////////////////////////////////////////////////////////////////
//
// Each tick: the windows show a new stop, or the memory they waited for;
// a value opened is read; QuickWatch is shown if it was asked for; the
// data tip follows the pointer.  What any of them couldn't read is asked
// for.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::Poll(int64 iNow)
{
	size	ui;

	if (mcStop.Update())
	{
		ShowAll();
	}
	mpLocals->Poll();
	mpAutos->Poll();
	for (ui = 0; ui < maWatches.NumElements(); ui++)
	{
		maWatches.Get(ui)->Poll();
	}
	if (mpQuickPane.IsNotNull())
	{
		mpQuickPane->Poll();
	}
	if (mpTipPane.IsNotNull())
	{
		mpTipPane->Poll();
	}
	if (mbQuickWatchDue)
	{
		mbQuickWatchDue = false;
		ShowQuickWatch();
	}
	PollDataTip(iNow);
	mcStop.Fetch();
}


//////////////////////////////////////////////////////////////////////////
//
// Every window shows the stop as it is now; the editors' gutters the
// chosen call's line.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowAll(void)
{
	size	ui;

	ShowCallStack();
	ShowLocals();
	ShowAutos();
	for (ui = 0; ui < maWatches.NumElements(); ui++)
	{
		ShowWatch(ui);
	}
	ShowQuick();
	if (mpTipPane.IsNotNull())
	{
		if (IsStopped())
		{
			Evaluate(mszTipExpression.Text(), mpTipPane);
		}
		else
		{
			HideDataTip();
		}
	}
	mpcContent->GetDebugging()->ShowAllBreakpoints();
	mcStop.Fetch();
}


//////////////////////////////////////////////////////////////////////////
//
// A row per call, the innermost's arrow yellow, the chosen one's green.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowCallStack(void)
{
	Ptr<CTreeContainer>	pTree;
	Ptr<CImageCel>		pArrow;
	CChars				szText;
	size				ui;

	pTree = mpCallStack->GetTree();
	pTree->RemoveAllNodes();
	szText.Init();
	for (ui = 0; ui < mcStop.NumFrames(); ui++)
	{
		szText.Clear();
		mcStop.AppendFrame(ui, &szText);
		pArrow = (ui == 0) ? mpCurrentArrow : ((ui == mcStop.GetSelectedFrame()) ? mpChosenArrow : mpNoArrow);
		pTree->AddNode(TREE_NO_NODE, szText.Text(), pArrow, (int64)ui);
	}
	szText.Kill();
	if (mcStop.NumFrames() > 0)
	{
		pTree->SelectNode((int32)mcStop.GetSelectedFrame());
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowLocals(void)
{
	CIDEDebugRoots	cRoots;

	cRoots.Init();
	mcStop.GetLocals(&cRoots);
	mpLocals->Show(&cRoots);
	cRoots.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// From the chosen call's line and the one before, as the editor has them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowAutos(void)
{
	CIDEDebugRoots	cRoots;
	CChars			szSource;
	size			uiFrame;

	cRoots.Init();
	szSource.Init();
	uiFrame = mcStop.GetSelectedFrame();
	if (mcStop.IsStopped() && GetSourceLines(mcStop.GetFrameFile(uiFrame), mcStop.GetFrameLine(uiFrame), &szSource))
	{
		mcStop.GetAutos(szSource.Text(), &cRoots);
	}
	mpAutos->Show(&cRoots);
	szSource.Kill();
	cRoots.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Its expressions' values, or why they haven't one; their names only
// while not stopped.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowWatch(size uiWatch)
{
	CIDEDebugRoots	cRoots;
	size			ui;

	cRoots.Init();
	for (ui = 0; ui < maszWatches[uiWatch].NumElements(); ui++)
	{
		mcStop.Evaluate(maszWatches[uiWatch].GetText(ui), cRoots.Add(maszWatches[uiWatch].GetText(ui)));
	}
	maWatches.Get(uiWatch)->Show(&cRoots);
	cRoots.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowQuick(void)
{
	if (mpQuickPane.IsNotNull())
	{
		Evaluate(mszQuickExpression.Text(), mpQuickPane);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One expression's value shown in pPane.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::Evaluate(const char* szExpression, Ptr<CIDEValuesPane> pPane)
{
	CIDEDebugRoots	cRoots;

	cRoots.Init();
	mcStop.Evaluate(szExpression, cRoots.Add(szExpression));
	pPane->Show(&cRoots);
	cRoots.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Line uiLine (1 based) of szFile and the line before, as its editor has
// them if it is open, else as it is on disk.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::GetSourceLines(const char* szFile, uint32 uiLine, CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	CFileUtil			cFileUtil;
	CChars				szText;
	CArrayChars			aszLines;
	char*				pvFile;
	size				uiSize;

	if ((szFile[0] == '\0') || (uiLine == 0))
	{
		return false;
	}
	pTab = mpcContent->GetDocuments()->FindTab(szFile);
	if (pTab.IsNotNull())
	{
		szText.Init(pTab->GetEditor()->GetText());
	}
	else
	{
		pvFile = (char*)cFileUtil.ReadAll(szFile, &uiSize);
		if (!pvFile)
		{
			return false;
		}
		szText.Init();
		szText.AppendSubString(pvFile, uiSize);
		free(pvFile);
	}
	aszLines.Init();
	aszLines.Split(szText.Text(), '\n');
	if (uiLine >= 2)
	{
		pszDest->Append(aszLines.GetText(uiLine - 2))->AppendNewLine();
	}
	if (uiLine <= aszLines.NumElements())
	{
		pszDest->Append(aszLines.GetText(uiLine - 1));
	}
	aszLines.Kill();
	szText.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The expression at the active editor's caret, as Visual Studio's Add
// Watch and QuickWatch take it: what is selected, else the name (and the
// members before it) the caret is in.  False if there is neither.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::GetEditorExpression(CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	size				uiStart;
	size				uiEnd;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	if (!pEditor->GetSelection(&uiStart, &uiEnd) || (uiStart == uiEnd))
	{
		if (!FindExpressionAt(pEditor->GetText(), pEditor->GetCaretPosition(), &uiStart, &uiEnd))
		{
			return false;
		}
	}
	pszDest->Set("");
	pszDest->AppendSubString(pEditor->GetText(), uiStart, uiEnd);
	pszDest->StripWhitespace();
	return !pszDest->Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// The name at uiPosition in szText, and the members it is one of ("p->x",
// "a.b.c" from c): what a data tip or Add Watch shows.  False if there is
// no name there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::FindExpressionAt(const char* szText, size uiPosition, size* puiStart, size* puiEnd)
{
	size	uiStart;
	size	uiEnd;
	size	uiBefore;

	if (!FindIDENameAt(szText, uiPosition, puiStart, puiEnd))
	{
		return false;
	}
	for (;;)
	{
		uiBefore = *puiStart;
		if ((uiBefore >= 1) && (szText[uiBefore - 1] == '.'))
		{
			uiBefore -= 1;
		}
		else if ((uiBefore >= 2) && (szText[uiBefore - 2] == '-') && (szText[uiBefore - 1] == '>'))
		{
			uiBefore -= 2;
		}
		else
		{
			return true;
		}
		if ((uiBefore == 0) || !FindIDENameAt(szText, uiBefore - 1, &uiStart, &uiEnd) || (uiEnd != uiBefore))
		{
			return true;
		}
		*puiStart = uiStart;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Debugging started: Call Stack and Locals are shown, Locals in front.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::DebuggingStarted(void)
{
	mpcContent->GetDock()->Show(mpLocalsTool);
	mpcContent->GetDock()->Show(mpCallStackTool);
	mpcContent->GetDock()->Show(mpLocalsTool);
}


//////////////////////////////////////////////////////////////////////////
//
// A Step Out is about to leave the innermost call: Autos shows what it
// returned.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::SteppingOut(void)
{
	mcStop.SteppingOut();
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen call's line, for the editor's gutter (a green arrow), when
// it isn't the innermost (the yellow arrow's).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::GetChosenLine(CChars* pszFile, uint32* puiLine)
{
	size	uiFrame;

	uiFrame = mcStop.GetSelectedFrame();
	if (!mcStop.IsStopped() || (uiFrame == 0) || (mcStop.GetFrameLine(uiFrame) == 0))
	{
		return false;
	}
	pszFile->Set(mcStop.GetFrameFile(uiFrame));
	*puiLine = mcStop.GetFrameLine(uiFrame);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Enter or a double click on a call in Call Stack.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::FrameActivated(void)
{
	int32	iNode;

	iNode = mpCallStack->GetTree()->GetSelectedNode();
	if (iNode >= 0)
	{
		ChooseFrame((size)mpCallStack->GetTree()->GetNodeData(iNode));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The windows show the call uiFrame (0 the innermost), and its line is
// shown in its editor.  False if there is no such call.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::ChooseFrame(size uiFrame)
{
	Ptr<CIDEEditorTab>	pTab;
	const char*			szFile;

	if (!mcStop.SelectFrame(uiFrame))
	{
		return false;
	}
	szFile = mcStop.GetFrameFile(uiFrame);
	if ((szFile[0] != '\0') && mpcContent->GetDocuments()->OpenFile(szFile))
	{
		pTab = mpcContent->GetDocuments()->FindTab(szFile);
		if (pTab.IsNotNull())
		{
			pTab->SetCaretPlace((int)mcStop.GetFrameLine(uiFrame) - 1, 0);
		}
	}
	ShowAll();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A value was typed into one of the windows and written: they all show
// what is read back.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ValueWritten(void)
{
	ShowAll();
}


//////////////////////////////////////////////////////////////////////////
//
// A Watch's name typed: a new expression in its empty row, or one
// changed, or one emptied (removed).  The watches are saved.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::WatchRenamed(CIDEValuesPane* pcPane)
{
	CArrayChars*	paszWatches;
	CChars			szName;
	size			uiWatch;
	size			uiRoot;

	for (uiWatch = 0; (uiWatch < maWatches.NumElements()) && (&maWatches.Get(uiWatch) != pcPane); uiWatch++)
	{
	}
	if (uiWatch == maWatches.NumElements())
	{
		return;
	}
	paszWatches = &maszWatches[uiWatch];
	uiRoot = (size)pcPane->GetEditedRoot();
	szName.Init(pcPane->GetEditedName());
	szName.StripWhitespace();
	if (uiRoot >= paszWatches->NumElements())
	{
		if (!szName.Empty())
		{
			paszWatches->Add(szName.Text());
		}
	}
	else if (szName.Empty())
	{
		paszWatches->Remove(uiRoot);
	}
	else
	{
		paszWatches->Get(uiRoot)->Set(szName);
	}
	szName.Kill();
	ShowWatch(uiWatch);
	mcStop.Fetch();
	mpcContent->GetUserFile()->Write();
}


//////////////////////////////////////////////////////////////////////////
//
// szExpression added to Watch uiWatch (0 for Watch 1), which is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::AddWatch(size uiWatch, const char* szExpression)
{
	if ((uiWatch >= IDE_DEBUG_WATCHES) || (szExpression[0] == '\0'))
	{
		return false;
	}
	maszWatches[uiWatch].Add(szExpression);
	ShowWatch(uiWatch);
	mcStop.Fetch();
	mpcContent->GetDock()->Show(maWatchTools.Get(uiWatch));
	mpcContent->GetUserFile()->Write();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The editor's Add Watch: the expression at the caret, in Watch 1.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::AddWatchAtCaret(void)
{
	CChars	szExpression;
	bool	bResult;

	szExpression.Init();
	bResult = GetEditorExpression(&szExpression) && AddWatch(0, szExpression.Text());
	szExpression.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Shift+F9: QuickWatch is shown from the next tick, as a dialog must be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::QueueQuickWatch(void)
{
	if (!IsStopped())
	{
		return false;
	}
	mbQuickWatchDue = true;
	mpcContent->GetWindow()->RequestTick(0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// QuickWatch, as Visual Studio's: the expression at the caret evaluated,
// its value under it to open; Reevaluate evaluates what is typed, Add
// Watch adds it to Watch 1, Close closes it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ShowQuickWatch(void)
{
	Ptr<CTextField>	pField;
	bool			bAgain;

	mszQuickExpression.Clear();
	GetEditorExpression(&mszQuickExpression);
	mpQuickPane = OMalloc<CIDEValuesPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetFontColours()->GetFont(IDE_CHANGED_VALUE_COLOUR), &mcStop, false);
	mpQuickPane->GetWrittenEvent()->Add(this, &CIDEDebugWindows::ValueWritten);
	do
	{
		mpQuickWatch = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "QuickWatch");
		pField = mpQuickWatch->AddTextField("Expression", mszQuickExpression.Text());
		mpQuickWatch->AddEditor("Value", mpQuickPane->GetComponent());
		mpQuickWatch->AddExtraButton("Reevaluate");
		mpQuickWatch->SetOKCaption("Add Watch");
		mpQuickWatch->SetCancelCaption("Close");
		mpQuickWatch->SetResizable(IDE_QUICK_WATCH_WIDTH, IDE_QUICK_WATCH_HEIGHT);
		ShowQuick();
		mcStop.Fetch();
		mpcContent->GetScript()->WatchDialog(mpQuickWatch);
		if (mpQuickWatch->Show(IDE_QUICK_WATCH_WIDTH, IDE_QUICK_WATCH_HEIGHT))
		{
			AddWatch(0, pField->GetText());
		}
		bAgain = StringCompare(mpQuickWatch->GetClicked(), "Reevaluate") == 0;
		mszQuickExpression.Set(pField->GetText());
		mszQuickExpression.StripWhitespace();
		mpQuickWatch = NULL;
	}
	while (bAgain && IsStopped());
	mpQuickPane->GetWrittenEvent()->Remove(this);
	mpQuickPane = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The pointer resting on a name in the active editor while stopped shows
// its data tip, until it moves off the name and the tip.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::PollDataTip(int64 iNow)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	Ptr<CPopup>			pPopup;
	CInt32Rect			cBounds;
	SInt32Vec2			sPointer;
	size				uiPosition;
	bool				bOver;

	if (!IsStopped())
	{
		HideDataTip();
		muiRestPosition = MAX_SIZE;
		return;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	sPointer = mpcContent->GetWindow()->GetFocus()->GetPointer();
	if (mpTipPane.IsNotNull() && (sPointer.x == msTipPointer.x) && (sPointer.y == msTipPointer.y))
	{
		return;
	}
	if (mpTipPane.IsNotNull())
	{
		pPopup = mpcContent->GetWindow()->GetPopup(mpTipPane->GetComponent());
		if (pPopup.IsNotNull())
		{
			pPopup->GetActualBounds(&cBounds);
			if (cBounds.ContainsPoint(sPointer.x, sPointer.y))
			{
				return;
			}
		}
	}
	uiPosition = MAX_SIZE;
	bOver = pTab.IsNotNull() && mpcContent->GetWindow()->GetFocus()->IsPointerIn() && (mpcContent->GetWindow()->GetFocus()->GetMouseOverComponent() == pTab->GetEditor());
	if (bOver)
	{
		pEditor = pTab->GetEditor();
		pEditor->GetActualBounds(&cBounds);
		bOver = pEditor->FindCharacterAt(sPointer.x - cBounds.GetLeft(), sPointer.y - cBounds.GetTop(), &uiPosition);
	}
	if (mpTipPane.IsNotNull() && (!bOver || (pTab != mpTipTab) || (uiPosition < muiTipStart) || (uiPosition >= muiTipEnd)))
	{
		HideDataTip();
	}
	if (uiPosition != muiRestPosition)
	{
		muiRestPosition = uiPosition;
		miRestTime = iNow;
		return;
	}
	if (bOver && mpTipPane.IsNull() && (iNow - miRestTime >= IDE_DATA_TIP_REST))
	{
		ShowDataTip(pTab, uiPosition);
		miRestTime = iNow;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The data tip of the expression at uiPosition in pTab's text, under its
// line: its value, opened like a Locals row.  False, with none shown, if
// there is no name there, or it has no value.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::ShowDataTip(Ptr<CIDEEditorTab> pTab, size uiPosition)
{
	Ptr<CTextArea>	pEditor;
	SIDEDebugRoot*	psRoot;
	CIDEDebugRoots	cRoots;
	CInt32Rect		cLine;
	CInt32Rect		cBounds;
	size			uiStart;
	size			uiEnd;
	bool			bValue;

	HideDataTip();
	pEditor = pTab->GetEditor();
	if (!IsStopped() || !FindExpressionAt(pEditor->GetText(), uiPosition, &uiStart, &uiEnd))
	{
		return false;
	}
	mszTipExpression.Set("");
	mszTipExpression.AppendSubString(pEditor->GetText(), uiStart, uiEnd);
	cRoots.Init();
	psRoot = cRoots.Add(mszTipExpression.Text());
	mcStop.Evaluate(mszTipExpression.Text(), psRoot);
	bValue = psRoot->bValue;
	if (bValue)
	{
		mpTipPane = OMalloc<CIDEValuesPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetFontColours()->GetFont(IDE_CHANGED_VALUE_COLOUR), &mcStop, false);
		mpTipPane->GetWrittenEvent()->Add(this, &CIDEDebugWindows::ValueWritten);
		mpTipPane->Show(&cRoots);
		mpTipTab = pTab;
		muiTipStart = uiStart;
		muiTipEnd = uiEnd;
		msTipPointer = mpcContent->GetWindow()->GetFocus()->GetPointer();
		pEditor->GetActualBounds(&cBounds);
		if (!pEditor->GetLineBounds(pEditor->FindLine(uiStart), &cLine))
		{
			cLine.Init(0, 0, 0, 0);
		}
		mpcContent->GetWindow()->OpenPopup(mpTipPane->GetComponent(), SInt32Vec2(mpcContent->GetWindow()->GetFocus()->GetPointer().x, cBounds.GetTop() + cLine.GetBottom()), pEditor);
		mcStop.Fetch();
	}
	cRoots.Kill();
	return bValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::HideDataTip(void)
{
	if (mpTipPane.IsNull())
	{
		return;
	}
	mpcContent->GetWindow()->ClosePopup(mpTipPane->GetComponent());
	mpTipPane->GetWrittenEvent()->Remove(this);
	mpTipPane = NULL;
	mpTipTab = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Hexadecimal Display: integers in hexadecimal in every window, or not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugWindows::ToggleHexadecimal(void)
{
	mcStop.SetHexadecimal(!mcStop.IsHexadecimal());
	ShowAll();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A window of values by its pane's ID ("Locals", "Autos", "Watch1",
// "QuickWatch", "DataTip"), NULL for none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEValuesPane> CIDEDebugWindows::GetPane(const char* szName)
{
	if (StringCompare(szName, IDE_LOCALS_ID) == 0)
	{
		return mpLocals;
	}
	if (StringCompare(szName, IDE_AUTOS_ID) == 0)
	{
		return mpAutos;
	}
	if (StringCompare(szName, "QuickWatch") == 0)
	{
		return mpQuickPane;
	}
	if (StringCompare(szName, "DataTip") == 0)
	{
		return mpTipPane;
	}
	if ((strncmp(szName, IDE_WATCH_ID, strlen(IDE_WATCH_ID)) == 0) && (atoi(&szName[strlen(IDE_WATCH_ID)]) >= 1) && (atoi(&szName[strlen(IDE_WATCH_ID)]) <= IDE_DEBUG_WATCHES))
	{
		return maWatches.Get(atoi(&szName[strlen(IDE_WATCH_ID)]) - 1);
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: Call Stack's rows, a line each, "=>" before the
// innermost and "->" before the chosen one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::PrintCallStack(CChars* pszDest)
{
	Ptr<CTreeContainer>	pTree;
	CChars				szText;
	int32				iNode;

	pTree = mpCallStack->GetTree();
	szText.Init();
	for (iNode = 0; iNode < pTree->NumNodes(); iNode++)
	{
		pszDest->Append((iNode == 0) ? "=> " : (((size)iNode == mcStop.GetSelectedFrame()) ? "-> " : "   "));
		szText.Clear();
		pszDest->Append(pTree->GetNodeText(iNode, &szText))->AppendNewLine();
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The Watches' expressions, in the per-user file with the breakpoints.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::WriteUser(CXMLWriter* pcWriter)
{
	CChars	szWindow;
	size	ui;
	size	uiWatch;

	szWindow.Init();
	for (uiWatch = 0; uiWatch < IDE_DEBUG_WATCHES; uiWatch++)
	{
		for (ui = 0; ui < maszWatches[uiWatch].NumElements(); ui++)
		{
			szWindow.Clear();
			szWindow.Append((int32)(uiWatch + 1));
			pcWriter->Begin(IDE_WATCH_TAG);
			pcWriter->Attribute("Window", szWindow.Text());
			pcWriter->Attribute("Expression", maszWatches[uiWatch].GetText(ui));
			pcWriter->End();
		}
	}
	szWindow.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugWindows::ReadUser(CMarkupTag* pcRoot)
{
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szWindow;
	char*			szExpression;
	size			ui;

	for (ui = 0; ui < IDE_DEBUG_WATCHES; ui++)
	{
		maszWatches[ui].ReInit();
	}
	for (pcTag = pcRoot ? pcRoot->GetTag(&sIterator) : NULL; pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
	{
		if (StringCompare(pcTag->GetName(), IDE_WATCH_TAG) != 0)
		{
			continue;
		}
		szWindow = pcTag->GetAttribute((char*)"Window");
		szExpression = pcTag->GetAttribute((char*)"Expression");
		if (szWindow && szExpression && (atoi(szWindow) >= 1) && (atoi(szWindow) <= IDE_DEBUG_WATCHES))
		{
			maszWatches[atoi(szWindow) - 1].Add(szExpression);
		}
	}
	for (ui = 0; ui < maWatches.NumElements(); ui++)
	{
		ShowWatch(ui);
	}
}


bool CIDEDebugWindows::ShowCallStackTool(void) { mpcContent->GetDock()->Show(mpCallStackTool); return true; }
bool CIDEDebugWindows::ShowLocalsTool(void) { mpcContent->GetDock()->Show(mpLocalsTool); return true; }
bool CIDEDebugWindows::ShowAutosTool(void) { mpcContent->GetDock()->Show(mpAutosTool); return true; }
bool CIDEDebugWindows::ShowWatch1Tool(void) { mpcContent->GetDock()->Show(maWatchTools.Get(0)); return true; }
bool CIDEDebugWindows::ShowWatch2Tool(void) { mpcContent->GetDock()->Show(maWatchTools.Get(1)); return true; }
bool CIDEDebugWindows::ShowWatch3Tool(void) { mpcContent->GetDock()->Show(maWatchTools.Get(2)); return true; }
bool CIDEDebugWindows::ShowWatch4Tool(void) { mpcContent->GetDock()->Show(maWatchTools.Get(3)); return true; }
bool CIDEDebugWindows::IsStopped(void) { return mcStop.IsStopped(); }
bool CIDEDebugWindows::IsHexadecimal(void) { return mcStop.IsHexadecimal(); }
CIDEDebugStop* CIDEDebugWindows::GetStop(void) { return &mcStop; }
