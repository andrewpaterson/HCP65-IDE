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
#include "IDEContent.h"
#include "IDEMarks.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	muiChangesBars = MAX_SIZE;
	mbChangesDue = false;
	masUsages.Init();
	muiUsagesChanges = 0;
	muiOverridesOutline = MAX_SIZE;
	muiOverridesIndexings = MAX_SIZE;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpChangesTab);
	U_Size(muiChangesBars);
	U_Bool(mbChangesDue);
	M_Pointer(mpUsagesTab);
	U_Unknown(CArrayTextRange, masUsages);
	U_Size(muiUsagesChanges);
	M_Pointer(mpOverridesTab);
	U_Size(muiOverridesOutline);
	U_Size(muiOverridesIndexings);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::Free(void)
{
	masUsages.Kill();
}


bool CIDEMarks::Save(CObjectWriter* pcFile) { return false; }
bool CIDEMarks::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Navigate's, with IntelliJ IDEA's keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.NextChange", "Next Change", "Control+Alt+Shift+Cursor Down", Ptr<CImageCel>(), this, &CIDEMarks::NextChange)->SetEnabledCheck(this, &CIDEMarks::HasEditor);
	pCommands->Add("Navigate.PreviousChange", "Previous Change", "Control+Alt+Shift+Cursor Up", Ptr<CImageCel>(), this, &CIDEMarks::PreviousChange)->SetEnabledCheck(this, &CIDEMarks::HasEditor);
	pCommands->Add("Navigate.HighlightUsages", "Highlight Usages in File", "Control+Shift+F7", Ptr<CImageCel>(), this, &CIDEMarks::HighlightUsages)->SetEnabledCheck(this, &CIDEMarks::HasEditor);
}


void CIDEMarks::AddNavigateItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.NextChange"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.PreviousChange"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.HighlightUsages"));
}


bool CIDEMarks::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// Next Change and Previous Change: the caret goes to the first line of
// the next run of changed lines after it, or the last before it (for
// lines removed, the line they were above).  False, the status line
// saying so, if there is none that way.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMarks::StepChange(bool bNext)
{
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CCodeEditor>		pEditor;
	CArrayIDEStepPlaces		asPlaces;
	SCodeEditorChangeBar*	psBar;
	SIDEStepPlace*			psPlace;
	SIDELocation			sLocation;
	size					uiLines;
	size					i;
	int32					iFound;
	int						iLine;
	int						iColumn;
	bool					bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}

	//The bars as the text is now.
	if (pTab->NeedsChangeBars())
	{
		mpcContent->GetGitStatus()->UpdateChangeBars(pTab);
	}
	pEditor = pTab->GetCodeEditor();
	uiLines = pTab->GetEditor()->NumLines();
	asPlaces.Init();
	for (i = 0; i < pEditor->NumChangeBars(); i++)
	{
		//A caret anywhere in a run of changed lines is at it.
		psBar = pEditor->GetChangeBar(i);
		psPlace = asPlaces.Add();
		psPlace->iLine = (int32)((psBar->uiStartLine < uiLines) ? psBar->uiStartLine : uiLines - 1);
		psPlace->iColumn = 0;
		psPlace->iEndColumn = MAX_INT32;
		if ((psBar->uiEndLine > psBar->uiStartLine) && (iLine >= (int)psBar->uiStartLine) && (iLine < (int)psBar->uiEndLine))
		{
			psPlace->iLine = iLine;
		}
	}
	iFound = FindIDEStepPlace(&asPlaces, iLine, iColumn, bNext, false);
	bResult = false;
	if (iFound < 0)
	{
		if (asPlaces.NumElements() == 0)
		{
			mpcContent->ShowStatus(pTab->HasBaseText() ? "This file hasn't changed since the last commit." : "This file has no last commit to have changed from.");
		}
		else
		{
			mpcContent->ShowStatus(bNext ? "There is no change after the caret." : "There is no change before the caret.");
		}
	}
	else
	{
		psBar = pEditor->GetChangeBar((size)iFound);
		memset(&sLocation, 0, sizeof(SIDELocation));
		sLocation.szFileName.Init(pTab->GetFileName());
		sLocation.iLine = (int32)((psBar->uiStartLine < uiLines) ? psBar->uiStartLine : uiLines - 1);
		sLocation.iContainer = -1;
		bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, false);
		sLocation.szFileName.Kill();
	}
	asPlaces.Kill();
	return bResult;
}


bool CIDEMarks::NextChange(void) { return StepChange(true); }
bool CIDEMarks::PreviousChange(void) { return StepChange(false); }


//////////////////////////////////////////////////////////////////////////
//
// A tab's changed lines marked on its scroll bar, in the bars' colours:
// added, modified, and where lines were removed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::ShowChangeMarks(Ptr<CIDEEditorTab> pTab)
{
	Ptr<CCodeEditor>			pEditor;
	Ptr<CScrollBar>				pBar;
	CArrayTemplate<SInt32Vec2>	aasMarks[3];
	SCodeEditorChangeBar*		psBar;
	CInt32Rect					cFirst;
	CInt32Rect					cLast;
	size						uiLines;
	size						uiKind;
	size						i;

	pEditor = pTab->GetCodeEditor();
	pBar = pTab->GetPane()->GetScrollBar(SBO_Vertical);
	uiLines = pTab->GetEditor()->NumLines();
	for (i = 0; i < 3; i++)
	{
		aasMarks[i].Init();
	}
	mbChangesDue = false;
	for (i = 0; i < pEditor->NumChangeBars(); i++)
	{
		psBar = pEditor->GetChangeBar(i);
		uiKind = (psBar->sColour == IDE_CHANGE_ADDED) ? 0 : ((psBar->sColour == IDE_CHANGE_REMOVED) ? 2 : 1);
		if (!pTab->GetEditor()->GetLineBounds((psBar->uiStartLine < uiLines) ? psBar->uiStartLine : uiLines - 1, &cFirst))
		{
			mbChangesDue = true;
			continue;
		}
		cLast = cFirst;
		if (psBar->uiEndLine > psBar->uiStartLine + 1)
		{
			pTab->GetEditor()->GetLineBounds(((psBar->uiEndLine <= uiLines) ? psBar->uiEndLine : uiLines) - 1, &cLast);
		}
		aasMarks[uiKind].Add()->Init(cFirst.GetTop(), (psBar->uiEndLine > psBar->uiStartLine) ? cLast.GetBottom() - cFirst.GetTop() : 1);
	}
	pBar->SetMarks(IDE_CHANGE_ADDED_MARKS, aasMarks[0].NumElements() ? aasMarks[0].Get(0) : NULL, aasMarks[0].NumElements(), IDE_CHANGE_ADDED);
	pBar->SetMarks(IDE_CHANGE_MODIFIED_MARKS, aasMarks[1].NumElements() ? aasMarks[1].Get(0) : NULL, aasMarks[1].NumElements(), IDE_CHANGE_MODIFIED);
	pBar->SetMarks(IDE_CHANGE_REMOVED_MARKS, aasMarks[2].NumElements() ? aasMarks[2].Get(0) : NULL, aasMarks[2].NumElements(), IDE_CHANGE_REMOVED);
	for (i = 0; i < 3; i++)
	{
		aasMarks[i].Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Highlight Usages in File (Control+Shift+F7): the usages in the active
// file of the symbol at its caret are kept highlighted, in place of any
// that were, and marked on its scroll bar; the status line says how many
// and how to step and end.  False, the status line saying so, if the
// caret isn't on a symbol the index knows.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMarks::HighlightUsages(void)
{
	Ptr<CIDEEditorTab>			pTab;
	Ptr<CTextArea>				pEditor;
	CArrayIDELocations			asLocations;
	CArrayTemplate<SInt32Vec2>	asMarks;
	SIDELocation*				psLocation;
	STextRange					sRange;
	CInt32Rect					cLine;
	CChars						szStatus;
	size						i;
	size						j;
	int							iLine;
	int							iColumn;
	int32						iLastLine;
	uint8						uiRole;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	ClearUsages();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	asLocations.Init();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) || !mpcContent->GetCodeModel()->FindFileReferences(pTab->GetFileName(), iLine, iColumn, &asLocations) || (asLocations.NumElements() == 0))
	{
		KillIDELocations(&asLocations);
		mpcContent->ShowStatus("The caret isn't on a name whose usages in this file are known.");
		return false;
	}

	//In the text's order, each once.
	pEditor = pTab->GetEditor();
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		sRange.uiStart = pTab->FindPosition(psLocation->iLine, psLocation->iColumn);
		sRange.uiEnd = pTab->FindPosition(psLocation->iLine, psLocation->iColumn + psLocation->iLength);
		for (j = 0; (j < masUsages.NumElements()) && (masUsages.Get(j)->uiStart < sRange.uiStart); j++)
		{
		}
		if ((j == masUsages.NumElements()) || (masUsages.Get(j)->uiStart != sRange.uiStart))
		{
			masUsages.InsertAt(&sRange, j);
		}
	}
	KillIDELocations(&asLocations);

	mpUsagesTab = pTab;
	muiUsagesChanges = pEditor->GetChangeCount();
	pEditor->SetHighlightRanges(pTab->GetKeptUsagesLayer(mpcContent->GetTheme()->GetColour(TC_SearchMatch)), masUsages.Get(0), masUsages.NumElements());

	asMarks.Init();
	iLastLine = -1;
	for (i = 0; i < masUsages.NumElements(); i++)
	{
		iLine = (int)pEditor->FindLine(masUsages.Get(i)->uiStart);
		if ((iLine != iLastLine) && pEditor->GetLineBounds((size)iLine, &cLine))
		{
			asMarks.Add()->Init(cLine.GetTop(), cLine.GetHeight());
		}
		iLastLine = iLine;
	}
	pTab->GetPane()->GetScrollBar(SBO_Vertical)->SetMarks(IDE_USAGES_MARKS, asMarks.NumElements() ? asMarks.Get(0) : NULL, asMarks.NumElements(), mpcContent->GetTheme()->GetColour(TC_SearchMatch));
	asMarks.Kill();

	szStatus.Init();
	szStatus.Append((uint32)masUsages.NumElements());
	szStatus.Append((masUsages.NumElements() == 1) ? " usage" : " usages");
	szStatus.Append(" highlighted: F3 and Shift+F3 step through them, Escape ends it.");
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a tab has usages kept highlighted, of its text as it is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMarks::HasKeptUsages(Ptr<CIDEEditorTab> pTab)
{
	return pTab.IsNotNull() && (pTab == mpUsagesTab) && (masUsages.NumElements() > 0) && (pTab->GetEditor()->GetChangeCount() == muiUsagesChanges);
}


//////////////////////////////////////////////////////////////////////////
//
// Find Next and Find Previous with usages kept highlighted: the next
// usage that starts after the caret, or the last that starts before it,
// round the ends, is selected and shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMarks::StepUsage(bool bNext)
{
	Ptr<CTextArea>	pEditor;
	STextRange*		psRange;
	size			uiStart;
	size			uiEnd;
	size			uiFound;
	size			i;

	if (!HasKeptUsages(mpcContent->GetDocuments()->GetActiveTab()))
	{
		return false;
	}
	pEditor = mpUsagesTab->GetEditor();
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		uiStart = pEditor->GetCaretPosition();
	}
	uiFound = bNext ? 0 : masUsages.NumElements() - 1;
	for (i = 0; i < masUsages.NumElements(); i++)
	{
		psRange = masUsages.Get(i);
		if (bNext && (psRange->uiStart > uiStart))
		{
			uiFound = i;
			break;
		}
		if (!bNext && (psRange->uiStart < uiStart))
		{
			uiFound = i;
		}
	}
	psRange = masUsages.Get(uiFound);
	pEditor->Select(psRange->uiStart, psRange->uiEnd);
	pEditor->ShowCaret();
	mpcContent->GetNavigationHistory()->CaretPlaced();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The usages kept highlighted aren't any more.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::ClearUsages(void)
{
	if (mpUsagesTab.IsNotNull())
	{
		mpUsagesTab->GetEditor()->ClearHighlightRanges(mpUsagesTab->GetKeptUsagesLayer(mpcContent->GetTheme()->GetColour(TC_SearchMatch)));
		mpUsagesTab->GetPane()->GetScrollBar(SBO_Vertical)->ClearMarks(IDE_USAGES_MARKS);
	}
	mpUsagesTab = NULL;
	masUsages.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Escape in an editor: the usages kept highlighted in it end.  It is the
// editor's own too (the selection cleared), whatever this does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::EscapeAction(SComponentInputEvent* psEvent)
{
	if (mpUsagesTab.IsNotNull() && (mpUsagesTab == mpcContent->GetDocuments()->GetActiveTab()))
	{
		ClearUsages();
	}
	else
	{
		psEvent->bPass = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A tab is closing: nothing of it is kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::TabClosed(Ptr<CIDEEditorTab> pTab)
{
	if (mpUsagesTab == pTab)
	{
		mpUsagesTab = NULL;
		masUsages.ReInit();
	}
	if (mpChangesTab == pTab)
	{
		mpChangesTab = NULL;
	}
	if (mpOverridesTab == pTab)
	{
		mpOverridesTab = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A tab's override marks, in its gutter's second icon column: for each
// function and class of its outline the index knows, whether it
// overrides, is overridden (or derived from), or both.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::ShowOverrides(Ptr<CIDEEditorTab> pTab)
{
	Ptr<CCodeEditor>	pEditor;
	CIDECodeModel*		pcModel;
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	CArrayIDETargets	asTargets;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	CChars				szKey;
	size				i;
	uint8				uiKind;
	bool				bUp;
	bool				bDown;

	pEditor = pTab->GetCodeEditor();
	pcModel = mpcContent->GetCodeModel();
	pcOutline = pTab->GetLastOutline();
	pEditor->ClearLineIcons(IDE_GUTTER_OVERRIDES);
	for (i = 0; (i < pcOutline->NumEntries()) && (i < IDE_OVERRIDE_ENTRIES); i++)
	{
		psEntry = pcOutline->Get(i);
		if ((psEntry->eKind != IOK_Function) && (psEntry->eKind != IOK_Class))
		{
			continue;
		}

		//The name's last character: a definition's name has its class before it (CCircle::Draw).
		szKey.Init();
		bUp = false;
		bDown = false;
		if (pcModel->GetSymbolAt(pTab->GetFileName(), psEntry->sName.iLine, (psEntry->sName.iEndColumn > psEntry->sName.iColumn) ? psEntry->sName.iEndColumn - 1 : psEntry->sName.iColumn, &szKey, &uiKind))
		{
			if (psEntry->eKind == IOK_Class)
			{
				aszKeys.Init();
				bDown = pcModel->FindClassDerived(szKey.Text(), false, &aszKeys) > 0;
				aszKeys.Kill();
			}
			else
			{
				asTargets.Init();
				bUp = pcModel->FindSuperMethods(szKey.Text(), &asTargets) > 0;
				KillIDETargets(&asTargets);
				asLocations.Init();
				bDown = pcModel->FindOverriders(szKey.Text(), &asLocations) > 0;
				KillIDELocations(&asLocations);
			}
		}
		szKey.Kill();
		if (bUp || bDown)
		{
			pEditor->SetLineIcon(IDE_GUTTER_OVERRIDES, (size)psEntry->sName.iLine, (bUp && bDown) ? IDE_OVERRIDES_BOTH : (bUp ? IDE_OVERRIDES_UP : IDE_OVERRIDES_DOWN), mpcContent->GetTheme()->GetColour(bUp ? TC_Accent : TC_Success));
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A press in an editor's gutter, on its override column: the caret goes
// to the name on that line and what the mark says is gone to: Go to Super
// Method, Go to Implementation(s), or for both Go to Related Symbol.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::IconClicked(CCodeEditor* pcEditor, SInt32Vec2 sIcon)
{
	Ptr<CIDEEditorTab>	pTab;
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	SCodeEditorGlyph*	psIcon;
	size				i;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if ((sIcon.x != IDE_GUTTER_OVERRIDES) || pTab.IsNull() || (&pTab->GetCodeEditor() != pcEditor))
	{
		return;
	}
	psIcon = pcEditor->GetLineIcon(IDE_GUTTER_OVERRIDES, (size)sIcon.y);
	pcOutline = pTab->GetLastOutline();
	for (i = 0; psIcon && (i < pcOutline->NumEntries()); i++)
	{
		psEntry = pcOutline->Get(i);
		if ((psEntry->sName.iLine == sIcon.y) && ((psEntry->eKind == IOK_Function) || (psEntry->eKind == IOK_Class)))
		{
			pTab->SetCaretPlace(psEntry->sName.iLine, (psEntry->sName.iEndColumn > psEntry->sName.iColumn) ? psEntry->sName.iEndColumn - 1 : psEntry->sName.iColumn);
			mpcContent->GetDocuments()->FocusActiveEditor();
			if (psIcon->cGlyph == IDE_OVERRIDES_UP)
			{
				mpcContent->GetNavigator()->GoToSuperMethod();
			}
			else if (psIcon->cGlyph == IDE_OVERRIDES_DOWN)
			{
				mpcContent->GetNavigator()->GoToImplementation();
			}
			else
			{
				mpcContent->GetNavigator()->GoToRelatedSymbol();
			}
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// At each tick, for the active editor: its changed lines' marks are made
// again when its change bars have been; usages kept highlighted end when
// its text has changed; and its override marks are made again when its
// outline or the index has changed, once both have the file as it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMarks::Poll(void)
{
	Ptr<CIDEEditorTab>	pTab;
	size				uiIndexed;
	size				uiToIndex;

	if (mpUsagesTab.IsNotNull() && (mpUsagesTab->GetEditor()->GetChangeCount() != muiUsagesChanges))
	{
		ClearUsages();
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return;
	}

	if ((pTab != mpChangesTab) || (pTab->GetBarChanges() != muiChangesBars) || mbChangesDue)
	{
		mpChangesTab = pTab;
		muiChangesBars = pTab->GetBarChanges();
		ShowChangeMarks(pTab);
	}

	mpcContent->GetCodeModel()->GetIndexingProgress(&uiIndexed, &uiToIndex);
	if (((pTab != mpOverridesTab) || (pTab->GetOutlineChanges() != muiOverridesOutline) || (mpcContent->GetCodeModel()->NumIndexings() != muiOverridesIndexings)) &&
		(uiIndexed >= uiToIndex) && pTab->IsOutlineCurrent() && pTab->IsIndexCurrent())
	{
		mpOverridesTab = pTab;
		muiOverridesOutline = pTab->GetOutlineChanges();
		muiOverridesIndexings = mpcContent->GetCodeModel()->NumIndexings();
		ShowOverrides(pTab);
	}
}


size CIDEMarks::NumKeptUsages(void) const { return masUsages.NumElements(); }

