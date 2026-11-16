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
#ifndef __IDE_EDIT_H__
#define __IDE_EDIT_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/UndoLog.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/EditCommands.h"
#include "WindowLib/RevealingUndo.h"


class CIDEContent;
class CIDEEditorTab;


//Edit's Undo, Redo, Cut, Copy, Paste and Select All: of the text the keys last went to (a document, the Build output,
//Claude's request, a text field), or of the active document once that has gone.
class CIDEEditCommands : public CEditCommands
{
CONSTRUCTABLE(CIDEEditCommands);
DESTRUCTABLE(CIDEEditCommands);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.

public:
	void				Init(CIDEContent* pcContent, Ptr<CCommandSet> pCommands);
	void				Class(void) override;
	Ptr<CComponent>		GetDefaultTarget(void) override;
};


//The Edit menu, as IntelliJ's: Undo, Redo, Cut, Copy, Paste and Select All (CIDEEditCommands).  In the editors Undo
//and Redo (Ctrl+Z and Ctrl+Shift+Z) go back and forward through the edits of every open file in the order they were
//made (the undo log, fed by the editors' undo events), not only the file's own.  A file renamed is renamed back, and
//a file a refactoring made is taken away again (off the disk, out of the projects and out of git's index), its text
//kept for a redo to make it again.
//
//Undo doesn't undo what can't be seen (CRevealingUndo): an edit in a file that isn't the one active, or that its
//editor is scrolled away from, is shown by the first press, the status line saying to press again, and undone by the
//next.
class CIDEEdit : public CObject, public CRevealingUndoHost, public CTextUndoPresser
{
CONSTRUCTABLE(CIDEEdit);
DESTRUCTABLE(CIDEEdit);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>		mpEditMenu;
	Ptr<CIDEEditCommands>	mpEditCommands;
	CUndoLog				mcUndoLog;  //Every open file's edits in the order they were made, by file name.
	bool					mbLogUndoing;  //The undo log is undoing or redoing: the editors' undo events are its own.
	CRevealingUndo			mcUndo;  //Undo and Redo, shown before they're done.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	Ptr<CMenuContainer>	CreateEditMenu(void);
	Ptr<CMenuContainer>	GetEditMenu(void);
	void				EditorUndoChanged(CTextArea* pcEditor, STextUndoChange* psUndo);
	void				FileClosed(char* szFileName);
	void				FileRenamed(char* szOldFileName, char* szNewFileName);
	void				LogFileRenamed(char* szOldFileName, char* szNewFileName);
	void				LogFileCreated(char* szFileName, char* szBeside);
	void				LogFileDeleted(char* szFileName, char* szBeside, char* szContent);
	void				PressUndo(bool bRedo) override;
	bool				CanPressUndo(bool bRedo) override;
	void				BeginUndoGroup(void);
	void				EndUndoGroup(void);

protected:
	bool				UndoLogGroup(size uiGroup, bool bShow);
	bool				RedoLogGroup(size uiGroup, bool bShow);
	void				RenamesChanged(size uiGroup);
	void				ShowRenameNotMoved(char* szFrom, char* szTo);
	void				KeepCreatedTexts(size uiGroup);
	bool				RemoveCreatedFile(SUndoLogStep* psStep);

	Ptr<CIDEEditorTab>	FindGroupTab(bool bRedo);
	bool				IsUndoShown(bool bRedo) override;
	bool				ShowUndo(bool bRedo) override;
	bool				DoUndo(bool bRedo) override;
	void				AppendUndoWhere(bool bRedo, CChars* pszDest) override;
};


#endif // __IDE_EDIT_H__

