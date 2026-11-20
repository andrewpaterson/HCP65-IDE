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
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/Label.h"
#include "IDEContent.h"
#include "IDEBreadcrumbs.h"


#define IDE_BREADCRUMB_CHOOSER_GAP	6	//Between a crumb and the bottom of the chooser it opens.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::Init(CIDEContent* pcContent, CIDEEditorTab* pcTab)
{
	Ptr<CButton>	pMeasure;

	PreInit();
	mpcContent = pcContent;
	mpcTab = pcTab;
	maButtons.Init();
	maiEntries.Init();
	muiShownChanges = MAX_SIZE;
	miShownEntry = -1;

	//The strip is as high as a crumb whether or not it has any, so the code doesn't move as the caret does.
	pMeasure = OMalloc<CButton>(pcContent->GetWindow(), pcContent->GetTheme(), BL_Flat, Ptr<CImageCel>(), "Crumb");
	mpRow = OMalloc<CGridContainer>(pcContent->GetWindow());
	mpRow->SetAlignment(CSH_Stretch, CSV_Center);
	mpRow->SetDesiredSize(-1, pMeasure->GetDesiredSize().y);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::Class(void)
{
	U_Pointer(mpcContent);
	U_Pointer(mpcTab);
	M_Pointer(mpRow);
	M_Embedded(maButtons);
	U_Unknown(CArrayInt, maiEntries);
	U_Size(muiShownChanges);
	U_Int32(miShownEntry);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off its buttons.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::Free(void)
{
	size	i;

	for (i = 0; i < maButtons.NumElements(); i++)
	{
		maButtons.Get(i)->GetClickEvent()->Remove(this);
	}
	maiEntries.Kill();
}


bool CIDEBreadcrumbs::Save(CObjectWriter* pcFile) { return false; }
bool CIDEBreadcrumbs::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::RemoveButtons(void)
{
	size	i;

	for (i = 0; i < maButtons.NumElements(); i++)
	{
		maButtons.Get(i)->GetClickEvent()->Remove(this);
	}
	maButtons.RemoveAll();
	maiEntries.ReInit();
	mpRow->RemoveAllComponents();
}


//////////////////////////////////////////////////////////////////////////
//
// The crumbs are what the caret is in now, by the outline as it was last
// parsed: made again if either has changed since they were made.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::Update(void)
{
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	Ptr<CButton>		pButton;
	Ptr<CLabel>			pBetween;
	Ptr<CFillContainer>	pFiller;
	CArrayInt			aiChain;
	size				i;
	int32				iEntry;
	int					iLine;
	int					iColumn;
	int					iGridColumn;

	pcOutline = mpcTab->GetLastOutline();
	iEntry = mpcTab->GetCaretPlace(&iLine, &iColumn) ? pcOutline->FindEntryAt(iLine, iColumn) : -1;
	if ((mpcTab->GetOutlineChanges() == muiShownChanges) && (iEntry == miShownEntry))
	{
		return;
	}
	muiShownChanges = mpcTab->GetOutlineChanges();
	miShownEntry = iEntry;

	RemoveButtons();
	aiChain.Init();
	for (; iEntry >= 0; iEntry = pcOutline->Get((size)iEntry)->iParent)
	{
		aiChain.InsertAt((int)iEntry, 0);
	}
	iGridColumn = 0;
	for (i = 0; i < aiChain.NumElements(); i++)
	{
		psEntry = pcOutline->Get((size)aiChain.GetValue(i));
		if (i > 0)
		{
			pBetween = OMalloc<CLabel>(mpcContent->GetWindow(), mpcContent->GetDimFont(), ">");
			pBetween->SetAlignment(CSH_Left, CSV_Center);
			mpRow->AddComponent(pBetween, iGridColumn, 0);
			iGridColumn++;
		}
		pButton = OMalloc<CButton>(mpcContent->GetWindow(), mpcContent->GetTheme(), BL_Flat, Ptr<CImageCel>(), psEntry->szName.Text());
		pButton->SetAlignment(CSH_Left, CSV_Center);
		pButton->GetClickEvent()->Add(this, &CIDEBreadcrumbs::CrumbClicked);
		mpRow->AddComponent(pButton, iGridColumn, 0);
		iGridColumn++;
		maButtons.Add(pButton);
		maiEntries.Add(aiChain.GetValue(i));
	}
	aiChain.Kill();

	//What is left of the strip, so the crumbs stay at its left.
	pFiller = OMalloc<CFillContainer>(mpcContent->GetWindow());
	mpRow->AddComponent(pFiller, iGridColumn, 0, 1, 1, 1, 0);
}


//////////////////////////////////////////////////////////////////////////
//
// A crumb pressed: the caret goes to its entry's name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::CrumbClicked(CButton* pcButton)
{
	size	i;

	for (i = 0; i < maButtons.NumElements(); i++)
	{
		if (&maButtons.Get(i) == pcButton)
		{
			Click(i);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// As a press on crumb uiCrumb: the caret goes to its entry's name, and a
// chooser of what is beside it (the class's other members, say) opens at
// the crumb, to go on to one of those.  False if there is no such crumb,
// or the outline has changed so its entry has gone.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBreadcrumbs::Click(size uiCrumb)
{
	CIDEOutline*	pcOutline;
	CInt32Rect		cCrumb;
	SInt32Vec2		sPosition;
	int32			iEntry;

	pcOutline = mpcTab->GetLastOutline();
	if (uiCrumb >= maiEntries.NumElements())
	{
		return false;
	}
	iEntry = maiEntries.GetValue(uiCrumb);
	if ((size)iEntry >= pcOutline->NumEntries())
	{
		return false;
	}
	maButtons.Get(uiCrumb)->GetActualBounds(&cCrumb);
	sPosition.Init(cCrumb.GetLeft(), cCrumb.GetTop() - IDE_BREADCRUMB_CHOOSER_GAP);
	if (!mpcContent->GetStructure()->GoToEntry(mpcTab, pcOutline->Get((size)iEntry), false))
	{
		return false;
	}
	mpcContent->GetStructure()->ShowSiblings(mpcTab, iEntry, sPosition);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "CShape > Move", or "(none)".
//
//////////////////////////////////////////////////////////////////////////
void CIDEBreadcrumbs::Print(CChars* pszDest)
{
	size	i;

	if (maButtons.NumElements() == 0)
	{
		pszDest->Append("(none)");
	}
	for (i = 0; i < maButtons.NumElements(); i++)
	{
		if (i > 0)
		{
			pszDest->Append(" > ");
		}
		pszDest->Append(maButtons.Get(i)->GetCaption());
	}
}


Ptr<CComponent> CIDEBreadcrumbs::GetComponent(void) const { return mpRow; }
size CIDEBreadcrumbs::NumCrumbs(void) const { return maiEntries.NumElements(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Init(CIDEContent* pcContent, CIDEEditorTab* pcTab)
{
	Ptr<CCanvas>	pBackground;
	Ptr<CCanvas>	pLine;

	PreInit();
	mpcContent = pcContent;
	mpcTab = pcTab;
	maiEntries.Init();
	muiShownChanges = MAX_SIZE;
	miShownTop = -1;
	mbShown = false;

	//The lines, in the code's font over the editor's colour, with a line under them to part them from the code.
	mpText = OMalloc<CTextArea>(pcContent->GetWindow(), pcContent->GetTheme(), pcContent->GetCodeFamily(), "");
	mpText->SetEditable(false);
	mpText->SetSelectable(false);
	mpText->SetTiledText(true);
	mpText->SetColour(pcContent->GetTheme()->GetColour(TC_Text));
	mpText->SetAlignment(CSH_Stretch, CSV_Top);
	mpGap = OMalloc<CFillContainer>(pcContent->GetWindow());
	mpGap->SetDesiredSize(0, 1);
	pLine = OMalloc<CCanvas>(pcContent->GetWindow(), OMalloc<CClearCanvasDraw>(pcContent->GetTheme()->GetColour(TC_Separator)));
	pLine->SetDesiredSize(-1, 1);
	pLine->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpRows = OMalloc<CGridContainer>(pcContent->GetWindow());
	mpRows->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpRows->AddComponent(mpGap, 0, 0);
	mpRows->AddComponent(mpText, 1, 0, 1, 1, 1, 1);
	mpRows->AddComponent(pLine, 0, 1, 2, 1, 1, 0);
	mpRows->AddInputAction(CIC_ActivateComponent, this, &CIDEStickyLines::Pressed);

	pBackground = OMalloc<CCanvas>(pcContent->GetWindow(), OMalloc<CClearCanvasDraw>(pcContent->GetTheme()->GetColour(TC_Editor)));
	pBackground->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel = OMalloc<CFillContainer>(pcContent->GetWindow());
	mpPanel->SetAlignment(CSH_Stretch, CSV_Top);
	mpPanel->AddComponent(pBackground);
	mpPanel->AddComponent(mpRows);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Class(void)
{
	U_Pointer(mpcContent);
	U_Pointer(mpcTab);
	M_Pointer(mpPanel);
	M_Pointer(mpRows);
	M_Pointer(mpGap);
	M_Pointer(mpText);
	U_Unknown(CArrayInt, maiEntries);
	U_Size(muiShownChanges);
	U_Int32(miShownTop);
	U_Bool(mbShown);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Free(void)
{
	maiEntries.Kill();
}


bool CIDEStickyLines::Save(CObjectWriter* pcFile) { return false; }
bool CIDEStickyLines::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// A line of the code's height, 0 if the editor has none laid out.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEStickyLines::GetLineHeight(void)
{
	CInt32Rect	cLine;

	if (!mpcTab->GetEditor()->GetLineBounds(0, &cLine))
	{
		return 0;
	}
	return cLine.GetHeight();
}


//////////////////////////////////////////////////////////////////////////
//
// The lines kept at the top are those of what the first line in view is
// in whose own first lines are above it, worked out again if the view
// has scrolled or the outline has been parsed again since.  Each line
// kept takes a line of the view, so what comes next is asked of the line
// under it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Update(void)
{
	Ptr<CTextArea>		pEditor;
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	CArrayInt			aiChain;
	CChars				szText;
	size				i;
	size				uiLine;
	int32				iHeight;
	int32				iTop;
	int32				iEntry;

	iHeight = GetLineHeight();
	if (iHeight <= 0)
	{
		return;
	}
	iTop = (int32)mpcTab->GetEditor()->FindLine(mpcTab->GetEditor()->FindPositionAt(0, mpcTab->GetPane()->GetScrollPosition().y));
	if ((mpcTab->GetOutlineChanges() == muiShownChanges) && (iTop == miShownTop))
	{
		return;
	}
	muiShownChanges = mpcTab->GetOutlineChanges();
	miShownTop = iTop;

	pEditor = mpcTab->GetEditor();
	pcOutline = mpcTab->GetLastOutline();
	maiEntries.ReInit();
	szText.Init();
	for (;;)
	{
		//What the first line under the lines kept so far is in, outermost first; the first not kept yet whose own
		//first line is out of sight is kept.
		aiChain.Init();
		for (iEntry = pcOutline->FindEntryAt(iTop + (int32)maiEntries.NumElements(), 0x7fff); iEntry >= 0; iEntry = pcOutline->Get((size)iEntry)->iParent)
		{
			aiChain.InsertAt((int)iEntry, 0);
		}
		iEntry = -1;
		for (i = 0; i < aiChain.NumElements(); i++)
		{
			psEntry = pcOutline->Get((size)aiChain.GetValue(i));
			if (psEntry->bDefinition && (maiEntries.Find(aiChain.GetValue(i)) == ARRAY_ELEMENT_NOT_FOUND) &&
				(psEntry->sName.iLine < iTop + (int32)maiEntries.NumElements()) && (psEntry->sRange.iEndLine > iTop + (int32)maiEntries.NumElements()))
			{
				iEntry = aiChain.GetValue(i);
				break;
			}
		}
		aiChain.Kill();
		if ((iEntry < 0) || (maiEntries.NumElements() >= IDE_STICKY_LINES))
		{
			break;
		}
		maiEntries.Add((int)iEntry);
		uiLine = (size)pcOutline->Get((size)iEntry)->sName.iLine;
		if (uiLine < pEditor->NumLines())
		{
			if (maiEntries.NumElements() > 1)
			{
				szText.Append('\n');
			}
			szText.Append(&pEditor->GetText()[pEditor->GetLineStart(uiLine)], pEditor->GetLineEnd(uiLine) - pEditor->GetLineStart(uiLine));
		}
	}

	if (maiEntries.NumElements() == 0)
	{
		szText.Kill();
		Hide();
		return;
	}
	mpText->SetText(szText.Text());
	szText.Kill();
	mpGap->SetDesiredSize(mpcTab->GetCodeEditor()->GetGutter()->GetActualSize().x, 1);
	mpPanel->SetDesiredSize(-1, (int32)maiEntries.NumElements() * iHeight + 1);
	if (!mbShown)
	{
		mpcTab->SetOverlay(mpPanel);
		mbShown = true;
	}
	mpcContent->GetWindow()->InvalidateLayout();
}


//////////////////////////////////////////////////////////////////////////
//
// No lines are kept: the panel is taken off the view.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Hide(void)
{
	if (mbShown)
	{
		mpcTab->SetOverlay(Ptr<CComponent>());
		mbShown = false;
	}
	maiEntries.ReInit();
	muiShownChanges = MAX_SIZE;
	miShownTop = -1;
}


//////////////////////////////////////////////////////////////////////////
//
// A press on the lines: the caret goes to the name of what the line
// pressed starts.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Pressed(SComponentInputEvent* psEvent)
{
	int32	iHeight;

	iHeight = GetLineHeight();
	if ((iHeight > 0) && (psEvent->y >= 0))
	{
		Click((size)(psEvent->y / iHeight));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// As a press on line uiLine of those kept.  False if there is no such
// line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStickyLines::Click(size uiLine)
{
	CIDEOutline*	pcOutline;
	int32			iEntry;

	pcOutline = mpcTab->GetLastOutline();
	if (uiLine >= maiEntries.NumElements())
	{
		return false;
	}
	iEntry = maiEntries.GetValue(uiLine);
	if ((size)iEntry >= pcOutline->NumEntries())
	{
		return false;
	}
	return mpcContent->GetStructure()->GoToEntry(mpcTab, pcOutline->Get((size)iEntry), false);
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: the lines kept, each in brackets, or "(none)".
//
//////////////////////////////////////////////////////////////////////////
void CIDEStickyLines::Print(CChars* pszDest)
{
	CChars	szText;

	if (maiEntries.NumElements() == 0)
	{
		pszDest->Append("(none)");
		return;
	}
	szText.Init(mpText->GetText());
	szText.Replace("\n", "] [");
	pszDest->Append("[");
	pszDest->Append(szText);
	pszDest->Append("]");
	szText.Kill();
}


size CIDEStickyLines::NumLines(void) const { return maiEntries.NumElements(); }

