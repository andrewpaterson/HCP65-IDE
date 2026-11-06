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
#include "BaseLib/FileUtil.h"
#include "SupportLib/PNGWriter.h"
#include "IDEContent.h"


//////////////////////////////////////////////////////////////////////////
//
// Every file of the open solutions' projects, once, with its project:
// what Go to File searches, and the files Go to Class and Go to Symbol
// show symbols from.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::FindGoToFiles(void)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CChars			szPath;

	maszGoToFiles.ReInit();
	maiGoToProjects.ReInit();
	mcGoToFileSet.Kill();
	mcGoToFileSet.Init();
	for (i = 0; i < mcWorkspace.NumProjects(); i++)
	{
		pcProject = mcWorkspace.GetSolutionProject(i)->GetProject();
		if (pcProject == NULL)
		{
			continue;
		}
		for (j = 0; j < pcProject->NumItems(); j++)
		{
			szPath.Init();
			pcProject->GetFullPath(pcProject->GetItem(j), &szPath);
			if (mcGoToFileSet.Get(szPath.Text(), -1) < 0)
			{
				mcGoToFileSet.Put(szPath.Text(), (int)maszGoToFiles.NumElements());
				maszGoToFiles.Add(szPath.Text());
				maiGoToProjects.Add((int)i);
			}
			szPath.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Class (uiTab 0) and Go to Symbol (2) find the index's symbols
// declared in the solutions' files, Go to File (1) the files.  A file is
// shown by its name, with its folder, "~" for the home folder, after it.
// The content is its Go to dialog's searcher.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::GoToSearch(size uiTab, CNameMatcher* pcMatcher, CGoToItems* pcDest)
{
	size		i;
	SNameMatch	sMatch;
	char*		szPath;
	CChars		szFolder;
	CChars		szName;
	CFileUtil	cFileUtil;
	const char*	szHome;
	size		uiHome;

	if (uiTab != IDE_GO_TO_FILE)
	{
		mcCodeModel.FindGoToSymbols(pcMatcher, uiTab == IDE_GO_TO_CLASS, &mcGoToFileSet, maiGoToSymbolIcons, pcDest);
		return;
	}

	szHome = getenv("HOME");
	uiHome = szHome ? strlen(szHome) : 0;
	for (i = 0; i < maszGoToFiles.NumElements(); i++)
	{
		szPath = maszGoToFiles.GetText(i);
		if (pcMatcher->MatchPath(szPath, &sMatch))
		{
			szName.Init();
			szFolder.Init();
			cFileUtil.SplitPath(szPath, &szName, &szFolder);
			if ((uiHome > 0) && (strncmp(szFolder.Text(), szHome, uiHome) == 0))
			{
				szFolder.Replace(szHome, "~");
			}
			pcDest->Add(szName.Text(), szFolder.Text(), miGoToFileIcon + (int32)CIDEIcons::GetFileType(szPath), i, &sMatch);
			szFolder.Kill();
			szName.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Navigate, as IntelliJ's: Class, File and Symbol, each the Go to dialog
// on its tab.  The keys are the content's own: the shortcuts are only
// shown.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEContent::CreateNavigateMenu(void)
{
	Ptr<CMenuContainer>	pMenu;

	pMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpGoToClassItem = pMenu->AddItem("Class...");
	mpGoToClassItem->SetShortcut("Ctrl+N");
	mpGoToFileItem = pMenu->AddItem("File...");
	mpGoToFileItem->SetShortcut("Ctrl+Shift+N");
	mpGoToSymbolItem = pMenu->AddItem("Symbol...");
	mpGoToSymbolItem->SetShortcut("Ctrl+Alt+Shift+N");
	return pMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Navigate, Class (Control+N), File (Control+Shift+N) and
// Symbol (Control+Alt+Shift+N): the Go to dialog on that tab with what
// was last typed in it.  The file, or the symbol's definition (or its
// declaration), is opened, at the line typed after a colon if one was.
// False if nothing was chosen.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowGoTo(size uiTab)
{
	bool	bResult;

	if (!CreateGoTo(uiTab))
	{
		return false;
	}
	bResult = false;
	miGoToTicks = 0;
	if (getenv("HCP65_IDE_GOTO_SNAPSHOT") != NULL)
	{
		mpGoTo->GetDialog()->SetTicker(this, &CIDEContent::GoToSnapshotTick);
	}
	if (mpGoTo->Show())
	{
		bResult = OpenGoToChoice();
	}
	mszGoToPattern.Set(mpGoTo->GetPattern());
	mpGoTo->GetDialog()->ClearTicker();
	mpGoTo = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to is shown at the next tick: ShowModal can't be called from the
// input (a key or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::QueueGoTo(size uiTab)
{
	miQueuedGoTo = (int32)uiTab;
	mpWindow->RequestTick(0);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog, started on uiTab, not shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::CreateGoTo(size uiTab)
{
	size	i;

	FindGoToFiles();
	mpGoTo = OMalloc<CGoToDialog>(mpWindow, &msDialogLook, "Go To", this, mpMatchFont);
	mpGoTo->AddTab("Classes");
	mpGoTo->AddTab("Files");
	mpGoTo->AddTab("Symbols");
	miGoToFileIcon = -1;
	for (i = 0; i < maFileIcons.Size(); i++)
	{
		if (i == 0)
		{
			miGoToFileIcon = mpGoTo->AddIcon(maFileIcons.Get(i));
		}
		else
		{
			mpGoTo->AddIcon(maFileIcons.Get(i));
		}
	}
	for (i = 0; i < IST_Count; i++)
	{
		maiGoToSymbolIcons[i] = mpGoTo->AddIcon(maSymbolIcons.Get(i));
	}
	mpGoTo->Start(uiTab, mszGoToPattern.Text());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Opens what the dialog chose.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenGoToChoice(void)
{
	SGoToItem*		psItem;
	SIDELocation	sLocation;
	bool			bResult;
	int32			iLine;

	psItem = mpGoTo->GetChosen();
	if (psItem == NULL)
	{
		return false;
	}
	iLine = mpGoTo->GetLine();
	if (mpGoTo->GetTab() == IDE_GO_TO_FILE)
	{
		if (psItem->uiData >= maszGoToFiles.NumElements())
		{
			return false;
		}
		bResult = OpenFile(maszGoToFiles.GetText(psItem->uiData), (size)maiGoToProjects.GetValue(psItem->uiData));
		if (bResult)
		{
			if (iLine > 0)
			{
				SetCaretPlace(iLine - 1, 0);
			}
			mpActive->GetEditor()->GetWindow()->GetFocus()->SetFocus(mpActive->GetEditor());
		}
		return bResult;
	}

	if (!mcCodeModel.GetSymbolLocation(psItem->uiData, &sLocation))
	{
		return false;
	}
	if (iLine > 0)
	{
		sLocation.iLine = iLine - 1;
		sLocation.iColumn = 0;
	}
	bResult = OpenLocation(&sLocation);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CGoToDialog> CIDEContent::GetGoTo(void) { return mpGoTo; }



//////////////////////////////////////////////////////////////////////////
//
// For checking: the shown dialog is saved to HCP65_IDE_GOTO_SNAPSHOT's
// PNG once it has been drawn, then closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::GoToSnapshotTick(void)
{
	Ptr<CWindow>	pWindow;

	pWindow = mpGoTo->GetDialog()->GetWindow();
	miGoToTicks++;
	if (miGoToTicks < 5)
	{
		pWindow->Invalidate();
		pWindow->RequestTick(0);
		return;
	}
	SavePNG(pWindow->GetCanvas()->GetImageOrNull(), getenv("HCP65_IDE_GOTO_SNAPSHOT"));
	mpGoTo->GetDialog()->Close("Cancel");
}
