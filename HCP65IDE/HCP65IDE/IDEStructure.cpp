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
	M_Pointer(mpPrompt);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::Free(void)
{
	if (mpPrompt.IsNotNull())
	{
		mpPrompt->GetAcceptedEvent()->Remove(this);
	}
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
	pCommands->Add("Navigate.GoToLine", "Line:Column...", "Control+G", Ptr<CImageCel>(), this, &CIDEStructure::ShowGoToLine)->SetEnabledCheck(this, &CIDEStructure::HasEditor);
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


//////////////////////////////////////////////////////////////////////////
//
// A line, or a line and a column, as Go to Line takes them: digits, then
// nothing or a colon and digits ("120", "120:8"), both from 1 and neither
// 0; the column is 1 if it isn't given.  False if szPlace isn't that.
//
//////////////////////////////////////////////////////////////////////////
bool ParseIDELinePlace(const char* szPlace, size* puiLine, size* puiColumn)
{
	const char*	sz;
	size		uiLine;
	size		uiColumn;

	uiLine = 0;
	for (sz = szPlace; (*sz >= '0') && (*sz <= '9') && (uiLine < 100000000); sz++)
	{
		uiLine = uiLine * 10 + (size)(*sz - '0');
	}
	if ((sz == szPlace) || (uiLine == 0))
	{
		return false;
	}
	uiColumn = 1;
	if (*sz == ':')
	{
		sz++;
		if ((*sz < '0') || (*sz > '9'))
		{
			return false;
		}
		uiColumn = 0;
		for (; (*sz >= '0') && (*sz <= '9') && (uiColumn < 100000000); sz++)
		{
			uiColumn = uiColumn * 10 + (size)(*sz - '0');
		}
	}
	if ((*sz != '\0') || (uiColumn == 0))
	{
		return false;
	}
	*puiLine = uiLine;
	*puiColumn = uiColumn;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Line (Control+G), as IntelliJ's Line:Column: a prompt over the
// top of the editor with the caret's line and column in it, selected.
// Enter puts the caret at the line typed, or the line and column
// ("120" or "120:8", from 1); Escape leaves it where it is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::ShowGoToLine(void)
{
	Ptr<CIDEEditorTab>	pTab;
	CInt32Rect			cBounds;
	SInt32Vec2			sPosition;
	CChars				szPlace;
	int					iLine;
	int					iColumn;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	if (mpPrompt.IsNull())
	{
		mpPrompt = OMalloc<CPromptPopup>(mpcContent->GetWindow(), mpcContent->GetDialogLook());
		mpPrompt->GetAcceptedEvent()->Add(this, &CIDEStructure::LineAccepted);
	}
	szPlace.Init();
	szPlace.Append(iLine + 1);
	szPlace.Append(':');
	szPlace.Append(iColumn + 1);
	pTab->GetPane()->GetActualBounds(&cBounds);
	sPosition.Init(cBounds.GetLeft() + cBounds.GetWidth() / 3, cBounds.GetTop() + 24);
	mpPrompt->Open(sPosition, pTab->GetEditor(), "Line[:column]:", szPlace.Text());
	szPlace.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Enter in Go to Line's prompt: the caret goes where it says and the
// prompt closes, or it says what it wants.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::LineAccepted(void)
{
	CChars	szPlace;

	size	uiLine;
	size	uiColumn;

	szPlace.Init(mpPrompt->GetText());
	szPlace.StripWhitespace();
	if (ParseIDELinePlace(szPlace.Text(), &uiLine, &uiColumn))
	{
		//Closed first: the keys go back to the editor, which the caret is then moved in.
		mpPrompt->Close();
		GoToLine(szPlace.Text());
	}
	else
	{
		mpPrompt->SetProblem("Type a line, or a line and a column: 120 or 120:8.");
	}
	szPlace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The active editor's caret goes to szPlace, "<line>" or
// "<line>:<column>", both from 1: a line past the last is the last, a
// column past the line's end its end.  The place it left is on Back if it
// was far.  False, and nothing moves, if szPlace isn't either.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::GoToLine(const char* szPlace)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	SIDELocation		sLocation;
	size				uiLine;
	size				uiColumn;
	size				uiLength;
	bool				bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}

	if (!ParseIDELinePlace(szPlace, &uiLine, &uiColumn))
	{
		return false;
	}

	pEditor = pTab->GetEditor();
	if (uiLine > pEditor->NumLines())
	{
		uiLine = pEditor->NumLines();
	}
	uiLength = pEditor->GetLineEnd(uiLine - 1) - pEditor->GetLineStart(uiLine - 1);
	if (uiColumn > uiLength + 1)
	{
		uiColumn = uiLength + 1;
	}

	sLocation.szFileName.Init(pTab->GetFileName());
	sLocation.iLine = (int32)uiLine - 1;
	sLocation.iColumn = (int32)uiColumn - 1;
	sLocation.iLength = 0;
	sLocation.uiRole = CPPIR_Reference;
	sLocation.uiFlags = 0;
	sLocation.uiUsage = 0;
	sLocation.iContainer = -1;
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, false);
	sLocation.szFileName.Kill();
	return bResult;
}


Ptr<CPromptPopup> CIDEStructure::GetPrompt(void) const { return mpPrompt; }

