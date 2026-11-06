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


//////////////////////////////////////////////////////////////////////////
//
// Whether szPath ends with szEnd.
//
//////////////////////////////////////////////////////////////////////////
static bool EndsWithIDEPath(const char* szPath, const char* szEnd)
{
	size	uiPath;
	size	uiEnd;

	uiPath = strlen(szPath);
	uiEnd = strlen(szEnd);
	return (uiPath >= uiEnd) && (strcmp(&szPath[uiPath - uiEnd], szEnd) == 0);
}

//////////////////////////////////////////////////////////////////////////
//
// The file an #include on the active editor's caret line names, and
// whether it's a <system> one.  False if the line isn't an #include.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GetIncludeAtCaret(CChars* pszName, bool* pbSystem)
{
	CChars		szLine;
	char*		szText;
	char*		szEnd;
	char		cClose;
	bool		bResult;
	int			iLine;
	int			iColumn;

	if (!GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	szLine.Init();
	GetFileLine(mpActive->GetFileName(), iLine, &szLine);
	szText = szLine.Text();
	bResult = false;
	if (szText[0] == '#')
	{
		szText++;
		while ((*szText == ' ') || (*szText == '\t'))
		{
			szText++;
		}
		if (strncmp(szText, "include", 7) == 0)
		{
			szText += 7;
			while ((*szText == ' ') || (*szText == '\t'))
			{
				szText++;
			}
			if ((*szText == '"') || (*szText == '<'))
			{
				cClose = (*szText == '"') ? '"' : '>';
				*pbSystem = (cClose == '>');
				szEnd = strchr(szText + 1, cClose);
				if (szEnd && (szEnd > szText + 1))
				{
					pszName->Init();
					pszName->AppendSubString(szText + 1, szEnd);
					bResult = true;
				}
			}
		}
	}
	szLine.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file the active editor's #include names, found as the compiler
// would (see CIDECodeModel::FindIncludeFile), or else a solution file
// with that path.  False if the caret line isn't an #include or the file
// isn't found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::FindIncludedFile(CChars* pszDest)
{
	CChars	szName;
	CChars	szEnd;
	bool	bSystem;
	bool	bFound;
	size	i;

	if (!GetIncludeAtCaret(&szName, &bSystem))
	{
		return false;
	}
	bFound = mcCodeModel.FindIncludeFile(mpActive->GetProject(), mpActive->GetFileName(), szName.Text(), bSystem, pszDest);
	if (!bFound)
	{
		//A solution file whose path ends with it.
		szEnd.Init("/");
		szEnd.Append(szName);
		szEnd.Replace('\\', '/');
		FindGoToFiles();
		for (i = 0; (i < maszGoToFiles.NumElements()) && !bFound; i++)
		{
			if (EndsWithIDEPath(maszGoToFiles.GetText(i), szEnd.Text()))
			{
				pszDest->Set(maszGoToFiles.GetText(i));
				bFound = true;
			}
		}
		szEnd.Kill();
	}
	szName.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Control+B, as IntelliJ's: on an #include the file it names is opened;
// anywhere else, the definition of what is at the caret (as F12).  False
// if there's nothing to go to.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GoToSource(void)
{
	CChars	szFile;
	bool	bResult;

	if (mpActive.IsNull())
	{
		return false;
	}
	szFile.Init();
	if (FindIncludedFile(&szFile))
	{
		bResult = OpenFile(szFile.Text(), mcCodeModel.FindProjectOfFile(szFile.Text()) != MAX_SIZE ? mcCodeModel.FindProjectOfFile(szFile.Text()) : mpActive->GetProject());
		if (bResult)
		{
			mpWindow->GetFocus()->SetFocus(mpActive->GetEditor());
		}
		szFile.Kill();
		return bResult;
	}
	szFile.Kill();
	return GoToDefinition();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Go to Source has somewhere to go: an #include's file, or a
// symbol at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::CanGoToSource(void)
{
	CChars				szFile;
	CArrayIDELocations	asLocations;
	CChars				szKey;
	bool				bResult;

	if (mpActive.IsNull())
	{
		return false;
	}
	szFile.Init();
	bResult = FindIncludedFile(&szFile);
	szFile.Kill();
	if (!bResult)
	{
		asLocations.Init();
		szKey.Init();
		bResult = FindSymbolAtCaret(&asLocations, &szKey);
		szKey.Kill();
		KillIDELocations(&asLocations);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+O, as Visual Assist's: a .cpp (.c, .cc, .cxx) opens its header
// (.h, .hpp, .hxx, .inl) of the same name, and a header its .cpp: the one
// in the same folder, else a solution file of that name.  False if there
// isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SwitchHeaderSource(void)
{
	static const char*	aszSources[] = { "cpp", "c", "cc", "cxx" };
	static const char*	aszHeaders[] = { "h", "hpp", "hxx", "inl" };
	const char**		aszWanted;
	size				uiWanted;
	CFileUtil			cFileUtil;
	CChars				szBase;
	CChars				szPath;
	CChars				szName;
	CChars				szFound;
	EIDEFileType		eType;
	size				i;
	size				j;
	bool				bFound;
	size				uiProject;

	if (mpActive.IsNull())
	{
		return false;
	}
	eType = CIDEIcons::GetFileType(mpActive->GetFileName());
	if (eType == IFT_Header)
	{
		aszWanted = aszSources;
		uiWanted = sizeof(aszSources) / sizeof(char*);
	}
	else if ((eType == IFT_CPP) || (eType == IFT_C))
	{
		aszWanted = aszHeaders;
		uiWanted = sizeof(aszHeaders) / sizeof(char*);
	}
	else
	{
		return false;
	}

	//The same name, beside it.
	szBase.Init(mpActive->GetFileName());
	cFileUtil.RemoveExtension(&szBase);
	szFound.Init();
	bFound = false;
	for (i = 0; (i < uiWanted) && !bFound; i++)
	{
		szPath.Init(szBase);
		szPath.Append('.');
		szPath.Append(aszWanted[i]);
		if (cFileUtil.Exists(szPath.Text()))
		{
			szFound.Set(szPath);
			bFound = true;
		}
		szPath.Kill();
	}

	//Else a solution file of that name.
	if (!bFound)
	{
		szName.Init();
		cFileUtil.GetFileName(&szName, szBase.Text());
		FindGoToFiles();
		for (j = 0; (j < maszGoToFiles.NumElements()) && !bFound; j++)
		{
			for (i = 0; (i < uiWanted) && !bFound; i++)
			{
				szPath.Init("/");
				szPath.Append(szName);
				szPath.Append('.');
				szPath.Append(aszWanted[i]);
				if (EndsWithIDEPath(maszGoToFiles.GetText(j), szPath.Text()))
				{
					szFound.Set(maszGoToFiles.GetText(j));
					bFound = true;
				}
				szPath.Kill();
			}
		}
		szName.Kill();
	}
	szBase.Kill();

	if (bFound)
	{
		uiProject = mcCodeModel.FindProjectOfFile(szFound.Text());
		bFound = OpenFile(szFound.Text(), (uiProject != MAX_SIZE) ? uiProject : mpActive->GetProject());
		if (bFound)
		{
			mpWindow->GetFocus()->SetFocus(mpActive->GetEditor());
		}
	}
	szFound.Kill();
	return bFound;
}
