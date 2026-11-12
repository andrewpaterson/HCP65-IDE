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
#include "BaseLib/DebugOutput.h"
#include "StandardLib/ClassDefines.h"
#include "WindowLib/ScrollPane.h"
#include "IDEContent.h"
#include "IDEFindInFiles.h"


#define IDE_FIND_STEP_MILLISECONDS		8		//How long a step of the dialog's search goes on for.
#define IDE_FIND_LINE_LENGTH			200		//The most of a line a result shows.
#define IDE_FIND_LINE_BEFORE			60		//Of a long line, how much is shown before the text found.
#define IDE_FIND_FILE_MAXIMUM			(8 MB)	//Bigger files aren't searched.
#define IDE_FIND_BINARY_CHECK			4096	//A file with a zero in its first bytes isn't text, and isn't searched.
#define IDE_FIND_RESULTS_MAXIMUM		10000	//The most results the Find tool window lists.
#define IDE_FIND_OPEN_FILES_MAXIMUM		1000	//With more results than this the tree's files start closed.
#define IDE_FIND_SETTINGS_TAG			"FindInFiles"
#define IDE_FIND_REFRESH				500		//Milliseconds after the last edit that moved a result until the tree shows it.


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpDialog = NULL;
	mszText.Init();
	mszDirectory.Init();
	muiScope = IDE_FIND_IN_SOLUTION;
	mbMatchCase = false;
	mbWholeWords = false;
	mbSearch = false;
	muiProject = MAX_SIZE;
	maszFiles.Init();
	maszDirectories.Init();
	muiNextFile = 0;
	masPlaces.Init();
	mpPane = NULL;
	mpTool = NULL;
	maszFoundFiles.Init();
	masFound.Init();
	mszFoundTitle.Init();
	mbFound = false;
	mbMoved = false;
	miMoved = 0;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_String(mszText);
	U_String(mszDirectory);
	U_Size(muiScope);
	U_Bool(mbMatchCase);
	U_Bool(mbWholeWords);
	U_Unknown(CTextSearch, mcSearch);
	U_Bool(mbSearch);
	U_Size(muiProject);
	U_Unknown(CArrayChars, maszFiles);
	U_Unknown(CArrayChars, maszDirectories);
	U_Size(muiNextFile);
	U_Unknown(CArrayIDEFindPlaces, masPlaces);
	M_Pointer(mpPane);
	M_Pointer(mpTool);
	U_Unknown(CArrayChars, maszFoundFiles);
	U_Unknown(CArrayIDEFindPlaces, masFound);
	U_String(mszFoundTitle);
	U_Bool(mbFound);
	U_Bool(mbMoved);
	U_Int64(miMoved);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::Free(void)
{
	if (mbSearch)
	{
		mcSearch.Kill();
	}
	mszFoundTitle.Kill();
	masFound.Kill();
	maszFoundFiles.Kill();
	masPlaces.Kill();
	maszDirectories.Kill();
	maszFiles.Kill();
	mszDirectory.Kill();
	mszText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::Save(CObjectWriter* pcFile) { return false; }
bool CIDEFindInFiles::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Find in Files (Control+Shift+F, as IntelliJ IDEA's).
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Edit.FindInFiles", "Find in Files...", "Control+Shift+F", Ptr<CImageCel>(), this, &CIDEFindInFiles::FindInFiles);
}


//////////////////////////////////////////////////////////////////////////
//
// The Edit menu's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::AddEditItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.FindInFiles"));
}


//////////////////////////////////////////////////////////////////////////
//
// The Find tool window: the tree of what Open in Find Window listed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::CreatePane(SDockPlace* psPlace)
{
	mpPane = OMalloc<CIDEResultsPane>(mpcContent, this, "matches", true);
	mpTool = mpcContent->GetDock()->AddPane(IDE_FIND_ID, "Find", NULL, mpPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog is shown at the next tick: ShowModal can't be called from
// the input (a key or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::FindInFiles(void)
{
	if (mpDialog.IsNull())
	{
		mpcContent->GetWindow()->Post(this, &CIDEFindInFiles::ShowQueuedFind);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::ShowQueuedFind(void)
{
	ShowFind();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog shown, and what it was closed for done: the result chosen
// is opened, or every result is listed in the Find tool window.  What was
// typed is kept.  False if nothing came of it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::ShowFind(void)
{
	EFindInFilesResult	eResult;
	bool				bResult;

	if (!CreateFind())
	{
		return false;
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	if (!mpcContent->GetScript()->IsWatchingDialog())
	{
		mpDialog->GetDialog()->SetTicker(this, &CIDEFindInFiles::DialogTick);  //A watched dialog's ticks are the script's, which calls DialogTick.
	}
	eResult = mpDialog->Show();
	mpDialog->GetDialog()->ClearTicker();
	mpcContent->GetDocuments()->ForgetPreviewAsker(this);
	mpcContent->GetScript()->StopWatchingDialog();

	mszText.Set(mpDialog->GetText());
	mszDirectory.Set(mpDialog->GetDirectory());
	muiScope = mpDialog->GetScope();
	mbMatchCase = mpDialog->IsMatchCase();
	mbWholeWords = mpDialog->IsWholeWords();

	bResult = false;
	if (eResult == FIFR_Chosen)
	{
		bResult = OpenFindChoice();
	}
	mpDialog = NULL;
	if (eResult == FIFR_FindWindow)
	{
		bResult = OpenInFindWindow() > 0;
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog, started on the scope and text of the last time (or the
// text selected in the editor), not shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::CreateFind(void)
{
	Ptr<CIDEEditorTab>	pTab;
	CChars				szText;
	CChars				szDirectory;
	CChars				szProject;

	//The project of the active file, for In Project.
	muiProject = MAX_SIZE;
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull())
	{
		muiProject = mpcContent->GetCodeModel()->FindProjectOfFile(pTab->GetFileName());
		if ((muiProject == MAX_SIZE) && (pTab->GetProject() < mpcContent->GetWorkspace()->NumProjects()))
		{
			muiProject = pTab->GetProject();
		}
	}

	szText.Init();
	szDirectory.Init();
	szProject.Init();
	GetInitialText(&szText);
	GetInitialDirectory(&szDirectory);
	GetProjectDescription(&szProject);

	mpDialog = OMalloc<CFindInFilesDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Find in Files", this, mpcContent->GetMatchFont(), mpcContent->GetCodeFamily());
	mpDialog->AddScope("In Solution", "Every project's files");
	mpDialog->AddScope("In Project", szProject.Text());
	mpDialog->AddDirectoryScope("Directory", szDirectory.Text());
	mpDialog->SetOptions(mbMatchCase, mbWholeWords);
	mpDialog->Start(muiScope, szText.Text());

	szProject.Kill();
	szDirectory.Kill();
	szText.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What the dialog starts with: the text selected in the active editor,
// if it is on one line, else what was typed the last time.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::GetInitialText(CChars* pszDest)
{
	Ptr<CTextArea>	pEditor;
	size			uiStart;
	size			uiEnd;

	pEditor = mpcContent->GetDocuments()->GetEditor();
	if (pEditor.IsNotNull() && pEditor->GetSelection(&uiStart, &uiEnd) && (uiEnd > uiStart) && (pEditor->FindLine(uiStart) == pEditor->FindLine(uiEnd)))
	{
		pszDest->AppendSubString(pEditor->GetText(), uiStart, uiEnd);
		return;
	}
	pszDest->Append(mszText);
}


//////////////////////////////////////////////////////////////////////////
//
// The directory the dialog starts with: the one typed the last time, or
// the active file's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::GetInitialDirectory(CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	CFileUtil			cFileUtil;

	if (!mszDirectory.Empty())
	{
		pszDest->Append(mszDirectory);
		return;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull())
	{
		pszDest->Append(pTab->GetFileName());
		cFileUtil.RemoveLastFromPath(pszDest);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What In Project searches, for its page.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::GetProjectDescription(CChars* pszDest)
{
	CVcxProject*	pcProject;

	pcProject = (muiProject != MAX_SIZE) ? mpcContent->GetWorkspace()->GetSolutionProject(muiProject)->GetProject() : NULL;
	if (pcProject)
	{
		pszDest->Append("The files of ");
		pszDest->Append(pcProject->GetName());
	}
	else
	{
		pszDest->Append("The active file isn't in a project");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The place of a result, as a location to open: its file name is made,
// for the caller to kill, if it answers true.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::GetPlaceLocation(CArrayChars* paszFiles, SIDEFindPlace* psPlace, SIDELocation* psDest)
{
	if (psPlace->uiFile >= paszFiles->NumElements())
	{
		return false;
	}
	memset(psDest, 0, sizeof(SIDELocation));
	psDest->szFileName.Init(paszFiles->GetText(psPlace->uiFile));
	psDest->iLine = psPlace->iLine;
	psDest->iColumn = psPlace->iColumn;
	psDest->iLength = psPlace->iLength;
	psDest->iContainer = -1;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's source: the place of the result a line's node is of
// (its index in what was found, and one).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest)
{
	if ((iData <= 0) || ((size)iData > masFound.NumElements()))
	{
		return false;
	}
	return GetPlaceLocation(&maszFoundFiles, masFound.Get((size)iData - 1), psDest);
}


//////////////////////////////////////////////////////////////////////////
//
// Opens what the dialog chose, the text found selected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::OpenFindChoice(void)
{
	SFindInFilesItem*	psItem;
	SIDELocation		sLocation;
	bool				bResult;

	psItem = mpDialog->GetChosen();
	if ((psItem == NULL) || (psItem->uiData >= masPlaces.NumElements()))
	{
		return false;
	}
	if (!GetPlaceLocation(&maszFiles, masPlaces.Get(psItem->uiData), &sLocation))
	{
		return false;
	}
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, true);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Open in Find Window: everything what was last typed finds (up to
// IDE_FIND_RESULTS_MAXIMUM) is listed in the Find tool window, which is
// shown with the keys.  How many were found.
//
//////////////////////////////////////////////////////////////////////////
size CIDEFindInFiles::OpenInFindWindow(void)
{
	SIDEFindPlace*	psPlace;
	SIDEFindPlace*	psFound;
	size			uiFile;
	size			uiFiles;
	size			i;
	bool			bMore;

	if (mszText.Empty())
	{
		return 0;
	}
	StartSearch(muiScope, mszText.Text(), mszDirectory.Text(), mbMatchCase, mbWholeWords);
	bMore = true;
	while (bMore && (masPlaces.NumElements() < IDE_FIND_RESULTS_MAXIMUM))
	{
		bMore = SearchSome(NULL);
	}

	//The tree's files are those something was found in.
	maszFoundFiles.ReInit();
	masFound.ReInit();
	uiFile = MAX_SIZE;
	uiFiles = 0;
	for (i = 0; (i < masPlaces.NumElements()) && (i < IDE_FIND_RESULTS_MAXIMUM); i++)
	{
		psPlace = masPlaces.Get(i);
		if (psPlace->uiFile != uiFile)
		{
			uiFile = psPlace->uiFile;
			maszFoundFiles.Add(maszFiles.GetText(uiFile));
			uiFiles++;
		}
		psFound = masFound.Add();
		memcpy(psFound, psPlace, sizeof(SIDEFindPlace));
		psFound->uiFile = uiFiles - 1;
	}

	mszFoundTitle.Set("Occurrences of '");
	mszFoundTitle.Append(mszText);
	mszFoundTitle.Append("' ");
	AppendScopeName(&mszFoundTitle);
	mszFoundTitle.Append("  ");
	mszFoundTitle.Append((uint32)masFound.NumElements());
	mszFoundTitle.Append((masFound.NumElements() == 1) ? " match in " : " matches in ");
	mszFoundTitle.Append((uint32)uiFiles);
	mszFoundTitle.Append((uiFiles == 1) ? " file" : " files");
	if (bMore || (masPlaces.NumElements() > IDE_FIND_RESULTS_MAXIMUM))
	{
		mszFoundTitle.Append(", only the first are listed");
	}

	mbFound = true;
	mbMoved = false;
	ShowResults(false);
	if (mpTool.IsNotNull())
	{
		mpcContent->GetDock()->Show(mpTool);
		mpPane->Focus();
	}
	return masFound.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// "in the solution", "in BaseLib" or "in <directory>".
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::AppendScopeName(CChars* pszDest)
{
	CVcxProject*	pcProject;

	if (muiScope == IDE_FIND_IN_PROJECT)
	{
		pcProject = (muiProject != MAX_SIZE) ? mpcContent->GetWorkspace()->GetSolutionProject(muiProject)->GetProject() : NULL;
		pszDest->Append("in ");
		pszDest->Append(pcProject ? pcProject->GetName() : "no project");
	}
	else if (muiScope == IDE_FIND_IN_DIRECTORY)
	{
		pszDest->Append("in ");
		pszDest->Append(mszDirectory);
	}
	else
	{
		pszDest->Append("in the solution");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's searcher: a new search.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::FindStart(SFindInFilesQuery* psQuery)
{
	StartSearch(psQuery->uiScope, psQuery->szText, psQuery->szDirectory, psQuery->bMatchCase, psQuery->bWholeWords);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's searcher: files are searched until the time is up, or
// there are more results than the dialog lists.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::FindStep(CFindInFilesItems* pcDest)
{
	int64	iStart;
	bool	bMore;

	iStart = GetDebugTickCount();
	bMore = true;
	while (bMore && (pcDest->NumItems() <= FIND_IN_FILES_DIALOG_ROWS) && (GetDebugTickCount() - iStart < IDE_FIND_STEP_MILLISECONDS))
	{
		bMore = SearchSome(pcDest);
	}
	return bMore;
}


//////////////////////////////////////////////////////////////////////////
//
// The scope's files, or for a directory the directory to list, and
// nothing found yet.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::StartSearch(size uiScope, const char* szText, const char* szDirectory, bool bMatchCase, bool bWholeWords)
{
	CIDECodeModel*	pcCodeModel;
	CFileUtil		cFileUtil;
	CChars			szPath;
	size			i;

	if (mbSearch)
	{
		mcSearch.Kill();
	}
	mcSearch.Init(szText, bMatchCase, bWholeWords);
	mbSearch = true;
	maszFiles.ReInit();
	maszDirectories.ReInit();
	masPlaces.ReInit();
	muiNextFile = 0;
	if (mcSearch.IsEmpty())
	{
		return;
	}

	pcCodeModel = mpcContent->GetCodeModel();
	if (uiScope == IDE_FIND_IN_DIRECTORY)
	{
		szPath.Init(szDirectory);
		szPath.StripWhitespace();
		if (!szPath.Empty() && cFileUtil.IsDirectory(szPath.Text()))
		{
			maszDirectories.Add(szPath.Text());
		}
		szPath.Kill();
	}
	else
	{
		for (i = 0; i < pcCodeModel->NumProjectFiles(); i++)
		{
			if ((uiScope == IDE_FIND_IN_SOLUTION) || ((muiProject != MAX_SIZE) && (pcCodeModel->FindProjectOfFile(pcCodeModel->GetProjectFile(i)) == muiProject)))
			{
				maszFiles.Add(pcCodeModel->GetProjectFile(i));
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A little of the search: the next file is searched or, with none left,
// the next directory's files and directories are listed.  False once
// there is nothing left to search.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::SearchSome(CFindInFilesItems* pcDest)
{
	CChars	szDirectory;

	if (muiNextFile < maszFiles.NumElements())
	{
		SearchFile(muiNextFile, pcDest);
		muiNextFile++;
	}
	else if (maszDirectories.NumElements() > 0)
	{
		szDirectory.Init(maszDirectories.Tail()->Text());
		maszDirectories.RemoveTail();
		ListDirectory(szDirectory.Text());
		szDirectory.Kill();
	}
	return (muiNextFile < maszFiles.NumElements()) || (maszDirectories.NumElements() > 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The directory's files are added to those to search, and its
// directories to those to list, the first of them next.  Hidden ones
// (.git) aren't.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::ListDirectory(const char* szDirectory)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszDirectories;
	size		i;

	cFileUtil.FindAllFiles(szDirectory, &maszFiles, false, false);
	aszDirectories.Init();
	cFileUtil.FindAllDirectories(szDirectory, &aszDirectories, false);
	for (i = aszDirectories.NumElements(); i > 0; i--)
	{
		maszDirectories.Add(aszDirectories.GetText(i - 1));
	}
	aszDirectories.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file's text: as its editor has it if it is open (pcFile isn't used),
// else read into pcFile.  False if it can't be read, is too big or isn't
// text.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::GetFileText(const char* szFileName, CTextFile* pcFile, char** pszText, size* puiLength)
{
	Ptr<CIDEEditorTab>	pTab;
	CFileUtil			cFileUtil;
	size				uiCheck;

	pTab = mpcContent->GetDocuments()->FindTab(szFileName);
	if (pTab.IsNotNull())
	{
		*pszText = pTab->GetEditor()->GetText();
		*puiLength = strlen(*pszText);
		return true;
	}
	if ((cFileUtil.Size(szFileName) > IDE_FIND_FILE_MAXIMUM) || !pcFile->Read(szFileName))
	{
		return false;
	}
	*pszText = pcFile->Text();
	*puiLength = (size)pcFile->Length();
	uiCheck = (*puiLength < IDE_FIND_BINARY_CHECK) ? *puiLength : IDE_FIND_BINARY_CHECK;
	return memchr(*pszText, '\0', uiCheck) == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place the text is in the file, added to the places found and,
// for the dialog, to pcDest (NULL for none) with its line.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::SearchFile(size uiFile, CFindInFilesItems* pcDest)
{
	CTextFile		cFile;
	SIDEFindPlace*	psPlace;
	char*			szText;
	const char*		szNewLine;
	const char*		szLineEnd;
	size			uiLength;
	size			uiPosition;
	size			uiLineStart;
	int32			iLine;

	cFile.Init();
	if (!GetFileText(maszFiles.GetText(uiFile), &cFile, &szText, &uiLength))
	{
		cFile.Kill();
		return;
	}

	uiLineStart = 0;
	iLine = 0;
	uiPosition = mcSearch.Find(szText, uiLength, 0);
	while (uiPosition != TEXT_SEARCH_NOT_FOUND)
	{
		//The line it is on: the lines between the last one found and it are counted.
		for (;;)
		{
			szNewLine = (const char*)memchr(&szText[uiLineStart], '\n', uiPosition - uiLineStart);
			if (szNewLine == NULL)
			{
				break;
			}
			uiLineStart = (size)(szNewLine - szText) + 1;
			iLine++;
		}

		psPlace = masPlaces.Add();
		psPlace->uiFile = uiFile;
		psPlace->uiPosition = uiPosition;
		psPlace->iLine = iLine;
		psPlace->iColumn = (int32)(uiPosition - uiLineStart);
		psPlace->iLength = (int32)mcSearch.Length();
		if (pcDest)
		{
			szLineEnd = (const char*)memchr(&szText[uiPosition], '\n', uiLength - uiPosition);
			AddItem(pcDest, uiFile, psPlace, &szText[uiLineStart], szLineEnd ? szLineEnd : &szText[uiLength], masPlaces.NumElements() - 1);
		}
		uiPosition = mcSearch.Find(szText, uiLength, uiPosition + mcSearch.Length());
	}
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A result for the dialog: its line without the white space at its
// ends, its tabs as spaces, and a long line only around the text found;
// and its file's name and line.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::AddItem(CFindInFilesItems* pcDest, size uiFile, SIDEFindPlace* psPlace, const char* szLine, const char* szLineEnd, size uiPlace)
{
	CChars		szShown;
	CChars		szLocation;
	const char*	szMatch;
	const char*	szStart;
	size		uiMatchStart;
	size		i;

	szMatch = szLine + psPlace->iColumn;
	szStart = szLine;
	while ((szStart < szMatch) && ((*szStart == ' ') || (*szStart == '\t')))
	{
		szStart++;
	}
	while ((szLineEnd > szMatch + psPlace->iLength) && ((szLineEnd[-1] == ' ') || (szLineEnd[-1] == '\t') || (szLineEnd[-1] == '\r')))
	{
		szLineEnd--;
	}

	szShown.Init();
	if (((size)(szLineEnd - szStart) > IDE_FIND_LINE_LENGTH) && ((size)(szMatch - szStart) > IDE_FIND_LINE_BEFORE))
	{
		szStart = szMatch - IDE_FIND_LINE_BEFORE;
		szShown.Append("...");
	}
	uiMatchStart = szShown.Length() + (size)(szMatch - szStart);
	if ((size)(szLineEnd - szStart) > IDE_FIND_LINE_LENGTH)
	{
		szLineEnd = szStart + IDE_FIND_LINE_LENGTH;
		if (szLineEnd < szMatch + psPlace->iLength)
		{
			szLineEnd = szMatch + psPlace->iLength;
		}
	}
	szShown.AppendSubString(szStart, szLineEnd);
	for (i = 0; i < szShown.Length(); i++)
	{
		if ((szShown.Text()[i] == '\t') || (szShown.Text()[i] == '\r') || (szShown.Text()[i] == '\n'))
		{
			szShown.Text()[i] = ' ';
		}
	}

	szLocation.Init(GetIDEFileNamePart(maszFiles.GetText(uiFile)));
	szLocation.Append(' ');
	szLocation.Append(psPlace->iLine + 1);
	pcDest->Add(szShown.Text(), uiMatchStart, (size)psPlace->iLength, szLocation.Text(), uiFile, uiPlace);
	szLocation.Kill();
	szShown.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's searcher: where a result is in its file.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::FindPreviewPlace(SFindInFilesItem* psItem, CChars* pszTitle, size* puiStart, size* puiLength)
{
	SIDEFindPlace*	psPlace;

	if (psItem->uiData >= masPlaces.NumElements())
	{
		return false;
	}
	psPlace = masPlaces.Get(psItem->uiData);
	pszTitle->Append(maszFiles.GetText(psPlace->uiFile));
	*puiStart = psPlace->uiPosition;
	*puiLength = (size)psPlace->iLength;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's searcher: a result's file's text.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::FindPreviewText(SFindInFilesItem* psItem, CChars* pszText)
{
	CTextFile	cFile;
	char*		szText;
	size		uiLength;
	bool		bResult;

	if (psItem->uiData >= masPlaces.NumElements())
	{
		return false;
	}
	cFile.Init();
	bResult = GetFileText(maszFiles.GetText(masPlaces.Get(psItem->uiData)->uiFile), &cFile, &szText, &uiLength);
	if (bResult)
	{
		pszText->Append(szText, uiLength);
	}
	cFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's searcher: the preview shows a result.  A file newly shown
// is given to be highlighted.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::FindPreviewShown(SFindInFilesItem* psItem, Ptr<CTextArea> pPreview, bool bNewText)
{
	if (bNewText && (psItem->uiData < masPlaces.NumElements()) && mpDialog.IsNotNull())
	{
		mpcContent->GetDocuments()->HighlightPreview(this, maszFiles.GetText(masPlaces.Get(psItem->uiData)->uiFile), pPreview->GetText());
		mpDialog->GetDialog()->GetWindow()->RequestTick(0);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The edits thread highlighted a file for the preview: if the preview
// still shows it, it is coloured as its editor would be.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights)
{
	if (mpDialog.IsNotNull() && (strcmp(mpDialog->GetPreviewTitle(), szFileName) == 0))
	{
		ApplyIDEHighlights(mpDialog->GetPreview()->GetTextArea(), pacHighlights);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each tick of the dialog: while its preview is being highlighted the
// edits thread is asked for what it made, as the main window's ticks ask
// while the dialog isn't shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::DialogTick(void)
{
	if (mpDialog.IsNotNull() && mpcContent->GetDocuments()->IsHighlightingPreview())
	{
		mpcContent->GetDocuments()->UpdateEditedTabs(GetDebugTickCount());
		if (mpcContent->GetDocuments()->IsHighlightingPreview())
		{
			mpDialog->GetDialog()->GetWindow()->RequestTick(20);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// How much of the found files' paths they all start with, up to and
// with a separator: the tree shows the rest of each file's directory.
//
//////////////////////////////////////////////////////////////////////////
size CIDEFindInFiles::GetCommonDirectoryLength(void)
{
	const char*	szFirst;
	const char*	szFile;
	size		uiCommon;
	size		i;
	size		j;

	if (maszFoundFiles.NumElements() == 0)
	{
		return 0;
	}
	szFirst = maszFoundFiles.GetText(0);
	uiCommon = (size)(GetIDEFileNamePart(szFirst) - szFirst);
	for (i = 1; i < maszFoundFiles.NumElements(); i++)
	{
		szFile = maszFoundFiles.GetText(i);
		for (j = 0; (j < uiCommon) && (szFile[j] == szFirst[j]); j++)
		{
		}
		uiCommon = j;
	}
	while ((uiCommon > 0) && (szFirst[uiCommon - 1] != '/') && (szFirst[uiCommon - 1] != '\\'))
	{
		uiCommon--;
	}
	return uiCommon;
}


//////////////////////////////////////////////////////////////////////////
//
// The tree of what was found: under what was looked for, each file with
// how many are in it (and its directory, from where the files' paths
// differ), and under it each result's line, as it is now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::ShowResults(bool bKeepSelected)
{
	Ptr<CTreeContainer>	pTree;
	CIDEFileLines		cLines;
	SIDEFindPlace*		psPlace;
	CChars				szText;
	const char*			szFile;
	const char*			szName;
	size				uiCommon;
	size				uiCount;
	size				i;
	size				j;
	int64				iSelectedData;
	int32				iRoot;
	int32				iFile;
	bool				bOpenFiles;

	if (mpPane.IsNull())
	{
		return;
	}
	pTree = mpPane->GetTree();
	iSelectedData = mpPane->BeginShow(bKeepSelected);
	if (!mbFound)
	{
		return;
	}

	iRoot = pTree->AddNode(TREE_NO_NODE, mszFoundTitle.Text(), NULL, 0);
	pTree->SetExpanded(iRoot, true);

	//The tree has a row for each line that can be seen, which thousands of make it slow: past
	//IDE_FIND_OPEN_FILES_MAXIMUM results the files start closed.
	bOpenFiles = (masFound.NumElements() <= IDE_FIND_OPEN_FILES_MAXIMUM);
	uiCommon = GetCommonDirectoryLength();
	cLines.Init(&mpcContent->GetDocuments());
	for (i = 0; i < masFound.NumElements(); i += uiCount)
	{
		psPlace = masFound.Get(i);
		for (uiCount = 1; (i + uiCount < masFound.NumElements()) && (masFound.Get(i + uiCount)->uiFile == psPlace->uiFile); uiCount++)
		{
		}
		szFile = maszFoundFiles.GetText(psPlace->uiFile);
		szName = GetIDEFileNamePart(szFile);
		szText.Init("  ");
		szText.Append((uint32)uiCount);
		szText.Append((uiCount == 1) ? " match" : " matches");
		if ((size)(szName - szFile) > uiCommon + 1)
		{
			szText.Append("  ");
			szText.AppendSubString(&szFile[uiCommon], szName - 1);
		}
		iFile = mpPane->AddGroup(iRoot, szName, szText.Text(), szFile);
		pTree->SetExpanded(iFile, bOpenFiles);
		szText.Kill();

		for (j = i; j < i + uiCount; j++)
		{
			psPlace = masFound.Get(j);
			mpPane->AddResult(iFile, &cLines, szFile, psPlace->iLine, psPlace->iColumn, psPlace->iLength, NULL, (int64)j + 1);
		}
	}
	cLines.Kill();

	mpPane->EndShow(iSelectedData);
}


//////////////////////////////////////////////////////////////////////////
//
// Next or Previous Occurrence (CIDEOccurrences) steps through the tree's
// results (CIDEResultsPane::Step).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindInFiles::Step(bool bNext, bool bKeepKeys)
{
	return mpPane->Step(bNext, bKeepKeys);
}


Ptr<CDockPane> CIDEFindInFiles::GetSteppablePane(void) { return mpTool; }
const char* CIDEFindInFiles::GetSteppableName(void) { return "Find"; }
bool CIDEFindInFiles::HasSteps(void) { return mbFound && (masFound.NumElements() > 0) && mpPane.IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// An edit of the open file szFile (CIDEDocuments::DocumentChanged): the
// tree's results in it move with their text.  The tree's lines say where
// they were until Poll shows them again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	SIDEFindPlace*	psPlace;
	size			uiFile;
	size			i;

	if (!mbFound)
	{
		return;
	}
	for (uiFile = 0; uiFile < maszFoundFiles.NumElements(); uiFile++)
	{
		if (maszFoundFiles.Get(uiFile)->Equals(szFile))
		{
			break;
		}
	}
	if (uiFile == maszFoundFiles.NumElements())
	{
		return;
	}
	for (i = 0; i < masFound.NumElements(); i++)
	{
		psPlace = masFound.Get(i);
		if ((psPlace->uiFile == uiFile) && AdjustIDEPlace(&psPlace->iLine, &psPlace->iColumn, psChange))
		{
			mbMoved = true;
			miMoved = GetDebugTickCount();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: once the edits that moved the tree's results have
// stopped for IDE_FIND_REFRESH, the tree is shown again with the lines as
// they are now, the selected result still selected and in view.
// Milliseconds until it should be looked at again, -1 if not until
// something changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEFindInFiles::Poll(int64 iNow)
{
	if (!mbMoved)
	{
		return -1;
	}
	if (iNow - miMoved < IDE_FIND_REFRESH)
	{
		return IDE_FIND_REFRESH - (iNow - miMoved);
	}
	mbMoved = false;
	ShowResults(true);
	if (mpPane.IsNotNull())
	{
		mpPane->ShowSelected();
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so have the places found in them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::Clear(void)
{
	maszFoundFiles.ReInit();
	masFound.ReInit();
	mbFound = false;
	mbMoved = false;
	if (mpPane.IsNotNull())
	{
		mpPane->Clear();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What is kept of Find in Files in the IDE's settings: the Find tool
// window's preview and its share (the results pane's).
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindInFiles::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;

	pcTag = pcSettings->GetTag(IDE_FIND_SETTINGS_TAG);
	if (pcTag && mpPane.IsNotNull())
	{
		mpPane->ReadSettings(pcTag);
	}
}


void CIDEFindInFiles::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin(IDE_FIND_SETTINGS_TAG);
	if (mpPane.IsNotNull())
	{
		mpPane->WriteSettings(pcWriter);
	}
	pcWriter->End();
}


Ptr<CFindInFilesDialog> CIDEFindInFiles::GetFind(void) { return mpDialog; }
Ptr<CTreeContainer> CIDEFindInFiles::GetFindTree(void) { return mpPane.IsNotNull() ? mpPane->GetTree() : Ptr<CTreeContainer>(); }
Ptr<CIDEResultsPane> CIDEFindInFiles::GetResultsPane(void) { return mpPane; }
Ptr<CDockPane> CIDEFindInFiles::GetTool(void) { return mpTool; }
size CIDEFindInFiles::NumFound(void) { return masFound.NumElements(); }

