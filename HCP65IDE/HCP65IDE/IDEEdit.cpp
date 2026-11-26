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
	mcUndo.Init(this);
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
	U_Unknown(CRevealingUndo, mcUndo);
	M_Pointer(mpEditMenu);
	M_Pointer(mpEditCommands);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::Free(void)
{
	mcUndo.Kill();
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
// Edit's commands: the standard ones (CIDEEditCommands).  The editors'
// Undo and Redo come here (PressUndo).
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::AddCommands(Ptr<CCommandSet> pCommands)
{
	mpEditCommands = OMalloc<CIDEEditCommands>(mpcContent, pCommands);
	mpEditCommands->AddKeymapCommands(pCommands);
}


//////////////////////////////////////////////////////////////////////////
//
// Edit, as IntelliJ's.  What can't be done is disabled as the menu opens
// (the commands' enabled checks).
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEEdit::CreateEditMenu(void)
{
	Ptr<CCommandSet>	pCommands;

	pCommands = mpcContent->GetCommands();
	mpcContent->GetTheme()->GetMenuStyle()->SetDimFont(mpcContent->GetDimFont());
	mpEditMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpEditMenu->AddItem(pCommands->Find("Edit.Undo"));
	mpEditMenu->AddItem(pCommands->Find("Edit.Redo"));
	mpEditMenu->AddSeparator();
	mpEditMenu->AddItem(pCommands->Find("Edit.Cut"));
	mpEditMenu->AddItem(pCommands->Find("Edit.Copy"));
	mpEditMenu->AddItem(pCommands->Find("Edit.Paste"));
	mpEditMenu->AddItem(pCommands->Find("Edit.PasteFromHistory"));
	mpEditMenu->AddSeparator();
	mpEditMenu->AddItem(pCommands->Find("Edit.SelectAll"));
	mpEditMenu->AddItem(pCommands->Find("Editor.ExtendSelection"));
	mpEditMenu->AddItem(pCommands->Find("Editor.ShrinkSelection"));
	mpEditMenu->AddSeparator();
	mpEditMenu->AddItem(pCommands->Find("Editor.CommentLines"));
	mpEditMenu->AddItem(pCommands->Find("Editor.UncommentLines"));
	mpEditMenu->AddItem(pCommands->Find("Editor.CommentBlock"));
	mpEditMenu->AddItem(pCommands->Find("Editor.MoveLinesUp"));
	mpEditMenu->AddItem(pCommands->Find("Editor.MoveLinesDown"));
	mpcContent->GetReformat()->AddEditItems(mpEditMenu);
	mpEditMenu->AddSeparator();
	mpcContent->GetFind()->AddEditItems(mpEditMenu);
	mpcContent->GetFindInFiles()->AddEditItems(mpEditMenu);
	mpcContent->GetDuplicates()->AddEditItems(mpEditMenu);
	mpcContent->GetCompletion()->AddEditItems(mpEditMenu);
	return mpEditMenu;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::CanPressUndo(bool bRedo) { return (bRedo ? mcUndoLog.FindRedoGroup() : mcUndoLog.FindUndoGroup()) != MAX_SIZE; }


//////////////////////////////////////////////////////////////////////////
//
// An open file's edit is logged in the order made.  Its own undo (Ctrl+Z
// in it) of an edit made with others as one, a reload of the files Claude
// changed say, undoes the others too, as its redo redoes them.  Undoing
// and redoing from the log is told here too; it is the log's own doing.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::EditorUndoChanged(CTextArea* pcEditor, STextUndoChange* psUndo)
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

	if (psUndo->eChange == TUC_Kept)
	{
		mcUndoLog.Kept(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
	}
	else if (psUndo->eChange == TUC_Merged)
	{
		mcUndoLog.Merged(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
	}
	else if (psUndo->eChange == TUC_Undone)
	{
		uiGroup = mcUndoLog.Undone(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
		UndoLogGroup(uiGroup, false);
	}
	else if (psUndo->eChange == TUC_Redone)
	{
		uiGroup = mcUndoLog.Redone(pTab->GetFileName(), psUndo->uiIdBefore, psUndo->uiIdAfter);
		RedoLogGroup(uiGroup, false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Undo or redo in an editor (its keys, or Edit's Undo and Redo): the
// newest edit of any open file, or the one undone last, through the
// revealing undo; an edit in a file that isn't active, or out of view, is
// shown first, the status line saying so.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::PressUndo(bool bRedo)
{
	CChars	szKeys;
	CChars	szMessage;

	szKeys.Init();
	if ((mpcContent->GetWindow()->GetComponentInput() == NULL) || !mpcContent->GetWindow()->GetComponentInput()->GetShortcutText(bRedo ? CIC_Redo : CIC_Undo, &szKeys))
	{
		szKeys.Append(bRedo ? "Redo" : "Undo");
	}
	szMessage.Init();
	mcUndo.Press(bRedo, szKeys.Text(), &szMessage);
	if (!szMessage.Empty())
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	szKeys.Kill();
}

//////////////////////////////////////////////////////////////////////////
//
// The open file the next undo (or redo) across files would show: the one
// it would undo last, as UndoLogGroup and RedoLogGroup go.  NULL if
// there's nothing to undo, or none of its files is open.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEEdit::FindGroupTab(bool bRedo)
{
	size				uiGroup;
	size				i;
	size				uiStep;
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEEditorTab>	pFound;

	uiGroup = bRedo ? mcUndoLog.FindRedoGroup() : mcUndoLog.FindUndoGroup();
	if (uiGroup == MAX_SIZE)
	{
		return NULL;
	}
	for (i = 0; i < mcUndoLog.NumSteps(); i++)
	{
		uiStep = bRedo ? i : (mcUndoLog.NumSteps() - 1 - i);
		psStep = mcUndoLog.Get(uiStep);
		if ((psStep->uiGroup == uiGroup) && !psStep->bRename && (mcUndoLog.IsUndone(uiStep) == bRedo))
		{
			pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
			if (pTab.IsNotNull())
			{
				pFound = pTab;
			}
		}
	}
	return pFound;
}


//////////////////////////////////////////////////////////////////////////
//
// In view if its file is the one active and its editor shows the edit;
// with no open file to show (a group of renames), as it is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::IsUndoShown(bool bRedo)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindGroupTab(bRedo);
	if (pTab.IsNull())
	{
		return true;
	}
	return (pTab == mpcContent->GetDocuments()->GetActiveTab()) && pTab->GetEditor()->IsUndoShown(bRedo);
}


//////////////////////////////////////////////////////////////////////////
//
// Its file is opened, and its editor scrolled to the edit.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::ShowUndo(bool bRedo)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindGroupTab(bRedo);
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->OpenFile(pTab->GetFileName(), pTab->GetProject());
	pTab->GetEditor()->ShowUndo(bRedo);
	return true;
}


bool CIDEEdit::DoUndo(bool bRedo)
{
	return bRedo ? RedoLogGroup(mcUndoLog.FindRedoGroup(), true) : UndoLogGroup(mcUndoLog.FindUndoGroup(), true);
}


//////////////////////////////////////////////////////////////////////////
//
// "in CalculatorParser.cpp at line 36".
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::AppendUndoWhere(bool bRedo, CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	char*				szFile;
	char*				szLeaf;

	pTab = FindGroupTab(bRedo);
	if (pTab.IsNull())
	{
		return;
	}
	szFile = pTab->GetFileName();
	szLeaf = szFile;
	for (; *szFile != '\0'; szFile++)
	{
		if ((*szFile == '/') || (*szFile == '\\'))
		{
			szLeaf = szFile + 1;
		}
	}
	pszDest->Append("in ");
	pszDest->Append(szLeaf);
	pszDest->Append(' ');
	pTab->GetEditor()->AppendUndoWhere(bRedo, pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// Undoes the steps of uiGroup not undone yet, newest first, each in its
// own file.  A file edited since (its text isn't as the step left it)
// isn't changed: its later edits would go too.  A file Rename File
// renamed is renamed back, before the group's edits are undone, as they
// are kept under its name.  With bShow the last file undone is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::UndoLogGroup(size uiGroup, bool bShow)
{
	size				i;
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEEditorTab>	pShown;
	bool				bRenames;

	if (uiGroup == MAX_SIZE)
	{
		return false;
	}
	bRenames = false;
	KeepCreatedTexts(uiGroup);
	mbLogUndoing = true;
	for (i = mcUndoLog.NumSteps(); i > 0; i--)
	{
		psStep = mcUndoLog.Get(i - 1);
		if ((psStep->uiGroup == uiGroup) && !mcUndoLog.IsUndone(i - 1))
		{
			if (psStep->bDeleted)
			{
				//A file Safe Delete deleted, made again as it was.
				bRenames = true;
				if (!mpcContent->GetMoveClass()->RecreateFile(psStep->szDocument.Text(), psStep->szRenamed.Text(), psStep->szContent.Text()))
				{
					ShowRenameNotMoved(psStep->szRenamed.Text(), psStep->szDocument.Text());
				}
				mcUndoLog.MarkUndone(i - 1);
				continue;
			}
			if (psStep->bCreated)
			{
				//A file a refactoring made, taken away.  Its tab's closing forgets the tab's edits: the step is found
				//again by what it is.
				bRenames = true;
				mcUndoLog.MarkUndone(i - 1);
				if (!RemoveCreatedFile(psStep))
				{
					ShowRenameNotMoved(psStep->szDocument.Text(), (char*)"nothing: it couldn't be deleted");
				}
				i = mcUndoLog.NumSteps() + 1;
				continue;
			}
			if (psStep->bRename)
			{
				//A file Rename File renamed, renamed back.
				bRenames = true;
				if (!mpcContent->GetFileRename()->MoveFile(psStep->szRenamed.Text(), psStep->szDocument.Text(), NULL))
				{
					ShowRenameNotMoved(psStep->szRenamed.Text(), psStep->szDocument.Text());
				}
			}
			else
			{
				pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
				if (pTab.IsNotNull() && (pTab->GetEditor()->GetUndoId() == psStep->uiIdAfter))
				{
					pTab->GetEditor()->UndoTo(psStep->uiIdBefore);
					pShown = pTab;
				}
			}
			mcUndoLog.MarkUndone(i - 1);
		}
	}
	mbLogUndoing = false;
	if (bRenames)
	{
		RenamesChanged(uiGroup);
	}

	if (bShow && pShown.IsNotNull())
	{
		mpcContent->GetDocuments()->OpenFile(pShown->GetFileName(), pShown->GetProject());
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Redoes the undone steps of uiGroup, oldest first: Rename File's edits,
// then its renames.  With bShow the last file redone is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::RedoLogGroup(size uiGroup, bool bShow)
{
	size				i;
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEEditorTab>	pShown;
	bool				bRenames;

	if (uiGroup == MAX_SIZE)
	{
		return false;
	}
	bRenames = false;
	mbLogUndoing = true;
	for (i = 0; i < mcUndoLog.NumSteps(); i++)
	{
		psStep = mcUndoLog.Get(i);
		if ((psStep->uiGroup == uiGroup) && mcUndoLog.IsUndone(i))
		{
			if (psStep->bDeleted)
			{
				//Deleted again.  Its tab's closing forgets the tab's edits: the steps are looked through from the start.
				bRenames = true;
				mcUndoLog.MarkRedone(i);
				if (!RemoveCreatedFile(psStep))
				{
					ShowRenameNotMoved(psStep->szDocument.Text(), (char*)"nothing: it couldn't be deleted");
				}
				i = MAX_SIZE;
				continue;
			}
			if (psStep->bCreated)
			{
				//Made again, with the text it had.
				bRenames = true;
				if (!mpcContent->GetMoveClass()->RecreateFile(psStep->szDocument.Text(), psStep->szRenamed.Text(), psStep->szContent.Text()))
				{
					ShowRenameNotMoved(psStep->szRenamed.Text(), psStep->szDocument.Text());
				}
			}
			else if (psStep->bRename)
			{
				bRenames = true;
				if (!mpcContent->GetFileRename()->MoveFile(psStep->szDocument.Text(), psStep->szRenamed.Text(), NULL))
				{
					ShowRenameNotMoved(psStep->szDocument.Text(), psStep->szRenamed.Text());
				}
			}
			else
			{
				pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
				if (pTab.IsNotNull() && (pTab->GetEditor()->GetUndoId() == psStep->uiIdBefore))
				{
					pTab->GetEditor()->UndoTo(psStep->uiIdAfter);
					pShown = pTab;
				}
			}
			mcUndoLog.MarkRedone(i);
		}
	}
	mbLogUndoing = false;
	if (bRenames)
	{
		RenamesChanged(uiGroup);
	}

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
Ptr<CMenuContainer> CIDEEdit::GetEditMenu(void) const { return mpEditMenu; }
void CIDEEdit::EndUndoGroup(void) { mcUndoLog.EndGroup(); }


//////////////////////////////////////////////////////////////////////////
//
// szFileName has closed: its edits are forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::FileClosed(char* szFileName) { mcUndoLog.RemoveDocument(szFileName); }
void CIDEEdit::FileRenamed(char* szOldFileName, char* szNewFileName) { mcUndoLog.RenameDocument(szOldFileName, szNewFileName); }
void CIDEEdit::LogFileRenamed(char* szOldFileName, char* szNewFileName) { mcUndoLog.Renamed(szOldFileName, szNewFileName); }
void CIDEEdit::LogFileCreated(char* szFileName, char* szBeside) { mcUndoLog.Created(szFileName, szBeside); }
void CIDEEdit::LogFileDeleted(char* szFileName, char* szBeside, char* szContent) { mcUndoLog.Deleted(szFileName, szBeside, szContent); }


//////////////////////////////////////////////////////////////////////////
//
// Before a group is undone: the text of each file the group made, as its
// editor has it (or the disk, if it isn't open), kept in its step for a
// redo to make it again.  The edits that wrote it are undone first, so it
// is taken now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::KeepCreatedTexts(size uiGroup)
{
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;
	CTextFile			cFile;
	size				i;

	for (i = 0; i < mcUndoLog.NumSteps(); i++)
	{
		psStep = mcUndoLog.Get(i);
		if ((psStep->uiGroup != uiGroup) || !psStep->bCreated || psStep->bDeleted || mcUndoLog.IsUndone(i))
		{
			continue;
		}
		pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
		if (pTab.IsNotNull())
		{
			psStep->szContent.Set(pTab->GetEditor()->GetText());
		}
		else
		{
			cFile.Init();
			psStep->szContent.Clear();
			if (cFile.Read(psStep->szDocument.Text()))
			{
				psStep->szContent.AppendSubString(cFile.Text(), (size)cFile.Length());
			}
			cFile.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A file a refactoring made, taken away: its tab closed without asking
// (its text is kept in the step), and the file off the disk, out of the
// projects and out of git's index.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEdit::RemoveCreatedFile(SUndoLogStep* psStep)
{
	Ptr<CIDEEditorTab>	pTab;
	CChars				szFileName;
	bool				bRemoved;

	szFileName.Init(psStep->szDocument);
	pTab = mpcContent->GetDocuments()->FindTab(szFileName.Text());
	if (pTab.IsNotNull())
	{
		pTab->ForgetModified();
		pTab = NULL;
		mpcContent->GetDocuments()->CloseFile(szFileName.Text());
	}
	bRemoved = mpcContent->GetMoveClass()->RemoveCreatedFile(szFileName.Text());
	szFileName.Kill();
	return bRemoved;
}


//////////////////////////////////////////////////////////////////////////
//
// Files of uiGroup were renamed back, or again: its files are saved, as
// Rename File saves them, and the workspace is shown and indexed again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::RenamesChanged(size uiGroup)
{
	size				i;
	SUndoLogStep*		psStep;
	Ptr<CIDEEditorTab>	pTab;

	for (i = 0; i < mcUndoLog.NumSteps(); i++)
	{
		psStep = mcUndoLog.Get(i);
		if ((psStep->uiGroup == uiGroup) && !psStep->bRename && !psStep->bCreated && !psStep->bDeleted)
		{
			pTab = mpcContent->GetDocuments()->FindTab(psStep->szDocument.Text());
			if (pTab.IsNotNull())
			{
				mpcContent->GetDocuments()->SaveFile(pTab);
			}
		}
	}
	mpcContent->GetCodeModel()->Reset();
	mpcContent->GetSolutions()->WorkspaceChanged(true);
}


//////////////////////////////////////////////////////////////////////////
//
// The status line says a file couldn't be renamed (the name was taken,
// say), so the undo or redo is only part done.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEdit::ShowRenameNotMoved(char* szFrom, char* szTo)
{
	CChars	szMessage;

	szMessage.Init("Couldn't rename ");
	szMessage.Append(szFrom);
	szMessage.Append(" to ");
	szMessage.Append(szTo);
	szMessage.Append(".");
	mpcContent->ShowStatus(szMessage.Text());
	szMessage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditCommands::Init(CIDEContent* pcContent, Ptr<CCommandSet> pCommands)
{
	mpcContent = pcContent;
	CEditCommands::Init(pcContent->GetWindow(), pCommands);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditCommands::Class(void)
{
	CEditCommands::Class();
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
// The active document's editor, NULL with no document open.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEEditCommands::GetDefaultTarget(void)
{
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return NULL;
	}
	return mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
}
