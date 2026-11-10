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
	muiIndexings = 0;
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
	U_Size(muiIndexings);
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
	SIDENavigationPlace	sFrom;
	bool				bRecord;

	//A jump: where it leaves goes on Back (CIDENavigationHistory).
	bRecord = mpcContent->GetNavigationHistory()->BeginJump(&sFrom);
	pTab = OpenFileTab(szFileName, uiProject, true);
	if (pTab.IsNull())
	{
		mpcContent->GetNavigationHistory()->EndJump(&sFrom, false);
		return false;
	}
	mpcContent->GetDock()->Activate(pTab->GetDockPane());
	mpActive = pTab;
	mpcContent->GetDiffs()->SetActiveDiff(NULL);
	mpcContent->GetGitStatus()->ShowBranch();
	mpcContent->GetNavigationHistory()->EndJump(&sFrom, bRecord);
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
	pTab = OMalloc<CIDEEditorTab>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetFontColours()->GetFont(mpcContent->GetTheme()->GetColour(TC_Icon)), mpcContent->GetCodeFamily(), mpcContent->GetTheme()->GetColour(TC_Text), szFileName, uiProject, cFile.Text());
	cFile.Kill();
	maTabs.Add(pTab);
	RequestHighlight(pTab);
	if (mpcContent->GetBuilding()->GetListings()->NumListings() > 0)
	{
		pTab->SetListings(mpcContent->GetBuilding()->GetListings());
	}

	szName.Init();
	cFileUtil.GetFileName(&szName, (char*)szFileName);
	szID.Init(IDE_DOCUMENT_PREFIX);
	szID.Append(szFileName);
	pPane = mpcContent->GetDock()->AddPane(szID.Text(), szName.Text(), mpcContent->GetFileTypeIcon(CIDEIcons::GetFileType(szFileName)), pTab->GetPane(), DPK_Document, NULL, bShow);
	szID.Kill();
	szName.Kill();
	pTab->SetDockPane(pPane);

	//Activating the document, or clicking its tab, gives its editor the keys with its caret where it was.
	pPane->SetFocusTarget(pTab->GetEditor());

	//The right button: Rename and Refactor.
	pTab->GetEditor()->SetContextMenu(mpcContent->GetEditorMenu());
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Add(&mpcContent->GetGitStatus(), &CIDEGitStatus::ChangeBarClicked);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Add(&mpcContent->GetDebugging(), &CIDEDebugging::GlyphClicked);
	pTab->GetEditor()->GetUndoEvent()->Add(&mpcContent->GetEdit(), &CIDEEdit::EditorUndoChanged);
	mpcContent->GetDebugging()->ShowBreakpoints(pTab);
	mpcContent->GetGitStatus()->ReadGitBase(pTab);
	mpcContent->GetGitStatus()->UpdateChangeBars(pTab);
	pPane->SetTabFont(GetGitStateFont(mpcContent->GetFontColours(), pTab->GetGitState()));
	mpcContent->GetNavigationHistory()->FileOpened(pTab);
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
	return mpcContent->GetDock()->Close(pTab->GetDockPane());
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
	for (i = maTabs.NumElements(); i > 0; i--)
	{
		if (i <= maTabs.NumElements())
		{
			mpcContent->GetDock()->Close(maTabs.Get(i - 1)->GetDockPane());
		}
	}
	mpcContent->GetDiffs()->CloseAllDiffs();
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
	mpcContent->GetGitStatus()->GitFileSaved(pTab->GetFileName());
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
	for (i = 0; i < maTabs.NumElements(); i++)
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
// A tab's text coloured by what the parser found each part of it to be,
// as its file is or as it has been edited to, soon: on the code model's
// edits thread (see UpdateEditedTabs), not holding the window up.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::RequestHighlight(Ptr<CIDEEditorTab> pTab)
{
	pTab->RequestRecolour();
	mpcContent->GetWindow()->RequestTick(0);
}

//////////////////////////////////////////////////////////////////////////
//
// Everything is indexed again, from the files; the open files with edits
// are indexed again from their editors once that has finished.  What the
// edits thread made is shown first: the model forgets it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::ReindexAll(void)
{
	TakeEditedFiles(true);
	mpcContent->GetCodeModel()->ReindexAll();
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
// solution has been (IsIndexing), and again after the index is built
// again from the files.  Without bNow, nothing is given while the thread
// is busy: the tabs keep their edits and are given once it has finished.
// See UpdateEditedTabs for what is returned.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEDocuments::GiveEditedTabs(int64 iNow, bool bNow)
{
	size					i;
	Ptr<CIDEEditorTab>		pTab;
	bool					bCanIndex;
	bool					bHighlight;
	bool					bIndex;
	int64					iWait;
	int64					iSoonest;
	CArrayIDEEditedFiles	apsFiles;

	if (!TakeEditedFiles(bNow))
	{
		return IDE_EDITS_POLL;
	}
	if (muiIndexings != mpcContent->GetCodeModel()->NumIndexings())
	{
		muiIndexings = mpcContent->GetCodeModel()->NumIndexings();
		for (i = 0; i < maTabs.NumElements(); i++)
		{
			maTabs.Get(i)->IndexRestarted();
		}
	}

	bCanIndex = !mpcContent->GetCodeModel()->IsIndexing();
	iSoonest = -1;
	apsFiles.Init();
	for (i = 0; i < maTabs.NumElements(); i++)
	{
		pTab = maTabs.Get(i);
		if (pTab->TakeEdits(iNow, bNow, bCanIndex, &bHighlight, &bIndex, &iWait))
		{
			AddEditedFile(pTab->GetProject(), pTab->GetFileName(), pTab->GetEditor()->GetText(), pTab->GetEditor()->GetChangeCount(), bHighlight, bIndex, pTab->IsEdited(), &apsFiles);
		}
		else if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
		{
			iSoonest = iWait;
		}
	}

	if (apsFiles.NumElements() > 0)
	{
		mpcContent->GetCodeModel()->ProcessEditedFiles(&apsFiles, bNow);
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

	if ((pTab->GetProject() == MAX_SIZE) || mpcContent->GetCodeModel()->IsIndexing())
	{
		return;
	}
	TakeEditedFiles(true);
	apsFiles.Init();
	AddEditedFile(pTab->GetProject(), pTab->GetFileName(), NULL, 0, false, true, false, &apsFiles);
	mpcContent->GetCodeModel()->ProcessEditedFiles(&apsFiles, false);
	apsFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file to work out added to papsFiles, its text copied from szText
// (NULL: the file is read).  Only C and C++ files are worked out.  A
// header is parsed in the first unit that included it, from that unit's
// editor if it is open: to index it, preprocessed from szText from now
// on; unedited (bEdited false), as its file is.  An edited header that
// isn't indexed, or one no unit has included, is parsed on its own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddEditedFile(size uiProject, const char* szFileName, const char* szText, size uiChanges, bool bHighlight, bool bIndex, bool bEdited, CArrayIDEEditedFiles* papsFiles)
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
	if ((eType == IFT_Header) && (bIndex || !bEdited))
	{
		if (bIndex)
		{
			mpcContent->GetCodeModel()->SetHeaderText(szFileName, szText);
		}
		szUnit.Init();
		if (mpcContent->GetCodeModel()->GetIncluder(szFileName, &uiUnitProject, &szUnit))
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
	if (!mpcContent->GetCodeModel()->TakeEditedFiles(&apsFiles, bWait))
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

	for (i = 0; i < maTabs.NumElements(); i++)
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

	for (i = 0; i < maTabs.NumElements(); i++)
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

	for (i = 0; i < maTabs.NumElements(); i++)
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

	for (i = 0; i < maTabs.NumElements(); i++)
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
size CIDEDocuments::NumTabs(void) { return maTabs.NumElements(); }


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
	size				uiProject;
	SIDENavigationPlace	sFrom;
	bool				bRecord;

	//A jump, the file opened with it: where it leaves goes on Back (CIDENavigationHistory).
	bRecord = mpcContent->GetNavigationHistory()->BeginJump(&sFrom);
	uiProject = mpcContent->GetCodeModel()->FindProjectOfFile(psLocation->szFileName.Text());
	if ((uiProject == MAX_SIZE) && mpActive.IsNotNull())
	{
		uiProject = mpActive->GetProject();
	}
	if (!OpenFile(psLocation->szFileName.Text(), uiProject) || !SetCaretPlace(psLocation->iLine, psLocation->iColumn))
	{
		mpcContent->GetNavigationHistory()->EndJump(&sFrom, false);
		return false;
	}
	mpActive->GetEditor()->GetWindow()->GetFocus()->SetFocus(mpActive->GetEditor());
	mpcContent->GetNavigationHistory()->EndJump(&sFrom, bRecord);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The active editor's text from its caret, uiLength bytes of it, selected,
// the caret at its end, as a step through usages selects the name it went
// to.  Nothing is selected for 0.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::SelectAtCaret(size uiLength)
{
	Ptr<CTextArea>	pEditor;
	size			uiStart;
	size			uiEnd;

	if (mpActive.IsNull() || (uiLength == 0))
	{
		return;
	}
	pEditor = mpActive->GetEditor();
	uiStart = pEditor->GetCaretPosition();
	uiEnd = uiStart + uiLength;
	if (uiEnd > strlen(pEditor->GetText()))
	{
		uiEnd = strlen(pEditor->GetText());
	}
	pEditor->Select(uiStart, uiEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// The name (letters, digits and underscores) starting at the active
// editor's caret selected, as a step through build messages selects what
// the message is at.  Nothing if no name starts there.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::SelectWordAtCaret(void)
{
	char*	szText;
	size	uiStart;
	size	uiEnd;
	char	c;

	if (mpActive.IsNull())
	{
		return;
	}
	szText = mpActive->GetEditor()->GetText();
	uiStart = mpActive->GetEditor()->GetCaretPosition();
	for (uiEnd = uiStart; szText[uiEnd] != '\0'; uiEnd++)
	{
		c = szText[uiEnd];
		if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_')))
		{
			break;
		}
	}
	SelectAtCaret(uiEnd - uiStart);
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
	bool	bResult;

	if (mpActive.IsNull())
	{
		return false;
	}
	bResult = mpActive->SetCaretPlace(iLine, iColumn);

	//Put there by the IDE, not moved by the user: not a move to go back from.
	mpcContent->GetNavigationHistory()->CaretPlaced();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a symbol is named in the active editor: at its caret, or just
// before it (as after the name is typed), or where its selection starts.
// Its line and a column in the name (0 based), and how it is named there
// (ECPPIndexRole: declared, defined or used).  False if none is.  Every
// command that works on the symbol at the caret finds it here, so they
// all agree on where that is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::FindSymbolPlace(int* piLine, int* piColumn, uint8* puiRole)
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
	if (!mpcContent->GetCodeModel()->FindRoleAt(mpActive->GetFileName(), (int)uiLine, iColumn, puiRole))
	{
		if (bSelected || (iColumn == 0) || !mpcContent->GetCodeModel()->FindRoleAt(mpActive->GetFileName(), (int)uiLine, iColumn - 1, puiRole))
		{
			return false;
		}
		iColumn--;
	}
	*piLine = (int)uiLine;
	*piColumn = iColumn;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol named at the active editor's caret (FindSymbolPlace): its
// key and every place it is named.  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDocuments::FindSymbolAtCaret(CArrayIDELocations* pasDest, CChars* pszKey)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return FindSymbolPlace(&iLine, &iColumn, &uiRole) && mpcContent->GetCodeModel()->FindReferences(mpActive->GetFileName(), iLine, iColumn, pasDest, pszKey);
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


//////////////////////////////////////////////////////////////////////////
//
// Save (Control+S) and Save All (Control+Shift+S), and Close All
// Documents.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("File.Save", "Save", "Control+S", Ptr<CImageCel>(), this, &CIDEDocuments::SaveActiveFile);
	pCommands->Add("File.SaveAll", "Save All", "Control+Shift+S", Ptr<CImageCel>(), this, &CIDEDocuments::SaveAllFiles);
	pCommands->Add("Window.CloseAllDocuments", "Close All Documents", NULL, Ptr<CImageCel>(), this, &CIDEDocuments::CloseAllFiles);
}


//////////////////////////////////////////////////////////////////////////
//
// File's Save and Save All.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddFileItems(Ptr<CMenuContainer> pFile)
{
	pFile->AddItem(mpcContent->GetCommands()->Find("File.Save"));
	pFile->AddItem(mpcContent->GetCommands()->Find("File.SaveAll"));
}


//////////////////////////////////////////////////////////////////////////
//
// Window's Close All Documents.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDocuments::AddWindowItems(Ptr<CMenuContainer> pWindowMenu)
{
	pWindowMenu->AddItem(mpcContent->GetCommands()->Find("Window.CloseAllDocuments"));
}


