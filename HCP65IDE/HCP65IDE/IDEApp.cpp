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
#include <stdio.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/FileUtil.h"
#include "IDEWindowTick.h"
#include "BaseLib/Timer.h"
#include "IDEApp.h"


//The environment variables that script the IDE for checking it (see CIDEApp::Setup).  HCP65_IDE_EMULATOR isn't one: it
//only says which emulator to run.
static const char*	gaszIDEScriptVariables[] =
{
	"HCP65_IDE_ADD",
	"HCP65_IDE_KEYMAP_EDITS",
	"HCP65_IDE_ATTACH",
	"HCP65_IDE_BUILD",
	"HCP65_IDE_BUILD_EDITS",
	"HCP65_IDE_CARET",
	"HCP65_IDE_CHANGE",
	"HCP65_IDE_CLAUDE",
	"HCP65_IDE_CLOSE",
	"HCP65_IDE_CLOSE_SOLUTION",
	"HCP65_IDE_COMPARE",
	"HCP65_IDE_CONFIGURATION",
	"HCP65_IDE_DEBUG",
	"HCP65_IDE_DEFINITION",
	"HCP65_IDE_DIALOG_ANSWER",
	"HCP65_IDE_DIALOG_KEYS",
	"HCP65_IDE_DIALOG_SNAPSHOT",
	"HCP65_IDE_DOCK",
	"HCP65_IDE_EDIT",
	"HCP65_IDE_EXPAND",
	"HCP65_IDE_GOTO",
	"HCP65_IDE_STARTUP_PROJECT",
	"HCP65_IDE_FIND",
	"HCP65_IDE_LISTING",
	"HCP65_IDE_OPEN",
	"HCP65_IDE_RECENT",
	"HCP65_IDE_RECENT_OPEN",
	"HCP65_IDE_REFERENCES",
	"HCP65_IDE_EDITS_DELAY",
	"HCP65_IDE_REMOVE",
	"HCP65_IDE_REOPEN",
	"HCP65_IDE_RUN_EDITS",
	"HCP65_IDE_SAVE",
	"HCP65_IDE_SCROLL",
	"HCP65_IDE_SNAPSHOT",
	"HCP65_IDE_SNAPSHOT_TICKS",
	"HCP65_IDE_STARTUP",
	"HCP65_IDE_SYSTEM_ROOT",
	"HCP65_IDE_TYPE",
	"HCP65_IDE_TYPE_AFTER",
	"HCP65_IDE_WAIT_INDEX",
	NULL
};


//////////////////////////////////////////////////////////////////////////
//
// Whether the IDE is being scripted for checking.  A scripted run doesn't
// save the window layout (or the open solution) for the next time,
// unless HCP65_IDE_SAVE_LAYOUT says to: a check that saves it is checking
// that.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEScripted(void)
{
	size	i;

	for (i = 0; gaszIDEScriptVariables[i] != NULL; i++)
	{
		if (getenv(gaszIDEScriptVariables[i]) != NULL)
		{
			return true;
		}
	}
	return false;
}


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
#define IDE_KEYMAP_FILE				"Keymap.xml"
#define IDE_SETTINGS_FILE			"Settings.xml"


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


static void GetIDEUserFile(CChars* pszDest, const char* szFile, const char* szVariable);


//////////////////////////////////////////////////////////////////////////
//
// The user's file the recent solutions are kept in:
// %APPDATA%\HCP65IDE\RecentSolutions.xml, or ~/.config/HCP65IDE/ on
// Linux; HCP65_IDE_RECENT if it is set.  Empty if there's no such place.
//
//////////////////////////////////////////////////////////////////////////
void GetIDERecentFile(CChars* pszDest)
{
	GetIDEUserFile(pszDest, IDE_RECENT_FILE, "HCP65_IDE_RECENT");
}


//////////////////////////////////////////////////////////////////////////
//
// A file of the user's, beside the recent solutions, that says how they
// set the IDE up; the variable szVariable if it is set.  A scripted run,
// checking the IDE, has none unless it is set: the user's own isn't what's
// checked.
//
//////////////////////////////////////////////////////////////////////////
static void GetIDEUsersOwnFile(CChars* pszDest, const char* szFile, const char* szVariable)
{
	if ((getenv(szVariable) == NULL) && IsIDEScripted())
	{
		pszDest->Init();
		return;
	}
	GetIDEUserFile(pszDest, szFile, szVariable);
}


//////////////////////////////////////////////////////////////////////////
//
// The user's files the keys they've changed (HCP65_IDE_KEYMAP if it is
// set) and how they left the IDE (HCP65_IDE_SETTINGS) are kept in.
//
//////////////////////////////////////////////////////////////////////////
static void GetIDEKeymapFile(CChars* pszDest) { GetIDEUsersOwnFile(pszDest, IDE_KEYMAP_FILE, "HCP65_IDE_KEYMAP"); }
static void GetIDESettingsFile(CChars* pszDest) { GetIDEUsersOwnFile(pszDest, IDE_SETTINGS_FILE, "HCP65_IDE_SETTINGS"); }


//////////////////////////////////////////////////////////////////////////
//
// A file of the user's in %APPDATA%\HCP65IDE, or ~/.config/HCP65IDE/ on
// Linux; the variable szVariable says if it is set.  Empty if there's no
// such place.
//
//////////////////////////////////////////////////////////////////////////
static void GetIDEUserFile(CChars* pszDest, const char* szFile, const char* szVariable)
{
	CFileUtil	cFileUtil;
	const char*	szBase;

	pszDest->Init();
	if (getenv(szVariable) != NULL)
	{
		pszDest->Set(getenv(szVariable));
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
	cFileUtil.AppendToPath(pszDest, szFile);
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
// HCP65_IDE_WAIT_INDEX, HCP65_IDE_EDITS_DELAY (milliseconds the thread
// working out edits waits before each start, to check what waits for it),
// HCP65_IDE_CARET=line:column (1 based), HCP65_IDE_TYPE,
// HCP65_IDE_REFERENCES, HCP65_IDE_DEFINITION, HCP65_IDE_REOPEN,
// HCP65_IDE_DEBUG (CIDEContent::SetDebugScript's commands),
// HCP65_IDE_RUN_EDITS (CIDERunConfigurationsDialog::ApplyEdits),
// HCP65_IDE_BUILD_EDITS (CIDEBuildConfigurationsDialog::ApplyEdits),
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
	Ptr<CIDEWindowTick>		pTick;
	int32					iNode;
	int						iLine;
	int						iColumn;
	const char*				szTyped;
	const char*				szCompare;
	Ptr<CIDEDiffTab>		pDiff;
	Ptr<CClearCanvasDraw>	pBackground;
	CArrayChars				aszOpen;
	CChars					szPath;
	size					i;

	pTick = OMalloc<CIDEWindowTick>();
	if (getenv("HCP65_IDE_SNAPSHOT") != NULL)
	{
		pTick->SetSnapshot(getenv("HCP65_IDE_SNAPSHOT"), (getenv("HCP65_IDE_SNAPSHOT_TICKS") != NULL) ? atoi(getenv("HCP65_IDE_SNAPSHOT_TICKS")) : 30);
	}
	pBackground = OMalloc<CClearCanvasDraw>(0);  //Coloured once the content has made the theme.
	mpWindow = OMalloc<CWindow>("HCP65 IDE", pcNativeFactory, pTick, pBackground, CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);

	//Only what changed is drawn.  HCP65_IDE_VERIFY_REDRAW checks every partial paint against drawing all of the window
	//and logs where they differ, for finding a component that changes without being invalidated.
	mpWindow->SetPartialRedraw(true);
	mpWindow->SetVerifyRedraw(getenv("HCP65_IDE_VERIFY_REDRAW") != NULL);

	//A scripted run's commands are for it whatever window the platform's focus is in: another program's window coming
	//to the front (a second run of the checks, the person at the machine) mustn't make it ignore them, as an inactive
	//window does.
	if (IsIDEScripted())
	{
		mpWindow->SetKeepsActive(true);
	}

	mpContent = OMalloc<CIDEContent>();
	mpContent->SetKeepSettings(!IsIDEScripted() || (getenv("HCP65_IDE_SAVE_LAYOUT") != NULL));
	pTick->SetContent(mpContent);
	mbValid = mpContent->Setup(mpWindow, "Resources/InterVariable.ttf", "Resources/JetBrainsMonoNL-Regular.ttf");
	if (mbValid)
	{
		pBackground->SetColour(mpContent->GetTheme()->GetColour(TC_Panel));
		//The HCP65 repository is beside the IDE's.
		mpContent->GetBuilding()->SetLibc65Directory("../../../HCP65/Libraries/Libc65");
		mpContent->GetDebugging()->SetDefaultEmulator((getenv("HCP65_IDE_EMULATOR") != NULL) ? getenv("HCP65_IDE_EMULATOR") : IDE_EMULATOR, IDE_EMULATOR_DIRECTORY);
		GetIDERecentFile(&szPath);
		mpContent->GetSolutions()->SetRecentFile(szPath.Text());
		szPath.Kill();
		GetIDESettingsFile(&szPath);
		mpContent->GetSettings()->ReadSettings(szPath.Text());  //How the user left the IDE.
		szPath.Kill();

		//Sized first: what the checks below scroll to depends on the size.
		mpWindow->SetWindowSize(1280, 800);
		if (getenv("HCP65_IDE_RECENT_OPEN") != NULL)
		{
			//As File, Recent Solutions: the one of that number (0 the latest), opened at the first tick.
			mpContent->GetSolutions()->OpenRecentSolution((size)atoi(getenv("HCP65_IDE_RECENT_OPEN")));
		}
		//With no solution named, the one open when the IDE was last closed.
		if (mpContent->GetSolutions()->OpenSolutions(paszSolutions) || ((paszSolutions->NumElements() == 0) && (getenv("HCP65_IDE_RECENT_OPEN") == NULL) && mpContent->GetSolutions()->OpenLastSolution()))
		{
			if (getenv("HCP65_IDE_ADD") != NULL)
			{
				//As File, Add Solution: another solution opened with those open.
				mpContent->GetSolutions()->AddSolution(getenv("HCP65_IDE_ADD"));
			}
			if (getenv("HCP65_IDE_STARTUP") != NULL)
			{
				//As a solution's Set as Startup Solution, by its place in Solution Explorer (0 based).
				mpContent->GetSolutions()->SetStartupSolution((size)atoi(getenv("HCP65_IDE_STARTUP")));
			}
			if (getenv("HCP65_IDE_STARTUP_PROJECT") != NULL)
			{
				//As a project's Set as Startup Project: <solution>:<project>, the solution by its place in Solution
				//Explorer (0 based) and the project by its name.  Each solution's startup project is printed.
				mpContent->GetScript()->SetStartupProject(getenv("HCP65_IDE_STARTUP_PROJECT"));
			}
			if (getenv("HCP65_IDE_REMOVE") != NULL)
			{
				//As a solution's Remove Solution.
				mpContent->GetSolutions()->RemoveSolution((size)atoi(getenv("HCP65_IDE_REMOVE")));
			}
			if (getenv("HCP65_IDE_CONFIGURATION") != NULL)
			{
				mpContent->GetBuildConfigurations()->SetConfiguration(getenv("HCP65_IDE_CONFIGURATION"));
			}
			if (getenv("HCP65_IDE_EDITS_DELAY") != NULL)
			{
				mpContent->GetCodeModel()->SetEditsDelay(atoi(getenv("HCP65_IDE_EDITS_DELAY")));
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
						mpContent->GetSolutions()->ExplorerActivated();
					}
				}
				aszOpen.Kill();
			}
			if (getenv("HCP65_IDE_DOCK") != NULL)
			{
				//A comma separated list of moves, each as a pane's window position menu does it: float:<pane>,
				//dock:<pane>, autohide:<pane>, hide:<pane>, document:<pane>, show:<pane>, and reset.  Panes are named
				//by their IDs in the layout file (SolutionExplorer, FindUsages, Build, Indexing, Debug, Claude).
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_DOCK"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					mpContent->GetScript()->RunDockMove(aszOpen.GetText(i));
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
						mpContent->GetDocuments()->CloseFile(szPath.Text());
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
					mpContent->GetSolutions()->CloseSelectedSolution();
				}
			}
			if ((getenv("HCP65_IDE_CARET") != NULL) && (sscanf(getenv("HCP65_IDE_CARET"), "%d:%d", &iLine, &iColumn) == 2))
			{
				mpContent->GetDocuments()->SetCaretPlace(iLine - 1, iColumn - 1);
			}
			if ((getenv("HCP65_IDE_TYPE") != NULL) && mpContent->GetDocuments()->FocusActiveEditor())
			{
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
				//Rename would rename at the caret), rename:<name> (renamed, without its dialog), usages (Alt+F7, the tree
				//printed), usagegroups:<tpc> (Find Usages grouped by usage type, project and class, or none of them),
				//focus:usages (its tree focussed), select:<node> (its row of that node selected), printselected (the
				//selected row printed), printselection (the active editor's selected text and caret line), printhistory
				//(Back and Forward's places), printpopup (Control+B's usages popup) and key:<f4|space|enter|escape|documentend|documenthome> (what the key
				//gives, Jump to Source for F4).
				//What each open file's caret line then is is printed.
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_EDIT"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					mpContent->GetScript()->RunEditStep(aszOpen.GetText(i));
				}
				aszOpen.Kill();
				mpContent->GetScript()->RunEditStep("print");
			}
			if (getenv("HCP65_IDE_GOTO") != NULL)
			{
				//<tab>:<pattern>, as Navigate's Class (0), File (1) or Symbol (2) with the pattern typed: the best
				//results are printed and the first opened.
				mpContent->GetScript()->RunGoTo(getenv("HCP65_IDE_GOTO"));
			}
			if (getenv("HCP65_IDE_FIND") != NULL)
			{
				//<scope>:<options>:<text>, as Find in Files in the solution (0), the active file's project (1),
				//HCP65_IDE_FIND_DIRECTORY (2), the startup solution (3), the current file (4) or the open files (5), with Match case (c) and Words (w) or neither (-): the first results and
				//the preview are printed, the first opened, and all of them listed in the Find tool window.
				mpContent->GetScript()->RunFind(getenv("HCP65_IDE_FIND"));
			}
			if ((getenv("HCP65_IDE_CHANGE") != NULL) && mpContent->GetDocuments()->GetActiveTab().IsNotNull())
			{
				//The change at the line (from 1) shown, as clicking its change bar does; ",rollback" or ",diff" then
				//presses its button.
				szCompare = getenv("HCP65_IDE_CHANGE");
				mpContent->GetGitStatus()->QueueChange((size)(atoi(szCompare) - 1), (strstr(szCompare, ",rollback") != NULL) ? 1 : ((strstr(szCompare, ",diff") != NULL) ? 2 : 0));
			}
			if (getenv("HCP65_IDE_SAVE") != NULL)
			{
				//As Control+Shift+S: every changed file is saved.
				mpContent->GetDocuments()->SaveAllFiles();
			}
			if (getenv("HCP65_IDE_COMPARE") != NULL)
			{
				//As Git, Compare with HEAD (or "revision:<revision>", or "file:<path>" as File, Compare with File): the
				//selected file's changes in a tab of their own.
				szCompare = getenv("HCP65_IDE_COMPARE");
				if (strncmp(szCompare, "revision:", 9) == 0)
				{
					mpContent->GetDiffs()->CompareWithRevision(&szCompare[9]);
				}
				else if (strncmp(szCompare, "file:", 5) == 0)
				{
					mpContent->GetDiffs()->CompareWithFile(&szCompare[5]);
				}
				else
				{
					mpContent->GetDiffs()->CompareWithHead();
				}

				//HCP65_IDE_TYPE_AFTER's text typed into the compared file's editor, then the diff selected again: the
				//diff follows the edit.
				pDiff = mpContent->GetDiffs()->GetActiveDiff();
				if ((getenv("HCP65_IDE_TYPE_AFTER") != NULL) && pDiff.IsNotNull())
				{
					mpContent->GetDocuments()->OpenFile(pDiff->GetSide(TDS_Right)->mszFile.Text());
					mpContent->GetDocuments()->FocusActiveEditor();
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
				mpContent->GetSolutions()->OpenSolution(getenv("HCP65_IDE_REOPEN"));
			}
			if ((getenv("HCP65_IDE_SCROLL") != NULL) && mpContent->GetDocuments()->GetEditorPane().IsNotNull())
			{
				mpContent->GetDocuments()->GetEditorPane()->SetScrollPosition(0, atoi(getenv("HCP65_IDE_SCROLL")));
			}
			if (getenv("HCP65_IDE_LISTING") != NULL)
			{
				//The code of the line (from 1) of the selected file is shown once it is built, as clicking its mark does.
				mpContent->GetBuilding()->ShowListingAfterBuild((size)(atoi(getenv("HCP65_IDE_LISTING")) - 1));
			}
			if (getenv("HCP65_IDE_BUILD") != NULL)
			{
				//As Control+Shift+B: the chosen configuration is built, its output in the Build tab.
				mpContent->GetBuilding()->Build();
			}
			if (getenv("HCP65_IDE_ATTACH") != NULL)
			{
				mpContent->GetDebugging()->AttachDebugger(atoi(getenv("HCP65_IDE_ATTACH")));
			}
			if (getenv("HCP65_IDE_DEBUG") != NULL)
			{
				mpContent->GetDebugging()->SetDebugScript(getenv("HCP65_IDE_DEBUG"));
			}
			if (getenv("HCP65_IDE_CLAUDE") != NULL)
			{
				//As typed in the Claude tab and sent, from where the caret and selection are.
				mpContent->GetAskClaude()->AskClaude(getenv("HCP65_IDE_CLAUDE"));
			}
		}

		mpInput = OMalloc<CInput>((CNativeInput*)NULL);
		mbValid = mpInput->Read((char*)szInputDirectory);
		//A scripted run has the platform's keyboard and mouse but takes no input from them: what is typed while its window
		//has the focus would go into the files it edits, and a check saves them.
		if (IsIDEScripted())
		{
			pcNativeInput->AddDevicesTo(mpInput->GetDevices());
		}
		else
		{
			mpInput->AddNativeInput(pcNativeInput);
		}
		mpInputSource = OMalloc<CWindowInputSource>(mpInput);
		mcComponentInput.Init(&mpInputSource, &mpWindow);
		mbInputs = true;
		mcComponentInput.AddKeyboards();
		mcComponentInput.AddMice();
		GetIDEKeymapFile(&szPath);
		mpContent->GetSettings()->ReadKeymap(szPath.Text());  //Any the user changed.
		szPath.Kill();
		mpWindow->GetCommands()->BindKeys(&mcComponentInput);  //The IDE's keys, F5 and so on.
		mpWindow->SetComponentInput(&mcComponentInput);

		//The open editor has the keys, as IntelliJ's; Solution Explorer does with no file open.
		if (!mpContent->GetDocuments()->FocusActiveEditor())
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
	U_Unknown(CComponentInput, mcComponentInput);
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
		mpContent->SaveLayout();
		mpContent->GetSolutions()->RememberOpenSolution();

		//The emulator is quit while nothing is being freed: as its window goes Windows activates this one.
		mpContent->GetDebugging()->StopDebugging();
	}

	if (mbInputs)
	{
		mpWindow->SetComponentInput(NULL);
	}
	return mbValid;
}

