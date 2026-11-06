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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/Timer.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ObjectReader.h"
#include "StandardLib/ObjectWriter.h"
#include "SupportLib/ImageColour.h"
#include "SupportLib/ImageColourConverter.h"
#include "SupportLib/TrueTypeFont.h"
#include "SupportLib/TrueTypeFontFactory.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/Label.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/JewelTheme.h"
#include "WindowLib/JewelImageGenerator.h"
#include "IDEGit.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEDocuments.h"


//Milliseconds between looks at the edits thread while it is busy.
#define IDE_EDITS_POLL	20


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	maTabs.Init();
	mpActive = NULL;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::Class(void)
{
	U_Pointer(mpcContent);
	M_Embedded(maTabs);
	M_Pointer(mpActive);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDocuments::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Shows szFileName as a document of its own, active: the one it is
// already in if it is open.  A file of the solution's project uiProject
// is highlighted as that project builds it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::OpenFile(const char* szFileName, size uiProject)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = OpenFileTab(szFileName, uiProject, true);
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->mpDock->Activate(pTab->GetDockPane());
	mpActive = pTab;
	mpcContent->mpDiffs->SetActiveDiff(NULL);
	mpcContent->mpGitStatus->ShowBranch();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The editor of szFileName, read into a document pane if it isn't open.
// Without bShow the pane isn't put anywhere, for a layout being read to
// place.  NULL if the file can't be read.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEDocuments::OpenFileTab(const char* szFileName, size uiProject, bool bShow)
{
	CTextFile			cFile;
	CFileUtil			cFileUtil;
	CChars				szName;
	CChars				szID;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CDockPane>		pPane;

	pTab = FindTab(szFileName);
	if (pTab.IsNotNull())
	{
		return pTab;
	}

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		gcLogger.Error2(__METHOD__, " Could not read [", szFileName, "].", NULL);
		return NULL;
	}
	pTab = OMalloc<CIDEEditorTab>(mpcContent->mpWindow, mpcContent->mpTheme, mpcContent->mpFontColours->GetFont(JEWEL_ICON), mpcContent->mpCodeFamily, JEWEL_TEXT, szFileName, uiProject, cFile.Text());
	cFile.Kill();
	maTabs.Add(pTab);
	HighlightTab(pTab);
	if (mpcContent->mpBuilding->GetListings()->NumListings() > 0)
	{
		pTab->SetListings(mpcContent->mpBuilding->GetListings());
	}

	szName.Init();
	cFileUtil.GetFileName(&szName, (char*)szFileName);
	szID.Init(IDE_DOCUMENT_PREFIX);
	szID.Append(szFileName);
	pPane = mpcContent->mpDock->AddPane(szID.Text(), szName.Text(), mpcContent->maFileIcons.Get(CIDEIcons::GetFileType(szFileName)), pTab->GetPane(), DPK_Document, NULL, bShow);
	szID.Kill();
	szName.Kill();
	pTab->SetDockPane(pPane);

	//Activating the document, or clicking its tab, gives its editor the keys with its caret where it was.
	pPane->SetFocusTarget(pTab->GetEditor());

	//The right button: Rename and Refactor.
	pTab->GetEditor()->SetContextMenu(mpcContent->mpEditorMenu);
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Add(&mpcContent->mpGitStatus, &CIDEGitStatus::ChangeBarClicked);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Add(&mpcContent->mpDebugging, &CIDEDebugging::GlyphClicked);
	pTab->GetEditor()->GetUndoEvent()->Add(&mpcContent->mpEdit, &CIDEEdit::EditorUndoChanged);
	mpcContent->mpDebugging->ShowBreakpoints(pTab);
	mpcContent->mpGitStatus->ReadGitBase(pTab);
	mpcContent->mpGitStatus->UpdateChangeBars(pTab);
	pPane->SetTabFont(GetGitStateFont(mpcContent->mpFontColours, pTab->GetGitState()));
	return pTab;
}


//////////////////////////////////////////////////////////////////////////
//
// Closes szFileName's document.  False if it isn't open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::CloseFile(const char* szFileName)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindTab(szFileName);
	if (pTab.IsNull())
	{
		return false;
	}
	return mpcContent->mpDock->Close(pTab->GetDockPane());
}


//////////////////////////////////////////////////////////////////////////
//
// Every open file's and diff's document is closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::CloseAllFiles(void)
{
	size	i;

	//Closing the document lets its file go (PaneClosed); one with changes is asked about first (PaneClosing).
	for (i = maTabs.Size(); i > 0; i--)
	{
		if (i <= maTabs.Size())
		{
			mpcContent->mpDock->Close(maTabs.Get(i - 1)->GetDockPane());
		}
	}
	mpcContent->mpDiffs->CloseAllDiffs();
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the tab's text to its file if it has changed since it was opened
// or saved, then reads its repository's status again.  False if it
// couldn't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::SaveFile(Ptr<CIDEEditorTab> pTab)
{
	if (!pTab->IsModified())
	{
		return true;
	}
	if (!pTab->SaveFile())
	{
		return gcLogger.Error2(__METHOD__, " Could not save [", pTab->GetFileName(), "].", NULL);
	}
	mpcContent->mpGitStatus->GitFileSaved(pTab->GetFileName());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Saves the selected tab's file.  True if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::SaveActiveFile(void)
{
	if (mpActive.IsNull())
	{
		return true;
	}
	return SaveFile(mpActive);
}


//////////////////////////////////////////////////////////////////////////
//
// Saves every open file that has changed.  False if any couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::SaveAllFiles(void)
{
	size	i;
	bool	bResult;

	bResult = true;
	for (i = 0; i < maTabs.Size(); i++)
	{
		if (!SaveFile(maTabs.Get(i)))
		{
			bResult = false;
		}
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Colours a tab's text by what the parser found each part of it to be:
// as its file is, or as it has been edited to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::HighlightTab(Ptr<CIDEEditorTab> pTab)
{
	CArrayHighlights	acHighlights;
	CChars				szErrors;

	//Only C and C++ are parsed; a text file or a project file is shown as it is.
	if ((pTab->GetProject() == SOLUTION_EXPLORER_NO_PROJECT) || (CIDEIcons::GetFileType(pTab->GetFileName()) == IFT_Other))
	{
		return;
	}
	acHighlights.Init();
	szErrors.Init();
	if (mpcContent->mcCodeModel.Highlight(pTab->GetProject(), pTab->GetFileName(), pTab->IsEdited() ? pTab->GetEditor()->GetText() : NULL, &acHighlights, &szErrors))
	{
		pTab->SetHighlights(&acHighlights);
	}
	szErrors.Kill();
	acHighlights.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Everything is indexed again, from the files; the open .cpp files with
// edits are indexed again from their editors once that has finished.
// What the edits thread made is shown first: the model forgets it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::ReindexAll(void)
{
	size	i;

	TakeEditedFiles(true);
	mpcContent->mcCodeModel.ReindexAll();
	for (i = 0; i < maTabs.Size(); i++)
	{
		if (maTabs.Get(i)->IsEdited())
		{
			maTabs.Get(i)->SetIndexStale(true);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each tick: what the edits thread made is shown, then the tabs whose
// text has been left alone for a moment since an edit are given to it,
// to be highlighted and indexed again (from one parse).  Returns how many
// milliseconds until a tab might be due or the thread is to be looked at
// again, -1 for never.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEDocuments::UpdateEditedTabs(int64 iNow)
{
	return GiveEditedTabs(iNow, false);
}


//////////////////////////////////////////////////////////////////////////
//
// The tabs changed since they were indexed are indexed again (and
// highlighted, if that is pending) straight away, on this thread, after
// waiting for the edits thread: before Rename, Find Usages or Go to is
// worked out.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::UpdateEditedTabsNow(void)
{
	GiveEditedTabs(GetDebugTickCount(), true);
}


//////////////////////////////////////////////////////////////////////////
//
// The edited tabs given to the code model's edits thread, or with bNow
// worked out on this thread.  A tab is indexed again only once the whole
// solution has been (IsIndexing).  Without bNow, nothing is given while
// the thread is busy: the tabs stay pending and are given once it has
// finished.  See UpdateEditedTabs for what is returned.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEDocuments::GiveEditedTabs(int64 iNow, bool bNow)
{
	size					i;
	Ptr<CIDEEditorTab>		pTab;
	bool					bIndexing;
	bool					bHighlight;
	bool					bIndex;
	int64					iWait;
	int64					iSoonest;
	CArrayIDEEditedFiles	apsFiles;

	if (!TakeEditedFiles(bNow))
	{
		return IDE_EDITS_POLL;
	}

	bIndexing = mpcContent->mcCodeModel.IsIndexing();
	iSoonest = -1;
	apsFiles.Init();
	for (i = 0; i < maTabs.Size(); i++)
	{
		pTab = maTabs.Get(i);
		if (bNow)
		{
			pTab->SeeChanges(iNow);
			bHighlight = pTab->IsRecolourPending() && (pTab->GetProject() != MAX_SIZE);
		}
		else
		{
			bHighlight = pTab->IsRecolourDue(iNow, &iWait);
			if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
			{
				iSoonest = iWait;
			}
		}

		//A tab is indexed with its highlights, or after them (once the solution has been indexed).
		bIndex = pTab->IsIndexStale() && !bIndexing && (pTab->GetProject() != MAX_SIZE) && (bNow || bHighlight || !pTab->IsRecolourPending());
		if (bHighlight)
		{
			pTab->SetRecolourPending(false);
		}
		if (bIndex)
		{
			pTab->SetIndexStale(false);
		}
		if (bHighlight || bIndex)
		{
			AddEditedFile(pTab->GetProject(), pTab->GetFileName(), pTab->GetEditor()->GetText(), pTab->GetEditor()->GetChangeCount(), bHighlight, bIndex, &apsFiles);
		}
	}

	if (apsFiles.NumElements() > 0)
	{
		mpcContent->mcCodeModel.ProcessEditedFiles(&apsFiles, bNow);
		if (bNow)
		{
			TakeEditedFiles(true);
		}
		else
		{
			iSoonest = IDE_EDITS_POLL;
		}
	}
	apsFiles.Kill();
	return iSoonest;
}


//////////////////////////////////////////////////////////////////////////
//
// A file closed without saving its edits is indexed again as it is on
// disk, on the edits thread: the index had its editor's text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::ReindexDiscarded(Ptr<CIDEEditorTab> pTab)
{
	CArrayIDEEditedFiles	apsFiles;

	if ((pTab->GetProject() == MAX_SIZE) || mpcContent->mcCodeModel.IsIndexing())
	{
		return;
	}
	TakeEditedFiles(true);
	apsFiles.Init();
	AddEditedFile(pTab->GetProject(), pTab->GetFileName(), NULL, 0, false, true, &apsFiles);
	mpcContent->mcCodeModel.ProcessEditedFiles(&apsFiles, false);
	apsFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An edited file added to papsFiles, its text copied from szText (NULL:
// the file is read).  Only C and C++ files are worked out.  A header to
// index is preprocessed from szText from now on and parsed in the first
// unit that included it, from that unit's editor if it is open; one that
// isn't indexed, or no unit has included, is parsed on its own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddEditedFile(size uiProject, const char* szFileName, const char* szText, size uiChanges, bool bHighlight, bool bIndex, CArrayIDEEditedFiles* papsFiles)
{
	SIDEEditedFile*		psFile;
	EIDEFileType		eType;
	size				uiUnitProject;
	CChars				szUnit;
	Ptr<CIDEEditorTab>	pUnitTab;

	eType = CIDEIcons::GetFileType(szFileName);
	if (eType == IFT_Other)
	{
		return;
	}

	psFile = NewMalloc<SIDEEditedFile>();
	psFile->Init(uiProject, szFileName, szText, uiChanges, bHighlight, bIndex);
	if ((eType == IFT_Header) && bIndex)
	{
		mpcContent->mcCodeModel.SetHeaderText(szFileName, szText);
		szUnit.Init();
		if (mpcContent->mcCodeModel.GetIncluder(szFileName, &uiUnitProject, &szUnit))
		{
			pUnitTab = FindTab(szUnit.Text());
			psFile->SetUnit(uiUnitProject, szUnit.Text(), pUnitTab.IsNotNull() ? pUnitTab->GetEditor()->GetText() : NULL);
		}
		else
		{
			psFile->bIndex = false;
		}
		szUnit.Kill();
	}
	papsFiles->Add(psFile);
}


//////////////////////////////////////////////////////////////////////////
//
// What the edits thread made shown: the highlights of each file whose
// editor hasn't changed since its text was given (one that has is given
// again).  Without bWait, false if the thread hasn't finished.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::TakeEditedFiles(bool bWait)
{
	CArrayIDEEditedFiles	apsFiles;
	SIDEEditedFile*			psFile;
	Ptr<CIDEEditorTab>		pTab;
	size					i;

	apsFiles.Init();
	if (!mpcContent->mcCodeModel.TakeEditedFiles(&apsFiles, bWait))
	{
		apsFiles.Kill();
		return false;
	}
	for (i = 0; i < apsFiles.NumElements(); i++)
	{
		psFile = apsFiles.GetPtr(i);
		pTab = psFile->bHighlighted ? FindTab(psFile->szFileName.Text()) : Ptr<CIDEEditorTab>();
		if (pTab.IsNotNull() && (pTab->GetEditor()->GetChangeCount() == psFile->uiChanges))
		{
			pTab->SetHighlights(&psFile->acHighlights);
		}
	}
	KillIDEEditedFiles(&apsFiles);
	apsFiles.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab szFileName is open in, NULL if it isn't open.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEDocuments::FindTab(const char* szFileName)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (StringCompare(maTabs.Get(i)->GetFileName(), szFileName) == 0)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The file shown in a document pane, NULL if it isn't one of them.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEDocuments::FindTab(CDockPane* pcPane)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetDockPane() == pcPane)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab whose editor pcEditor is, NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEDocuments::FindTab(CTextArea* pcEditor)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetEditor() == pcEditor)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab whose code editor is pcEditor, NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEDocuments::FindTab(CCodeEditor* pcEditor)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetCodeEditor() == pcEditor)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


Ptr<CTextArea> CIDEDocuments::GetEditor(void) { return mpActive.IsNotNull() ? mpActive->GetEditor() : Ptr<CTextArea>(); }


Ptr<CScrollPane> CIDEDocuments::GetEditorPane(void) { return mpActive.IsNotNull() ? mpActive->GetPane() : Ptr<CScrollPane>(); }


Ptr<CIDEEditorTab> CIDEDocuments::GetActiveTab(void) { return mpActive; }


//////////////////////////////////////////////////////////////////////////
//
// The open documents' tabs.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDocuments::NumTabs(void) { return maTabs.Size(); }


Ptr<CIDEEditorTab> CIDEDocuments::GetTab(size uiIndex) { return maTabs.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The file the location is in, in its tab, with the caret on it.  A
// file that isn't one of the solution's projects' items is highlighted as
// part of the project of the file it was found from.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::OpenLocation(SIDELocation* psLocation)
{
	size	uiProject;

	uiProject = mpcContent->mcCodeModel.FindProjectOfFile(psLocation->szFileName.Text());
	if ((uiProject == MAX_SIZE) && mpActive.IsNotNull())
	{
		uiProject = mpActive->GetProject();
	}
	if (!OpenFile(psLocation->szFileName.Text(), uiProject))
	{
		return false;
	}
	if (!SetCaretPlace(psLocation->iLine, psLocation->iColumn))
	{
		return false;
	}
	mpActive->GetEditor()->GetWindow()->GetFocus()->SetFocus(mpActive->GetEditor());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The caret's line and column (0 based, the column in bytes) in the
// selected tab.  False if no file is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::GetCaretPlace(int* piLine, int* piColumn)
{
	if (mpActive.IsNull())
	{
		return false;
	}
	return mpActive->GetCaretPlace(piLine, piColumn);
}


//////////////////////////////////////////////////////////////////////////
//
// Puts the selected tab's caret at a line and column (0 based, the
// column in bytes) and scrolls to show it.  False if no file is open or
// it hasn't that line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::SetCaretPlace(int iLine, int iColumn)
{
	if (mpActive.IsNull())
	{
		return false;
	}
	return mpActive->SetCaretPlace(iLine, iColumn);
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol named at the active editor's caret (or just before it, as
// after it's typed), or where its selection starts: its key and every
// place it is named.  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::FindSymbolAtCaret(CArrayIDELocations* pasDest, CChars* pszKey)
{
	Ptr<CTextArea>	pEditor;
	size			uiStart;
	size			uiEnd;
	size			uiLine;
	int				iColumn;
	bool			bSelected;

	if (mpActive.IsNull())
	{
		return false;
	}
	pEditor = mpActive->GetEditor();
	bSelected = pEditor->GetSelection(&uiStart, &uiEnd);
	if (!bSelected)
	{
		uiStart = pEditor->GetCaretPosition();
	}
	uiLine = pEditor->FindLine(uiStart);
	iColumn = (int)(uiStart - pEditor->GetLineStart(uiLine));
	if (mpcContent->mcCodeModel.FindReferences(mpActive->GetFileName(), (int)uiLine, iColumn, pasDest, pszKey))
	{
		return true;
	}
	return !bSelected && (iColumn > 0) && mpcContent->mcCodeModel.FindReferences(mpActive->GetFileName(), (int)uiLine, iColumn - 1, pasDest, pszKey);
}


//////////////////////////////////////////////////////////////////////////
//
// The text of a file's line (0 based), as its editor has it if it is
// open, else as it is on the disk; trimmed, tabs as spaces.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::GetFileLine(const char* szFileName, int iLine, CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CTextFile			cFile;
	char*				szPosition;
	char*				szEnd;
	int					i;

	pTab = FindTab(szFileName);
	szPosition = NULL;
	cFile.Init();
	if (pTab.IsNotNull())
	{
		pEditor = pTab->GetEditor();
		if ((size)iLine < pEditor->NumLines())
		{
			szPosition = &pEditor->GetText()[pEditor->GetLineStart((size)iLine)];
		}
	}
	else if (cFile.Read((char*)szFileName))
	{
		szPosition = cFile.Text();
		for (i = 0; (i < iLine) && szPosition; i++)
		{
			szPosition = strchr(szPosition, '\n');
			szPosition = szPosition ? szPosition + 1 : NULL;
		}
	}
	if (szPosition)
	{
		szEnd = strchr(szPosition, '\n');
		pszDest->AppendSubString(szPosition, szEnd ? szEnd : szPosition + strlen(szPosition));
		pszDest->Replace('\r', ' ');
		pszDest->Replace('\t', ' ');
		pszDest->StripWhitespace(true);
	}
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab whose file was selected last, NULL if none is (a diff is).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::SetActiveTab(Ptr<CIDEEditorTab> pTab) { mpActive = pTab; }


//////////////////////////////////////////////////////////////////////////
//
// pTab's pane has closed: it is forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::RemoveTab(Ptr<CIDEEditorTab> pTab)
{
	if (mpActive == pTab)
	{
		mpActive = NULL;
	}
	maTabs.Remove(pTab);
}
