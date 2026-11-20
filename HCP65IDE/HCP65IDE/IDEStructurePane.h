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
#ifndef __IDE_STRUCTURE_PANE_H__
#define __IDE_STRUCTURE_PANE_H__
#include "BaseLib/ArrayInt.h"
#include "StandardLib/Object.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TreeContainer.h"
#include "IDEOutline.h"


class CIDEContent;
class CIDEEditorTab;


//The Structure tool window's tree, as IntelliJ IDEA's Structure: the active editor's outline, each entry under the
//one it is in with its kind's icon, its name and its parameters, every node open.  It is made again when another
//editor is active or the edits thread has parsed the file again, and the entry the caret is in is selected as the
//caret moves (Update, at each tick).  A click or Enter on a row puts the caret on its entry's name, in the editor.
//Typing finds a row.  Its menu sorts the entries of each node by name, in place of the order they are in the file,
//and leaves fields out; both are kept with the settings (CIDEStructure has them).
class CIDEStructurePane : public CObject
{
CONSTRUCTABLE(CIDEStructurePane);
DESTRUCTABLE(CIDEStructurePane);
protected:
	CIDEContent*			mpcContent;  //Not held.
	Ptr<CTreeContainer>		mpTree;  //A node's data is its entry in the outline.
	Ptr<CScrollPane>		mpScrollPane;
	Ptr<CMenuContainer>		mpMenu;
	Ptr<CIDEEditorTab>		mpShownTab;  //The editor whose outline the tree shows, NULL for none.
	CArrayInt				maiEntryNodes;  //Each entry's node, TREE_NO_NODE for one left out.
	size					muiShownChanges;  //The outline's change count when the tree was made.
	int32					miShownEntry;  //The entry the caret was in when a node was last selected for it.
	bool					mbSortByName;
	bool					mbShowFields;
	bool					mbDirty;  //How it is sorted or what it shows has changed since the tree was made.

public:
	void				Init(CIDEContent* pcContent);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				Update(Ptr<CIDEEditorTab> pTab);
	void				SetSortByName(bool bSortByName);
	bool				IsSortByName(void) const;
	void				SetShowFields(bool bShowFields);
	bool				IsShowFields(void) const;
	bool				ActivateRow(int32 iRow);
	void				Print(CChars* pszDest);
	Ptr<CComponent>		GetComponent(void) const;
	Ptr<CTreeContainer>	GetTree(void) const;

protected:
	void				Rebuild(CIDEOutline* pcOutline);
	void				AddEntries(CIDEOutline* pcOutline, int32 iParentEntry, int32 iParentNode);
	void				Activated(void);
	void				MenuOpening(CMenuContainer* pcMenu);
	void				MenuItemClicked(CMenuItem* pcItem);
};


#endif // __IDE_STRUCTURE_PANE_H__

