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
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEEditorTab.h"


#define IDE_CARET_LINES_ABOVE	4		//How many lines show above a line jumped to.
#define IDE_RECOLOUR_DELAY		400		//Milliseconds after the last edit before the text is highlighted again.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEGutterListener::Init(CIDEEditorTab* pcTab)
{
	mpcTab = pcTab;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEGutterListener::GutterClicked(CUnknown* pcSource, void* pvContext)
{
	mpcTab->ToggleListing(*(size*)pvContext);
}


//////////////////////////////////////////////////////////////////////////
//
// The file's text (szText, already read) in a code editor of tiles.  A
// line's code is shown with pTitleFont over it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pTitleFont, Ptr<CFontFamily> pFamily, ARGB32 sColour, const char* szFileName, size uiProject, const char* szText)
{
	PreInit();

	mpWindow = pWindow;
	mpTheme = pTheme;
	mpTitleFont = pTitleFont;
	mpFamily = pFamily;
	mcGutterListener.Init(this);
	mpcListings = NULL;
	mbListingsOutOfDate = false;
	maZones.Init();
	mpCodeEditor = OMalloc<CCodeEditor>(pWindow, pFamily, szText);
	mpCodeEditor->SetAlignment(CSH_Left, CSV_Top);
	mpCodeEditor->AddListener(&mcGutterListener);
	mpEditor = mpCodeEditor->GetTextArea();
	mpEditor->SetTiledText(true);
	mpEditor->SetColour(sColour);
	mpPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpPane->SetContent(mpCodeEditor);
	mpPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mszFileName.Init(szFileName);
	muiProject = uiProject;
	muiSeenChanges = mpEditor->GetChangeCount();
	miChangedTime = 0;
	mbRecolour = false;
	mbEdited = false;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Class(void)
{
	M_Pointer(mpWindow);
	M_Pointer(mpTheme);
	M_Pointer(mpTitleFont);
	M_Pointer(mpFamily);
	M_Pointer(mpCodeEditor);
	M_Pointer(mpEditor);
	M_Pointer(mpPane);
	M_Pointer(mpTab);
	U_String(mszFileName);
	U_Data(size, muiProject);
	U_Data(size, muiSeenChanges);
	U_Data(int64, miChangedTime);
	U_Bool(mbRecolour);
	U_Bool(mbEdited);
	U_Unknown(CIDEGutterListener, mcGutterListener);
	U_Pointer(mpcListings);
	U_Bool(mbListingsOutOfDate);
	M_Embedded(maZones);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::Free(void)
{
	mcGutterListener.RemoveFromListenerCalls();
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
	SCPPHighlight*	psHighlight;
	size			i;
	size			uiStart;
	int				iPass;

	mpEditor->ClearSpans();
	for (iPass = 0; iPass < 2; iPass++)
	{
		for (i = 0; i < pacHighlights->NumElements(); i++)
		{
			psHighlight = pacHighlights->Get(i);
			if (((psHighlight->uiKind == CPPH_Inactive) == (iPass == 1)) && (psHighlight->iLine >= 0) && ((size)psHighlight->iLine < mpEditor->NumLines()))
			{
				uiStart = mpEditor->GetLineStart((size)psHighlight->iLine) + (size)psHighlight->iColumn;
				mpEditor->AddSpan(uiStart, (size)psHighlight->iLength, GetHighlightColour((ECPPHighlight)psHighlight->uiKind));
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
// Puts the caret at a line and column (0 based, the column in bytes)
// and scrolls the editor to show it.  False if the file hasn't that line.
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
	mpEditor->SetCaretPosition(mpEditor->GetLineStart((size)iLine) + (size)iColumn);
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
// True once the text has been left alone for a moment after an edit, so
// it should be highlighted again (from the editor's text).  Otherwise
// piWait is how many milliseconds until it might be, or -1 for never.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::NeedsRecolour(int64 iNow, int64* piWait)
{
	size	uiChanges;

	*piWait = -1;
	uiChanges = mpEditor->GetChangeCount();
	if (uiChanges != muiSeenChanges)
	{
		muiSeenChanges = uiChanges;
		miChangedTime = iNow;
		mbRecolour = true;
		mbEdited = true;
		ListingsOutOfDate();
	}
	if (!mbRecolour || (muiProject == MAX_SIZE))
	{
		return false;
	}
	if (iNow - miChangedTime < IDE_RECOLOUR_DELAY)
	{
		*piWait = IDE_RECOLOUR_DELAY - (iNow - miChangedTime);
		return false;
	}
	mbRecolour = false;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The listings of the last HCP65 build (NULL for none), up to date with
// the text as it is: each line with code is marked, and each zone shows
// its line's code, or goes if its line hasn't any now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::SetListings(CCPP65ListingIndex* pcListings)
{
	CArrayCPP65ListingLines		auiLines;
	CArrayCPP65ListingPlaces	asPlaces;
	Ptr<CIDEListingZone>		pZone;
	size						i;
	size						uiLine;

	mpcListings = pcListings;
	mbListingsOutOfDate = false;
	mpCodeEditor->ClearLineMarks();

	asPlaces.Init();
	i = maZones.Size();
	while (i > 0)
	{
		i--;
		pZone = maZones.Get(i);
		uiLine = mpCodeEditor->GetLineOf(pZone->GetComponent());
		if (mpcListings && (uiLine != MAX_SIZE))
		{
			mpcListings->GetPlaces(mszFileName.Text(), uiLine, &asPlaces);
		}
		if ((uiLine == MAX_SIZE) || !mpcListings || (asPlaces.NumElements() == 0))
		{
			RemoveZone(pZone);
		}
		else
		{
			pZone->SetPlaces(mpcListings, &asPlaces);
		}
	}
	asPlaces.Kill();

	if (mpcListings)
	{
		auiLines.Init();
		mpcListings->GetLines(mszFileName.Text(), &auiLines);
		for (i = 0; i < auiLines.NumElements(); i++)
		{
			MarkLine(*auiLines.Get(i));
		}
		auiLines.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Shows line uiLine's code under it, or hides it if it is shown.  False if
// there is no code to show: no listings, out of date ones (the lines have
// moved), or none for the line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEditorTab::ToggleListing(size uiLine)
{
	CArrayCPP65ListingPlaces	asPlaces;
	Ptr<CIDEListingZone>		pZone;

	pZone = FindZone(uiLine);
	if (pZone.IsNotNull())
	{
		RemoveZone(pZone);
		MarkLine(uiLine);
		return true;
	}
	if (!mpcListings || mbListingsOutOfDate)
	{
		return false;
	}

	asPlaces.Init();
	mpcListings->GetPlaces(mszFileName.Text(), uiLine, &asPlaces);
	if (asPlaces.NumElements() > 0)
	{
		pZone = OMalloc<CIDEListingZone>(mpWindow, mpTheme, mpTitleFont, mpFamily);
		pZone->SetPlaces(mpcListings, &asPlaces);
		maZones.Add(pZone);
		mpCodeEditor->AddLineComponent(uiLine, pZone->GetComponent());
		MarkLine(uiLine);
	}
	asPlaces.Kill();
	return pZone.IsNotNull();
}


//////////////////////////////////////////////////////////////////////////
//
// The zone under line uiLine, NULL if it has none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEListingZone> CIDEEditorTab::FindZone(size uiLine)
{
	size	i;

	for (i = 0; i < maZones.Size(); i++)
	{
		if (mpCodeEditor->GetLineOf(maZones.Get(i)->GetComponent()) == uiLine)
		{
			return maZones.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The text changed since the listings were given: their lines may not be
// the text's.  The marks go and the zones say they are out of date.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::ListingsOutOfDate(void)
{
	size	i;

	if (!mpcListings || mbListingsOutOfDate)
	{
		return;
	}
	mbListingsOutOfDate = true;
	mpCodeEditor->ClearLineMarks();
	for (i = 0; i < maZones.Size(); i++)
	{
		maZones.Get(i)->SetOutOfDate(true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::RemoveZone(Ptr<CIDEListingZone> pZone)
{
	mpCodeEditor->RemoveLineComponent(pZone->GetComponent());
	maZones.Remove(pZone);
}


//////////////////////////////////////////////////////////////////////////
//
// Line uiLine's mark: open if its code is shown, otherwise that it has
// code, if the listings are up to date and it has.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorTab::MarkLine(size uiLine)
{
	CArrayCPP65ListingPlaces	asPlaces;
	char						cMark;

	cMark = '\0';
	if (FindZone(uiLine).IsNotNull())
	{
		cMark = IDE_LISTING_OPEN_MARK;
	}
	else if (mpcListings && !mbListingsOutOfDate)
	{
		asPlaces.Init();
		mpcListings->GetPlaces(mszFileName.Text(), uiLine, &asPlaces);
		cMark = (asPlaces.NumElements() > 0) ? IDE_LISTING_MARK : '\0';
		asPlaces.Kill();
	}
	mpCodeEditor->SetLineMark(uiLine, cMark);
}


void CIDEEditorTab::SetTab(Ptr<CTabComponent> pTab) { mpTab = pTab; }
size CIDEEditorTab::NumZones(void) { return maZones.Size(); }
Ptr<CIDEListingZone> CIDEEditorTab::GetZone(size uiIndex) { return maZones.Get(uiIndex); }
bool CIDEEditorTab::IsListingsOutOfDate(void) { return mbListingsOutOfDate; }
Ptr<CTextArea> CIDEEditorTab::GetEditor(void) { return mpEditor; }
Ptr<CCodeEditor> CIDEEditorTab::GetCodeEditor(void) { return mpCodeEditor; }
Ptr<CScrollPane> CIDEEditorTab::GetPane(void) { return mpPane; }
Ptr<CTabComponent> CIDEEditorTab::GetTab(void) { return mpTab; }
char* CIDEEditorTab::GetFileName(void) { return mszFileName.Text(); }
size CIDEEditorTab::GetProject(void) { return muiProject; }
bool CIDEEditorTab::IsEdited(void) { return mbEdited; }

