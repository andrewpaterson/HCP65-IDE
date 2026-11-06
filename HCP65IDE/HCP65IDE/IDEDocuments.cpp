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
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::ReindexAll(void)
{
	size	i;

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
// The open files changed since they were indexed are indexed again from
// their editors' text: after a pause in the typing on the code model's
// reindexing thread, or with bNow straight away on this thread (before
// Rename, Find Usages or Go to is worked out; it waits for the reindexing
// thread first).  A header is preprocessed from its editor from then on,
// and indexed again with the first unit that included it; then the .cpp
// (and .c) files.  Not while everything is being indexed: they are
// indexed after.  Not while the reindexing thread is busy without bNow:
// the tabs stay stale and are given to it once it has finished.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::ReindexStaleTabs(bool bNow)
{
	size					i;
	size					iPass;
	Ptr<CIDEEditorTab>		pTab;
	EIDEFileType			eType;
	CArrayIDEReindexFiles	apsFiles;

	if (mpcContent->mcCodeModel.IsIndexing())
	{
		return;
	}
	if (bNow)
	{
		mpcContent->mcCodeModel.WaitForReindexing();
	}
	else if (mpcContent->mcCodeModel.IsReindexing())
	{
		return;
	}

	apsFiles.Init();
	for (iPass = 0; iPass < 2; iPass++)
	{
		for (i = 0; i < maTabs.Size(); i++)
		{
			pTab = maTabs.Get(i);
			pTab->SeeChanges(GetDebugTickCount());
			if (!pTab->IsIndexStale() || (pTab->GetProject() == MAX_SIZE) || (!bNow && pTab->IsRecolourPending()))
			{
				continue;
			}
			eType = CIDEIcons::GetFileType(pTab->GetFileName());
			if ((iPass == 0) && (eType == IFT_Header))
			{
				mpcContent->mcCodeModel.SetHeaderText(pTab->GetFileName(), pTab->GetEditor()->GetText());
				AddReindexHeader(pTab->GetFileName(), &apsFiles);
				pTab->SetIndexStale(false);
			}
			else if ((iPass == 1) && ((eType == IFT_CPP) || (eType == IFT_C)))
			{
				AddReindexFile(pTab->GetProject(), pTab->GetFileName(), pTab->GetEditor()->GetText(), NULL, &apsFiles);
				pTab->SetIndexStale(false);
			}
		}
	}
	mpcContent->mcCodeModel.Reindex(&apsFiles, bNow);
	apsFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file to index again added to papsFiles, its text copied from
// szContents (NULL: the file is read).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddReindexFile(size uiProject, const char* szFileName, const char* szContents, const char* szHeader, CArrayIDEReindexFiles* papsFiles)
{
	SIDEReindexFile*	psFile;

	psFile = NewMalloc<SIDEReindexFile>();
	psFile->Init(uiProject, szFileName, szContents, szHeader);
	papsFiles->Add(psFile);
}


//////////////////////////////////////////////////////////////////////////
//
// The header szFileName to index again (after its text changed) with the
// first unit that included it, from that unit's editor if it is open,
// added to papsFiles.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddReindexHeader(const char* szFileName, CArrayIDEReindexFiles* papsFiles)
{
	size				uiProject;
	CChars				szUnit;
	Ptr<CIDEEditorTab>	pUnitTab;

	szUnit.Init();
	if (mpcContent->mcCodeModel.GetIncluder(szFileName, &uiProject, &szUnit))
	{
		pUnitTab = FindTab(szUnit.Text());
		AddReindexFile(uiProject, szUnit.Text(), pUnitTab.IsNotNull() ? pUnitTab->GetEditor()->GetText() : NULL, szFileName, papsFiles);
	}
	szUnit.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file closed without saving its edits is indexed again as it is on
// disk, on the reindexing thread: the index had its editor's text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::ReindexDiscarded(Ptr<CIDEEditorTab> pTab)
{
	EIDEFileType			eType;
	CArrayIDEReindexFiles	apsFiles;

	if ((pTab->GetProject() == MAX_SIZE) || mpcContent->mcCodeModel.IsIndexing())
	{
		return;
	}
	apsFiles.Init();
	eType = CIDEIcons::GetFileType(pTab->GetFileName());
	if (eType == IFT_Header)
	{
		mpcContent->mcCodeModel.SetHeaderText(pTab->GetFileName(), NULL);
		AddReindexHeader(pTab->GetFileName(), &apsFiles);
	}
	else if ((eType == IFT_CPP) || (eType == IFT_C))
	{
		AddReindexFile(pTab->GetProject(), pTab->GetFileName(), NULL, NULL, &apsFiles);
	}
	mpcContent->mcCodeModel.Reindex(&apsFiles, false);
	apsFiles.Kill();
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
