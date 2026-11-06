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
#ifndef __IDE_EDIT_H__
#define __IDE_EDIT_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/UndoLog.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/TextField.h"
#include "WindowLib/Focus.h"


class CIDEContent;


//The Edit menu, as IntelliJ's: Undo, Redo, Cut, Copy, Paste and Select All of the text the keys last went to (a
//document, the Build output, Claude's request, a text field; the active document if that has gone), and Undo and
//Redo Across Files, which go back and forward through the edits of every open file in the order they were made
//(the undo log, fed by the editors' undo events).
class CIDEEdit : public CObject
{
CONSTRUCTABLE(CIDEEdit);
DESTRUCTABLE(CIDEEdit);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>	mpEditMenu;
	Ptr<CMenuItem>		mpUndoItem;  //Edit, as IntelliJ's: Undo, Redo, Cut, Copy, Paste and Select All of the
	Ptr<CMenuItem>		mpRedoItem;  //active document,
	Ptr<CMenuItem>		mpCutItem;
	Ptr<CMenuItem>		mpCopyItem;
	Ptr<CMenuItem>		mpPasteItem;
	Ptr<CMenuItem>		mpSelectAllItem;
	Ptr<CMenuItem>		mpUndoAllItem;  //and Undo and Redo Across Files, in the order the edits were made.
	Ptr<CMenuItem>		mpRedoAllItem;
	CUndoLog			mcUndoLog;  //Every open file's edits in the order they were made, by file name.
	bool				mbLogUndoing;  //The undo log is undoing or redoing: the editors' undo events are its own.
	Ptr<CComponent>		mpEditTarget;  //The text area or text field the keys last went to: what Edit acts on.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	Ptr<CMenuContainer>	CreateEditMenu(void);
	bool				EditMenuItemClicked(CMenuItem* pcItem);
	void				EditMenuOpening(CMenuContainer* pcMenu);
	Ptr<CMenuContainer>	GetEditMenu(void);
	void				EditorUndoChanged(CTextArea* pcEditor, STextAreaUndo* psUndo);
	void				FileClosed(char* szFileName);
	bool				UndoAcrossFiles(void);
	bool				RedoAcrossFiles(void);
	void				BeginUndoGroup(void);
	void				EndUndoGroup(void);
	void				FocusChanged(CFocus* pcFocus);

protected:
	bool				IsEditTargetShown(void);
	Ptr<CTextField>		GetEditField(void);
	Ptr<CTextArea>		GetEditArea(void);
	bool				UndoLogGroup(size uiGroup, bool bShow);
	bool				RedoLogGroup(size uiGroup, bool bShow);
};


#endif // __IDE_EDIT_H__

