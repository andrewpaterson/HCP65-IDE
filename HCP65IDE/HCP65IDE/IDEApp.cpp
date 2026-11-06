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
#include "StandardLib/Objects.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/JewelColours.h"
#include <stdio.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/FileUtil.h"
#include "IDEWindowTick.h"
#include "BaseLib/Timer.h"
#include "IDEApp.h"


//The HCP65 emulator a W65C816 configuration is debugged in when its configuration doesn't name one, in the HCP65
//repository beside the IDE's, built as the IDE is (HCP65_IDE_EMULATOR is another).
#ifdef _WIN32
#ifdef _WIN64
#define IDE_EMULATOR_PLATFORM		"x64/"
#else
#define IDE_EMULATOR_PLATFORM		""
#endif
#ifdef _DEBUG
#define IDE_EMULATOR				"../../../HCP65/HCP65-Emulator/" IDE_EMULATOR_PLATFORM "Debug/HCP65Emulator.exe"
#else
#define IDE_EMULATOR				"../../../HCP65/HCP65-Emulator/" IDE_EMULATOR_PLATFORM "Release/HCP65Emulator.exe"
#endif
#else
#define IDE_EMULATOR				"../../../build/HCP65Emulator/HCP65Emulator"	//The shared build directories, beside the repositories.
#endif
#define IDE_EMULATOR_DIRECTORY		"../../../HCP65/HCP65-Emulator/HCP65Emulator"


//The user's recent solutions (GetIDERecentFile).
#define IDE_RECENT_DIRECTORY		"HCP65IDE"
#define IDE_RECENT_FILE				"RecentSolutions.xml"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::Init(void)
{
	PreInit();

	mbValid = false;
	mbInputs = false;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The user's file the recent solutions are kept in:
// %APPDATA%\HCP65IDE\RecentSolutions.xml, or ~/.config/HCP65IDE/ on
// Linux; HCP65_IDE_RECENT if it is set.  Empty if there's no such place.
//
//////////////////////////////////////////////////////////////////////////
void GetIDERecentFile(CChars* pszDest)
{
	CFileUtil	cFileUtil;
	const char*	szBase;

	pszDest->Init();
	if (getenv("HCP65_IDE_RECENT") != NULL)
	{
		pszDest->Set(getenv("HCP65_IDE_RECENT"));
		return;
	}
#ifdef _WIN32
	szBase = getenv("APPDATA");
	if (szBase != NULL)
	{
		pszDest->Set(szBase);
	}
#else
	szBase = getenv("HOME");
	if (szBase != NULL)
	{
		pszDest->Set(szBase);
		cFileUtil.AppendToPath(pszDest, ".config");
	}
#endif
	if (pszDest->Empty())
	{
		return;
	}
	cFileUtil.AppendToPath(pszDest, IDE_RECENT_DIRECTORY);
	cFileUtil.AppendToPath(pszDest, IDE_RECENT_FILE);
}


//////////////////////////////////////////////////////////////////////////
//
// The window and its content, with paszSolutions open (a workspace, or
// solutions opened together, the first the startup one), or nothing if it
// is empty.  The recent solutions are read (HCP65_IDE_RECENT, the file
// they're in, and HCP65_IDE_RECENT_OPEN, one opened, for checking).  The
// devices are
// described by the XML in szInputDirectory; pcNativeInput (the platform's
// keyboard and mouse) is added to them.  Run fails if anything here did.
// HCP65_IDE_ADD (a solution added), HCP65_IDE_STARTUP and HCP65_IDE_REMOVE
// (a solution made the startup one, or removed, by its place),
// HCP65_IDE_CLOSE_SOLUTION (the solution of the file named closed, as File,
// Close Solution with it selected),
// HCP65_IDE_SNAPSHOT, HCP65_IDE_CONFIGURATION, HCP65_IDE_EXPAND,
// HCP65_IDE_OPEN, HCP65_IDE_DOCK, HCP65_IDE_CLOSE, HCP65_IDE_SCROLL, HCP65_IDE_CHANGE, HCP65_IDE_SAVE, HCP65_IDE_COMPARE,
// HCP65_IDE_TYPE_AFTER,
// HCP65_IDE_WAIT_INDEX, HCP65_IDE_CARET=line:column (1 based), HCP65_IDE_TYPE,
// HCP65_IDE_REFERENCES, HCP65_IDE_DEFINITION, HCP65_IDE_REOPEN,
// HCP65_IDE_DEBUG (CIDEContent::SetDebugScript's commands),
// HCP65_IDE_ATTACH (a port to attach the debugger to) and
// HCP65_IDE_CLAUDE (a request sent to Claude Code) are for
// checking it: a snapshot of the window, every folder expanded, files
// opened (a comma separated list, each in a tab), the
// editor scrolled down that many pixels, the indexing finished, the caret
// put somewhere, HCP65_IDE_TYPE's text typed there, every file saved, compared with
// its last commit, then Find All
// References, Go to Definition and another solution or project opened.
// Call it once the app is in the root: the objects made here are only
// held through it, and an object no root reaches is freed as soon as
// the last stack pointer to it goes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Setup(CNativeWindowFactory* pcNativeFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, CArrayChars* paszSolutions)
{
	Ptr<CIDEWindowTick>	pTick;
	int32				iNode;
	int					iLine;
	int					iColumn;
	const char*			szTyped;
	const char*			szCompare;
	Ptr<CIDEDiffTab>	pDiff;
	CArrayChars			aszOpen;
	CChars				szPath;
	size				i;

	pTick = OMalloc<CIDEWindowTick>();
	if (getenv("HCP65_IDE_SNAPSHOT") != NULL)
	{
		pTick->SetSnapshot(getenv("HCP65_IDE_SNAPSHOT"), (getenv("HCP65_IDE_SNAPSHOT_TICKS") != NULL) ? atoi(getenv("HCP65_IDE_SNAPSHOT_TICKS")) : 30);
	}
	mpWindow = OMalloc<CWindow>("HCP65 IDE", pcNativeFactory, pTick, OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);

	mpContent = OMalloc<CIDEContent>();
	pTick->SetContent(mpContent);
	mbValid = mpContent->Setup(mpWindow, "Resources/InterVariable.ttf", "Resources/JetBrainsMonoNL-Regular.ttf");
	if (mbValid)
	{
		//The HCP65 repository is beside the IDE's.
		mpContent->SetLibc65Directory("../../../HCP65/Libraries/Libc65");
		mpContent->SetDefaultEmulator((getenv("HCP65_IDE_EMULATOR") != NULL) ? getenv("HCP65_IDE_EMULATOR") : IDE_EMULATOR, IDE_EMULATOR_DIRECTORY);
		GetIDERecentFile(&szPath);
		mpContent->SetRecentFile(szPath.Text());
		szPath.Kill();

		//Sized first: what the checks below scroll to depends on the size.
		mpWindow->SetWindowSize(1280, 800);
		if (getenv("HCP65_IDE_RECENT_OPEN") != NULL)
		{
			//As File, Recent Solutions: the one of that number (0 the latest), opened at the first tick.
			mpContent->OpenRecentSolution((size)atoi(getenv("HCP65_IDE_RECENT_OPEN")));
		}
		//With no solution named, the one open when the IDE was last closed.
		if (mpContent->OpenSolutions(paszSolutions) || ((paszSolutions->NumElements() == 0) && (getenv("HCP65_IDE_RECENT_OPEN") == NULL) && mpContent->OpenLastSolution()))
		{
			if (getenv("HCP65_IDE_ADD") != NULL)
			{
				//As File, Add Solution: another solution opened with those open.
				mpContent->AddSolution(getenv("HCP65_IDE_ADD"));
			}
			if (getenv("HCP65_IDE_STARTUP") != NULL)
			{
				//As a solution's Set as Startup Solution, by its place in Solution Explorer (0 based).
				mpContent->SetStartupSolution((size)atoi(getenv("HCP65_IDE_STARTUP")));
			}
			if (getenv("HCP65_IDE_REMOVE") != NULL)
			{
				//As a solution's Remove Solution.
				mpContent->RemoveSolution((size)atoi(getenv("HCP65_IDE_REMOVE")));
			}
			if (getenv("HCP65_IDE_CONFIGURATION") != NULL)
			{
				mpContent->SetConfiguration(getenv("HCP65_IDE_CONFIGURATION"));
			}
			if (getenv("HCP65_IDE_WAIT_INDEX") != NULL)
			{
				mpContent->GetCodeModel()->WaitForIndexing();
			}
			if (getenv("HCP65_IDE_EXPAND") != NULL)
			{
				mpContent->GetExplorer()->GetTree()->ExpandAll();
			}
			if (getenv("HCP65_IDE_OPEN") != NULL)
			{
				//Each of a comma separated list, in its own tab; the last stays selected.
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_OPEN"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					iNode = mpContent->GetExplorer()->FindFileNode(aszOpen.GetText(i));
					if (iNode != TREE_NO_NODE)
					{
						mpContent->GetExplorer()->GetTree()->SelectNode(iNode);
						mpContent->ExplorerActivated();
					}
				}
				aszOpen.Kill();
			}
			if (getenv("HCP65_IDE_DOCK") != NULL)
			{
				//A comma separated list of moves, each as a pane's window position menu does it: float:<pane>,
				//dock:<pane>, autohide:<pane>, hide:<pane>, document:<pane>, show:<pane>, and reset.  Panes are named
				//by their IDs in the layout file (SolutionExplorer, FindAllReferences, Build, Debug, Claude).
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_DOCK"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					DockScripted(aszOpen.GetText(i));
				}
				aszOpen.Kill();
			}
			if (getenv("HCP65_IDE_CLOSE") != NULL)
			{
				//Each of a comma separated list of open files' names is closed, as its tab's cross does.
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_CLOSE"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					iNode = mpContent->GetExplorer()->FindFileNode(aszOpen.GetText(i));
					szPath.Init();
					if ((iNode != TREE_NO_NODE) && mpContent->GetExplorer()->GetNodeFilePath(iNode, &szPath))
					{
						mpContent->CloseFile(szPath.Text());
					}
					szPath.Kill();
				}
				aszOpen.Kill();
			}
			if (getenv("HCP65_IDE_CLOSE_SOLUTION") != NULL)
			{
				//As File, Close Solution with a file's node selected in Solution Explorer: its solution is closed.
				iNode = mpContent->GetExplorer()->FindFileNode(getenv("HCP65_IDE_CLOSE_SOLUTION"));
				if (iNode != TREE_NO_NODE)
				{
					mpContent->GetExplorer()->GetTree()->SelectNode(iNode);
					mpContent->CloseSelectedSolution();
				}
			}
			if ((getenv("HCP65_IDE_CARET") != NULL) && (sscanf(getenv("HCP65_IDE_CARET"), "%d:%d", &iLine, &iColumn) == 2))
			{
				mpContent->SetCaretPlace(iLine - 1, iColumn - 1);
			}
			if ((getenv("HCP65_IDE_TYPE") != NULL) && mpContent->GetEditor().IsNotNull())
			{
				mpWindow->GetFocus()->SetFocus(mpContent->GetEditor());
				for (szTyped = getenv("HCP65_IDE_TYPE"); *szTyped != '\0'; szTyped++)
				{
					mpWindow->InputPrintable(*szTyped, true);
				}
			}
			if (getenv("HCP65_IDE_EDIT") != NULL)
			{
				//A comma separated list of edits: open:<file>, type:<text>, undo, redo (Edit, Undo and Redo),
				//undoall, redoall (Edit, Undo and Redo Across Files), group and endgroup (the edits between as one),
				//menu (Edit opened), explorer (Alt+1, the active file selected in Solution Explorer), renamable (what
				//Rename would rename at the caret) and rename:<name> (renamed, without its dialog).  What each open file's caret line then is is printed.
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_EDIT"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					EditScripted(aszOpen.GetText(i));
				}
				aszOpen.Kill();
				EditScripted("print");
			}
			if (getenv("HCP65_IDE_GOTO") != NULL)
			{
				//<tab>:<pattern>, as Navigate's Class (0), File (1) or Symbol (2) with the pattern typed: the best
				//results are printed and the first opened.
				GoToScripted(getenv("HCP65_IDE_GOTO"));
			}
			if ((getenv("HCP65_IDE_CHANGE") != NULL) && mpContent->GetActiveTab().IsNotNull())
			{
				//The change at the line (from 1) shown, as clicking its change bar does; ",rollback" or ",diff" then
				//presses its button.
				szCompare = getenv("HCP65_IDE_CHANGE");
				mpContent->QueueChange((size)(atoi(szCompare) - 1), (strstr(szCompare, ",rollback") != NULL) ? 1 : ((strstr(szCompare, ",diff") != NULL) ? 2 : 0));
			}
			if (getenv("HCP65_IDE_SAVE") != NULL)
			{
				//As Control+Shift+S: every changed file is saved.
				mpContent->SaveAllFiles();
			}
			if (getenv("HCP65_IDE_COMPARE") != NULL)
			{
				//As Git, Compare with HEAD (or "revision:<revision>", or "file:<path>" as File, Compare with File): the
				//selected file's changes in a tab of their own.
				szCompare = getenv("HCP65_IDE_COMPARE");
				if (strncmp(szCompare, "revision:", 9) == 0)
				{
					mpContent->CompareWithRevision(&szCompare[9]);
				}
				else if (strncmp(szCompare, "file:", 5) == 0)
				{
					mpContent->CompareWithFile(&szCompare[5]);
				}
				else
				{
					mpContent->CompareWithHead();
				}

				//HCP65_IDE_TYPE_AFTER's text typed into the compared file's editor, then the diff selected again: the
				//diff follows the edit.
				pDiff = mpContent->GetActiveDiff();
				if ((getenv("HCP65_IDE_TYPE_AFTER") != NULL) && pDiff.IsNotNull())
				{
					mpContent->OpenFile(pDiff->GetSide(TDS_Right)->mszFile.Text());
					mpWindow->GetFocus()->SetFocus(mpContent->GetEditor());
					for (szTyped = getenv("HCP65_IDE_TYPE_AFTER"); *szTyped != '\0'; szTyped++)
					{
						mpWindow->InputPrintable(*szTyped, true);
					}
					mpContent->GetDock()->Show(pDiff->GetDockPane());
				}
			}
			if (getenv("HCP65_IDE_REFERENCES") != NULL)
			{
				mpContent->FindReferences();
			}
			if (getenv("HCP65_IDE_DEFINITION") != NULL)
			{
				mpContent->GoToDefinition();
			}
			if (getenv("HCP65_IDE_REOPEN") != NULL)
			{
				//Another solution or project opened, as File, Open Project/Solution does: the open files are closed.
				mpContent->OpenSolution(getenv("HCP65_IDE_REOPEN"));
			}
			if ((getenv("HCP65_IDE_SCROLL") != NULL) && mpContent->GetEditorPane().IsNotNull())
			{
				mpContent->GetEditorPane()->SetScrollPosition(0, atoi(getenv("HCP65_IDE_SCROLL")));
			}
			if (getenv("HCP65_IDE_LISTING") != NULL)
			{
				//The code of the line (from 1) of the selected file is shown once it is built, as clicking its mark does.
				mpContent->ShowListingAfterBuild((size)(atoi(getenv("HCP65_IDE_LISTING")) - 1));
			}
			if (getenv("HCP65_IDE_BUILD") != NULL)
			{
				//As Control+Shift+B: the chosen configuration is built, its output in the Build tab.
				mpContent->Build();
			}
			if (getenv("HCP65_IDE_ATTACH") != NULL)
			{
				mpContent->AttachDebugger(atoi(getenv("HCP65_IDE_ATTACH")));
			}
			if (getenv("HCP65_IDE_DEBUG") != NULL)
			{
				mpContent->SetDebugScript(getenv("HCP65_IDE_DEBUG"));
			}
			if (getenv("HCP65_IDE_CLAUDE") != NULL)
			{
				//As typed in the Claude tab and sent, from where the caret and selection are.
				mpContent->AskClaude(getenv("HCP65_IDE_CLAUDE"));
			}
		}

		mpInput = OMalloc<CInput>((CNativeInput*)NULL);
		mbValid = mpInput->Read((char*)szInputDirectory);
		mpInput->AddNativeInput(pcNativeInput);
		mpInputSource = OMalloc<CWindowInputSource>(mpInput);
		mcComponentInput.Init(&mpInputSource, &mpWindow);
		mbInputs = true;
		mcComponentInput.AddKeyboards();
		mcComponentInput.AddMice();
		mpWindow->SetComponentInput(&mcComponentInput);

		//The open editor has the keys, as IntelliJ's; Solution Explorer does with no file open.
		if (mpContent->GetActiveTab().IsNotNull())
		{
			mpWindow->GetFocus()->SetFocus(mpContent->GetActiveTab()->GetEditor());
		}
		else
		{
			mpWindow->GetFocus()->SetFocus(mpContent->GetExplorer()->GetTree());
		}
	}

	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::Class(void)
{
	M_Pointer(mpWindow);
	M_Pointer(mpContent);
	M_Pointer(mpInput);
	M_Pointer(mpInputSource);
	U_Data(CComponentInput, mcComponentInput);
	U_Bool(mbValid);
	U_Bool(mbInputs);
}


//////////////////////////////////////////////////////////////////////////
//
// Only this object's own members: the window, its content and the input
// may already be freed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::Free(void)
{
	if (mbInputs)
	{
		mcComponentInput.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the window until it is closed (if Init worked), then takes the
// component input out of the window.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Run(void)
{
	if (mbValid)
	{
		mpWindow->Show();

		//The windows' layout is kept beside the solution, with the files open, for the next time it is opened.
		if (getenv("HCP65_IDE_SNAPSHOT") == NULL)
		{
			mpContent->SaveLayout();
			mpContent->RememberOpenSolution();
		}

		//The emulator is quit while nothing is being freed: as its window goes Windows activates this one.
		mpContent->StopDebugging();
	}

	if (mbInputs)
	{
		mpWindow->SetComponentInput(NULL);
	}
	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65_IDE_GOTO's search.
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::GoToScripted(const char* szGoTo)
{
	Ptr<CGoToDialog>	pGoTo;
	SGoToItem*			psItem;
	size				i;
	size				uiTab;
	int64				iStart;

	uiTab = (size)atoi(szGoTo);
	szGoTo = strchr(szGoTo, ':');
	if (szGoTo == NULL)
	{
		return;
	}
	mpContent->CreateGoTo(uiTab);
	pGoTo = mpContent->GetGoTo();
	iStart = GetDebugTickCount();
	pGoTo->SetText(&szGoTo[1]);
	printf("Go to [%s]: %d results in %d ms\n", &szGoTo[1], (int)pGoTo->NumResults(), (int)(GetDebugTickCount() - iStart));
	for (i = 0; (i < pGoTo->NumResults()) && (i < 10); i++)
	{
		psItem = pGoTo->GetResult(i);
		printf("  %s  [%s]  score %d\n", psItem->szName.Text(), psItem->szLocation.Text(), psItem->sMatch.iScore);
	}
	if (pGoTo->Approve())
	{
		mpContent->OpenGoToChoice();
		if (mpContent->GetActiveTab().IsNotNull())
		{
			printf("Opened %s\n", mpContent->GetActiveTab()->GetFileName());
		}
	}
	fflush(stdout);
}


//////////////////////////////////////////////////////////////////////////
//
// One of HCP65_IDE_EDIT's steps.
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::EditScripted(const char* szStep)
{
	int32					iNode;
	const char*				szTyped;
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CMenuContainer>		pMenu;
	Ptr<CTextArea>			pEditor;
	CChars					szLine;
	size					uiLine;
	size					uiEnd;
	size					i;

	pTab = mpContent->GetActiveTab();
	if (strncmp(szStep, "open:", 5) == 0)
	{
		iNode = mpContent->GetExplorer()->FindFileNode(&szStep[5]);
		if (iNode != TREE_NO_NODE)
		{
			mpContent->GetExplorer()->GetTree()->SelectNode(iNode);
			mpContent->ExplorerActivated();
		}
	}
	else if ((strncmp(szStep, "type:", 5) == 0) && pTab.IsNotNull())
	{
		mpWindow->GetFocus()->SetFocus(pTab->GetEditor());
		for (szTyped = &szStep[5]; *szTyped != '\0'; szTyped++)
		{
			mpWindow->InputPrintable(*szTyped, true);
		}
	}
	else if ((strcmp(szStep, "undo") == 0) && pTab.IsNotNull())
	{
		pTab->GetEditor()->Undo();
	}
	else if ((strcmp(szStep, "redo") == 0) && pTab.IsNotNull())
	{
		pTab->GetEditor()->Redo();
	}
	else if ((strcmp(szStep, "renamable") == 0) || (strncmp(szStep, "rename:", 7) == 0))
	{
		//What Rename would rename at the caret, or renames it.
		SIDERenameTarget	sTarget;

		sTarget.Init();
		if (!mpContent->FindRenameTarget(&sTarget))
		{
			printf("Rename: nothing at the caret\n");
		}
		else if (!sTarget.bRenamable)
		{
			printf("Rename: %s [%s] can't be renamed: %s\n", sTarget.szName.Text(), sTarget.szKey.Text(), sTarget.szWhyNot.Text());
		}
		else if (szStep[6] == ':')
		{
			mpContent->RenameSymbol(&sTarget, &szStep[7]);
			printf("Rename: %s [%s] to %s\n", sTarget.szName.Text(), sTarget.szKey.Text(), &szStep[7]);
		}
		else
		{
			printf("Rename: %s [%s] at %d places\n", sTarget.szName.Text(), sTarget.szKey.Text(), (int)sTarget.asLocations.NumElements());
		}
		sTarget.Kill();
	}
	else if (strcmp(szStep, "explorer") == 0)
	{
		mpContent->ShowActiveFileInExplorer();
	}
	else if (strcmp(szStep, "undoall") == 0)
	{
		mpContent->UndoAcrossFiles();
	}
	else if (strcmp(szStep, "redoall") == 0)
	{
		mpContent->RedoAcrossFiles();
	}
	else if (strcmp(szStep, "group") == 0)
	{
		mpContent->BeginUndoGroup();
	}
	else if (strcmp(szStep, "endgroup") == 0)
	{
		mpContent->EndUndoGroup();
	}
	else if (strcmp(szStep, "menu") == 0)
	{
		mpWindow->Layout();
		mpContent->GetEditMenu()->Open(SInt32Vec2(40, 24), NULL, true);
	}
	else if (strcmp(szStep, "print") == 0)
	{
		pMenu = mpContent->GetEditMenu();
		mpContent->EditMenuOpening(&pMenu);
		printf("Edit menu:");
		for (i = 0; i < pMenu->NumItems(); i++)
		{
			printf(" %s%s", pMenu->GetItem(i)->GetText(), pMenu->GetItem(i)->IsEnabled() ? "" : "(disabled)");
		}
		printf("\n");
		for (i = 0; i < mpContent->NumTabs(); i++)
		{
			pEditor = mpContent->GetTab(i)->GetEditor();
			uiLine = pEditor->FindLine(pEditor->GetCaretPosition());
			uiEnd = (uiLine + 1 < pEditor->NumLines()) ? pEditor->GetLineStart(uiLine + 1) - 1 : strlen(pEditor->GetText());
			szLine.Init();
			szLine.AppendSubString(pEditor->GetText(), pEditor->GetLineStart(uiLine), uiEnd);
			printf("%s%s%s line %d: [%s]\n", mpContent->GetTab(i)->GetFileName(), mpContent->GetTab(i)->IsModified() ? " *" : "", (mpWindow->GetFocus()->GetFocussedComponent() == pEditor) ? " (focus)" : "", (int)uiLine + 1, szLine.Text());
			szLine.Kill();
		}
		fflush(stdout);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One of HCP65_IDE_DOCK's moves: "<action>:<pane ID>", or "reset".
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::DockScripted(const char* szMove)
{
	Ptr<CDockManager>	pDock;
	Ptr<CDockPane>		pPane;
	const char*			szColon;
	CChars				szAction;

	pDock = mpContent->GetDock();
	if (strcmp(szMove, "reset") == 0)
	{
		pDock->ResetLayout();
		return;
	}
	szColon = strchr(szMove, ':');
	if (szColon == NULL)
	{
		return;
	}
	pPane = pDock->GetPane(szColon + 1);
	if (pPane.IsNull())
	{
		gcLogger.Error2(__METHOD__, " No pane [", szColon + 1, "].", NULL);
		return;
	}
	szAction.Init(szMove, 0, (size)(szColon - szMove));
	if (szAction.Equals("float"))
	{
		pDock->Float(pPane);
	}
	else if (szAction.Equals("dock"))
	{
		pDock->Dock(pPane);
	}
	else if (szAction.Equals("autohide"))
	{
		pDock->AutoHide(pPane);
	}
	else if (szAction.Equals("hide"))
	{
		pDock->Hide(pPane);
	}
	else if (szAction.Equals("document"))
	{
		pDock->DockAsDocument(pPane);
	}
	else if (szAction.Equals("show"))
	{
		pDock->Activate(pPane);
	}
	szAction.Kill();
}

