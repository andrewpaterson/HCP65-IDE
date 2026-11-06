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
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDENavigator.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpGoTo = NULL;
	mszGoToPattern.Init();
	maszGoToFiles.Init();
	maiGoToProjects.Init();
	mcGoToFileSet.Init();
	miGoToFileIcon = -1;
	miQueuedGoTo = -1;
	mbGoToFilesFound = false;
	memset(maiGoToSymbolIcons, 0, sizeof(maiGoToSymbolIcons));
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpGoTo);
	U_String(mszGoToPattern);
	U_Unknown(CArrayChars, maszGoToFiles);
	U_Unknown(CArrayInt, maiGoToProjects);
	U_Unknown(CMapStringInt, mcGoToFileSet);
	U_Int32(miGoToFileIcon);
	U_Int32(miQueuedGoTo);
	U_Bool(mbGoToFilesFound);
	U_Int32Array(maiGoToSymbolIcons, IST_Count);
	M_Pointer(mpGoToClassItem);
	M_Pointer(mpGoToFileItem);
	M_Pointer(mpGoToSymbolItem);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Free(void)
{
	mcGoToFileSet.Kill();
	maiGoToProjects.Kill();
	maszGoToFiles.Kill();
	mszGoToPattern.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::Save(CObjectWriter* pcFile) { return false; }
bool CIDENavigator::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Every file of the open solutions' projects, once, with its project:
// what Go to File searches, and the files Go to Class and Go to Symbol
// show symbols from.  Found again only after the workspace has changed
// (ForgetGoToFiles).
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::FindGoToFiles(void)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CChars			szPath;

	if (mbGoToFilesFound)
	{
		return;
	}
	mbGoToFilesFound = true;
	maszGoToFiles.ReInit();
	maiGoToProjects.ReInit();
	mcGoToFileSet.Kill();
	mcGoToFileSet.Init();
	for (i = 0; i < mpcContent->mcWorkspace.NumProjects(); i++)
	{
		pcProject = mpcContent->mcWorkspace.GetSolutionProject(i)->GetProject();
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
// The solutions or their projects have changed: their files are found
// again when next needed.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::ForgetGoToFiles(void)
{
	mbGoToFilesFound = false;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Class (uiTab 0) and Go to Symbol (2) find the index's symbols
// declared in the solutions' files, Go to File (1) the files.  A file is
// shown by its name, with its folder, "~" for the home folder, after it.
// The content is its Go to dialog's searcher.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::GoToSearch(size uiTab, CNameMatcher* pcMatcher, CGoToItems* pcDest)
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
		mpcContent->mpDocuments->ReindexStaleTabs(true);
		mpcContent->mcCodeModel.FindGoToSymbols(pcMatcher, uiTab == IDE_GO_TO_CLASS, &mcGoToFileSet, maiGoToSymbolIcons, pcDest);
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
Ptr<CMenuContainer> CIDENavigator::CreateNavigateMenu(void)
{
	Ptr<CMenuContainer>	pMenu;

	pMenu = OMalloc<CMenuContainer>(mpcContent->mpWindow, mpcContent->mpTheme);
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
bool CIDENavigator::ShowGoTo(size uiTab)
{
	bool	bResult;

	if (!CreateGoTo(uiTab))
	{
		return false;
	}
	bResult = false;
	mpcContent->mpScript->WatchDialog(mpGoTo->GetDialog());
	if (mpGoTo->Show())
	{
		bResult = OpenGoToChoice();
	}
	mszGoToPattern.Set(mpGoTo->GetPattern());
	mpcContent->mpScript->StopWatchingDialog();
	mpGoTo = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to is shown at the next tick: ShowModal can't be called from the
// input (a key or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::QueueGoTo(size uiTab)
{
	miQueuedGoTo = (int32)uiTab;
	mpcContent->mpWindow->Post(this, &CIDENavigator::ShowQueuedGoTo);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::ShowQueuedGoTo(void)
{
	ShowGoTo((size)miQueuedGoTo);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog, started on uiTab, not shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::CreateGoTo(size uiTab)
{
	size	i;

	FindGoToFiles();
	mpGoTo = OMalloc<CGoToDialog>(mpcContent->mpWindow, &mpcContent->msDialogLook, "Go To", this, mpcContent->mpMatchFont);
	mpGoTo->AddTab("Classes");
	mpGoTo->AddTab("Files");
	mpGoTo->AddTab("Symbols");
	miGoToFileIcon = -1;
	for (i = 0; i < mpcContent->maFileIcons.Size(); i++)
	{
		if (i == 0)
		{
			miGoToFileIcon = mpGoTo->AddIcon(mpcContent->maFileIcons.Get(i));
		}
		else
		{
			mpGoTo->AddIcon(mpcContent->maFileIcons.Get(i));
		}
	}
	for (i = 0; i < IST_Count; i++)
	{
		maiGoToSymbolIcons[i] = mpGoTo->AddIcon(mpcContent->maSymbolIcons.Get(i));
	}
	mpGoTo->Start(uiTab, mszGoToPattern.Text());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Opens what the dialog chose.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::OpenGoToChoice(void)
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
		bResult = mpcContent->mpDocuments->OpenFile(maszGoToFiles.GetText(psItem->uiData), (size)maiGoToProjects.GetValue(psItem->uiData));
		if (bResult)
		{
			if (iLine > 0)
			{
				mpcContent->mpDocuments->SetCaretPlace(iLine - 1, 0);
			}
			mpcContent->mpDocuments->GetActiveTab()->GetEditor()->GetWindow()->GetFocus()->SetFocus(mpcContent->mpDocuments->GetActiveTab()->GetEditor());
		}
		return bResult;
	}

	if (!mpcContent->mcCodeModel.GetSymbolLocation(psItem->uiData, &sLocation))
	{
		return false;
	}
	if (iLine > 0)
	{
		sLocation.iLine = iLine - 1;
		sLocation.iColumn = 0;
	}
	bResult = mpcContent->mpDocuments->OpenLocation(&sLocation);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CGoToDialog> CIDENavigator::GetGoTo(void) { return mpGoTo; }



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
bool CIDENavigator::GetIncludeAtCaret(CChars* pszName, bool* pbSystem)
{
	CChars		szLine;
	char*		szText;
	char*		szEnd;
	char		cClose;
	bool		bResult;
	int			iLine;
	int			iColumn;

	if (!mpcContent->mpDocuments->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	szLine.Init();
	mpcContent->mpDocuments->GetFileLine(mpcContent->mpDocuments->GetActiveTab()->GetFileName(), iLine, &szLine);
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
bool CIDENavigator::FindIncludedFile(CChars* pszDest)
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
	bFound = mpcContent->mcCodeModel.FindIncludeFile(mpcContent->mpDocuments->GetActiveTab()->GetProject(), mpcContent->mpDocuments->GetActiveTab()->GetFileName(), szName.Text(), bSystem, pszDest);
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
bool CIDENavigator::GoToSource(void)
{
	CChars	szFile;
	bool	bResult;

	if (mpcContent->mpDocuments->GetActiveTab().IsNull())
	{
		return false;
	}
	szFile.Init();
	if (FindIncludedFile(&szFile))
	{
		bResult = mpcContent->mpDocuments->OpenFile(szFile.Text(), mpcContent->mcCodeModel.FindProjectOfFile(szFile.Text()) != MAX_SIZE ? mpcContent->mcCodeModel.FindProjectOfFile(szFile.Text()) : mpcContent->mpDocuments->GetActiveTab()->GetProject());
		if (bResult)
		{
			mpcContent->mpWindow->GetFocus()->SetFocus(mpcContent->mpDocuments->GetActiveTab()->GetEditor());
		}
		szFile.Kill();
		return bResult;
	}
	szFile.Kill();
	return mpcContent->GoToDefinition();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Go to Source has somewhere to go: an #include's file, or a
// symbol at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::CanGoToSource(void)
{
	CChars				szFile;
	CArrayIDELocations	asLocations;
	CChars				szKey;
	bool				bResult;

	if (mpcContent->mpDocuments->GetActiveTab().IsNull())
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
		bResult = mpcContent->mpDocuments->FindSymbolAtCaret(&asLocations, &szKey);
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
bool CIDENavigator::SwitchHeaderSource(void)
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

	if (mpcContent->mpDocuments->GetActiveTab().IsNull())
	{
		return false;
	}
	eType = CIDEIcons::GetFileType(mpcContent->mpDocuments->GetActiveTab()->GetFileName());
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
	szBase.Init(mpcContent->mpDocuments->GetActiveTab()->GetFileName());
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
		uiProject = mpcContent->mcCodeModel.FindProjectOfFile(szFound.Text());
		bFound = mpcContent->mpDocuments->OpenFile(szFound.Text(), (uiProject != MAX_SIZE) ? uiProject : mpcContent->mpDocuments->GetActiveTab()->GetProject());
		if (bFound)
		{
			mpcContent->mpWindow->GetFocus()->SetFocus(mpcContent->mpDocuments->GetActiveTab()->GetEditor());
		}
	}
	szFound.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions' files by path (FindGoToFiles), what Rename can edit.
//
//////////////////////////////////////////////////////////////////////////
CMapStringInt* CIDENavigator::GetSolutionFileSet(void)
{
	return &mcGoToFileSet;
}


//////////////////////////////////////////////////////////////////////////
//
// Navigate's items.  False if pcItem isn't one of them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::MenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem == &mpGoToClassItem)
	{
		QueueGoTo(IDE_GO_TO_CLASS);
	}
	else if (pcItem == &mpGoToFileItem)
	{
		QueueGoTo(IDE_GO_TO_FILE);
	}
	else if (pcItem == &mpGoToSymbolItem)
	{
		QueueGoTo(IDE_GO_TO_SYMBOL);
	}
	else
	{
		return false;
	}
	return true;
}
