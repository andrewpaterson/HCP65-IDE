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
#include "StandardLib/ClassDefines.h"
#include "SupportLib/PNGWriter.h"
#include "IDEContent.h"
#include "WindowLib/ComponentInput.h"
#include "IDEScript.h"


//The debug script's commands that are the IDE's (RunCommand); "goto:" takes what follows it.
static const char*	gaszIDEScriptCommands[] = { "goto:", "rename", "closetab", "closeall", "exit", "gotosource", "switchfile", "printtabs", "tabmenu", "contextmenu", "edit:", "printmenus", "clickmenu:", "command:", "printkeys", "printbuttons", "printdiffs", "printgit", "printconfig", "printrun", "printsystem", "printbuildfile", "printkeymap", "printbuild", "printstatus", "printrunconfigurations", "printexplorer", "printedits", "printspans", "configuration:", "waitindex" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpDialog = NULL;
	miDialogTicks = 0;
	muiSpans = 0;
	muiAnswered = 0;
	msDialogDrawn.Clear();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_Int32(miDialogTicks);
	U_Size(muiSpans);
	U_Size(muiAnswered);
	U_Unknown(SMemberCall, msDialogDrawn);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::Free(void)
{
	StopWatchingDialog();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEScript::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEScript::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a debug script command is one of the IDE's (RunCommand).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEScript::IsCommand(const char* szCommand)
{
	size	i;
	size	uiLength;

	for (i = 0; i < sizeof(gaszIDEScriptCommands) / sizeof(char*); i++)
	{
		uiLength = strlen(gaszIDEScriptCommands[i]);
		if ((gaszIDEScriptCommands[i][uiLength - 1] == ':') ? (strncmp(szCommand, gaszIDEScriptCommands[i], uiLength) == 0) : (strcmp(szCommand, gaszIDEScriptCommands[i]) == 0))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// One of the IDE's debug script commands, done at once.  A dialog it
// shows is shown here, from the tick the script is run in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::RunCommand(const char* szCommand)
{
	size	uiTab;

	if (strncmp(szCommand, "edit:", 5) == 0)
	{
		//One of HCP65_IDE_EDIT's steps, after the commands before it (a file closed, say).
		RunEditStep(&szCommand[5]);
		fflush(stdout);
	}
	else if (strncmp(szCommand, "goto:", 5) == 0)
	{
		//Navigate's Go to dialog shown on that tab, from a tick as it must be.
		//goto:<tab>:<pattern> types the pattern too.
		uiTab = (size)atoi(&szCommand[5]);
		if (strchr(&szCommand[5], ':') != NULL)
		{
			mpcContent->GetNavigator()->mszGoToPattern.Set(strchr(&szCommand[5], ':') + 1);
		}
		mpcContent->GetNavigator()->ShowGoTo(uiTab);
	}
	else if (strcmp(szCommand, "rename") == 0)
	{
		//Rename's dialog for what is at the caret, from a tick as it must be.
		mpcContent->GetRename()->ShowRename();
	}
	else if (strcmp(szCommand, "closetab") == 0)
	{
		//The active document closed, as its tab's cross does.
		if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
		{
			mpcContent->GetDock()->Close(mpcContent->GetDocuments()->GetActiveTab()->GetDockPane());
		}
	}
	else if (strcmp(szCommand, "closeall") == 0)
	{
		//Window, Close All Documents.
		mpcContent->GetDocuments()->CloseAllFiles();
	}
	else if (strcmp(szCommand, "exit") == 0)
	{
		//File, Exit.
		if (mpcContent->GetCloseGuard()->RequestExit())
		{
			mpcContent->GetWindow()->Stop();
		}
	}
	else if (strcmp(szCommand, "gotosource") == 0)
	{
		//Control+B.
		mpcContent->GetNavigator()->GoToSource();
	}
	else if (strcmp(szCommand, "switchfile") == 0)
	{
		//Alt+O.
		mpcContent->GetNavigator()->SwitchHeaderSource();
	}
	else if (strcmp(szCommand, "printmenus") == 0)
	{
		//Every menu, as it is when it opens: the menu bar's, the editors' and Solution Explorer's.
		size	i;

		for (i = 0; i < mpcContent->GetMenuBar()->NumMenus(); i++)
		{
			PrintMenu(mpcContent->GetMenuBar()->GetItem(i)->GetCaption(), mpcContent->GetMenuBar()->GetItem(i)->GetMenu());
		}
		PrintMenu("Editor", mpcContent->GetEditorMenu());
		PrintMenu("Explorer", mpcContent->GetSolutions()->GetExplorerMenu());
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printkeys") == 0)
	{
		//Every command with keys, and whether the keyboard gives them.
		size				i;
		size				j;
		Ptr<CCommand>		pCommand;
		CComponentInput*	pcInput;

		pcInput = mpcContent->GetWindow()->GetComponentInput();
		for (i = 0; i < mpcContent->GetCommands()->NumCommands(); i++)
		{
			pCommand = mpcContent->GetCommands()->GetCommand(i);
			for (j = 0; j < pCommand->NumKeys(); j++)
			{
				printf("Keys: %s %s %s\n", pCommand->GetName(), pCommand->GetKeys(j), ((pcInput != NULL) && ((pCommand->GetKeyScope() == CKS_Editor) ? pcInput->IsEditorBound(pCommand->GetKeys(j)) : pcInput->IsBound(pCommand->GetKeys(j)))) ? "bound" : "NOT BOUND");
			}
		}
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printbuttons") == 0)
	{
		//The toolbar's buttons' commands.
		printf("Buttons: Start [%s]\n", GetCommandName(mpcContent->GetRunning()->GetStartButton()));
		fflush(stdout);
	}
	else if (strncmp(szCommand, "command:", 8) == 0)
	{
		//command:<name>, the window's command run by name (Debug.StepOver), as its keys would.
		if (!mpcContent->GetCommands()->Run(&szCommand[8]))
		{
			printf("Command [%s] not found, not enabled or did nothing.\n", &szCommand[8]);
			fflush(stdout);
		}
	}
	else if (strncmp(szCommand, "clickmenu:", 10) == 0)
	{
		//clickmenu:<menu>/<item>[/<item>], a menu bar menu's (or Editor's or Explorer's) item clicked.
		ClickMenu(&szCommand[10]);
	}
	else if (strcmp(szCommand, "printdiffs") == 0)
	{
		//Each diff's tab, '>' before the active one.
		size	i;

		printf("Diffs:");
		for (i = 0; i < mpcContent->GetDiffs()->NumDiffs(); i++)
		{
			printf(" [%s%s]", (mpcContent->GetDiffs()->GetDiff(i) == mpcContent->GetDiffs()->GetActiveDiff()) ? ">" : "", mpcContent->GetDiffs()->GetDiff(i)->GetDockPane()->GetTitle());
		}
		printf("\n");
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printgit") == 0)
	{
		printf("Git: branch [%s] change popup %s\n", mpcContent->GetGitStatus()->GetBranchLabel()->GetText(), mpcContent->GetGitStatus()->GetChangePopup().IsNotNull() ? "open" : "closed");
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printedits") == 0)
	{
		//Whether the code model's edits thread is working out edited files.
		printf("Edits thread: %s\n", mpcContent->GetCodeModel()->IsProcessingEditedFiles() ? "busy" : "idle");
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printspans") == 0)
	{
		//Whether the active editor's coloured spans changed since printspans last looked: when highlights are shown.
		size	uiSpans;

		if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
		{
			uiSpans = mpcContent->GetDocuments()->GetActiveTab()->GetEditor()->NumSpans();
			printf("Spans: %s\n", (uiSpans == muiSpans) ? "same" : "changed");
			muiSpans = uiSpans;
			fflush(stdout);
		}
	}
	else if (strncmp(szCommand, "configuration:", 14) == 0)
	{
		//As a build configuration chosen in the toolbar: the solutions are indexed again with it.
		mpcContent->GetBuildConfigurations()->SetConfiguration(&szCommand[14]);
	}
	else if (strcmp(szCommand, "waitindex") == 0)
	{
		mpcContent->GetCodeModel()->WaitForIndexing();
	}
	else if (strcmp(szCommand, "printconfig") == 0)
	{
		//The toolbar's run configuration, the build configuration it builds and each it can choose.
		Ptr<CComboBox>	pBox;
		size			i;

		pBox = mpcContent->GetBuildConfigurations()->GetRunConfigurationBox();
		printf("Configuration: [%s] [%s]:", pBox->GetSelectedText(), mpcContent->GetBuildConfigurations()->GetConfigurationName());
		for (i = 0; i < pBox->NumItems(); i++)
		{
			printf(" %s,", pBox->GetItemText(i));
		}
		printf("\n");
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printbuildfile") == 0)
	{
		//The startup solution's build configurations file.
		CChars		szFile;
		CTextFile	cFile;

		szFile.Init();
		CBuildConfigurations::GetFileName(mpcContent->GetWorkspace()->GetStartupSolution()->mszFile.Text(), &szFile);
		cFile.Init();
		printf("Build file:\n%s\n", cFile.Read(szFile.Text()) ? cFile.Text() : "(none)");
		fflush(stdout);
		cFile.Kill();
		szFile.Kill();
	}
	else if (strcmp(szCommand, "printkeymap") == 0)
	{
		//The user's keys, as they're kept.
		CTextFile	cFile;

		cFile.Init();
		printf("Keymap file:\n%s\n", cFile.Read(mpcContent->GetSettings()->GetKeymapFile()) ? cFile.Text() : "(none)");
		fflush(stdout);
		cFile.Kill();
	}
	else if (strcmp(szCommand, "printbuild") == 0)
	{
		//What the Build tab shows.
		printf("Build tab:\n%s\n", mpcContent->GetBuilding()->GetBuildPane()->GetOutput());
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printstatus") == 0)
	{
		//What the status line shows, as ShowStatus prints it only for a run that keeps no settings.
		printf("Status line: %s\n", mpcContent->GetStatusBar()->GetMessage());
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printrunconfigurations") == 0)
	{
		//What each of the workspace's run configurations builds.
		size	i;

		for (i = 0; i < mpcContent->GetWorkspace()->NumRunConfigurations(); i++)
		{
			printf("Run configuration: %s builds %s\n", mpcContent->GetWorkspace()->GetRunConfiguration(i)->GetName(), mpcContent->GetWorkspace()->GetRunConfiguration(i)->GetConfiguration());
		}
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printsystem") == 0)
	{
		//Where the code model looks for the compiler's own headers.
		CArrayChars*	paszDirectories;
		size			i;

		paszDirectories = mpcContent->GetCodeModel()->GetSystemDirectories();
		printf("System directories:");
		for (i = 0; i < paszDirectories->NumElements(); i++)
		{
			printf(" %s,", paszDirectories->GetText(i));
		}
		printf("\n");
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printrun") == 0)
	{
		//Whether the tool is running, the Start button's caption, and what the Run tab shows.
		printf("Run: %s [%s]\n%s\n", mpcContent->GetRunning()->IsRunning() ? "running" : "not running", mpcContent->GetRunning()->GetStartButton()->GetCaption(), mpcContent->GetRunning()->GetRunPane()->GetOutput());
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printexplorer") == 0)
	{
		CChars	szPath;

		szPath.Init();
		mpcContent->GetSolutions()->GetExplorer()->GetSelectedFilePath(&szPath);
		printf("Explorer: [%s]\n", szPath.Text());
		szPath.Kill();
		fflush(stdout);
	}
	else if (strcmp(szCommand, "printtabs") == 0)
	{
		//Each open file, '>' before the active one, '*' after one with changes.
		size	i;

		printf("Tabs:");
		for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
		{
			printf(" %s%s%s", (mpcContent->GetDocuments()->GetTab(i) == mpcContent->GetDocuments()->GetActiveTab()) ? ">" : "", strrchr(mpcContent->GetDocuments()->GetTab(i)->GetFileName(), '/') + 1, mpcContent->GetDocuments()->GetTab(i)->IsModified() ? "*" : "");
		}
		printf("\n");
		fflush(stdout);
	}
	else if (strcmp(szCommand, "tabmenu") == 0)
	{
		//The active document tab's menu opened under the tab, with Close Multiple's sub menu.
		if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->GetActiveTab()->GetDockPane()->GetGroup().IsNotNull())
		{
			Ptr<CDockGroup>		pGroup;
			Ptr<CTabComponent>	pTab;
			Ptr<CMenuContainer>	pMenu;
			size				i;

			pGroup = mpcContent->GetDocuments()->GetActiveTab()->GetDockPane()->GetGroup();
			pTab = pGroup->GetTab(mpcContent->GetDocuments()->GetActiveTab()->GetDockPane());
			pGroup->OpenMenu(mpcContent->GetDocuments()->GetActiveTab()->GetDockPane(), SInt32Vec2(pTab->GetPosition().x + 10, pTab->GetPosition().y + pTab->GetActualSize().y));
			pMenu = pGroup->GetMenu(mpcContent->GetDocuments()->GetActiveTab()->GetDockPane());
			for (i = 0; i < pMenu->NumItems(); i++)
			{
				if (pMenu->GetItem(i)->HasSubMenu())
				{
					mpcContent->GetWindow()->Layout();
					pMenu->OpenSubMenu(pMenu->GetItemRow(pMenu->GetItem(i)), false);
				}
			}
		}
	}
	else if (strcmp(szCommand, "contextmenu") == 0)
	{
		//The active editor's context menu opened at its caret, as the Menu key does.
		if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
		{
			mpcContent->GetDocuments()->GetActiveTab()->GetEditor()->OpenContextMenu(mpcContent->GetDocuments()->GetActiveTab()->GetEditor()->GetContextMenuPosition());
		}
	}
}



//////////////////////////////////////////////////////////////////////////
//
// The name of the command a button runs, "none" if it runs none.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEScript::GetCommandName(Ptr<CButton> pButton)
{
	if (pButton.IsNull() || pButton->GetCommand().IsNull())
	{
		return "none";
	}
	return pButton->GetCommand()->GetName();
}


//////////////////////////////////////////////////////////////////////////
//
// "<szName>: item, item(disabled), item>" for the menu as it is when it
// opens, then each of its sub menus as "<szName>/<item>: ...".
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::PrintMenu(const char* szName, Ptr<CMenuContainer> pMenu)
{
	size			i;
	Ptr<CMenuItem>	pItem;
	CChars			szSub;

	if (pMenu.IsNull())
	{
		return;
	}
	pMenu->GetOpeningEvent()->Call(&pMenu);
	printf("Menu %s:", szName);
	for (i = 0; i < pMenu->NumItems(); i++)
	{
		pItem = pMenu->GetItem(i);
		printf("%s %s", (i == 0) ? "" : ",", pItem->GetText());
		if (pItem->GetShortcut()[0] != '\0')
		{
			printf(" [%s]", pItem->GetShortcut());
		}
		printf("%s%s", pItem->IsEnabled() ? "" : "(disabled)", pItem->HasSubMenu() ? ">" : "");
	}
	printf("\n");
	for (i = 0; i < pMenu->NumItems(); i++)
	{
		pItem = pMenu->GetItem(i);
		if (pItem->HasSubMenu())
		{
			szSub.Init(szName);
			szSub.Append('/');
			szSub.Append(pItem->GetText());
			PrintMenu(szSub.Text(), pItem->GetSubMenu());
			szSub.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The menu named by szPath's first part (a menu bar menu's, or Editor or
// Explorer), its sub menus by the next parts, and the item last.  Not
// found, it says so.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::ClickMenu(const char* szPath)
{
	CArrayChars			aszParts;
	Ptr<CMenuContainer>	pMenu;
	Ptr<CMenuItem>		pItem;
	size				i;
	size				j;

	aszParts.Init();
	aszParts.Split(szPath, '/');
	if (aszParts.NumElements() < 2)
	{
		aszParts.Kill();
		return;
	}
	if (strcmp(aszParts.GetText(0), "Editor") == 0)
	{
		pMenu = mpcContent->GetEditorMenu();
	}
	else if (strcmp(aszParts.GetText(0), "Explorer") == 0)
	{
		pMenu = mpcContent->GetSolutions()->GetExplorerMenu();
	}
	for (i = 0; pMenu.IsNull() && (i < mpcContent->GetMenuBar()->NumMenus()); i++)
	{
		if (strcmp(mpcContent->GetMenuBar()->GetItem(i)->GetCaption(), aszParts.GetText(0)) == 0)
		{
			pMenu = mpcContent->GetMenuBar()->GetItem(i)->GetMenu();
		}
	}
	for (j = 1; pMenu.IsNotNull() && (j < aszParts.NumElements()); j++)
	{
		pMenu->GetOpeningEvent()->Call(&pMenu);
		pItem = NULL;
		for (i = 0; i < pMenu->NumItems(); i++)
		{
			if (strcmp(pMenu->GetItem(i)->GetText(), aszParts.GetText(j)) == 0)
			{
				pItem = pMenu->GetItem(i);
			}
		}
		if (pItem.IsNull())
		{
			pMenu = NULL;
		}
		else if (j + 1 < aszParts.NumElements())
		{
			pMenu = pItem->GetSubMenu();
		}
	}
	if (pMenu.IsNull() || pItem.IsNull())
	{
		printf("Menu item [%s] not found\n", szPath);
	}
	else if (!pItem->IsEnabled())
	{
		printf("Menu item [%s] is disabled\n", szPath);
	}
	else
	{
		pMenu->Click(pItem);
	}
	fflush(stdout);
	aszParts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65_IDE_GOTO's search.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::RunGoTo(const char* szGoTo)
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
	mpcContent->GetNavigator()->CreateGoTo(uiTab);
	pGoTo = mpcContent->GetNavigator()->GetGoTo();
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
		mpcContent->GetNavigator()->OpenGoToChoice();
		if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
		{
			printf("Opened %s\n", mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
		}
	}
	fflush(stdout);
}



//////////////////////////////////////////////////////////////////////////
//
// One of HCP65_IDE_EDIT's steps.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::RunEditStep(const char* szStep)
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

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (strncmp(szStep, "open:", 5) == 0)
	{
		iNode = mpcContent->GetExplorer()->FindFileNode(&szStep[5]);
		if (iNode != TREE_NO_NODE)
		{
			mpcContent->GetExplorer()->GetTree()->SelectNode(iNode);
			mpcContent->GetSolutions()->ExplorerActivated();
		}
	}
	else if ((strncmp(szStep, "replace:", 8) == 0) && pTab.IsNotNull())
	{
		//replace:<line>|<old>|<new>: the first <old> on the line (from 1) made <new>, as one edit.
		CArrayChars	aszParts;
		size		uiStart;
		size		uiEnd;
		const char*	szFound;

		aszParts.Init();
		aszParts.Split(&szStep[8], '|');
		if (aszParts.NumElements() >= 2)
		{
			pEditor = pTab->GetEditor();
			uiLine = (size)(atoi(aszParts.GetText(0)) - 1);
			if (uiLine < pEditor->NumLines())
			{
				szFound = strstr(&pEditor->GetText()[pEditor->GetLineStart(uiLine)], aszParts.GetText(1));
				if (szFound)
				{
					uiStart = (size)(szFound - pEditor->GetText());
					uiEnd = uiStart + strlen(aszParts.GetText(1));
					pEditor->ReplaceRanges(&uiStart, &uiEnd, 1, (aszParts.NumElements() > 2) ? aszParts.GetText(2) : "");
				}
			}
		}
		aszParts.Kill();
	}
	else if ((strncmp(szStep, "type:", 5) == 0) && pTab.IsNotNull())
	{
		mpcContent->GetDocuments()->FocusActiveEditor();
		for (szTyped = &szStep[5]; *szTyped != '\0'; szTyped++)
		{
			mpcContent->GetWindow()->InputPrintable(*szTyped, true);
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
		if (!mpcContent->GetRename()->FindRenameTarget(&sTarget))
		{
			printf("Rename: nothing at the caret\n");
		}
		else if (!sTarget.bRenamable)
		{
			printf("Rename: %s [%s] can't be renamed: %s\n", sTarget.szName.Text(), sTarget.szKey.Text(), sTarget.szWhyNot.Text());
		}
		else if (szStep[6] == ':')
		{
			mpcContent->GetRename()->RenameSymbol(&sTarget, &szStep[7]);
			printf("Rename: %s [%s] to %s\n", sTarget.szName.Text(), sTarget.szKey.Text(), &szStep[7]);
		}
		else
		{
			printf("Rename: %s [%s] at %d places, %d overrides\n", sTarget.szName.Text(), sTarget.szKey.Text(), (int)sTarget.asLocations.NumElements(), (int)sTarget.uiOverrides);
		}
		sTarget.Kill();
	}
	else if ((strcmp(szStep, "usages") == 0) || (strcmp(szStep, "references") == 0) || (strcmp(szStep, "printusages") == 0))
	{
		//Alt+F7 (or Shift+F12, with the declarations), the Find Usages tree printed, each node indented by its depth, in
		//the order the tree shows them; printusages prints the tree as it is.
		Ptr<CTreeContainer>	pTree;
		int32				iNode;
		int32				iDepth;
		CChars				szNode;

		if (szStep[0] != 'p')
		{
			mpcContent->GetUsages()->FindUsages(szStep[0] == 'r');
		}
		pTree = mpcContent->GetUsages()->GetUsagesTree();
		for (iNode = 0; iNode < pTree->NumNodes(); iNode++)
		{
			szNode.Init();
			pTree->GetNodeText(iNode, &szNode);
			for (iDepth = pTree->GetNodeDepth(iNode); iDepth > 0; iDepth--)
			{
				printf("  ");
			}
			printf("%s\n", szNode.Text());
			szNode.Kill();
		}
	}
	else if (strncmp(szStep, "usagegroups:", 12) == 0)
	{
		//Find Usages' groups, as its check boxes set them: t by usage type, p by project, c by class; none is files.
		uint8	uiGrouping;

		uiGrouping = 0;
		uiGrouping |= (strchr(&szStep[12], 't') != NULL) ? IDE_USAGES_BY_TYPE : 0;
		uiGrouping |= (strchr(&szStep[12], 'p') != NULL) ? IDE_USAGES_BY_PROJECT : 0;
		uiGrouping |= (strchr(&szStep[12], 'c') != NULL) ? IDE_USAGES_BY_CLASS : 0;
		mpcContent->GetUsages()->SetGrouping(uiGrouping);
	}
	else if (strncmp(szStep, "focus:", 6) == 0)
	{
		//The keys given to the Build output, Claude's request or the active document.
		if (strcmp(&szStep[6], "build") == 0)
		{
			mpcContent->GetDock()->Show(mpcContent->GetBuilding()->GetBuildTool());
			mpcContent->GetWindow()->GetFocus()->SetFocus(mpcContent->GetBuilding()->GetBuildPane()->GetOutputArea());
		}
		else if (strcmp(&szStep[6], "claude") == 0)
		{
			mpcContent->GetDock()->Show(mpcContent->GetAskClaude()->GetTool());
			mpcContent->GetWindow()->GetFocus()->SetFocus(mpcContent->GetAskClaude()->GetClaudePane()->GetRequestArea());
		}
		else if (strcmp(&szStep[6], "usages") == 0)
		{
			mpcContent->GetDock()->Show(mpcContent->GetUsages()->GetTool());
			mpcContent->GetWindow()->GetFocus()->SetFocus(mpcContent->GetUsages()->GetUsagesTree());
		}
		else
		{
			mpcContent->GetDocuments()->FocusActiveEditor();
		}
	}
	else if (strcmp(szStep, "printselected") == 0)
	{
		//The Find Usages tree's selected row, as "Selected: <its text>", or "Selected: none".
		Ptr<CTreeContainer>	pTree;
		CChars				szNode;

		pTree = mpcContent->GetUsages()->GetUsagesTree();
		szNode.Init();
		if (pTree->GetSelectedNode() != TREE_NO_NODE)
		{
			pTree->GetNodeText(pTree->GetSelectedNode(), &szNode);
		}
		printf("Selected: %s\n", szNode.Empty() ? "none" : szNode.Text());
		szNode.Kill();
	}
	else if (strcmp(szStep, "printrows") == 0)
	{
		//How many of the Find Usages tree's nodes can be seen (those in open nodes), as "Rows: <count>".
		printf("Rows: %d\n", (int)mpcContent->GetUsages()->GetUsagesTree()->NumRows());
	}
	else if (strcmp(szStep, "printpopup") == 0)
	{
		//Control+B's usages popup's rows, "Popup: <row> | <row> ...", or "Popup: none" if it isn't open.
		Ptr<CMenuContainer>	pPopup;
		size				i;

		pPopup = mpcContent->GetUsages()->GetUsagesPopup();
		if (pPopup.IsNull() || !pPopup->IsOpen())
		{
			printf("Popup: none\n");
		}
		else
		{
			printf("Popup:");
			for (i = 0; i < pPopup->NumItems(); i++)
			{
				printf("%s %s", (i > 0) ? " |" : "", pPopup->GetItem(i)->GetText());
			}
			printf("\n");
			if (pPopup->GetParent().IsNotNull())
			{
				//What holds the menu (a scroll pane once its height is limited), as the window lays it out, and the menu:
				//"Popup height: 300 scrolling, the menu 2394".
				mpcContent->GetWindow()->Layout();
				printf("Popup height: %d %s, the menu %d\n", pPopup->GetParent()->GetActualSize().y, (pPopup->GetParent()->GetActualSize().y < pPopup->GetActualSize().y) ? "scrolling" : "all shown", pPopup->GetActualSize().y);
			}
		}
	}
	else if (strcmp(szStep, "printhistory") == 0)
	{
		//Back and Forward's places: "History: back [Calculator.cpp:367, ...] forward [...]".
		CChars	szHistory;

		szHistory.Init();
		mpcContent->GetNavigationHistory()->Print(&szHistory);
		printf("History: %s\n", szHistory.Text());
		szHistory.Kill();
	}
	else if (strcmp(szStep, "printselection") == 0)
	{
		//The active editor's selected text, as "Selection: [<text>] on line <caret's line, from 1>", or
		//"Selection: none", and the line its caret line highlight is on.
		CChars	szSelected;
		size	uiStart;
		size	uiEnd;

		szSelected.Init();
		if (pTab.IsNotNull() && pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
		{
			szSelected.AppendSubString(pTab->GetEditor()->GetText(), uiStart, uiEnd);
			printf("Selection: [%s] on line %d, caret line %d\n", szSelected.Text(), (int)pTab->GetEditor()->FindLine(uiEnd) + 1, (int)pTab->GetEditor()->GetCaretLine() + 1);
		}
		else
		{
			printf("Selection: none\n");
		}
		szSelected.Kill();
	}
	else if (strncmp(szStep, "select:", 7) == 0)
	{
		//select:<node>, from 0 in the order "usages" prints them: that row of the Find Usages tree selected.
		mpcContent->GetUsages()->GetUsagesTree()->SelectNode(atoi(&szStep[7]));
	}
	else if (strncmp(szStep, "key:", 4) == 0)
	{
		//key:f4, key:space, key:enter, key:escape, key:documentend or key:documenthome: what the key gives (Jump to
		//Source for F4, Control+End and Control+Home in an editor) to the focussed component and its parents.
		if (strcmp(&szStep[4], "f4") == 0)
		{
			mpcContent->GetWindow()->InputCommand(CIC_JumpToSource, true);
		}
		else if (strcmp(&szStep[4], "space") == 0)
		{
			mpcContent->GetWindow()->InputCommand(CIC_Space, true);
		}
		else if (strcmp(&szStep[4], "enter") == 0)
		{
			mpcContent->GetWindow()->InputCommand(CIC_Enter, true);
		}
		else if (strcmp(&szStep[4], "escape") == 0)
		{
			mpcContent->GetWindow()->InputCommand(CIC_ClearSelection, true);
		}
		else if (strcmp(&szStep[4], "documentend") == 0)
		{
			mpcContent->GetWindow()->InputCommand(CIC_DocumentEnd, true);
		}
		else if (strcmp(&szStep[4], "documenthome") == 0)
		{
			mpcContent->GetWindow()->InputCommand(CIC_DocumentHome, true);
		}
	}
	else if (strcmp(szStep, "explorer") == 0)
	{
		mpcContent->GetSolutions()->ShowActiveFileInExplorer();
	}
	else if (strncmp(szStep, "caret:", 6) == 0)
	{
		//caret:<line>:<column>, from 1, as HCP65_IDE_CARET.
		if (strchr(&szStep[6], ':') != NULL)
		{
			mpcContent->GetDocuments()->SetCaretPlace(atoi(&szStep[6]) - 1, atoi(strchr(&szStep[6], ':') + 1) - 1);
		}
	}
	else if (strcmp(szStep, "reindexall") == 0)
	{
		//As saving a header does: everything indexed again, waited for, then the open files' edits.
		mpcContent->GetDocuments()->ReindexAll();
		mpcContent->GetCodeModel()->WaitForIndexing();
		mpcContent->GetDocuments()->UpdateEditedTabsNow();
	}
	else if (strcmp(szStep, "undoall") == 0)
	{
		mpcContent->GetEdit()->UndoAcrossFiles();
	}
	else if (strcmp(szStep, "redoall") == 0)
	{
		mpcContent->GetEdit()->RedoAcrossFiles();
	}
	else if (strcmp(szStep, "group") == 0)
	{
		mpcContent->GetEdit()->BeginUndoGroup();
	}
	else if (strcmp(szStep, "endgroup") == 0)
	{
		mpcContent->GetEdit()->EndUndoGroup();
	}
	else if (strcmp(szStep, "menu") == 0)
	{
		mpcContent->GetWindow()->Layout();
		mpcContent->GetEdit()->GetEditMenu()->Open(SInt32Vec2(40, 24), NULL, true);
	}
	else if (strcmp(szStep, "print") == 0)
	{
		pMenu = mpcContent->GetEdit()->GetEditMenu();
		printf("Edit menu:");
		for (i = 0; i < pMenu->NumItems(); i++)
		{
			if (pMenu->GetItem(i)->GetCommand().IsNotNull())
			{
				pMenu->GetItem(i)->SetEnabled(pMenu->GetItem(i)->GetCommand()->IsEnabled());
			}
			printf(" %s%s", pMenu->GetItem(i)->GetText(), pMenu->GetItem(i)->IsEnabled() ? "" : "(disabled)");
		}
		printf("\n");
		for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
		{
			pEditor = mpcContent->GetDocuments()->GetTab(i)->GetEditor();
			uiLine = pEditor->FindLine(pEditor->GetCaretPosition());
			uiEnd = pEditor->GetLineEnd(uiLine);
			szLine.Init();
			szLine.AppendSubString(pEditor->GetText(), pEditor->GetLineStart(uiLine), uiEnd);
			printf("%s%s%s line %d: [%s]\n", mpcContent->GetDocuments()->GetTab(i)->GetFileName(), mpcContent->GetDocuments()->GetTab(i)->IsModified() ? " *" : "", (mpcContent->GetWindow()->GetFocus()->GetFocussedComponent() == pEditor) ? " (focus)" : "", (int)uiLine + 1, szLine.Text());
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
void CIDEScript::RunDockMove(const char* szMove)
{
	Ptr<CDockManager>	pDock;
	Ptr<CDockPane>		pPane;
	const char*			szColon;
	CChars				szAction;

	pDock = mpcContent->GetDock();
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



//////////////////////////////////////////////////////////////////////////
//
// For checking, pDialog is snapshot (HCP65_IDE_DIALOG_SNAPSHOT) and
// answered (HCP65_IDE_DIALOG_ANSWER) once it has been drawn, see the
// class's comment.  Nothing is done if neither is set.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::WatchDialog(Ptr<CDialog> pDialog)
{
	if ((getenv("HCP65_IDE_DIALOG_SNAPSHOT") == NULL) && (getenv("HCP65_IDE_DIALOG_ANSWER") == NULL))
	{
		return;
	}
	mpDialog = pDialog;
	miDialogTicks = 0;
	mpDialog->SetTicker(this, &CIDEScript::DialogTick);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog shown has gone.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::StopWatchingDialog(void)
{
	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearTicker();
		mpDialog = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A few ticks in, so it has been drawn, the watched dialog is saved and
// answered: Save or OK click OK, any other caption closes it as that
// button does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::DialogTick(void)
{
	Ptr<CWindow>	pWindow;
	const char*		szAnswer;
	SMemberCall		sDrawn;

	pWindow = mpDialog->GetWindow();
	miDialogTicks++;
	if (miDialogTicks < 5)
	{
		pWindow->Invalidate();
		pWindow->RequestTick(0);
		return;
	}
	if (getenv("HCP65_IDE_DIALOG_SNAPSHOT") != NULL)
	{
		SavePNG(pWindow->GetCanvas()->GetImageOrNull(), getenv("HCP65_IDE_DIALOG_SNAPSHOT"));
	}
	if (msDialogDrawn.IsSet())
	{
		sDrawn = msDialogDrawn;
		msDialogDrawn.Clear();
		sDrawn.Call(this);
		return;
	}
	PressDialogKeys(pWindow);
	szAnswer = getenv("HCP65_IDE_DIALOG_ANSWER");
	if (szAnswer == NULL)
	{
		szAnswer = "Cancel";
	}
	printf("Dialog [%s] answered %s\n", pWindow->GetWindowTitle(), szAnswer);
	fflush(stdout);
	muiAnswered++;
	mpDialog->Close(((strcmp(szAnswer, "Save") == 0) || (strcmp(szAnswer, "OK") == 0)) ? "OK" : szAnswer);
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, HCP65_IDE_DIALOG_KEYS, tab and backtab separated by
// commas, are pressed before the dialog is answered, and what has the
// keys after each is printed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScript::PressDialogKeys(Ptr<CWindow> pWindow)
{
	CArrayChars		aszKeys;
	Ptr<CComponent>	pFocussed;
	size			i;

	if (getenv("HCP65_IDE_DIALOG_KEYS") == NULL)
	{
		return;
	}
	aszKeys.Init();
	aszKeys.Split(getenv("HCP65_IDE_DIALOG_KEYS"), ',');
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		pWindow->InputCommand(aszKeys.Get(i)->Equals("backtab") ? CIC_BackTab : CIC_Tab, true);
		pFocussed = pWindow->GetFocus()->GetFocussedComponent();
		printf("Focus: %s\n", pFocussed.IsNotNull() ? pFocussed.ClassName() : "(none)");
	}
	fflush(stdout);
	aszKeys.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// How many dialogs the script has answered since the IDE started.
//
//////////////////////////////////////////////////////////////////////////
size CIDEScript::NumAnsweredDialogs(void)
{
	return muiAnswered;
}

