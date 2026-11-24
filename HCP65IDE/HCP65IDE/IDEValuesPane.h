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
#ifndef __IDE_VALUES_PANE_H__
#define __IDE_VALUES_PANE_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/MapStringString.h"
#include "StandardLib/Object.h"
#include "WindowLib/TreeTable.h"
#include "IDEDebugStop.h"


typedef CArrayTemplate<SCPP65DebugValue>	CArrayIDEDebugValues;


//A debugger window's tree of values, as Visual Studio's Locals, Autos and Watch are (and QuickWatch's and a data
//tip's): Name | Value | Type, each value's members or elements under it, read only once it is opened (until then one
//empty child, so its chevron shows).  Its owner gives the values at the top (Show) at each stop and whenever memory it
//waited for comes; which values are open, and which is selected, are kept by their paths (their names from the top
//down) from one showing to the next, and a value that isn't what it was at the stop before is red.
//
//A value typed into its Value cell is written (CIDEDebugStop::Write), and the owner told (the written event) to show
//every window again.  A Watch's names are typed too (bWatch): a new expression in its empty last row, or one changed or
//emptied (the renamed event: GetEditedRoot, GetEditedName), for its owner to keep.
class CIDEValuesPane : public CObject
{
CONSTRUCTABLE(CIDEValuesPane);
DESTRUCTABLE(CIDEValuesPane);
protected:
	Ptr<CTreeTable>						mpTable;
	Ptr<CFont>							mpChangedFont;
	CIDEDebugStop*						mpcStop;		//Not held: the debugger's.
	bool								mbWatch;
	CIDEDebugRoots						mcRoots;
	CArrayChars							maszPaths;		//Each node's path, "" for an unread child or the empty row.
	CArrayIDEDebugValues				masValues;		//Each node's value, pcRecords NULL for none.
	CArrayInt							maiRootNodes;	//Each root's node, and the empty row's last.
	CArrayChars							maszOpen;		//The open values' paths.
	CMapStringString					macShown[2];	//Each path's Value as shown at a stop, and at the stop before.
	size								muiShown;		//Which of them is the latest stop's.
	size								muiStop;		//The stop it is (CIDEDebugStop::GetStop), 0 for none yet.
	bool								mbShowing;		//The nodes are being made: their opening isn't a user's.
	bool								mbOpened;		//A value was opened: its children are to be read (Poll).
	int32								miEditedRoot;
	CChars								mszEditedName;
	CComponentEvent<CIDEValuesPane>		mcWritten;
	CComponentEvent<CIDEValuesPane>		mcRenamed;

public:
	void								Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pChangedFont, CIDEDebugStop* pcStop, bool bWatch);
	void								Class(void) override;
	void								Free(void) override;
	bool								Save(CObjectWriter* pcFile) override;
	bool								Load(CObjectReader* pcFile) override;

	void								Show(CIDEDebugRoots* pcRoots);
	void								Poll(void);
	bool								Open(const char* szPath);
	bool								WriteValue(const char* szPath, const char* szText);
	void								Print(CChars* pszDest);

	Ptr<CComponent>						GetComponent(void) const;
	Ptr<CTreeTable>						GetTable(void) const;
	CComponentEvent<CIDEValuesPane>*	GetWrittenEvent(void);
	CComponentEvent<CIDEValuesPane>*	GetRenamedEvent(void);
	int32								GetEditedRoot(void) const;
	const char*							GetEditedName(void);

protected:
	void								Rebuild(void);
	int32								AddValue(int32 iParent, const char* szName, const char* szPath, SCPP65DebugValue* psValue, const char* szError);
	void								KeepOpen(void);
	size								FindOpen(const char* szPath);
	int32								FindPath(const char* szPath);
	void								Edited(CTreeTable* pcTable);
	void								Expanded(CTreeContainer* pcTree, int32 iNode);
	void								PrintNode(int32 iNode, size uiDepth, CChars* pszDest);
};


#endif // __IDE_VALUES_PANE_H__
