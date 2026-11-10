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
#ifndef __IDE_USAGES_H__
#define __IDE_USAGES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/TreeContainer.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/CheckBox.h"
#include "IDECodeModel.h"


class CIDEContent;


//How Find Usages groups what it found (CIDEUsages::muiGrouping): above each file by how each is used and by project,
//and under each file by the class it is in.
#define IDE_USAGES_BY_TYPE		0x01
#define IDE_USAGES_BY_PROJECT	0x02
#define IDE_USAGES_BY_CLASS		0x04
#define IDE_USAGES_BY_ALL		(IDE_USAGES_BY_TYPE | IDE_USAGES_BY_PROJECT | IDE_USAGES_BY_CLASS)


//IntelliJ's Find Usages (Alt+F7): every use of the symbol at the caret, not its declarations, in the Find Usages tool
//window beside Build: a tree of how each is used (a call, a value read, a type in a field's declaration...), the
//project, the file, the class in it, and each line, in the order it is in its file, which opens the place.  Check boxes
//above it turn the usage type, project and class groups on and off; with them off it is files and their lines.  Find
//All References (Shift+F12) shows its declarations and definition there too.
class CIDEUsages : public CObject
{
CONSTRUCTABLE(CIDEUsages);
DESTRUCTABLE(CIDEUsages);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CTreeContainer>	mpUsages;  //What Find Usages found: usage types, projects, files, classes, lines.
	Ptr<CInputAction>	mpEscapeAction;  //Escape in the tree goes back to the editor.
	Ptr<CDockPane>		mpUsagesTool;
	Ptr<CCheckBox>		mpByType;
	Ptr<CCheckBox>		mpByProject;
	Ptr<CCheckBox>		mpByClass;
	CArrayIDELocations	masUsages;  //Each line's place, by its node's data less one.
	CChars				mszName;  //The symbol found's name, and how many overrides it has, for the tree's top.
	size				muiOverrides;
	bool				mbDeclarations;  //Find All References: the declarations and definition are there.
	bool				mbFound;  //Something has been looked for.
	uint8				muiGrouping;  //IDE_USAGES_BY_*.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	void				AddEditorItems(Ptr<CMenuContainer> pMenu);
	bool				CanFindUsages(void);
	void				CreatePane(SDockPlace* psPlace);
	bool				FindUsages(bool bDeclarations);
	void				Clear(void);
	void				SetGrouping(uint8 uiGrouping);
	uint8				GetGrouping(void);
	Ptr<CTreeContainer>	GetUsagesTree(void);
	Ptr<CDockPane>		GetTool(void);

	static const char*	GetUsageTypeName(SIDELocation* psLocation);
	static int			GetUsageTypeRank(SIDELocation* psLocation);

protected:
	void				ShowUsages(void);
	void				UsageActivated(void);
	void				ReturnToEditor(void);
	void				GroupingChanged(void);
	void				FindUsagesOnly(void);
};


#endif // __IDE_USAGES_H__

