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
#include "WindowLib/TabbedContainer.h"
#include "IDEResultsPane.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/CommandSet.h"
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


//What was found of a symbol: its name and key, where it is declared, how many overrides it has (their usages are with its own) and the usages, in the
//order of their files' names, then their lines and columns.  With bDeclarations its declarations and definition are
//among them.
struct SIDEFoundUsages
{
	CChars			szName;
	CChars			szKey;			//The symbol's key in the index.
	SIDELocation	sDeclaration;	//Where it is defined, or without a definition declared, if bDeclared: it follows edits too.
	bool			bDeclared;
	size			uiOverrides;
	bool			bDeclarations;
	CArrayIDEUsages	asUsages;

	void	Init(void);
	void	Kill(void);
	void	Clear(void);
	bool	AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	void	AppendTitle(CChars* pszDest);
};


//One Find Usages, in a tab of the tool window: its results pane, what it found (if it has found anything: a new tab
//hasn't), whether edits have moved its usages since its tree was made, whether the groupings or filters have changed
//since (it is made again when its tab is chosen), and whether it is pinned.
class CIDEUsagesSearch : public CObject
{
CONSTRUCTABLE(CIDEUsagesSearch);
DESTRUCTABLE(CIDEUsagesSearch);
public:
	Ptr<CIDEResultsPane>	mpPane;
	Ptr<CTabComponent>		mpTab;
	SIDEFoundUsages			msFound;  //What the tree shows, if mbFound: each line's usage by its node's data less one.
	bool					mbFound;
	bool					mbMoved;  //Edits have moved usages since the tree was shown, the last at miMoved.
	int64					miMoved;
	bool					mbStale;
	bool					mbPinned;

	void	Init(Ptr<CIDEResultsPane> pPane);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	SetFound(SIDEFoundUsages* psFound);
};


struct SIDEUsageRow;
//IntelliJ's Find Usages (Alt+F7): every use of the symbol at the caret, not its declarations, in the Find Usages tool
//window beside Build: a tree of how each is used (a call, a value read, a type in a field's declaration...), the
//project, the file, the class in it, and each line, in the order it is in its file, which opens the place.  Buttons on
//its toolbar turn the usage type, project and class groups on and off; with them off it is files and their lines.
//Rerun finds the symbol's usages again, from where it is declared, wherever the caret is now.  Show Read Access and
//Show Write Access leave a variable's or field's usages that only read it, or that write it, out of the tree, as
//IntelliJ's do; the tree's first row says how many are hidden.
//
//Each Find Usages shows what it finds in the tool window's tab that is chosen, named for the symbol, unless that tab
//is pinned (Pin Tab, on the toolbar): then it is left as it is and a new tab is made.  A tab is closed by its cross;
//the tool window always has one, empty until something is found, and its tab bar is only shown with more than one.
//The groupings and filters are the tool window's, the same for every tab.  Find
//All References (Shift+F12) shows its declarations and definition there too.  Control+B on a declaration shows the
//usages in a popup under the caret.  The tree is a results pane's (CIDEResultsPane), which opens and steps through
//the usages: a line's node's data is its usage's index in what was found, and one.
class CIDEUsages : public CObject, public CIDESteppable, public CIDEResultsSource
{
CONSTRUCTABLE(CIDEUsages);
DESTRUCTABLE(CIDEUsages);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CTabbedContainer>	mpTabs;  //A tab for each search, its page the search's results pane.
	CArray<CIDEUsagesSearch>	maSearches;
	Ptr<CIDEUsagesSearch>	mpSearch;  //The search whose tab is chosen: never NULL once the tool window is made.
	Ptr<CDockPane>		mpUsagesTool;
	Ptr<CMenuContainer>	mpPopup;  //Control+B's usages on a declaration, made when it is first shown.
	SIDEFoundUsages		msPopupFound;  //What the popup shows: each row's usage by its row less one (the first is its title).
	uint8				muiGrouping;  //IDE_USAGES_BY_*.
	bool				mbShowRead;  //Usages that read a value are in the tree.
	bool				mbShowWrite;  //Usages that write one are.

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
	void				SetAccessShown(bool bRead, bool bWrite);
	void				SetPinned(bool bPinned);
	size				NumSearches(void);
	Ptr<CTabbedContainer>	GetTabs(void);
	void				ReadSettings(CMarkupTag* pcSettings);
	void				WriteSettings(CXMLWriter* pcWriter);
	uint8				GetGrouping(void);
	Ptr<CTreeContainer>	GetUsagesTree(void);
	Ptr<CIDEResultsPane>	GetResultsPane(void);
	Ptr<CDockPane>		GetTool(void);

	Ptr<CDockPane>		GetSteppablePane(void) override;
	const char*			GetSteppableName(void) override;
	bool				HasSteps(void) override;
	bool				Step(bool bNext, bool bKeepKeys) override;
	bool				GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;
	bool				GetDeclarationLocation(CIDEResultsPane* pcPane, SIDELocation* psDest) override;
	bool				CanRerunResults(CIDEResultsPane* pcPane) override;
	void				RerunResults(CIDEResultsPane* pcPane) override;

protected:
	bool				Find(bool bDeclarations, SIDEFoundUsages* psDest);
	void				ShowUsages(bool bKeepSelected);
	int32				AddGroupNode(int32 iParent, const char* szName, size uiCount, const char* szWhat);
	int32				AddFileNode(int32 iParent, SIDEUsageRow* pasRows, size uiRows, size uiFirst, size uiCount);
	void				AddFileUsages(int32 iFile, SIDEUsageRow* pasRows, size uiCount, CIDEFileLines* pcLines);
	void				AddUsageLine(int32 iParent, SIDEUsageRow* psRow, CIDEFileLines* pcLines);
	void				OpenPopup(void);
	void				PopupUsageClicked(CMenuItem* pcItem);
	void				ShowGrouping(uint8 uiGrouping);
	void				AddPaneCommand(Ptr<CIDEResultsPane> pPane, const char* szName, const char* szCaption, EIDEToolIcon eIcon, void(CIDEUsages::*fToggle)(void), bool(CIDEUsages::*fIsOn)(void));
	Ptr<CIDEUsagesSearch>	AddSearch(void);
	void				RemoveSearch(Ptr<CIDEUsagesSearch> pSearch, bool bKeepOne);
	Ptr<CIDEUsagesSearch>	FindSearch(CIDEResultsPane* pcPane);
	Ptr<CIDEUsagesSearch>	FindSearch(CTabComponent* pcTab);
	void				TabSelected(CTabComponent* pcTab);
	void				TabCloseRequested(CTabComponent* pcTab);
	void				TogglePinned(void);
	bool				IsPinned(void);
	void				ShowAllUsages(void);
	void				ToggleByType(void);
	void				ToggleByProject(void);
	void				ToggleByClass(void);
	bool				IsByType(void);
	bool				IsByProject(void);
	bool				IsByClass(void);
	void				ToggleShowRead(void);
	void				ToggleShowWrite(void);
	bool				IsShowRead(void);
	bool				IsShowWrite(void);
	bool				IsUsageShown(SIDEUsage* psUsage);
	bool				FindAt(const char* szFileName, int iLine, int iColumn, bool bDeclarations, SIDEFoundUsages* psDest);
	void				FindUsagesOnly(void);
};


#endif // __IDE_USAGES_H__

