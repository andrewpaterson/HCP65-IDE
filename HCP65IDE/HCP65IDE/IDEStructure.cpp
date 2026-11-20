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
#include "CppCompilerLib/CPPIndex.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEStructure.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpChooser);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::Save(CObjectWriter* pcFile) { return false; }
bool CIDEStructure::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// File Structure (Control+F12) and Next and Previous Method (Alt+Down
// and Alt+Up), as IntelliJ IDEA's, while there is an editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.FileStructure", "File Structure", "Control+F12", Ptr<CImageCel>(), this, &CIDEStructure::ShowFileStructure)->SetEnabledCheck(this, &CIDEStructure::HasEditor);
	pCommands->Add("Navigate.NextMethod", "Next Method", "Alt+Cursor Down", Ptr<CImageCel>(), this, &CIDEStructure::NextMethod)->SetEnabledCheck(this, &CIDEStructure::HasEditor);
	pCommands->Add("Navigate.PreviousMethod", "Previous Method", "Alt+Cursor Up", Ptr<CImageCel>(), this, &CIDEStructure::PreviousMethod)->SetEnabledCheck(this, &CIDEStructure::HasEditor);
}


//////////////////////////////////////////////////////////////////////////
//
// Navigate's items for them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::AddNavigateItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.FileStructure"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.NextMethod"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.PreviousMethod"));
}


bool CIDEStructure::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// The kind of symbol (ECPPSymbolKind) an outline's entry is, for its
// icon.
//
//////////////////////////////////////////////////////////////////////////
uint8 CIDEStructure::GetEntrySymbolKind(SIDEOutlineEntry* psEntry)
{
	switch (psEntry->eKind)
	{
	case IOK_Namespace:
		return (uint8)CPPSK_Namespace;
	case IOK_Class:
		return (uint8)CPPSK_Class;
	case IOK_Enum:
		return (uint8)CPPSK_Enum;
	case IOK_Function:
		return (uint8)CPPSK_Function;
	case IOK_Field:
		return (uint8)CPPSK_Field;
	case IOK_Typedef:
		return (uint8)CPPSK_Typedef;
	default:
		return (uint8)CPPSK_Variable;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where an entry of pTab's outline is named, as a place to go to.
// psDest's file name is made: kill it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::GetEntryLocation(Ptr<CIDEEditorTab> pTab, SIDEOutlineEntry* psEntry, SIDELocation* psDest)
{
	psDest->szFileName.Init(pTab->GetFileName());
	psDest->iLine = psEntry->sName.iLine;
	psDest->iColumn = psEntry->sName.iColumn;
	psDest->iLength = (psEntry->sName.iEndLine == psEntry->sName.iLine) ? (psEntry->sName.iEndColumn - psEntry->sName.iColumn) : 0;
	psDest->uiRole = psEntry->bDefinition ? CPPIR_Definition : CPPIR_Declaration;
	psDest->uiFlags = 0;
	psDest->uiUsage = 0;
	psDest->iContainer = -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The caret goes to an entry's name in pTab, the place it left on Back
// if it was far.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::GoToEntry(Ptr<CIDEEditorTab> pTab, SIDEOutlineEntry* psEntry, bool bSelectName)
{
	SIDELocation	sLocation;
	bool			bResult;

	GetEntryLocation(pTab, psEntry, &sLocation);
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, bSelectName);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// File Structure (Control+F12): a chooser, in the middle of the editor's
// top, of what the active file declares, each entry indented under the
// one it is in, with its parameters after its name and its line after
// that; the entry the caret is in is selected.  Typing filters by name;
// Enter or a click puts the caret on the one chosen.  False, with the
// status line saying so, if the file has no outline.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::ShowFileStructure(void)
{
	Ptr<CIDEEditorTab>	pTab;
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	SIDELocation		sLocation;
	CInt32Rect			cBounds;
	SInt32Vec2			sPosition;
	CChars				szName;
	CChars				szWhere;
	size				i;
	int32				j;
	int32				iAtCaret;
	int					iLine;
	int					iColumn;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pcOutline = pTab->GetOutline();
	if ((pcOutline == NULL) || (pcOutline->NumEntries() == 0))
	{
		mpcContent->ShowStatus("This file has no structure to show: nothing it declares was found.");
		return false;
	}

	if (mpChooser.IsNull())
	{
		mpChooser = OMalloc<CIDEChooser>(mpcContent);
	}
	szName.Init("Structure of ");
	szName.Append(GetIDEFileNamePart(pTab->GetFileName()));
	mpChooser->Begin(szName.Text(), true, false, NULL);
	szName.Kill();
	mpChooser->SetSelectName(false);
	for (i = 0; i < pcOutline->NumEntries(); i++)
	{
		psEntry = pcOutline->Get(i);
		szName.Init();
		for (j = 0; j < psEntry->iDepth; j++)
		{
			szName.Append("    ");
		}
		szName.Append(psEntry->szName);
		szName.Append(psEntry->szDetail);
		szWhere.Init();
		szWhere.Append(psEntry->sName.iLine + 1);
		GetEntryLocation(pTab, psEntry, &sLocation);
		mpChooser->AddPlace(szName.Text(), szWhere.Text(), mpChooser->GetSymbolIcon(GetEntrySymbolKind(psEntry)), &sLocation);
		sLocation.szFileName.Kill();
		szWhere.Kill();
		szName.Kill();
	}

	iAtCaret = pTab->GetCaretPlace(&iLine, &iColumn) ? pcOutline->FindEntryAt(iLine, iColumn) : -1;
	pTab->GetPane()->GetActualBounds(&cBounds);
	sPosition.Init(cBounds.GetLeft() + cBounds.GetWidth() / 4, cBounds.GetTop() + 24);
	return mpChooser->OpenAt(sPosition, pTab->GetEditor(), (iAtCaret >= 0) ? (size)iAtCaret : 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Next Method (Alt+Down) and Previous Method (Alt+Up): the caret goes to
// the name of the first function the file declares or defines after the
// caret, or the last before it.  With none that way the status line says
// so.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::StepMethod(bool bNext)
{
	Ptr<CIDEEditorTab>	pTab;
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	SIDEOutlineEntry*	psFound;
	size				i;
	int					iLine;
	int					iColumn;
	bool				bAfter;
	bool				bBefore;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	pcOutline = pTab->GetOutline();
	psFound = NULL;
	for (i = 0; pcOutline && (i < pcOutline->NumEntries()); i++)
	{
		psEntry = pcOutline->Get(i);
		if (psEntry->eKind != IOK_Function)
		{
			continue;
		}
		bAfter = (psEntry->sName.iLine > iLine) || ((psEntry->sName.iLine == iLine) && (psEntry->sName.iColumn > iColumn));
		bBefore = (psEntry->sName.iLine < iLine) || ((psEntry->sName.iLine == iLine) && (psEntry->sName.iEndColumn < iColumn));
		if (bNext && bAfter)
		{
			psFound = psEntry;
			break;
		}
		if (!bNext && bBefore)
		{
			psFound = psEntry;
		}
	}
	if (psFound == NULL)
	{
		mpcContent->ShowStatus(bNext ? "There is no method after the caret." : "There is no method before the caret.");
		return false;
	}
	return GoToEntry(pTab, psFound, false);
}


bool CIDEStructure::NextMethod(void) { return StepMethod(true); }
bool CIDEStructure::PreviousMethod(void) { return StepMethod(false); }

