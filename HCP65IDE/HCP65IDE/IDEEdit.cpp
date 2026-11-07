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
#include "IDEContent.h"
#include "IDEEdit.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mcUndoLog.Init();
	mbLogUndoing = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CUndoLog, mcUndoLog);
	U_Bool(mbLogUndoing);
	M_Pointer(mpEditMenu);
	M_Pointer(mpUndoItem);
	M_Pointer(mpRedoItem);
	M_Pointer(mpCutItem);
	M_Pointer(mpCopyItem);
	M_Pointer(mpPasteItem);
	M_Pointer(mpSelectAllItem);
	M_Pointer(mpUndoAllItem);
	M_Pointer(mpRedoAllItem);
	M_Pointer(mpEditTarget);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::Free(void)
{
	if (mpEditMenu.IsNotNull())
	{
		mpEditMenu->GetOpeningEvent()->Remove(this);
		mpEditMenu->GetItemClickEvent()->Remove(this);
	}
	mcUndoLog.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::Save(CObjectWriter* pcFile) { return false; }
bool CIDEEdit::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Edit, as IntelliJ's: Undo, Redo, Cut, Copy, Paste and Select All of the
// active document, then Undo and Redo Across Files, which go back and
// forward through the edits of every open file in the order they were
// made.  The keys are the editor's own: the shortcuts are only shown.
// What can't be done is disabled as the menu opens.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEEdit::CreateEditMenu(void)
{
	mpcContent->GetTheme()->GetMenuStyle()->SetDimFont(mpcContent->GetDimFont());
	mpEditMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpUndoItem = mpEditMenu->AddItem("Undo");
	mpUndoItem->SetShortcut(CIC_Undo);
	mpRedoItem = mpEditMenu->AddItem("Redo");
	mpRedoItem->SetShortcut(CIC_Redo);
	mpEditMenu->AddSeparator();
	mpCutItem = mpEditMenu->AddItem("Cut");
	mpCutItem->SetShortcut(CIC_Cut);
	mpCopyItem = mpEditMenu->AddItem("Copy");
	mpCopyItem->SetShortcut(CIC_Copy);
	mpPasteItem = mpEditMenu->AddItem("Paste");
	mpPasteItem->SetShortcut(CIC_Paste);
	mpEditMenu->AddSeparator();
	mpSelectAllItem = mpEditMenu->AddItem("Select All");
	mpSelectAllItem->SetShortcut(CIC_SelectAll);
	mpEditMenu->AddSeparator();
	mpUndoAllItem = mpEditMenu->AddItem("Undo Across Files");
	mpRedoAllItem = mpEditMenu->AddItem("Redo Across Files");
	mpEditMenu->GetOpeningEvent()->Add(this, &CIDEEdit::EditMenuOpening);
	mpEditMenu->GetItemClickEvent()->Add(this, &CIDEEdit::EditMenuItemClicked);
	mpcContent->GetWindow()->GetFocus()->GetFocusChangeEvent()->Add(this, &CIDEEdit::FocusChanged);
	return mpEditMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// What can't be done to the text Edit acts on is disabled: Cut and Paste
// in text that can't be edited (the Build output).  With no text only the
// across files items can be done, if there are edits to go through.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::EditMenuOpening(CMenuContainer* pcMenu)
{
	Ptr<CTextArea>	pArea;
	Ptr<CTextField>	pField;
	bool			bText;
	bool			bEditable;

	pField = GetEditField();
	pArea = GetEditArea();
	bText = pField.IsNotNull() || pArea.IsNotNull();
	bEditable = bText && (pField.IsNotNull() || pArea->IsEditable());
	mpUndoItem->SetEnabled(bText && (pField.IsNotNull() ? pField->CanUndo() : pArea->CanUndo()));
	mpRedoItem->SetEnabled(bText && (pField.IsNotNull() ? pField->CanRedo() : pArea->CanRedo()));
	mpCutItem->SetEnabled(bEditable);
	mpCopyItem->SetEnabled(bText);
	mpPasteItem->SetEnabled(bEditable);
	mpSelectAllItem->SetEnabled(bText);
	mpUndoAllItem->SetEnabled(mcUndoLog.FindUndoGroup() != MAX_SIZE);
	mpRedoAllItem->SetEnabled(mcUndoLog.FindRedoGroup() != MAX_SIZE);
}


//////////////////////////////////////////////////////////////////////////
//
// Edit's items.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::EditMenuItemClicked(CMenuItem* pcItem)
{
	Ptr<CTextArea>	pArea;
	Ptr<CTextField>	pField;

	if (pcItem == &mpUndoAllItem)
	{
		UndoAcrossFiles();
		return;
	}
	if (pcItem == &mpRedoAllItem)
	{
		RedoAcrossFiles();
		return;
	}
	if ((pcItem != &mpUndoItem) && (pcItem != &mpRedoItem) && (pcItem != &mpCutItem) && (pcItem != &mpCopyItem) && (pcItem != &mpPasteItem) && (pcItem != &mpSelectAllItem))
	{
		return;
	}
	pField = GetEditField();
	pArea = GetEditArea();
	if (pField.IsNull() && pArea.IsNull())
	{
		return;
	}

	if (pField.IsNotNull())
	{
		if (pcItem == &mpUndoItem)
		{
			pField->Undo();
		}
		else if (pcItem == &mpRedoItem)
		{
			pField->Redo();
		}
		else if (pcItem == &mpCutItem)
		{
			pField->CutToClipboard();
		}
		else if (pcItem == &mpCopyItem)
		{
			pField->CopyToClipboard();
		}
		else if (pcItem == &mpPasteItem)
		{
			pField->PasteFromClipboard();
		}
		else if (pcItem == &mpSelectAllItem)
		{
			pField->SelectAll();
		}
		mpcContent->GetWindow()->GetFocus()->SetFocus(pField);
		return;
	}

	if (pcItem == &mpUndoItem)
	{
		pArea->Undo();
	}
	else if (pcItem == &mpRedoItem)
	{
		pArea->Redo();
	}
	else if ((pcItem == &mpCutItem) && pArea->IsEditable())
	{
		pArea->CutToClipboard();
	}
	else if (pcItem == &mpCopyItem)
	{
		pArea->CopyToClipboard();
	}
	else if ((pcItem == &mpPasteItem) && pArea->IsEditable())
	{
		pArea->PasteFromClipboard();
	}
	else if (pcItem == &mpSelectAllItem)
	{
		pArea->SelectAll();
	}

	//The keys go back to the text.
	mpcContent->GetWindow()->GetFocus()->SetFocus(pArea);
}


//////////////////////////////////////////////////////////////////////////
//
// The keys going to a text area or text field make it what Edit acts on.
// The menu taking them doesn't.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::FocusChanged(CFocus* pcFocus)
{
	Ptr<CComponent>	pFocussed;

	pFocussed = pcFocus->GetFocussedComponent();
	if (pFocussed.IsNotNull() && ((dynamic_cast<CTextArea*>(&pFocussed) != NULL) || (dynamic_cast<CTextField*>(&pFocussed) != NULL)))
	{
		mpEditTarget = pFocussed;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A pane closing: the text the keys last went to is let go of if it is
// in pContent (a closed document's editor, say), so it can be freed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::PaneClosed(Ptr<CComponent> pContent)
{
	if (mpEditTarget.IsNotNull() && pContent.IsNotNull() && ((&mpEditTarget == &pContent) || mpEditTarget->IsWithin(pContent)))
	{
		mpEditTarget = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the text area or text field the keys last went to is still in
// the window: its tab or tool window may have been closed or hidden.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::IsEditTargetShown(void)
{
	Ptr<CComponent>	pTop;

	if (mpEditTarget.IsNull())
	{
		return false;
	}
	for (pTop = mpEditTarget; pTop->GetParent().IsNotNull(); pTop = pTop->GetParent())
	{
	}
	return &pTop == &mpcContent->GetWindow();
}


//////////////////////////////////////////////////////////////////////////
//
// The text field Edit acts on: the one the keys last went to, if they
// last went to one still shown.  NULL if not.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextField> CIDEEdit::GetEditField(void)
{
	if (IsEditTargetShown())
	{
		return dynamic_cast<CTextField*>(&mpEditTarget);
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The text area Edit acts on: the one the keys last went to if it is
// still shown, else the active document if a text field isn't (see
// GetEditField).  NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextArea> CIDEEdit::GetEditArea(void)
{
	CTextArea*	pcArea;

	if (IsEditTargetShown())
	{
		pcArea = dynamic_cast<CTextArea*>(&mpEditTarget);
		if (pcArea)
		{
			return pcArea;
		}
		return NULL;
	}
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return NULL;
	}
	return mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
}


//////////////////////////////////////////////////////////////////////////
//
// An open file's edit is logged in the order made.  Its own undo (Ctrl+Z
// in it) of an edit made with others as one, a reload of the files Claude
// changed say, undoes the others too, as its redo redoes them.  Undoing
// and redoing from the log is told here too; it is the log's own doing.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::EditorUndoChanged(CTextArea* pcEditor, STextAreaUndo* psUndo)
{
	Ptr<CIDEEditorTab>	pTab;
	size				uiGroup;

	if (mbLogUndoing)
	{
		return;
	}
	pTab = mpcContent->GetDocuments()->FindTab(pcEditor);
	if (pTab.IsNull())
	{
		return;
	}

	if (psUndo->eChange == TAU_Kept)
	{
		mcUndoLog.Kept(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
	}
	else if (psUndo->eChange == TAU_Merged)
	{
		mcUndoLog.Merged(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
	}
	else if (psUndo->eChange == TAU_Undone)
	{
		uiGroup = mcUndoLog.Undone(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
		UndoLogGroup(uiGroup, false);
	}
	else if (psUndo->eChange == TAU_Redone)
	{
		uiGroup = mcUndoLog.Redone(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
		RedoLogGroup(uiGroup, false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Undoes the newest edit, or group of edits, of any open file.  The file
// is shown.  False if there is nothing to undo.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::UndoAcrossFiles(void)
{
	return UndoLogGroup(mcUndoLog.FindUndoGroup(), true);
}


//////////////////////////////////////////////////////////////////////////
//
// Redoes the edit, or group of edits, undone last.  False if there is
// nothing to redo.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::RedoAcrossFiles(void)
{
	return RedoLogGroup(mcUndoLog.FindRedoGroup(), true);
}


//////////////////////////////////////////////////////////////////////////
//
// Undoes the steps of uiGroup not undone yet, newest first, each in its
// own file.  A file edited since (its text isn't as the step left it)
// isn't changed: its later edits would go too.  With bShow the last file
// undone is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::UndoLogGroup(size uiGroup, bool bShow)
{
	size				i;
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEEditorTab>	pShown;

	if (uiGroup == MAX_SIZE)
	{
		return false;
	}
	mbLogUndoing = true;
	for (i = mcUndoLog.NumSteps(); i > 0; i--)
	{
		psStep = mcUndoLog.Get(i - 1);
		if ((psStep->uiGroup == uiGroup) && !mcUndoLog.IsUndone(i - 1))
		{
			pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
			if (pTab.IsNotNull() && (pTab->GetEditor()->GetUndoId() == psStep->uiIdAfter))
			{
				pTab->GetEditor()->UndoTo(psStep->uiIdBefore);
				pShown = pTab;
			}
			mcUndoLog.MarkUndone(i - 1);
		}
	}
	mbLogUndoing = false;

	if (bShow && pShown.IsNotNull())
	{
		mpcContent->GetDocuments()->OpenFile(pShown->GetFileName(), pShown->GetProject());
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Redoes the undone steps of uiGroup, oldest first.  With bShow the last
// file redone is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::RedoLogGroup(size uiGroup, bool bShow)
{
	size				i;
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEEditorTab>	pShown;

	if (uiGroup == MAX_SIZE)
	{
		return false;
	}
	mbLogUndoing = true;
	for (i = 0; i < mcUndoLog.NumSteps(); i++)
	{
		psStep = mcUndoLog.Get(i);
		if ((psStep->uiGroup == uiGroup) && mcUndoLog.IsUndone(i))
		{
			pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
			if (pTab.IsNotNull() && (pTab->GetEditor()->GetUndoId() == psStep->uiIdBefore))
			{
				pTab->GetEditor()->UndoTo(psStep->uiIdAfter);
				pShown = pTab;
			}
			mcUndoLog.MarkRedone(i);
		}
	}
	mbLogUndoing = false;

	if (bShow && pShown.IsNotNull())
	{
		mpcContent->GetDocuments()->OpenFile(pShown->GetFileName(), pShown->GetProject());
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The edits until EndUndoGroup, of any open files, are one for undo.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::BeginUndoGroup(void) { mcUndoLog.BeginGroup(); }
Ptr<CMenuContainer> CIDEEdit::GetEditMenu(void) { return mpEditMenu; }
void CIDEEdit::EndUndoGroup(void) { mcUndoLog.EndGroup(); }


//////////////////////////////////////////////////////////////////////////
//
// szFileName has closed: its edits are forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::FileClosed(char* szFileName) { mcUndoLog.RemoveDocument(szFileName); }
