/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLWriter.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/FillContainer.h"
#include "IDEContent.h"
#include "IDEFind.h"


#define IDE_FIND_FIELD_COLUMNS	32
#define IDE_FIND_GAP			8


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Init(CIDEContent* pcContent, CIDEEditorTab* pcTab)
{
	PreInit();
	mpcContent = pcContent;
	mpcTab = pcTab;
	mcSearch.Init("", false, false, false);
	masMatches.Init();
	mszCount.Init();
	muiCurrent = IDE_FIND_NONE;
	muiLayer = IDE_FIND_NONE;
	muiSearchedChanges = MAX_SIZE;
	muiSearchedLength = 0;
	muiAnchor = 0;
	msScope.uiStart = 0;
	msScope.uiEnd = 0;
	mbOpen = false;
	mbReplacing = false;
	mbSetting = false;
	CreateBar();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Class(void)
{
	U_Pointer(mpcContent);
	U_Pointer(mpcTab);
	M_Pointer(mpBar);
	M_Pointer(mpReplaceRow);
	M_Pointer(mpFindField);
	M_Pointer(mpReplaceField);
	M_Pointer(mpCount);
	M_Pointer(mpMatchCase);
	M_Pointer(mpWords);
	M_Pointer(mpRegex);
	M_Pointer(mpInSelection);
	M_Pointer(mpPrevious);
	M_Pointer(mpNext);
	M_Pointer(mpClose);
	M_Pointer(mpReplace);
	M_Pointer(mpReplaceAll);
	U_Unknown(CTextSearch, mcSearch);
	U_Unknown(CArrayTextRange, masMatches);
	U_String(mszCount);
	U_Size(muiCurrent);
	U_Size(muiLayer);
	U_Size(muiSearchedChanges);
	U_Size(muiSearchedLength);
	U_Size(muiAnchor);
	U_Data(STextRange, msScope);
	U_Bool(mbOpen);
	U_Bool(mbReplacing);
	U_Bool(mbSetting);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Free(void)
{
	mpFindField->GetChangeEvent()->Remove(this);
	mpMatchCase->GetChangeEvent()->Remove(this);
	mpWords->GetChangeEvent()->Remove(this);
	mpRegex->GetChangeEvent()->Remove(this);
	mpInSelection->GetChangeEvent()->Remove(this);
	mpPrevious->GetClickEvent()->Remove(this);
	mpNext->GetClickEvent()->Remove(this);
	mpClose->GetClickEvent()->Remove(this);
	mpReplace->GetClickEvent()->Remove(this);
	mpReplaceAll->GetClickEvent()->Remove(this);
	mszCount.Kill();
	masMatches.Kill();
	mcSearch.Kill();
}


bool CIDEFindBar::Save(CObjectWriter* pcFile) { return false; }
bool CIDEFindBar::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDEFindBar::CreateButton(const char* szCaption)
{
	Ptr<CButton>	pButton;

	pButton = OMalloc<CButton>(mpcContent->GetWindow(), mpcContent->GetTheme(), BL_Flat, Ptr<CImageCel>(), szCaption);
	pButton->SetAlignment(CSH_Left, CSV_Center);
	return pButton;
}


//////////////////////////////////////////////////////////////////////////
//
// The bar: the field, how many, Previous and Next, the ticks and Close in
// a row; under it, shown to replace, the replacement, Replace and
// Replace All; a line under both.  Enter in the find field is Next and
// Shift+Enter Previous, Enter in the other Replace, and Escape in either
// closes the bar.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::CreateBar(void)
{
	Ptr<CWindow>		pWindow;
	Ptr<CWindowTheme>	pTheme;
	Ptr<CGridContainer>	pFindRow;
	Ptr<CFillContainer>	pGap;
	Ptr<CCanvas>		pLine;
	Ptr<CFillContainer>	pPadding;
	int					iColumn;

	pWindow = mpcContent->GetWindow();
	pTheme = mpcContent->GetTheme();

	mpFindField = OMalloc<CTextField>(pWindow, pTheme, "");
	mpFindField->SetColumns(IDE_FIND_FIELD_COLUMNS);
	mpFindField->SetAlignment(CSH_Left, CSV_Center);
	mpFindField->GetChangeEvent()->Add(this, &CIDEFindBar::FieldChanged);
	mpFindField->AddInputAction(CIC_Enter, this, &CIDEFindBar::NextClicked);
	mpFindField->AddInputAction(CIC_EndEnter, this, &CIDEFindBar::PreviousClicked);
	mpFindField->AddInputAction(CIC_ClearSelection, this, &CIDEFindBar::Close);
	mpCount = OMalloc<CLabel>(pWindow, mpcContent->GetDimFont(), "");
	mpCount->SetAlignment(CSH_Left, CSV_Center);
	mpPrevious = CreateButton("Previous");
	mpPrevious->GetClickEvent()->Add(this, &CIDEFindBar::PreviousClicked);
	mpNext = CreateButton("Next");
	mpNext->GetClickEvent()->Add(this, &CIDEFindBar::NextClicked);
	mpMatchCase = OMalloc<CCheckBox>(pWindow, pTheme, "Match Case");
	mpMatchCase->GetChangeEvent()->Add(this, &CIDEFindBar::TickChanged);
	mpWords = OMalloc<CCheckBox>(pWindow, pTheme, "Words");
	mpWords->GetChangeEvent()->Add(this, &CIDEFindBar::TickChanged);
	mpRegex = OMalloc<CCheckBox>(pWindow, pTheme, "Regex");
	mpRegex->GetChangeEvent()->Add(this, &CIDEFindBar::TickChanged);
	mpInSelection = OMalloc<CCheckBox>(pWindow, pTheme, "In Selection");
	mpInSelection->GetChangeEvent()->Add(this, &CIDEFindBar::ScopeChanged);
	mpClose = CreateButton("Close");
	mpClose->GetClickEvent()->Add(this, &CIDEFindBar::Close);

	pFindRow = OMalloc<CGridContainer>(pWindow);
	pFindRow->SetAlignment(CSH_Stretch, CSV_Center);
	iColumn = 0;
	pFindRow->AddComponent(mpFindField, iColumn++, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	pFindRow->AddComponent(pGap, iColumn++, 0);
	pFindRow->AddComponent(mpPrevious, iColumn++, 0);
	pFindRow->AddComponent(mpNext, iColumn++, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	pFindRow->AddComponent(pGap, iColumn++, 0);
	mpMatchCase->SetAlignment(CSH_Left, CSV_Center);
	pFindRow->AddComponent(mpMatchCase, iColumn++, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	pFindRow->AddComponent(pGap, iColumn++, 0);
	mpWords->SetAlignment(CSH_Left, CSV_Center);
	pFindRow->AddComponent(mpWords, iColumn++, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	pFindRow->AddComponent(pGap, iColumn++, 0);
	mpRegex->SetAlignment(CSH_Left, CSV_Center);
	pFindRow->AddComponent(mpRegex, iColumn++, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	pFindRow->AddComponent(pGap, iColumn++, 0);
	mpInSelection->SetAlignment(CSH_Left, CSV_Center);
	pFindRow->AddComponent(mpInSelection, iColumn++, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	pFindRow->AddComponent(pGap, iColumn++, 0);
	pFindRow->AddComponent(mpCount, iColumn++, 0, 1, 1, 1, 0);
	pFindRow->AddComponent(mpClose, iColumn++, 0);

	mpReplaceField = OMalloc<CTextField>(pWindow, pTheme, "");
	mpReplaceField->SetColumns(IDE_FIND_FIELD_COLUMNS);
	mpReplaceField->SetAlignment(CSH_Left, CSV_Center);
	mpReplaceField->AddInputAction(CIC_Enter, this, &CIDEFindBar::ReplaceClicked);
	mpReplaceField->AddInputAction(CIC_ClearSelection, this, &CIDEFindBar::Close);
	mpReplace = CreateButton("Replace");
	mpReplace->GetClickEvent()->Add(this, &CIDEFindBar::ReplaceClicked);
	mpReplaceAll = CreateButton("Replace All");
	mpReplaceAll->GetClickEvent()->Add(this, &CIDEFindBar::ReplaceAllClicked);

	mpReplaceRow = OMalloc<CGridContainer>(pWindow);
	mpReplaceRow->SetAlignment(CSH_Stretch, CSV_Center);
	mpReplaceRow->AddComponent(mpReplaceField, 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_FIND_GAP, 1);
	mpReplaceRow->AddComponent(pGap, 1, 0);
	mpReplaceRow->AddComponent(mpReplace, 2, 0);
	mpReplaceRow->AddComponent(mpReplaceAll, 3, 0);
	pPadding = OMalloc<CFillContainer>(pWindow);
	mpReplaceRow->AddComponent(pPadding, 4, 0, 1, 1, 1, 0);

	pLine = OMalloc<CCanvas>(pWindow, OMalloc<CClearCanvasDraw>(pTheme->GetColour(TC_Separator)));
	pLine->SetDesiredSize(-1, 1);
	pLine->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpBar = OMalloc<CGridContainer>(pWindow);
	mpBar->SetAlignment(CSH_Stretch, CSV_Top);
	mpBar->SetInsets(2, 4, 0, 4);
	mpBar->AddComponent(pFindRow, 0, 0, 1, 1, 1, 0);
	mpBar->AddComponent(pLine, 0, 2, 1, 1, 1, 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The ticks every bar shares, as they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::ShowOptions(void)
{
	mbSetting = true;
	mpMatchCase->SetTicked(mpcContent->GetFind()->IsMatchCase());
	mpWords->SetTicked(mpcContent->GetFind()->IsWords());
	mpRegex->SetTicked(mpcContent->GetFind()->IsRegex());
	mbSetting = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The bar is shown over the code, to find or to replace, with the keys
// in its field.  Its field is the selected text, if that is on one line,
// or the word the caret is in, all of it selected so typing replaces it;
// with neither it keeps what it had.  A selection of more than a line is
// what In Selection searches, and ticks it.  The first match from where
// the selection started is selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Open(bool bReplace)
{
	Ptr<CTextArea>	pEditor;
	const char*		szText;
	CChars			szFind;
	size			uiStart;
	size			uiEnd;
	bool			bSelected;
	bool			bLines;

	pEditor = mpcTab->GetEditor();
	szText = pEditor->GetText();
	bSelected = pEditor->GetSelection(&uiStart, &uiEnd);
	if (!bSelected)
	{
		uiStart = pEditor->GetCaretPosition();
		uiEnd = uiStart;
	}
	bLines = bSelected && (memchr(&szText[uiStart], '\n', uiEnd - uiStart) != NULL);
	msScope.uiStart = uiStart;
	msScope.uiEnd = uiEnd;
	muiAnchor = uiStart;

	szFind.Init();
	if (bSelected && !bLines)
	{
		szFind.Append(&szText[uiStart], uiEnd - uiStart);
	}
	else if (!bSelected && FindIDEWordAt(szText, uiStart, &uiStart, &uiEnd))
	{
		szFind.Append(&szText[uiStart], uiEnd - uiStart);
		muiAnchor = uiStart;
	}

	mbSetting = true;
	if (!szFind.Empty())
	{
		mpFindField->SetText(szFind.Text());
	}
	mpInSelection->SetTicked(bLines);
	mbSetting = false;
	szFind.Kill();
	ShowOptions();

	if (bReplace != mbReplacing)
	{
		if (bReplace)
		{
			mpBar->AddComponent(mpReplaceRow, 0, 1, 1, 1, 1, 0);
		}
		else
		{
			mpBar->RemoveComponent(mpReplaceRow);
		}
		mbReplacing = bReplace;
	}
	if (!mbOpen)
	{
		mpcTab->SetTopBar(mpBar);
		mbOpen = true;
	}
	mpFindField->SelectAll();
	mpcContent->GetWindow()->GetFocus()->SetFocus(mpFindField);
	Search(true);
}


//////////////////////////////////////////////////////////////////////////
//
// The bar goes, its highlights and marks with it; the editor has the
// keys, the match that was selected still selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Close(void)
{
	if (!mbOpen)
	{
		return;
	}
	mbOpen = false;
	ClearMatches();
	mpcTab->SetTopBar(Ptr<CComponent>());
	mpcContent->GetWindow()->GetFocus()->SetFocus(mpcTab->GetEditor());
}


//////////////////////////////////////////////////////////////////////////
//
// No matches: none highlighted or marked.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::ClearMatches(void)
{
	masMatches.ReInit();
	muiCurrent = IDE_FIND_NONE;
	if (muiLayer != IDE_FIND_NONE)
	{
		mpcTab->GetEditor()->ClearHighlightRanges(muiLayer);
	}
	mpcTab->GetPane()->GetScrollBar(SBO_Vertical)->ClearMarks(IDE_FIND_MARKS);
}


//////////////////////////////////////////////////////////////////////////
//
// At each tick while it is open: if the file was edited since the search
// the matches are of the text before, so it is searched again.  The
// selection is left alone: whoever edited is working there.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Update(void)
{
	Ptr<CTextArea>	pEditor;
	size			uiLength;

	if (!mbOpen || (mpcTab->GetEditor()->GetChangeCount() == muiSearchedChanges))
	{
		return;
	}
	pEditor = mpcTab->GetEditor();

	//What In Selection searches grows or shrinks with the text, taken to be edited inside it.
	uiLength = strlen(pEditor->GetText());
	if (uiLength >= muiSearchedLength)
	{
		msScope.uiEnd += uiLength - muiSearchedLength;
	}
	else
	{
		msScope.uiEnd = (msScope.uiEnd > muiSearchedLength - uiLength) ? msScope.uiEnd - (muiSearchedLength - uiLength) : 0;
	}
	if (msScope.uiEnd < msScope.uiStart)
	{
		msScope.uiEnd = msScope.uiStart;
	}
	muiAnchor = pEditor->GetCaretPosition();
	Search(false);
}


//////////////////////////////////////////////////////////////////////////
//
// The field's text is looked for in the editor's text as it is, all of
// it or what was selected (In Selection), with the ticks' options: the
// matches are highlighted and marked, the first at or after the anchor
// (or the first of all, past the last) is the current one, and the bar
// says which of how many it is, or why there are none.  With bSelect the
// current one is selected and shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Search(bool bSelect)
{
	Ptr<CTextArea>	pEditor;
	STextRange*		psMatch;
	const char*		szText;
	size			uiLength;
	size			uiFrom;
	size			uiTo;
	size			uiPosition;
	size			i;
	bool			bScope;

	pEditor = mpcTab->GetEditor();
	szText = pEditor->GetText();
	uiLength = strlen(szText);
	muiSearchedChanges = pEditor->GetChangeCount();
	muiSearchedLength = uiLength;

	mcSearch.Kill();
	mcSearch.Init(mpFindField->GetText(), mpMatchCase->IsTicked(), mpWords->IsTicked(), mpRegex->IsTicked());
	mpcContent->GetFind()->SetLast(mpFindField->GetText());

	bScope = mpInSelection->IsTicked() && (msScope.uiEnd > msScope.uiStart);
	uiFrom = bScope ? msScope.uiStart : 0;
	uiTo = (bScope && (msScope.uiEnd < uiLength)) ? msScope.uiEnd : uiLength;
	masMatches.ReInit();
	muiCurrent = IDE_FIND_NONE;
	if (uiFrom <= uiTo)
	{
		for (uiPosition = mcSearch.Find(szText, uiTo, uiFrom); (uiPosition != TEXT_SEARCH_NOT_FOUND) && (masMatches.NumElements() < IDE_FIND_MAX_MATCHES); uiPosition = mcSearch.Find(szText, uiTo, uiPosition + mcSearch.GetFoundLength()))
		{
			psMatch = masMatches.Add();
			psMatch->uiStart = uiPosition;
			psMatch->uiEnd = uiPosition + mcSearch.GetFoundLength();
		}
	}

	for (i = 0; i < masMatches.NumElements(); i++)
	{
		if (masMatches.Get(i)->uiStart >= muiAnchor)
		{
			muiCurrent = i;
			break;
		}
	}
	if ((muiCurrent == IDE_FIND_NONE) && (masMatches.NumElements() > 0))
	{
		muiCurrent = 0;
	}

	ShowMatches();
	if (bSelect)
	{
		SelectCurrent();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The matches as they are: highlighted in the editor (its layer is made
// the first time), marked on its scroll bar, a mark for each line with
// one, and counted in the bar.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::ShowMatches(void)
{
	Ptr<CTextArea>				pEditor;
	CArrayTemplate<SInt32Vec2>	asMarks;
	CInt32Rect					cLine;
	size						uiLine;
	size						uiLastLine;
	size						i;

	pEditor = mpcTab->GetEditor();
	if (muiLayer == IDE_FIND_NONE)
	{
		muiLayer = pEditor->AddHighlightLayer(mpcContent->GetTheme()->GetColour(TC_SearchMatch), mpcContent->GetTheme()->GetColour(TC_SearchMatch));
	}
	if (masMatches.NumElements() > 0)
	{
		pEditor->SetHighlightRanges(muiLayer, masMatches.Get(0), masMatches.NumElements(), muiCurrent);
	}
	else
	{
		pEditor->ClearHighlightRanges(muiLayer);
	}

	asMarks.Init();
	uiLastLine = MAX_SIZE;
	for (i = 0; i < masMatches.NumElements(); i++)
	{
		uiLine = pEditor->FindLine(masMatches.Get(i)->uiStart);
		if ((uiLine != uiLastLine) && pEditor->GetLineBounds(uiLine, &cLine))
		{
			asMarks.Add()->Init(cLine.GetTop(), cLine.GetHeight());
		}
		uiLastLine = uiLine;
	}
	mpcTab->GetPane()->GetScrollBar(SBO_Vertical)->SetMarks(IDE_FIND_MARKS, asMarks.NumElements() ? asMarks.Get(0) : NULL, asMarks.NumElements(), mpcContent->GetTheme()->GetColour(TC_SearchMatch));
	asMarks.Kill();

	mszCount.Clear();
	if (!mcSearch.IsValid())
	{
		mszCount.Append("Not a regular expression: ");
		mszCount.Append(mcSearch.GetError());
	}
	else if (mcSearch.GaveUp())
	{
		mszCount.Append("The expression takes too long to match here");
	}
	else if (mcSearch.IsEmpty())
	{
	}
	else if (masMatches.NumElements() == 0)
	{
		mszCount.Append("0 results");
	}
	else
	{
		if (muiCurrent != IDE_FIND_NONE)
		{
			mszCount.Append((uint32)(muiCurrent + 1));
			mszCount.Append(" of ");
		}
		mszCount.Append((uint32)masMatches.NumElements());
		if (masMatches.NumElements() >= IDE_FIND_MAX_MATCHES)
		{
			mszCount.Append('+');
		}
		if (muiCurrent == IDE_FIND_NONE)
		{
			mszCount.Append(" results");
		}
	}
	mpCount->SetText(mszCount.Text());
	mpcContent->GetWindow()->InvalidateLayout();
}


//////////////////////////////////////////////////////////////////////////
//
// The current match selected in the editor and scrolled into view; the
// keys stay where they are.  Put there by the bar, it isn't a move to go
// back from.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::SelectCurrent(void)
{
	Ptr<CTextArea>	pEditor;
	STextRange*		psMatch;

	if (muiCurrent == IDE_FIND_NONE)
	{
		return;
	}
	pEditor = mpcTab->GetEditor();
	psMatch = masMatches.Get(muiCurrent);
	pEditor->Select(psMatch->uiStart, psMatch->uiEnd);
	pEditor->ShowCaret();
	pEditor->SetCurrentHighlight(muiLayer, muiCurrent);
	mpcContent->GetNavigationHistory()->CaretPlaced();
}


//////////////////////////////////////////////////////////////////////////
//
// The next match, or the previous, round the ends, is the current one:
// selected, shown and counted.  False if there are none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindBar::Next(void)
{
	if (masMatches.NumElements() == 0)
	{
		return false;
	}
	muiCurrent = ((muiCurrent == IDE_FIND_NONE) || (muiCurrent + 1 >= masMatches.NumElements())) ? 0 : muiCurrent + 1;
	muiAnchor = masMatches.Get(muiCurrent)->uiStart;
	ShowMatches();
	SelectCurrent();
	return true;
}


bool CIDEFindBar::Previous(void)
{
	if (masMatches.NumElements() == 0)
	{
		return false;
	}
	muiCurrent = ((muiCurrent == IDE_FIND_NONE) || (muiCurrent == 0)) ? masMatches.NumElements() - 1 : muiCurrent - 1;
	muiAnchor = masMatches.Get(muiCurrent)->uiStart;
	ShowMatches();
	SelectCurrent();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What match uiMatch is replaced with: the replace field's text, or for
// a regular expression what its groups make of it.  False, the status
// line saying why, if the replacement names a group the expression hasn't.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindBar::GetReplacement(size uiMatch, CChars* pszDest)
{
	const char*	szText;
	STextRange*	psMatch;
	CChars		szStatus;

	if (!mcSearch.CheckReplacement(mpReplaceField->GetText()))
	{
		szStatus.Init("Nothing was replaced: ");
		szStatus.Append(mcSearch.GetError());
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		return false;
	}
	szText = mpcTab->GetEditor()->GetText();
	psMatch = masMatches.Get(uiMatch);

	//Found again where it is, so the search has this match's groups.
	if (mcSearch.Find(szText, psMatch->uiEnd, psMatch->uiStart) != psMatch->uiStart)
	{
		return false;
	}
	mcSearch.AppendReplacement(szText, mpReplaceField->GetText(), pszDest);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Replace: the current match becomes the replacement, and the next match
// after it is the current one.  False if there is no current match, the
// file can't be edited or the replacement is wrong.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFindBar::ReplaceCurrent(void)
{
	Ptr<CTextArea>	pEditor;
	STextRange		sMatch;
	CChars			szReplacement;
	bool			bReplaced;

	if (!mbOpen || (muiCurrent == IDE_FIND_NONE))
	{
		return false;
	}
	pEditor = mpcTab->GetEditor();
	if (pEditor->GetChangeCount() != muiSearchedChanges)
	{
		//Edited since: the matches aren't where they were.
		Search(true);
		return false;
	}
	sMatch = *masMatches.Get(muiCurrent);
	szReplacement.Init();
	bReplaced = GetReplacement(muiCurrent, &szReplacement) && pEditor->ReplaceRanges(&sMatch, 1, szReplacement.Text());
	if (bReplaced)
	{
		if (mpInSelection->IsTicked() && (msScope.uiEnd >= sMatch.uiEnd))
		{
			msScope.uiEnd = msScope.uiEnd - (sMatch.uiEnd - sMatch.uiStart) + szReplacement.Length();
		}
		muiAnchor = sMatch.uiStart + szReplacement.Length();
		Search(true);
	}
	szReplacement.Kill();
	return bReplaced;
}


//////////////////////////////////////////////////////////////////////////
//
// Replace All: every match becomes its replacement, as one edit, and the
// status line says how many.  The number replaced; 0 if there are none,
// the file can't be edited or the replacement is wrong.
//
//////////////////////////////////////////////////////////////////////////
size CIDEFindBar::ReplaceAll(void)
{
	Ptr<CTextArea>	pEditor;
	CArrayChars		aszTexts;
	CChars			szReplacement;
	CChars			szStatus;
	size			uiReplaced;
	size			uiBefore;
	size			i;
	bool			bOK;

	if (!mbOpen)
	{
		return 0;
	}
	pEditor = mpcTab->GetEditor();
	if (pEditor->GetChangeCount() != muiSearchedChanges)
	{
		Search(false);
	}
	if (masMatches.NumElements() == 0)
	{
		return 0;
	}

	aszTexts.Init();
	bOK = true;
	for (i = 0; bOK && (i < masMatches.NumElements()); i++)
	{
		szReplacement.Init();
		bOK = GetReplacement(i, &szReplacement);
		aszTexts.Add(szReplacement.Text());
		szReplacement.Kill();
	}
	uiReplaced = 0;
	uiBefore = strlen(pEditor->GetText());
	if (bOK && pEditor->ReplaceRanges(masMatches.Get(0), masMatches.NumElements(), &aszTexts))
	{
		uiReplaced = masMatches.NumElements();
	}
	aszTexts.Kill();

	if (uiReplaced > 0)
	{
		szStatus.Init("Replaced ");
		szStatus.Append((uint32)uiReplaced);
		szStatus.Append((uiReplaced == 1) ? " occurrence." : " occurrences.");
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		if (mpInSelection->IsTicked())
		{
			msScope.uiEnd = msScope.uiEnd + strlen(pEditor->GetText()) - uiBefore;
		}
		muiAnchor = pEditor->GetCaretPosition();
		Search(false);
	}
	return uiReplaced;
}


//////////////////////////////////////////////////////////////////////////
//
// The field typed in: searched as it is, the first match from where the
// search began selected.  A tick changed: it is every bar's from now, and
// the same.  In Selection changed: searched again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::FieldChanged(void)
{
	if (!mbSetting && mbOpen)
	{
		Search(true);
	}
}


void CIDEFindBar::TickChanged(void)
{
	if (!mbSetting)
	{
		mpcContent->GetFind()->SetOptions(mpMatchCase->IsTicked(), mpWords->IsTicked(), mpRegex->IsTicked());
		if (mbOpen)
		{
			Search(true);
		}
	}
}


void CIDEFindBar::ScopeChanged(void)
{
	if (!mbSetting && mbOpen)
	{
		Search(true);
	}
}


void CIDEFindBar::NextClicked(void) { Next(); }
void CIDEFindBar::PreviousClicked(void) { Previous(); }
void CIDEFindBar::ReplaceClicked(void) { ReplaceCurrent(); }
void CIDEFindBar::ReplaceAllClicked(void) { ReplaceAll(); }


//////////////////////////////////////////////////////////////////////////
//
// The field's text set, as if typed: searched.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::SetFindText(const char* szText)
{
	mpFindField->SetText(szText);
}


void CIDEFindBar::SetReplaceText(const char* szText)
{
	mpReplaceField->SetText(szText);
}


//////////////////////////////////////////////////////////////////////////
//
// One of the bar's ticks by name, for checking: case, words, regex or
// selection.  NULL for any other.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CCheckBox> CIDEFindBar::GetTick(const char* szName)
{
	if (strcmp(szName, "case") == 0)
	{
		return mpMatchCase;
	}
	if (strcmp(szName, "words") == 0)
	{
		return mpWords;
	}
	if (strcmp(szName, "regex") == 0)
	{
		return mpRegex;
	}
	if (strcmp(szName, "selection") == 0)
	{
		return mpInSelection;
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Find bar: [text] 2 of 5 case regex" and, replacing,
// " replace [text]"; "Find bar: closed" when it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFindBar::Print(CChars* pszDest)
{
	if (!mbOpen)
	{
		pszDest->Append("Find bar: closed");
		return;
	}
	pszDest->Append("Find bar: [");
	pszDest->Append(mpFindField->GetText());
	pszDest->Append("] ");
	pszDest->Append(mszCount);
	pszDest->Append(mpMatchCase->IsTicked() ? " case" : "");
	pszDest->Append(mpWords->IsTicked() ? " words" : "");
	pszDest->Append(mpRegex->IsTicked() ? " regex" : "");
	pszDest->Append(mpInSelection->IsTicked() ? " selection" : "");
	if (mbReplacing)
	{
		pszDest->Append(" replace [");
		pszDest->Append(mpReplaceField->GetText());
		pszDest->Append("]");
	}
}


bool CIDEFindBar::IsOpen(void) const { return mbOpen; }
bool CIDEFindBar::IsReplacing(void) const { return mbReplacing; }
const char* CIDEFindBar::GetFindText(void) { return mpFindField->GetText(); }
size CIDEFindBar::NumMatches(void) const { return masMatches.NumElements(); }
size CIDEFindBar::GetCurrent(void) const { return muiCurrent; }
Ptr<CTextField> CIDEFindBar::GetFindField(void) const { return mpFindField; }
Ptr<CTextField> CIDEFindBar::GetReplaceField(void) const { return mpReplaceField; }


//////////////////////////////////////////////////////////////////////////
//
// The word (letters, digits and underscores) uiPosition is in or at an
// end of.  False if it is at none.
//
//////////////////////////////////////////////////////////////////////////
bool FindIDEWordAt(const char* szText, size uiPosition, size* puiStart, size* puiEnd)
{
	size	uiStart;
	size	uiEnd;
	char	c;

	uiStart = uiPosition;
	while (uiStart > 0)
	{
		c = szText[uiStart - 1];
		if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_')))
		{
			break;
		}
		uiStart--;
	}
	uiEnd = uiPosition;
	for (;;)
	{
		c = szText[uiEnd];
		if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_')))
		{
			break;
		}
		uiEnd++;
	}
	*puiStart = uiStart;
	*puiEnd = uiEnd;
	return uiEnd > uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszLast.Init();
	mbMatchCase = false;
	mbWords = false;
	mbRegex = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::Class(void)
{
	U_Pointer(mpcContent);
	U_String(mszLast);
	U_Bool(mbMatchCase);
	U_Bool(mbWords);
	U_Bool(mbRegex);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::Free(void)
{
	mszLast.Kill();
}


bool CIDEFind::Save(CObjectWriter* pcFile) { return false; }
bool CIDEFind::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Edit's, with IntelliJ IDEA's keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Edit.Find", "Find...", "Control+F", Ptr<CImageCel>(), this, &CIDEFind::ShowFind)->SetEnabledCheck(this, &CIDEFind::HasEditor);
	pCommands->Add("Edit.Replace", "Replace...", "Control+R", Ptr<CImageCel>(), this, &CIDEFind::ShowReplace)->SetEnabledCheck(this, &CIDEFind::HasEditor);
	pCommands->Add("Edit.FindNext", "Find Next", "F3", Ptr<CImageCel>(), this, &CIDEFind::FindNext)->SetEnabledCheck(this, &CIDEFind::HasEditor);
	pCommands->Add("Edit.FindPrevious", "Find Previous", "Shift+F3", Ptr<CImageCel>(), this, &CIDEFind::FindPrevious)->SetEnabledCheck(this, &CIDEFind::HasEditor);
	pCommands->Add("Edit.FindWordAtCaret", "Find Word at Caret", "Control+F3", Ptr<CImageCel>(), this, &CIDEFind::FindWordAtCaret)->SetEnabledCheck(this, &CIDEFind::HasEditor);
}


void CIDEFind::AddEditItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.Find"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.Replace"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.FindNext"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.FindPrevious"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.FindWordAtCaret"));
}


bool CIDEFind::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// A tab's Find bar, made the first time it is asked for.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEFindBar> CIDEFind::GetBar(Ptr<CIDEEditorTab> pTab)
{
	if (pTab->GetFindBar().IsNull())
	{
		pTab->SetFindBar(OMalloc<CIDEFindBar>(mpcContent, &pTab));
	}
	return pTab->GetFindBar();
}


//////////////////////////////////////////////////////////////////////////
//
// The active editor's bar if it has one that is open, else NULL.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEFindBar> CIDEFind::GetActiveBar(void)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || pTab->GetFindBar().IsNull() || !pTab->GetFindBar()->IsOpen())
	{
		return NULL;
	}
	return pTab->GetFindBar();
}


//////////////////////////////////////////////////////////////////////////
//
// Find (Control+F) and Replace (Control+R): the active editor's bar,
// opened or changed to the one asked for.  False with no editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFind::ShowBar(bool bReplace)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	GetBar(pTab)->Open(bReplace);
	return true;
}


bool CIDEFind::ShowFind(void) { return ShowBar(false); }
bool CIDEFind::ShowReplace(void) { return ShowBar(true); }


//////////////////////////////////////////////////////////////////////////
//
// Find Next (F3) and Find Previous (Shift+F3): the open bar's next or
// previous match; with no bar open, the next or previous place of what
// was last searched for from the caret, round the ends, with the options
// as they are.  False, the status line saying so, if there is nothing to
// look for or it isn't found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFind::Step(bool bNext)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEFindBar>	pBar;
	Ptr<CTextArea>		pEditor;
	CTextSearch			cSearch;
	CChars				szStatus;
	const char*			szText;
	size				uiLength;
	size				uiStart;
	size				uiEnd;
	size				uiPosition;
	size				uiFound;
	size				uiFoundLength;
	size				uiLast;
	size				uiLastLength;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pBar = GetActiveBar();
	if (pBar.IsNotNull())
	{
		return bNext ? pBar->Next() : pBar->Previous();
	}

	//Usages kept highlighted (Highlight Usages in File) are what there is to step through.
	if (mpcContent->GetMarks()->HasKeptUsages(pTab))
	{
		return mpcContent->GetMarks()->StepUsage(bNext);
	}
	if (mszLast.Empty())
	{
		mpcContent->ShowStatus("There is nothing to find again: Find (Control+F) or Find Word at Caret (Control+F3) first.");
		return false;
	}

	pEditor = pTab->GetEditor();
	szText = pEditor->GetText();
	uiLength = strlen(szText);
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		uiStart = pEditor->GetCaretPosition();
		uiEnd = uiStart;
	}
	cSearch.Init(mszLast.Text(), mbMatchCase, mbWords, mbRegex);
	uiFound = TEXT_SEARCH_NOT_FOUND;
	uiFoundLength = 0;
	if (bNext)
	{
		//The first that starts after the selection's start: the selection may be the last one found.
		uiFound = cSearch.Find(szText, uiLength, (uiStart < uiLength) ? uiStart + 1 : uiLength);
		if (uiFound == TEXT_SEARCH_NOT_FOUND)
		{
			uiFound = cSearch.Find(szText, uiLength, 0);
		}
		uiFoundLength = cSearch.GetFoundLength();
	}
	else
	{
		//The last that starts before the selection's start, or the last of all.
		uiLast = TEXT_SEARCH_NOT_FOUND;
		uiLastLength = 0;
		for (uiPosition = cSearch.Find(szText, uiLength, 0); uiPosition != TEXT_SEARCH_NOT_FOUND; uiPosition = cSearch.Find(szText, uiLength, uiPosition + cSearch.GetFoundLength()))
		{
			if (uiPosition < uiStart)
			{
				uiFound = uiPosition;
				uiFoundLength = cSearch.GetFoundLength();
			}
			uiLast = uiPosition;
			uiLastLength = cSearch.GetFoundLength();
		}
		if (uiFound == TEXT_SEARCH_NOT_FOUND)
		{
			uiFound = uiLast;
			uiFoundLength = uiLastLength;
		}
	}
	cSearch.Kill();

	if (uiFound == TEXT_SEARCH_NOT_FOUND)
	{
		szStatus.Init("\"");
		szStatus.Append(mszLast);
		szStatus.Append("\" wasn't found in this file.");
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		return false;
	}
	pEditor->Select(uiFound, uiFound + uiFoundLength);
	pEditor->ShowCaret();
	mpcContent->GetNavigationHistory()->CaretPlaced();
	return true;
}


bool CIDEFind::FindNext(void) { return Step(true); }
bool CIDEFind::FindPrevious(void) { return Step(false); }


//////////////////////////////////////////////////////////////////////////
//
// Find Word at Caret (Control+F3): the word the caret is in is what is
// searched for from now, as text whatever Regex says, and its next place
// is gone to.  False if the caret is at no word.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFind::FindWordAtCaret(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEFindBar>	pBar;
	const char*			szText;
	CChars				szWord;
	size				uiStart;
	size				uiEnd;
	bool				bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	szText = pTab->GetEditor()->GetText();
	if (!FindIDEWordAt(szText, pTab->GetEditor()->GetCaretPosition(), &uiStart, &uiEnd))
	{
		mpcContent->ShowStatus("The caret isn't at a word to find.");
		return false;
	}
	szWord.Init();
	szWord.Append(&szText[uiStart], uiEnd - uiStart);
	mbRegex = false;
	pBar = GetActiveBar();
	if (pBar.IsNotNull())
	{
		//The bar looks for it, and steps.
		pTab->GetEditor()->SetCaretPosition(uiStart);
		pBar->Open(pBar->IsReplacing());
		bResult = pBar->Next();
	}
	else
	{
		SetLast(szWord.Text());
		pTab->GetEditor()->SetCaretPosition(uiStart);
		bResult = Step(true);
	}
	szWord.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// At each tick: the active editor's bar keeps up with its text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::Poll(void)
{
	Ptr<CIDEFindBar>	pBar;

	pBar = GetActiveBar();
	if (pBar.IsNotNull())
	{
		pBar->Update();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The ticks every bar shares, written with the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::SetOptions(bool bMatchCase, bool bWords, bool bRegex)
{
	if ((bMatchCase != mbMatchCase) || (bWords != mbWords) || (bRegex != mbRegex))
	{
		mbMatchCase = bMatchCase;
		mbWords = bWords;
		mbRegex = bRegex;
		mpcContent->GetSettings()->WriteSettings();
	}
}


void CIDEFind::SetLast(const char* szText)
{
	mszLast.Set(szText);
}


//////////////////////////////////////////////////////////////////////////
//
// <Find MatchCase="false" Words="false" Regex="false"/> in the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFind::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szValue;

	pcTag = pcSettings->GetTag("Find");
	if (pcTag == NULL)
	{
		return;
	}
	szValue = pcTag->GetStringAttribute("MatchCase");
	mbMatchCase = (szValue != NULL) && (StringCompare(szValue, "true") == 0);
	szValue = pcTag->GetStringAttribute("Words");
	mbWords = (szValue != NULL) && (StringCompare(szValue, "true") == 0);
	szValue = pcTag->GetStringAttribute("Regex");
	mbRegex = (szValue != NULL) && (StringCompare(szValue, "true") == 0);
}


void CIDEFind::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin("Find");
	pcWriter->Attribute("MatchCase", mbMatchCase);
	pcWriter->Attribute("Words", mbWords);
	pcWriter->Attribute("Regex", mbRegex);
	pcWriter->End();
}


bool CIDEFind::IsMatchCase(void) const { return mbMatchCase; }
bool CIDEFind::IsWords(void) const { return mbWords; }
bool CIDEFind::IsRegex(void) const { return mbRegex; }
const char* CIDEFind::GetLast(void) { return mszLast.Text(); }

