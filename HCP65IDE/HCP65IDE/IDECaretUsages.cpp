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
#include "StandardLib/ClassDefines.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDECaretUsages.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECaretUsages::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpTab = NULL;
	muiCaret = MAX_SIZE;
	muiChanges = 0;
	miMoved = 0;
	mbLookedUp = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECaretUsages::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpTab);
	U_Size(muiCaret);
	U_Size(muiChanges);
	U_Int64(miMoved);
	U_Bool(mbLookedUp);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECaretUsages::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// What is shown is for the session only.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECaretUsages::Save(CObjectWriter* pcFile) { return false; }
bool CIDECaretUsages::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Looks at the active editor's caret, from the IDE's tick: a caret moved
// off the usages shown, or an edit, takes them away; one that has stayed
// put for IDE_CARET_USAGES_REST, with the index up to date with the text,
// shows the usages of the symbol it is on (the editor draws them only
// while nothing is selected).  How
// many milliseconds until it should look again, -1 for when something
// changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDECaretUsages::Poll(int64 iNow)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	size				uiCaret;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab != mpTab)
	{
		Watch(pTab);
	}
	if (pTab.IsNull())
	{
		return -1;
	}

	pEditor = pTab->GetEditor();
	uiCaret = pEditor->GetCaretPosition();
	if ((uiCaret != muiCaret) || (pEditor->GetChangeCount() != muiChanges))
	{
		muiCaret = uiCaret;
		muiChanges = pEditor->GetChangeCount();
		miMoved = iNow;
		mbLookedUp = IsOnShownUsage(pEditor, uiCaret);
		if (!mbLookedUp)
		{
			pEditor->ClearOccurrenceRanges();
		}
	}
	if (mbLookedUp)
	{
		return -1;
	}
	if (iNow - miMoved < IDE_CARET_USAGES_REST)
	{
		return IDE_CARET_USAGES_REST - (iNow - miMoved);
	}
	if (!pTab->IsIndexCurrent() || mpcContent->GetCodeModel()->IsIndexing())
	{
		return IDE_CARET_USAGES_WAIT;
	}

	mbLookedUp = true;
	ShowUsages();
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// Another tab is active (or none): the usages shown in the one watched go.
//
//////////////////////////////////////////////////////////////////////////
void CIDECaretUsages::Watch(Ptr<CIDEEditorTab> pTab)
{
	if (mpTab.IsNotNull())
	{
		mpTab->GetEditor()->ClearOccurrenceRanges();
	}
	mpTab = pTab;
	muiCaret = MAX_SIZE;
	mbLookedUp = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The caret is in one of the usages shown (or just after it), so they are
// still the usages of the symbol it is on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECaretUsages::IsOnShownUsage(Ptr<CTextArea> pEditor, size uiCaret)
{
	STextOccurrenceRange*	psRange;
	size					i;

	for (i = 0; i < pEditor->NumOccurrenceRanges(); i++)
	{
		psRange = pEditor->GetOccurrenceRange(i);
		if ((psRange->uiStart <= uiCaret) && (uiCaret <= psRange->uiEnd))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The places in the watched tab's file (it is the active one, whose caret
// the documents find the symbol at) that name the symbol at its caret (or just
// before it) drawn as occurrences: where it is declared or defined in the
// write colour, its uses in the occurrence colour.  Nothing if no symbol
// the index knows is named there.
//
//////////////////////////////////////////////////////////////////////////
void CIDECaretUsages::ShowUsages(void)
{
	CArrayIDELocations			asLocations;
	CArrayTextOccurrenceRange	asRanges;
	STextOccurrenceRange*		psRange;
	SIDELocation*				psLocation;
	int							iLine;
	int							iColumn;
	uint8						uiRole;
	size						i;

	asLocations.Init();
	if (mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) && mpcContent->GetCodeModel()->FindFileReferences(mpTab->GetFileName(), iLine, iColumn, &asLocations))
	{
		asRanges.Init();
		for (i = 0; i < asLocations.NumElements(); i++)
		{
			psLocation = asLocations.Get(i);
			psRange = asRanges.Add();
			psRange->uiStart = mpTab->FindPosition(psLocation->iLine, psLocation->iColumn);
			psRange->uiEnd = mpTab->FindPosition(psLocation->iLine, psLocation->iColumn + psLocation->iLength);
			psRange->bWrite = psLocation->uiRole != CPPIR_Reference;
		}
		mpTab->GetEditor()->SetOccurrenceRanges(asRanges.GetData(), asRanges.NumElements());
		asRanges.Kill();
	}
	KillIDELocations(&asLocations);
}
