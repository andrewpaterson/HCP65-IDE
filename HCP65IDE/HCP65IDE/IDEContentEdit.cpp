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
#include "IDEContent.h"


//////////////////////////////////////////////////////////////////////////
//
// Edit, as IntelliJ's: Undo, Redo, Cut, Copy, Paste and Select All of the
// active document, then Undo and Redo Across Files, which go back and
// forward through the edits of every open file in the order they were
// made.  The keys are the editor's own: the shortcuts are only shown.
// What can't be done is disabled as the menu opens.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEContent::CreateEditMenu(void)
{
	mpTheme->GetMenuStyle()->SetDimFont(mpDimFont);
	mpEditMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpUndoItem = mpEditMenu->AddItem("Undo");
	mpUndoItem->SetShortcut("Ctrl+Z");
	mpRedoItem = mpEditMenu->AddItem("Redo");
	mpRedoItem->SetShortcut("Ctrl+Shift+Z");
	mpEditMenu->AddSeparator();
	mpCutItem = mpEditMenu->AddItem("Cut");
	mpCutItem->SetShortcut("Ctrl+X");
	mpCopyItem = mpEditMenu->AddItem("Copy");
	mpCopyItem->SetShortcut("Ctrl+C");
	mpPasteItem = mpEditMenu->AddItem("Paste");
	mpPasteItem->SetShortcut("Ctrl+V");
	mpEditMenu->AddSeparator();
	mpSelectAllItem = mpEditMenu->AddItem("Select All");
	mpSelectAllItem->SetShortcut("Ctrl+A");
	mpEditMenu->AddSeparator();
	mpUndoAllItem = mpEditMenu->AddItem("Undo Across Files");
	mpRedoAllItem = mpEditMenu->AddItem("Redo Across Files");
	mpEditMenu->GetOpeningEvent()->Add(this, &CIDEContent::EditMenuOpening);
	return mpEditMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// Without a document only the across files items can be done, if there
// are edits to go through.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::EditMenuOpening(CMenuContainer* pcMenu)
{
	Ptr<CTextArea>	pEditor;
	bool			bDocument;

	bDocument = mpActive.IsNotNull();
	if (bDocument)
	{
		pEditor = mpActive->GetEditor();
	}
	mpUndoItem->SetEnabled(bDocument && pEditor->CanUndo());
	mpRedoItem->SetEnabled(bDocument && pEditor->CanRedo());
	mpCutItem->SetEnabled(bDocument);
	mpCopyItem->SetEnabled(bDocument);
	mpPasteItem->SetEnabled(bDocument);
	mpSelectAllItem->SetEnabled(bDocument);
	mpUndoAllItem->SetEnabled(mcUndoLog.FindUndoGroup() != MAX_SIZE);
	mpRedoAllItem->SetEnabled(mcUndoLog.FindRedoGroup() != MAX_SIZE);
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcItem isn't one of Edit's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::EditMenuItemClicked(CMenuItem* pcItem)
{
	Ptr<CTextArea>	pEditor;

	if (pcItem == &mpUndoAllItem)
	{
		UndoAcrossFiles();
		return true;
	}
	if (pcItem == &mpRedoAllItem)
	{
		RedoAcrossFiles();
		return true;
	}
	if ((pcItem != &mpUndoItem) && (pcItem != &mpRedoItem) && (pcItem != &mpCutItem) && (pcItem != &mpCopyItem) && (pcItem != &mpPasteItem) && (pcItem != &mpSelectAllItem))
	{
		return false;
	}
	if (mpActive.IsNull())
	{
		return true;
	}

	pEditor = mpActive->GetEditor();
	if (pcItem == &mpUndoItem)
	{
		pEditor->Undo();
	}
	else if (pcItem == &mpRedoItem)
	{
		pEditor->Redo();
	}
	else if (pcItem == &mpCutItem)
	{
		pEditor->CutToClipboard();
	}
	else if (pcItem == &mpCopyItem)
	{
		pEditor->CopyToClipboard();
	}
	else if (pcItem == &mpPasteItem)
	{
		pEditor->PasteFromClipboard();
	}
	else if (pcItem == &mpSelectAllItem)
	{
		pEditor->SelectAll();
	}

	//The keys go back to the document.
	mpWindow->GetFocus()->SetFocus(pEditor);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab whose editor pcEditor is, NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(CTextArea* pcEditor)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetEditor() == pcEditor)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// An open file's edit is logged in the order made.  Its own undo (Ctrl+Z
// in it) of an edit made with others as one, a reload of the files Claude
// changed say, undoes the others too, as its redo redoes them.  Undoing
// and redoing from the log is told here too; it is the log's own doing.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::EditorUndoChanged(CTextArea* pcEditor, STextAreaUndo* psUndo)
{
	Ptr<CIDEEditorTab>	pTab;
	size				uiGroup;

	if (mbLogUndoing)
	{
		return;
	}
	pTab = FindTab(pcEditor);
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
bool CIDEContent::UndoAcrossFiles(void)
{
	return UndoLogGroup(mcUndoLog.FindUndoGroup(), true);
}


//////////////////////////////////////////////////////////////////////////
//
// Redoes the edit, or group of edits, undone last.  False if there is
// nothing to redo.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::RedoAcrossFiles(void)
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
bool CIDEContent::UndoLogGroup(size uiGroup, bool bShow)
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
			pTab = FindTab(psStep->szDocument.Text());
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
		OpenFile(pShown->GetFileName(), pShown->GetProject());
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Redoes the undone steps of uiGroup, oldest first.  With bShow the last
// file redone is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::RedoLogGroup(size uiGroup, bool bShow)
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
			pTab = FindTab(psStep->szDocument.Text());
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
		OpenFile(pShown->GetFileName(), pShown->GetProject());
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The edits until EndUndoGroup, of any open files, are one for undo.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::BeginUndoGroup(void) { mcUndoLog.BeginGroup(); }
Ptr<CMenuContainer> CIDEContent::GetEditMenu(void) { return mpEditMenu; }
size CIDEContent::NumTabs(void) { return maTabs.Size(); }
Ptr<CIDEEditorTab> CIDEContent::GetTab(size uiIndex) { return maTabs.Get(uiIndex); }
void CIDEContent::EndUndoGroup(void) { mcUndoLog.EndGroup(); }

