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
#ifndef __IDE_HIERARCHY_H__
#define __IDE_HIERARCHY_H__
#include "StandardLib/Object.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDEResultsPane.h"
#include "IDEResultsTabs.h"
#include "IDESteppable.h"


class CIDEContent;


#define IDE_HIERARCHY_NODES		2000	//The most nodes a hierarchy has.
#define IDE_HIERARCHY_DEPTH		64		//The deepest a hierarchy of classes is followed.
#define IDE_HIERARCHY_MORE		"..."	//Under a function whose callers haven't been looked for yet.


//What a hierarchy is of.
enum EIDEHierarchyKind
{
	IHK_Type,		//A class: its base classes and the classes derived from it.
	IHK_Method,		//A member function: the same classes, each saying whether it defines the function.
	IHK_Call,		//A function: what calls it, or what it calls.
};


//How it is shown, by the buttons on its toolbar.
enum EIDEHierarchyMode
{
	IHM_Class,			//The class under its first base class's, and so on up; the classes derived from it under it.
	IHM_Supertypes,		//The class, its base classes under it, theirs under them.
	IHM_Subtypes,		//The class, the classes derived from it under it, those derived from them under them.
	IHM_Callers,		//The function, the functions that call it under it, their callers under them as each is opened.
	IHM_Callees,		//The function, the functions it calls under it, theirs under them as each is opened.
};


//A row of a hierarchy: the class or function it is, how it is shown (its name, and after it, dim, what is said of
//it), where a press of enter goes (a class's definition; a method's in that class if the class defines it; a call),
//the row it is under (-1 at the top), and whether its own rows have been looked for and are shown.
struct SIDEHierarchyNode
{
	CChars			szKey;
	uint8			uiKind;		//ECPPSymbolKind.
	CChars			szName;
	CChars			szDetail;
	SIDELocation	sPlace;		//Its file name empty if there is nowhere to go.
	int32			iParent;
	bool			bChildrenFound;
	bool			bExpanded;
};


typedef CArrayTemplate<SIDEHierarchyNode>	CArrayIDEHierarchyNodes;


//One hierarchy shown in a tab of the Hierarchy tool window: what it is of and its rows, each known to its pane's tree
//by its index here plus one.
class CIDEHierarchySearch : public CIDEResultsSearch
{
CONSTRUCTABLE(CIDEHierarchySearch);
DESTRUCTABLE(CIDEHierarchySearch);
public:
	EIDEHierarchyKind		meKind;
	EIDEHierarchyMode		meMode;
	CChars					mszKey;			//The class or the function it is of.
	CChars					mszClassKey;	//A method's class.
	CArrayIDEHierarchyNodes	masNodes;
	int32					miSubject;		//The row that is what it is of.
	bool					mbShown;		//It has been given something to show.

	void	Init(Ptr<CIDEResultsPane> pPane);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				ClearNodes(void);
	SIDEHierarchyNode*	AddNode(int32 iParent, const char* szKey, uint8 uiKind);
};


//The Hierarchy tool window (NavigationPlan.txt's stage G), as IntelliJ IDEA's: one window with a tab for each
//hierarchy asked for (CIDEResultsTabs: a new one takes the chosen tab's place unless that is pinned).
//
//  - Type Hierarchy (Control+H): the class at the caret (a class named there, the class of a member named there, a
//    variable's type) with its base classes over it and the classes derived from it under it; its toolbar also shows
//    the base classes alone, all of them, or the derived classes alone.  Over it is its first base class, and that
//    one's first: a class with more says which after its name ("also: CNamed").
//  - Method Hierarchy (Control+Shift+H): the same classes for the member function at the caret (or the one the caret
//    is in), each saying whether it defines the function (+), inherits it (-), declares it without a body (abstract)
//    or should define it and doesn't (!).  Enter on a class that defines it goes to its function.
//  - Call Hierarchy (Control+Alt+H): the function at the caret (or the one the caret is in) with the functions that
//    call it under it, and theirs under them as each is opened, without end but for a function that calls itself;
//    or, by the toolbar, the functions it calls.  Enter goes to the call.  The callers of a virtual function have
//    after them the callers of what it overrides, each saying which ("via CShape::Draw"): a call through the base
//    class may land in it.
//
//Each tab is a results pane (CIDEResultsPane): the preview shows the selected row's place, enter or a double click
//goes there (a row's chevron and the left and right keys open and close it), Rerun makes it again from the index,
//and Next and Previous Occurrence step through its rows.  Alt+8 shows the window with the keys in its tree.
class CIDEHierarchy : public CObject, public CIDEResultsTabsOwner, public CIDEResultsSource, public CIDESteppable
{
CONSTRUCTABLE(CIDEHierarchy);
DESTRUCTABLE(CIDEHierarchy);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CIDEResultsTabs>	mpTabs;
	Ptr<CDockPane>			mpTool;
	Ptr<CIDEResultsPane>	mpExpandedPane;  //A row of this pane was opened and its rows aren't found yet: found at the next tick.
	int64					miExpandedData;
	bool					mbShowing;  //The tree is being made: its rows opening aren't the user's doing.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddNavigateItems(Ptr<CMenuContainer> pMenu);
	void	CreatePane(SDockPlace* psPlace);
	void	Clear(void);
	void	Detach(void);
	bool	HasEditor(void);

	bool	ShowTypeHierarchy(void);
	bool	ShowMethodHierarchy(void);
	bool	ShowCallHierarchy(void);
	bool	ShowHierarchyWindow(void);
	bool	Show(EIDEHierarchyKind eKind, EIDEHierarchyMode eMode, const char* szKey, const char* szClassKey);
	void	SetMode(EIDEHierarchyMode eMode);

	Ptr<CIDEResultsSearch>	CreateResultsSearch(void) override;
	bool	GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;
	bool	CanRerunResults(CIDEResultsPane* pcPane) override;
	void	RerunResults(CIDEResultsPane* pcPane) override;

	Ptr<CDockPane>	GetSteppablePane(void) const override;
	const char*		GetSteppableName(void) override;
	bool			HasSteps(void) override;
	bool			Step(bool bNext, bool bKeepKeys) override;

	Ptr<CDockPane>			GetTool(void) const;
	Ptr<CIDEResultsTabs>	GetTabs(void) const;
	Ptr<CIDEResultsPane>	GetResultsPane(void);
	Ptr<CIDEHierarchySearch>	GetSearch(void);
	void					Print(CChars* pszDest);
	bool					ExpandRow(const char* szName);

protected:
	void	AddModeCommand(Ptr<CIDEResultsPane> pPane, const char* szName, const char* szCaption, EIDEToolIcon eIcon, void(CIDEHierarchy::*fShow)(void), bool(CIDEHierarchy::*fIsShown)(void), bool(CIDEHierarchy::*fCanShow)(void));
	void	ShowClassMode(void);
	void	ShowSupertypesMode(void);
	void	ShowSubtypesMode(void);
	void	ShowCallersMode(void);
	void	ShowCalleesMode(void);
	bool	IsClassMode(void);
	bool	IsSupertypesMode(void);
	bool	IsSubtypesMode(void);
	bool	IsCallersMode(void);
	bool	IsCalleesMode(void);
	bool	IsOfClasses(void);
	bool	IsOfCalls(void);

	bool	FindCaretClass(CChars* pszKey);
	void	Build(Ptr<CIDEHierarchySearch> pSearch);
	int32	AddClass(Ptr<CIDEHierarchySearch> pSearch, int32 iParent, const char* szKey);
	void	AddClasses(Ptr<CIDEHierarchySearch> pSearch, int32 iNode, bool bDerived, int32 iDepth);
	void	NoteOtherBases(Ptr<CIDEHierarchySearch> pSearch, int32 iNode);
	void	MarkMethod(Ptr<CIDEHierarchySearch> pSearch);
	void	FindCalls(Ptr<CIDEHierarchySearch> pSearch, int32 iNode);
	void	AddCallRows(Ptr<CIDEHierarchySearch> pSearch, int32 iNode, CArrayIDECallRelations* pasRelations, const char* szVia);
	void	RemoveQualifiedCalls(CArrayIDECallRelations* pasRelations);
	void	ShowTree(Ptr<CIDEHierarchySearch> pSearch, int64 iSelectedData);
	void	AddTreeRows(Ptr<CIDEHierarchySearch> pSearch, int32 iParent, int32 iParentNode);
	void	KeepExpanded(Ptr<CIDEHierarchySearch> pSearch);
	void	NodeExpanded(CTreeContainer* pcTree, int32 iNode);
	void	RowActivated(CListContainer* pcList);
	void	ExpandPending(void);
	void	AppendName(CChars* pszDest);
};


#endif // __IDE_HIERARCHY_H__

