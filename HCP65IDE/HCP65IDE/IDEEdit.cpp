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
	mpcContent->mpTheme->GetMenuStyle()->SetDimFont(mpcContent->mpDimFont);
	mpEditMenu = OMalloc<CMenuContainer>(mpcContent->mpWindow, mpcContent->mpTheme);
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
	mpEditMenu->GetOpeningEvent()->Add(this, &CIDEEdit::EditMenuOpening);
	return mpEditMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// Without a document only the across files items can be done, if there
// are edits to go through.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::EditMenuOpening(CMenuContainer* pcMenu)
{
	Ptr<CTextArea>	pEditor;
	bool			bDocument;

	bDocument = mpcContent->mpActive.IsNotNull();
	if (bDocument)
	{
		pEditor = mpcContent->mpActive->GetEditor();
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
bool CIDEEdit::EditMenuItemClicked(CMenuItem* pcItem)
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
	if (mpcContent->mpActive.IsNull())
	{
		return true;
	}

	pEditor = mpcContent->mpActive->GetEditor();
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
	mpcContent->mpWindow->GetFocus()->SetFocus(pEditor);
	return true;
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
	pTab = mpcContent->FindTab(pcEditor);
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
			pTab = mpcContent->FindTab(psStep->szDocument.Text());
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
		mpcContent->OpenFile(pShown->GetFileName(), pShown->GetProject());
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
			pTab = mpcContent->FindTab(psStep->szDocument.Text());
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
		mpcContent->OpenFile(pShown->GetFileName(), pShown->GetProject());
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
