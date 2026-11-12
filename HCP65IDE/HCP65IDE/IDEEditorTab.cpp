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
#include "BaseLib/TextFile.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEEditorTab.h"


#define IDE_CARET_LINES_ABOVE	4		//How many lines show above a line jumped to.
#define IDE_RECOLOUR_DELAY		400		//Milliseconds after the last edit before the text is highlighted again.
#define IDE_LISTING_TEXT		Set32BitColour((uint8)0xE6, (uint8)0xDF, (uint8)0xB8)	//A line's assembly: the editor's text, a little yellow.
#define IDE_LISTING_BACKGROUND	Set32BitColour((uint8)0x14, (uint8)0x15, (uint8)0x17)	//Under a line's assembly: darker than the editor's background (the theme's editor colour, the document well's).

//IntelliJ IDEA's (dark theme) change bars in the gutter.
#define IDE_CHANGE_ADDED		Set32BitColour((uint8)0x54, (uint8)0x91, (uint8)0x59)
#define IDE_CHANGE_MODIFIED		Set32BitColour((uint8)0x37, (uint8)0x5F, (uint8)0xAD)
#define IDE_CHANGE_REMOVED		Set32BitColour((uint8)0x86, (uint8)0x8A, (uint8)0x91)


//////////////////////////////////////////////////////////////////////////
//
// The file's text (szText, already read) in a code editor of tiles.  A
// line's code is shown under it with pTitleFont over it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pTitleFont, Ptr<CFontFamily> pFamily, ARGB32 sColour, const char* szFileName, size uiProject, const char* szText)
{
	PreInit();

	mpWindow = pWindow;
	mcListings.Init();
	mpCodeEditor = OMalloc<CCodeEditor>(pWindow, pTheme, pFamily, szText);
	mpCodeEditor->SetGlyphColumn(true);
	//The editor fills the view so a press right of the text, or below it, is in the editor: the caret goes to the
	//nearest text, the end of the line or of the last line, as IntelliJ's.
	mpCodeEditor->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpZones = OMalloc<CLineZones>(mpCodeEditor, pTheme, pTitleFont, pFamily, IDE_LISTING_BACKGROUND, IDE_LISTING_TEXT);
	mpEditor = mpCodeEditor->GetTextArea();
	mpEditor->SetTiledText(true);
	mpEditor->SetColour(sColour);
	//The caret's line is highlighted, as IntelliJ's caret row, so where a step or a jump went shows without the keys.
	mpEditor->SetCaretLine(true, pTheme->GetColour(TC_CaretLine));
	//The selected text's other occurrences in the file are highlighted, as IntelliJ's are.
	mpEditor->SetHighlightOccurrences(true);
	mpEditor->SetSelectionRanges(this);
	mpPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpPane->SetContent(mpCodeEditor);
	mpPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mszFileName.Init(szFileName);
	muiProject = uiProject;
	muiSeenChanges = mpEditor->GetChangeCount();
	miChangedTime = 0;
	mbRecolour = false;
	mbRecolourNow = false;
	mbEdited = false;
	mbIndexStale = false;
	muiIndexedChanges = muiSeenChanges;
	masSyntaxRanges.Init();
	muiSyntaxChanges = MAX_SIZE;
	mcSyntaxBehind.Init();
	mbCarriageReturns = UsesIDECarriageReturns(szText);
	muiSavedUndo = mpEditor->GetUndoId();
	mbShownModified = false;
	meGitState = GFS_Unmodified;
	mbBase = false;
	mszBaseText.Init();
	mszBaseHead.Init();
	muiBarChanges = MAX_SIZE;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Class(void)
{
	M_Pointer(mpWindow);
	M_Pointer(mpCodeEditor);
	M_Pointer(mpEditor);
	M_Pointer(mpPane);
	M_Pointer(mpDockPane);
	U_String(mszFileName);
	U_Data(size, muiProject);
	U_Data(size, muiSeenChanges);
	U_Data(int64, miChangedTime);
	U_Bool(mbRecolour);
	U_Bool(mbRecolourNow);
	U_Bool(mbEdited);
	U_Bool(mbIndexStale);
	U_Size(muiIndexedChanges);
	U_Unknown(CArrayTextRange, masSyntaxRanges);
	U_Size(muiSyntaxChanges);
	U_Unknown(CComponentEvent<CIDEEditorTab>, mcSyntaxBehind);
	M_Pointer(mpZones);
	U_Unknown(CIDEListingSource, mcListings);
	U_Bool(mbCarriageReturns);
	U_Size(muiSavedUndo);
	U_Bool(mbShownModified);
	U_Enum(meGitState);
	U_Bool(mbBase);
	U_String(mszBaseText);
	U_String(mszBaseHead);
	U_Size(muiBarChanges);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Free(void)
{
	if (mpEditor.IsNotNull())
	{
		mpEditor->SetSelectionRanges(NULL);
	}
	mcSyntaxBehind.Kill();
	masSyntaxRanges.Kill();
	mszBaseHead.Kill();
	mszBaseText.Kill();
	mcListings.Kill();
	mszFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An editor tab isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The editor's text coloured by what the parser found each part of it to
// be, replacing its colours.  Code skipped by conditional compilation is
// dimmed whatever is in it, comments too, so it goes last.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SetHighlights(CArrayHighlights* pacHighlights)
{
	ApplyIDEHighlights(mpEditor, pacHighlights);
}


//////////////////////////////////////////////////////////////////////////
//
// What each node of the text's syntax was parsed from (lines and columns
// from 0, sorted by start then end), parsed with the editor's text as it
// is now, at change count uiChanges, for Extend Selection to grow
// through.  They are kept as ranges of the text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SetSyntaxRanges(CArrayCPPSyntaxRanges* pasRanges, size uiChanges)
{
	SCPPSyntaxRange*	psSyntax;
	STextRange*			psRange;
	size				uiLines;
	size				i;

	masSyntaxRanges.ReInit();
	uiLines = mpEditor->NumLines();
	for (i = 0; i < pasRanges->NumElements(); i++)
	{
		psSyntax = pasRanges->Get(i);
		if (((size)psSyntax->iLine < uiLines) && ((size)psSyntax->iEndLine < uiLines))
		{
			psRange = masSyntaxRanges.Add();
			psRange->uiStart = FindPosition(psSyntax->iLine, psSyntax->iColumn);
			psRange->uiEnd = FindPosition(psSyntax->iEndLine, psSyntax->iEndColumn);
		}
	}
	muiSyntaxChanges = uiChanges;
}


//////////////////////////////////////////////////////////////////////////
//
// Called when Extend Selection wants the syntax and the ranges are behind
// the text: whoever parses the tab's text does so before it returns (the
// documents, on this thread, as the edits thread would).
//
//////////////////////////////////////////////////////////////////////////
CComponentEvent<CIDEEditorTab>* CIDEEditorTab::GetSyntaxBehindEvent(void) { return &mcSyntaxBehind; }


//////////////////////////////////////////////////////////////////////////
//
// The syntax ranges around uiStart to uiEnd, for Extend Selection to pick
// from.  When the text has changed since they were parsed it is parsed
// now (the user's choice: parse on demand; see GetSyntaxBehindEvent); a
// text that isn't C or C++, or isn't in a project, has none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::AddSelectionRanges(size uiStart, size uiEnd, CArrayTextRange* pasDest)
{
	STextRange*	psRange;
	size		i;

	if (muiSyntaxChanges != mpEditor->GetChangeCount())
	{
		mcSyntaxBehind.Call(this);
	}
	if (muiSyntaxChanges != mpEditor->GetChangeCount())
	{
		return;
	}

	//They are sorted by start: one that starts after uiStart isn't around it, nor is any after it.
	for (i = 0; i < masSyntaxRanges.NumElements(); i++)
	{
		psRange = masSyntaxRanges.Get(i);
		if (psRange->uiStart > uiStart)
		{
			break;
		}
		if (psRange->uiEnd >= uiEnd)
		{
			*pasDest->Add() = *psRange;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// pTextArea's text coloured by pacHighlights (lines and columns from 0),
// replacing its colours, as an editor tab's is.  Inactive code goes last.
//
//////////////////////////////////////////////////////////////////////////
void ApplyIDEHighlights(Ptr<CTextArea> pTextArea, CArrayHighlights* pacHighlights)
{
	SCPPHighlight*	psHighlight;
	size			i;
	size			uiStart;
	int				iPass;

	pTextArea->ClearSpans();
	for (iPass = 0; iPass < 2; iPass++)
	{
		for (i = 0; i < pacHighlights->NumElements(); i++)
		{
			psHighlight = pacHighlights->Get(i);
			if (((psHighlight->uiKind == CPPH_Inactive) == (iPass == 1)) && (psHighlight->iLine >= 0) && ((size)psHighlight->iLine < pTextArea->NumLines()))
			{
				uiStart = pTextArea->GetLineStart((size)psHighlight->iLine) + (size)psHighlight->iColumn;
				pTextArea->AddSpan(uiStart, (size)psHighlight->iLength, GetHighlightColour((ECPPHighlight)psHighlight->uiKind));
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The caret's line and column (0 based, the column in bytes).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::GetCaretPlace(int* piLine, int* piColumn)
{
	size	uiCaret;
	size	uiLine;

	uiCaret = mpEditor->GetCaretPosition();
	uiLine = mpEditor->FindLine(uiCaret);
	*piLine = (int)uiLine;
	*piColumn = (int)(uiCaret - mpEditor->GetLineStart(uiLine));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The position in the text of a line and column (0 based, the column in
// bytes): the line's end if it hasn't that column, as it might not since
// the place was found.
//
//////////////////////////////////////////////////////////////////////////
size CIDEEditorTab::FindPosition(int iLine, int iColumn)
{
	size	uiPosition;
	size	uiLineEnd;

	uiPosition = mpEditor->GetLineStart((size)iLine) + (size)iColumn;
	uiLineEnd = mpEditor->GetLineEnd((size)iLine);
	return (uiPosition < uiLineEnd) ? uiPosition : uiLineEnd;
}


//////////////////////////////////////////////////////////////////////////
//
// Puts the caret at a line and column (0 based, the column in bytes; see
// FindPosition) and scrolls the editor to show it.  False if the file
// hasn't that line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::SetCaretPlace(int iLine, int iColumn)
{
	CInt32Rect	cCaret;
	int32		iView;
	int32		iRight;
	int32		x;
	int32		y;

	if ((iLine < 0) || ((size)iLine >= mpEditor->NumLines()))
	{
		return false;
	}

	//Laid out so the caret has a place: its line a few lines from the top, and the text only scrolled sideways if the
	//caret is past the right of the view (a window not shown yet has no width).  The code is right of the gutter.
	mpWindow->Layout();
	mpEditor->SetCaretPosition(FindPosition(iLine, iColumn));
	mpWindow->Layout();
	mpEditor->GetCaretBounds(&cCaret);
	iView = mpPane->GetActualSize().x;
	iRight = mpCodeEditor->GetGutterWidth() + cCaret.GetRight();
	x = ((iView <= 0) || (iRight < iView)) ? 0 : iRight - iView / 2;
	y = cCaret.GetTop() - IDE_CARET_LINES_ABOVE * cCaret.GetHeight();
	mpPane->SetScrollPosition((x > 0) ? x : 0, (y > 0) ? y : 0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What the tab wants worked out now from its text, both cleared as they
// are taken: pbHighlight, if it changed since it was last highlighted,
// and pbIndex, if it changed since it was last indexed and bCanIndex.
// Only once the text has been left alone for a moment after an edit, or
// with bNow (or after RequestRecolour) at once; until then piWait is how many milliseconds until it
// might be, -1 for never.  False if there is nothing.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::TakeEdits(int64 iNow, bool bNow, bool bCanIndex, bool* pbHighlight, bool* pbIndex, int64* piWait)
{
	*pbHighlight = false;
	*pbIndex = false;
	*piWait = -1;
	SeeChanges(iNow);
	if ((muiProject == MAX_SIZE) || (!mbRecolour && !mbIndexStale))
	{
		return false;
	}
	if (!bNow && !mbRecolourNow && (iNow - miChangedTime < IDE_RECOLOUR_DELAY))
	{
		*piWait = IDE_RECOLOUR_DELAY - (iNow - miChangedTime);
		return false;
	}

	*pbHighlight = mbRecolour;
	*pbIndex = mbIndexStale && bCanIndex;
	mbRecolour = false;
	mbRecolourNow = false;
	mbIndexStale = mbIndexStale && !bCanIndex;
	return *pbHighlight || *pbIndex;
}


//////////////////////////////////////////////////////////////////////////
//
// The index was built again from the files: an edited tab is indexed
// (and highlighted) again from its text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::IndexRestarted(void)
{
	if (mbEdited)
	{
		mbIndexStale = true;
		mbRecolour = true;
		muiIndexedChanges = MAX_SIZE;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The index was given the editor's text as it was at change count
// uiChanges.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Indexed(size uiChanges)
{
	muiIndexedChanges = uiChanges;
}


//////////////////////////////////////////////////////////////////////////
//
// The index has the editor's text as it is now, so its places are the
// editor's: from the file, unedited since it was opened, or given since
// the last edit.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::IsIndexCurrent(void)
{
	return muiIndexedChanges == mpEditor->GetChangeCount();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab is highlighted again from its text at the next chance, without
// waiting for typing to pause: it was opened, or the configuration or
// solutions its colours depend on changed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::RequestRecolour(void)
{
	mbRecolour = true;
	mbRecolourNow = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The listings of the last HCP65 build (NULL for none): each line of the
// file with code is marked, and the code shown under lines is up to date.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SetListings(CCPP65ListingIndex* pcListings)
{
	if (pcListings)
	{
		mcListings.Set(pcListings, mszFileName.Text());
		mpZones->SetSource(&mcListings);
	}
	else
	{
		mpZones->SetSource(NULL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Shows line uiLine's code under it, or hides it, as clicking its mark
// does.  False if it has no code to show.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::ToggleListing(size uiLine)
{
	return mpZones->Toggle(uiLine);
}


//////////////////////////////////////////////////////////////////////////
//
// szText (uiLength bytes) is the file's text in the last commit, szHead.
// The change bars are worked out again.  An empty text marks every line
// added, as for a file added since.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SetBaseText(const char* szText, size uiLength, const char* szHead)
{
	mbBase = true;
	mszBaseText.Clear();
	mszBaseText.Append(szText, uiLength);
	mszBaseHead.Clear();
	mszBaseHead.Append(szHead);
	muiBarChanges = MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The file has no last commit's text (it isn't in a repository, or isn't
// tracked), so no lines are marked.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::ClearBaseText(void)
{
	mbBase = false;
	mszBaseText.Clear();
	mszBaseHead.Clear();
	muiBarChanges = MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// True if the change bars aren't of the text as it is now.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::NeedsChangeBars(void)
{
	return muiBarChanges != mpEditor->GetChangeCount();
}


//////////////////////////////////////////////////////////////////////////
//
// The gutter marks pcChanges (of the text now from the base text), or
// nothing if it is NULL, in IntelliJ IDEA's colours.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SetChangeBars(CGitLineChanges* pcChanges)
{
	size			i;
	SGitLineChange*	psChange;
	ARGB32			sColour;

	mpCodeEditor->ClearChangeBars();
	if (pcChanges)
	{
		for (i = 0; i < pcChanges->NumChanges(); i++)
		{
			psChange = pcChanges->GetChange(i);
			switch (psChange->eType)
			{
			case GLC_Added:
				sColour = IDE_CHANGE_ADDED;
				break;
			case GLC_Removed:
				sColour = IDE_CHANGE_REMOVED;
				break;
			default:
				sColour = IDE_CHANGE_MODIFIED;
				break;
			}
			mpCodeEditor->AddChangeBar(psChange->uiStartLine, psChange->uiEndLine, sColour);
		}
	}
	muiBarChanges = mpEditor->GetChangeCount();
}


bool CIDEEditorTab::HasBaseText(void) { return mbBase; }
CChars* CIDEEditorTab::GetBaseText(void) { return &mszBaseText; }
char* CIDEEditorTab::GetBaseHead(void) { return mszBaseHead.Text(); }
void CIDEEditorTab::SetGitState(EGitFileState eState) { meGitState = eState; }
EGitFileState CIDEEditorTab::GetGitState(void) { return meGitState; }
//////////////////////////////////////////////////////////////////////////
//
// Writes the text to the file.  In a file whose lines ended with carriage
// returns and line feeds, line feeds on their own (typed lines) get a
// carriage return.  False if the file couldn't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::SaveFile(void)
{
	CTextFile	cFile;
	CChars		szText;
	char*		szSource;
	size		i;
	bool		bResult;

	szSource = mpEditor->GetText();
	szText.Init();
	if (mbCarriageReturns)
	{
		for (i = 0; szSource[i] != '\0'; i++)
		{
			if ((szSource[i] == '\n') && ((i == 0) || (szSource[i - 1] != '\r')))
			{
				szText.Append('\r');
			}
			szText.Append(szSource[i]);
		}
	}
	else
	{
		szText.Append(szSource);
	}

	cFile.Init(szText.Text());
	bResult = cFile.Write(mszFileName.Text());
	cFile.Kill();
	szText.Kill();
	if (bResult)
	{
		muiSavedUndo = mpEditor->GetUndoId();
		ShowModified();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file's text was changed by something else: the tab shows szText,
// the file as it is now, unmodified.  It is an edit, so the text is
// highlighted again and the lines' code and change bars are worked out
// again.  The caret stays on its line if the file still has it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Reload(const char* szText)
{
	int		iLine;
	int		iColumn;

	//Pasted over the whole text: line ends become line feeds, as they are typed.  SaveFile puts carriage returns back.
	GetCaretPlace(&iLine, &iColumn);
	mpEditor->SelectAll();
	if (szText[0] == '\0')
	{
		mpEditor->Delete();
	}
	else
	{
		mpEditor->Paste((char*)szText);
	}
	mbCarriageReturns = UsesIDECarriageReturns(szText);

	if ((size)iLine < mpEditor->NumLines())
	{
		mpEditor->SetCaretPosition(FindPosition(iLine, iColumn));
	}
	muiSavedUndo = mpEditor->GetUndoId();
	ShowModified();
}


//////////////////////////////////////////////////////////////////////////
//
// True if the text has changed since the file was opened or saved, and
// hasn't been undone back to how it was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::IsModified(void)
{
	return mpEditor->GetUndoId() != muiSavedUndo;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab's name ends with a '*' if the text is modified.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::ShowModified(void)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	bool		bModified;

	bModified = IsModified();
	if ((bModified == mbShownModified) || mpDockPane.IsNull())
	{
		return;
	}
	szName.Init();
	cFileUtil.GetFileName(&szName, mszFileName.Text());
	if (bModified)
	{
		szName.Append('*');
	}
	mpDockPane->SetTitle(szName.Text());
	szName.Kill();
	mbShownModified = bModified;
}


//////////////////////////////////////////////////////////////////////////
//
// True if szText's first line ends with a carriage return and line feed.
//
//////////////////////////////////////////////////////////////////////////
bool UsesIDECarriageReturns(const char* szText)
{
	const char*	szLineFeed;

	szLineFeed = strchr(szText, '\n');
	return (szLineFeed != NULL) && (szLineFeed > szText) && (szLineFeed[-1] == '\r');
}


bool CIDEEditorTab::UsesCarriageReturns(void) { return mbCarriageReturns; }
void CIDEEditorTab::SetDockPane(Ptr<CDockPane> pPane) { mpDockPane = pPane; }
Ptr<CLineZones> CIDEEditorTab::GetZones(void) { return mpZones; }
Ptr<CTextArea> CIDEEditorTab::GetEditor(void) { return mpEditor; }
Ptr<CCodeEditor> CIDEEditorTab::GetCodeEditor(void) { return mpCodeEditor; }
Ptr<CScrollPane> CIDEEditorTab::GetPane(void) { return mpPane; }
Ptr<CDockPane> CIDEEditorTab::GetDockPane(void) { return mpDockPane; }
char* CIDEEditorTab::GetFileName(void) { return mszFileName.Text(); }
size CIDEEditorTab::GetProject(void) { return muiProject; }
void CIDEEditorTab::SetProject(size uiProject) { muiProject = uiProject; }
bool CIDEEditorTab::IsEdited(void) { return mbEdited; }


//////////////////////////////////////////////////////////////////////////
//
// Notes whether the text has changed since it was last looked at: it is
// to be highlighted and indexed again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SeeChanges(int64 iNow)
{
	size	uiChanges;

	uiChanges = mpEditor->GetChangeCount();
	if (uiChanges != muiSeenChanges)
	{
		muiSeenChanges = uiChanges;
		miChangedTime = iNow;
		mbRecolour = true;
		mbEdited = true;
		mbIndexStale = true;
	}
}


