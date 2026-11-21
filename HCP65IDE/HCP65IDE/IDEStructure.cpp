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
#include "BaseLib/MarkupTag.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLWriter.h"
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
	mbBreadcrumbs = true;
	mbStickyLines = true;
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
	U_Bool(mbBreadcrumbs);
	U_Bool(mbStickyLines);
	M_Pointer(mpPane);
	M_Pointer(mpTool);
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
	pCommands->Add("View.Structure", "Structure", "Alt+7", Ptr<CImageCel>(), this, &CIDEStructure::ToggleStructureWindow);
}


//////////////////////////////////////////////////////////////////////////
//
// The Structure tool window, put at psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::CreatePane(SDockPlace* psPlace)
{
	mpPane = OMalloc<CIDEStructurePane>(mpcContent);
	mpTool = mpcContent->GetDock()->AddPane(IDE_STRUCTURE_ID, "Structure", NULL, mpPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+7, as IntelliJ IDEA's Structure tool window: it is shown (see
// ShowStructureWindow); pressed while it has the keys, it is hidden
// (CIDEToolWindows).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::ToggleStructureWindow(void)
{
	if (mpcContent->GetToolWindows()->HideWithKeys(IDE_STRUCTURE_ID))
	{
		return true;
	}
	return ShowStructureWindow();
}


//////////////////////////////////////////////////////////////////////////
//
// The Structure tool window is shown, as the active editor's outline is
// now, and its tree has the keys.  Select In's too.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::ShowStructureWindow(void)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull())
	{
		pTab->GetOutline();
	}
	mpPane->Update(pTab);
	mpcContent->GetDock()->Activate(mpTool);
	mpcContent->GetWindow()->GetFocus()->SetFocus(mpPane->GetTree());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The Structure tree's menu's choices, written with the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::SetStructureSettings(bool bSortByName, bool bShowFields)
{
	mpPane->SetSortByName(bSortByName);
	mpPane->SetShowFields(bShowFields);
	mpcContent->GetSettings()->WriteSettings();
}


Ptr<CIDEStructurePane> CIDEStructure::GetStructurePane(void) const { return mpPane; }
Ptr<CDockPane> CIDEStructure::GetTool(void) const { return mpTool; }
Ptr<CIDEChooser> CIDEStructure::GetChooser(void) const { return mpChooser; }


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
	CInt32Rect			cBounds;
	SInt32Vec2			sPosition;
	CChars				szTitle;
	int32				iAtCaret;
	int					iLine;
	int					iColumn;
	bool				bResult;

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

	szTitle.Init("Structure of ");
	szTitle.Append(GetIDEFileNamePart(pTab->GetFileName()));
	iAtCaret = pTab->GetCaretPlace(&iLine, &iColumn) ? pcOutline->FindEntryAt(iLine, iColumn) : -1;
	pTab->GetPane()->GetActualBounds(&cBounds);
	sPosition.Init(cBounds.GetLeft() + cBounds.GetWidth() / 4, cBounds.GetTop() + 24);
	bResult = OpenOutlineChooser(pTab, pcOutline, szTitle.Text(), -1, iAtCaret, sPosition, false);
	szTitle.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What entry iEntry of pTab's outline is beside, in a chooser over
// sPosition (its bottom left is there, as a breadcrumb is at the bottom): the entries in what it is in (a class's members, a
// namespace's classes, what the file declares outside everything), in the
// file's order, iEntry selected.  A breadcrumb opens it when it is
// pressed.  False if it has nothing beside it, and nothing opens.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::ShowSiblings(Ptr<CIDEEditorTab> pTab, int32 iEntry, SInt32Vec2 sPosition)
{
	CIDEOutline*	pcOutline;
	CChars			szTitle;
	size			i;
	size			uiSiblings;
	int32			iParent;
	bool			bResult;

	pcOutline = pTab->GetLastOutline();
	if ((iEntry < 0) || ((size)iEntry >= pcOutline->NumEntries()))
	{
		return false;
	}
	iParent = pcOutline->Get((size)iEntry)->iParent;
	uiSiblings = 0;
	for (i = 0; i < pcOutline->NumEntries(); i++)
	{
		if (pcOutline->Get(i)->iParent == iParent)
		{
			uiSiblings++;
		}
	}
	if (uiSiblings < 2)
	{
		return false;
	}

	szTitle.Init("In ");
	szTitle.Append((iParent >= 0) ? pcOutline->Get((size)iParent)->szName.Text() : GetIDEFileNamePart(pTab->GetFileName()));
	bResult = OpenOutlineChooser(pTab, pcOutline, szTitle.Text(), iEntry, iEntry, sPosition, true);
	szTitle.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The chooser opened at sPosition (its top left there, or its bottom
// left with bAbove) with pcOutline's entries, each indented
// under the one it is in, or only those beside entry iSiblingsOf (-1 for
// all of them); each has its parameters after its name and its line
// after that, and choosing one puts the caret on its name.  Entry
// iSelectedEntry is selected, the first if it isn't among them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructure::OpenOutlineChooser(Ptr<CIDEEditorTab> pTab, CIDEOutline* pcOutline, const char* szTitle, int32 iSiblingsOf, int32 iSelectedEntry, SInt32Vec2 sPosition, bool bAbove)
{
	SIDEOutlineEntry*	psEntry;
	SIDELocation		sLocation;
	CChars				szName;
	CChars				szWhere;
	size				i;
	size				uiPlace;
	size				uiSelected;
	int32				j;
	int32				iParent;

	if (mpChooser.IsNull())
	{
		mpChooser = OMalloc<CIDEChooser>(mpcContent);
	}
	mpChooser->Begin(szTitle, true, false, NULL);
	mpChooser->SetSelectName(false);
	mpChooser->SetOpensAbove(bAbove);
	iParent = (iSiblingsOf >= 0) ? pcOutline->Get((size)iSiblingsOf)->iParent : -1;
	uiSelected = 0;
	for (i = 0; i < pcOutline->NumEntries(); i++)
	{
		psEntry = pcOutline->Get(i);
		if ((iSiblingsOf >= 0) && (psEntry->iParent != iParent))
		{
			continue;
		}
		szName.Init();
		for (j = 0; (iSiblingsOf < 0) && (j < psEntry->iDepth); j++)
		{
			szName.Append("    ");
		}
		szName.Append(psEntry->szName);
		szName.Append(psEntry->szDetail);
		szWhere.Init();
		szWhere.Append(psEntry->sName.iLine + 1);
		GetEntryLocation(pTab, psEntry, &sLocation);
		uiPlace = mpChooser->AddPlace(szName.Text(), szWhere.Text(), mpChooser->GetSymbolIcon(GetEntrySymbolKind(psEntry)), &sLocation);
		if ((int32)i == iSelectedEntry)
		{
			uiSelected = uiPlace;
		}
		sLocation.szFileName.Kill();
		szWhere.Kill();
		szName.Kill();
	}
	return mpChooser->OpenAt(sPosition, pTab->GetEditor(), uiSelected);
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
	SIDEStepPlace*		psPlace;
	CArrayIDEStepPlaces	asPlaces;
	CArrayInt			aiEntries;
	size				i;
	int32				iFound;
	int					iLine;
	int					iColumn;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	pcOutline = pTab->GetOutline();
	asPlaces.Init();
	aiEntries.Init();
	for (i = 0; pcOutline && (i < pcOutline->NumEntries()); i++)
	{
		psEntry = pcOutline->Get(i);
		if (psEntry->eKind == IOK_Function)
		{
			psPlace = asPlaces.Add();
			psPlace->iLine = psEntry->sName.iLine;
			psPlace->iColumn = psEntry->sName.iColumn;
			psPlace->iEndColumn = psEntry->sName.iEndColumn;
			aiEntries.Add((int)i);
		}
	}
	iFound = FindIDEStepPlace(&asPlaces, iLine, iColumn, bNext, false);
	psEntry = (iFound >= 0) ? pcOutline->Get((size)aiEntries.GetValue((size)iFound)) : NULL;
	aiEntries.Kill();
	asPlaces.Kill();
	if (psEntry == NULL)
	{
		mpcContent->ShowStatus(bNext ? "There is no method after the caret." : "There is no method before the caret.");
		return false;
	}
	return GoToEntry(pTab, psEntry, false);
}


//////////////////////////////////////////////////////////////////////////
//
// Stepping the caret through places in a file (methods' names, changed
// lines, bookmarks, highlighted usages): of pasPlaces, in the file's
// order, the first that starts after iLine, iColumn (bNext), or the last
// that ends before it; round the ends with bWrap.  -1 if there is none.
// A caret in a place is at it: neither before nor after it.
//
//////////////////////////////////////////////////////////////////////////
int32 FindIDEStepPlace(CArrayIDEStepPlaces* pasPlaces, int32 iLine, int32 iColumn, bool bNext, bool bWrap)
{
	SIDEStepPlace*	psPlace;
	size			i;
	int32			iFound;

	iFound = -1;
	for (i = 0; i < pasPlaces->NumElements(); i++)
	{
		psPlace = pasPlaces->Get(i);
		if (bNext && ((psPlace->iLine > iLine) || ((psPlace->iLine == iLine) && (psPlace->iColumn > iColumn))))
		{
			return (int32)i;
		}
		if (!bNext && ((psPlace->iLine < iLine) || ((psPlace->iLine == iLine) && (psPlace->iEndColumn < iColumn))))
		{
			iFound = (int32)i;
		}
	}
	if ((iFound < 0) && bWrap && (pasPlaces->NumElements() > 0))
	{
		iFound = bNext ? 0 : (int32)pasPlaces->NumElements() - 1;
	}
	return iFound;
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


//////////////////////////////////////////////////////////////////////////
//
// At each tick: every editor shows breadcrumbs and sticky lines or not,
// as the settings say, and the active one's follow its caret, its
// scrolling and its outline.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::Poll(void)
{
	Ptr<CIDEEditorTab>	pTab;
	size				i;

	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		pTab = mpcContent->GetDocuments()->GetTab(i);
		pTab->ShowBreadcrumbs(mpcContent, mbBreadcrumbs);
		pTab->ShowStickyLines(mpcContent, mbStickyLines);
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (mpPane.IsNotNull())
	{
		mpPane->Update(pTab);
	}
	if (pTab.IsNotNull())
	{
		if (pTab->GetBreadcrumbs().IsNotNull())
		{
			pTab->GetBreadcrumbs()->Update();
		}
		if (pTab->GetStickyLines().IsNotNull())
		{
			pTab->GetStickyLines()->Update();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The Settings page's: whether editors show breadcrumbs and sticky lines,
// written with the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::SetEditorSettings(bool bBreadcrumbs, bool bStickyLines)
{
	mbBreadcrumbs = bBreadcrumbs;
	mbStickyLines = bStickyLines;
	mpcContent->GetSettings()->WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// <Editor Breadcrumbs="true" StickyLines="true"/> in the settings.
// Without it editors show both.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructure::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szValue;

	pcTag = pcSettings->GetTag("Editor");
	if (pcTag != NULL)
	{
		szValue = pcTag->GetStringAttribute("Breadcrumbs");
		mbBreadcrumbs = (szValue == NULL) || (StringCompare(szValue, "false") != 0);
		szValue = pcTag->GetStringAttribute("StickyLines");
		mbStickyLines = (szValue == NULL) || (StringCompare(szValue, "false") != 0);
	}

	//<Structure SortByName="false" ShowFields="true"/>: the Structure tree's menu's choices.
	pcTag = pcSettings->GetTag("Structure");
	if ((pcTag != NULL) && mpPane.IsNotNull())
	{
		szValue = pcTag->GetStringAttribute("SortByName");
		mpPane->SetSortByName((szValue != NULL) && (StringCompare(szValue, "true") == 0));
		szValue = pcTag->GetStringAttribute("ShowFields");
		mpPane->SetShowFields((szValue == NULL) || (StringCompare(szValue, "false") != 0));
	}
}


void CIDEStructure::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin("Editor");
	pcWriter->Attribute("Breadcrumbs", mbBreadcrumbs);
	pcWriter->Attribute("StickyLines", mbStickyLines);
	pcWriter->End();
	if (mpPane.IsNotNull() && (mpPane->IsSortByName() || !mpPane->IsShowFields()))
	{
		pcWriter->Begin("Structure");
		pcWriter->Attribute("SortByName", mpPane->IsSortByName());
		pcWriter->Attribute("ShowFields", mpPane->IsShowFields());
		pcWriter->End();
	}
}


bool CIDEStructure::IsBreadcrumbs(void) const { return mbBreadcrumbs; }
bool CIDEStructure::IsStickyLines(void) const { return mbStickyLines; }

