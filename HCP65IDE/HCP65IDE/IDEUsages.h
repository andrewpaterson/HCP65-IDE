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
#include "IDESteppable.h"


class CIDEContent;
class CIDEFileLines;
class CMarkupTag;
class CXMLWriter;
struct SIDEPlaceChange;


//How Find Usages groups what it found (CIDEUsages::muiGrouping): above each file by how each is used and by project,
//and under each file by the class it is in.
#define IDE_USAGES_BY_TYPE		0x01
#define IDE_USAGES_BY_PROJECT	0x02
#define IDE_USAGES_BY_CLASS		0x04
#define IDE_USAGES_BY_ALL		(IDE_USAGES_BY_TYPE | IDE_USAGES_BY_PROJECT | IDE_USAGES_BY_CLASS)


//A usage Find Usages found: its place, which follows its file's edits while the file is open (CIDEDocuments tells them),
//and what the tree groups it by, its file's project and the class it is in.  They are worked out when it is found, not
//when the tree is grouped: the index can have been built again by then.
struct SIDEUsage
{
	SIDELocation	sLocation;
	CChars			szProject;
	CChars			szClass;	//Empty if it is in none.
};


typedef CArrayTemplate<SIDEUsage>	CArrayIDEUsages;


//What was found of a symbol: its name, how many overrides it has (their usages are with its own) and the usages, in the
//order of their files' names, then their lines and columns.  With bDeclarations its declarations and definition are
//among them.
struct SIDEFoundUsages
{
	CChars			szName;
	size			uiOverrides;
	bool			bDeclarations;
	CArrayIDEUsages	asUsages;

	void	Init(void);
	void	Kill(void);
	void	Clear(void);
	bool	AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	void	AppendTitle(CChars* pszDest);
};


struct SIDEUsageRow;
//IntelliJ's Find Usages (Alt+F7): every use of the symbol at the caret, not its declarations, in the Find Usages tool
//window beside Build: a tree of how each is used (a call, a value read, a type in a field's declaration...), the
//project, the file, the class in it, and each line, in the order it is in its file, which opens the place.  Check boxes
//above it turn the usage type, project and class groups on and off; with them off it is files and their lines.  Find
//All References (Shift+F12) shows its declarations and definition there too.  Control+B on a declaration shows the
//usages in a popup under the caret.
class CIDEUsages : public CObject, public CIDESteppable
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
	SIDEFoundUsages		msFound;  //What the tree shows, if mbFound: each line's usage by its node's data less one.
	bool				mbFound;  //Something has been looked for.
	bool				mbMoved;  //Edits have moved usages since the tree was shown, the last at miMoved.
	int64				miMoved;
	Ptr<CMenuContainer>	mpPopup;  //Control+B's usages on a declaration, made when it is first shown.
	SIDEFoundUsages		msPopupFound;  //What the popup shows: each row's usage by its row less one (the first is its title).
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
	bool				IsOnOwnDeclaration(void);
	bool				ShowUsagesPopup(void);
	Ptr<CMenuContainer>	GetUsagesPopup(void);
	void				Clear(void);
	void				AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	int64				Poll(int64 iNow);
	void				SetGrouping(uint8 uiGrouping);
	void				ReadSettings(CMarkupTag* pcSettings);
	void				WriteSettings(CXMLWriter* pcWriter);
	uint8				GetGrouping(void);
	Ptr<CTreeContainer>	GetUsagesTree(void);
	Ptr<CDockPane>		GetTool(void);

	Ptr<CDockPane>		GetSteppablePane(void) override;
	const char*			GetSteppableName(void) override;
	bool				HasSteps(void) override;
	bool				Step(bool bNext, bool bKeepKeys) override;

protected:
	bool				Find(bool bDeclarations, SIDEFoundUsages* psDest);
	void				ShowUsages(bool bKeepSelected);
	int32				AddGroupNode(int32 iParent, const char* szName, size uiCount, const char* szWhat);
	int32				AddFileNode(int32 iParent, SIDEUsageRow* pasRows, size uiRows, size uiFirst, size uiCount);
	void				AddFileUsages(int32 iFile, SIDEUsageRow* pasRows, size uiCount, CIDEFileLines* pcLines);
	void				AddUsageLine(int32 iParent, SIDEUsageRow* psRow, CIDEFileLines* pcLines);
	int32				FindUsageNode(int64 iData);
	SIDEUsage*			GetNodeUsage(int32 iNode);
	void				OpenPopup(void);
	void				UsageActivated(void);
	void				ReturnToEditor(void);
	void				PopupUsageClicked(CMenuItem* pcItem);
	void				GroupingChanged(void);
	void				ShowGrouping(uint8 uiGrouping);
	void				FindUsagesOnly(void);
};


#endif // __IDE_USAGES_H__

