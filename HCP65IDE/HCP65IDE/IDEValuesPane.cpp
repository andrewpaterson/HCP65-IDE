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
#include "IDEValuesPane.h"


#define IDE_VALUES_NAME_WIDTH	180
#define IDE_VALUES_VALUE_WIDTH	240
#define IDE_VALUES_TYPE_WIDTH	140
#define IDE_VALUES_COLUMNS		3		//Name, Value and Type.
#define IDE_VALUES_SEPARATOR	'/'		//Between the names of a value's path.
#define IDE_VALUES_BREAK_WHEN_CHANGED	1	//The context menu's item.


//////////////////////////////////////////////////////////////////////////
//
// pChangedFont is a changed value's (red).  bWatch: the names are typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pChangedFont, CIDEDebugStop* pcStop, bool bWatch)
{
	PreInit();
	mpChangedFont = pChangedFont;
	mpcStop = pcStop;
	mbWatch = bWatch;
	mcRoots.Init();
	maszPaths.Init();
	masValues.Init();
	maiRootNodes.Init();
	maszOpen.Init();
	macShown[0].Init();
	macShown[1].Init();
	muiShown = 0;
	muiStop = 0;
	mbShowing = false;
	mbOpened = false;
	miEditedRoot = -1;
	mszEditedName.Init();
	mcWritten.Init();
	mcRenamed.Init();

	mpTable = OMalloc<CTreeTable>(pWindow, pTheme);
	mpTable->AddColumn("Name", IDE_VALUES_NAME_WIDTH, bWatch);
	mpTable->AddColumn("Value", IDE_VALUES_VALUE_WIDTH, true);
	mpTable->AddColumn("Type", IDE_VALUES_TYPE_WIDTH);
	mpTable->GetEditEvent()->Add(this, &CIDEValuesPane::Edited);
	mpTable->GetTree()->GetExpandedEvent()->Add(this, &CIDEValuesPane::Expanded);
	mpMenu = OMalloc<CMenuContainer>(pWindow, pTheme);
	mpMenu->GetOpeningEvent()->Add(this, &CIDEValuesPane::MenuOpening);
	mpMenu->GetItemClickEvent()->Add(this, &CIDEValuesPane::MenuItemClicked);
	mpTable->SetContextMenu(mpMenu);
	if (mbWatch)
	{
		Rebuild();
	}
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Class(void)
{
	M_Pointer(mpTable);
	M_Pointer(mpMenu);
	M_Pointer(mpChangedFont);
	U_Pointer(mpcStop);
	U_Bool(mbWatch);
	U_Unknown(CIDEDebugRoots, mcRoots);
	U_Unknown(CArrayChars, maszPaths);
	U_Unknown(CArrayIDEDebugValues, masValues);
	U_Unknown(CArrayInt, maiRootNodes);
	U_Unknown(CArrayChars, maszOpen);
	U_Unknown(CMapStringString, macShown[0]);
	U_Unknown(CMapStringString, macShown[1]);
	U_Size(muiShown);
	U_Size(muiStop);
	U_Bool(mbShowing);
	U_Bool(mbOpened);
	U_Int32(miEditedRoot);
	U_String(mszEditedName);
	U_Unknown(CComponentEvent<CIDEValuesPane>, mcWritten);
	U_Unknown(CComponentEvent<CIDEValuesPane>, mcRenamed);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Free(void)
{
	mpTable->GetEditEvent()->Remove(this);
	mpTable->GetTree()->GetExpandedEvent()->Remove(this);
	mpMenu->GetOpeningEvent()->Remove(this);
	mpMenu->GetItemClickEvent()->Remove(this);
	mcRenamed.Kill();
	mcWritten.Kill();
	mszEditedName.Kill();
	macShown[1].Kill();
	macShown[0].Kill();
	maszOpen.Kill();
	maiRootNodes.Kill();
	masValues.Kill();
	maszPaths.Kill();
	mcRoots.Kill();
}


bool CIDEValuesPane::Save(CObjectWriter* pcFile) { return false; }
bool CIDEValuesPane::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The values at the top, as they are now.  At a new stop what was shown
// at the last becomes what changes are seen against.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Show(CIDEDebugRoots* pcRoots)
{
	if (mpcStop->IsStopped() && (mpcStop->GetStop() != muiStop))
	{
		muiStop = mpcStop->GetStop();
		muiShown = 1 - muiShown;
		macShown[muiShown].Kill();
		macShown[muiShown].Init();
	}
	mcRoots.Copy(pcRoots);
	Rebuild();
}


//////////////////////////////////////////////////////////////////////////
//
// A value opened since: its children are read and shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Poll(void)
{
	if (mbOpened)
	{
		mbOpened = false;
		Rebuild();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The value at szPath ("cSquare/CShape") is opened, as its chevron opens
// it.  False if it isn't shown, or has no children.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEValuesPane::Open(const char* szPath)
{
	int32	iNode;

	iNode = FindPath(szPath);
	if ((iNode < 0) || !mpTable->GetTree()->HasChildren(iNode))
	{
		return false;
	}
	mpTable->GetTree()->SetExpanded(iNode, true);
	Poll();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// szText typed into the Value of the value at szPath, as its cell is
// edited.  False if it isn't shown, or can't be written so.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEValuesPane::WriteValue(const char* szPath, const char* szText)
{
	int32	iNode;

	iNode = FindPath(szPath);
	if ((iNode < 0) || !masValues.Get(iNode)->pcRecords || !mpcStop->Write(masValues.Get(iNode), szText))
	{
		return false;
	}
	mcWritten.Call(this);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Break When Value Changes on the value at szPath, as its context menu
// does: a data breakpoint put on it, or taken off.  False if it can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEValuesPane::ToggleDataBreakpoint(const char* szPath)
{
	return ToggleDataBreakpoint(FindPath(szPath));
}


//////////////////////////////////////////////////////////////////////////
//
// The node's value if a data breakpoint can watch it, NULL if not.
//
//////////////////////////////////////////////////////////////////////////
SCPP65DebugValue* CIDEValuesPane::GetWatchable(int32 iNode)
{
	if ((iNode < 0) || ((size)iNode >= masValues.NumElements()) || !mpcStop->CanWatch(masValues.Get(iNode)))
	{
		return NULL;
	}
	return masValues.Get(iNode);
}


//////////////////////////////////////////////////////////////////////////
//
// A data breakpoint on the node's value, named as C++ writes it; or the
// one there taken off.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEValuesPane::ToggleDataBreakpoint(int32 iNode)
{
	SCPP65DebugValue*	psValue;
	SIDEDataBreakpoint*	psBreakpoint;
	CChars				szName;
	bool				bResult;

	psValue = GetWatchable(iNode);
	if (psValue == NULL)
	{
		return false;
	}
	psBreakpoint = mpcStop->FindDataBreakpoint(psValue);
	if (psBreakpoint)
	{
		mpcStop->GetDebugger()->RemoveDataBreakpoint(psBreakpoint);
		return true;
	}
	szName.Init();
	AppendExpression(iNode, &szName);
	bResult = mpcStop->AddDataBreakpoint(szName.Text(), psValue, false);
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The node's value as an expression: its path's names joined as C++ joins
// a member ("cCounter.miCount") or an element ("gaiValues[2]").
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::AppendExpression(int32 iNode, CChars* pszDest)
{
	const char*	szPath;

	szPath = maszPaths.GetText(iNode);
	while (*szPath != '\0')
	{
		if (*szPath == IDE_VALUES_SEPARATOR)
		{
			if (szPath[1] != '[')
			{
				pszDest->Append('.');
			}
		}
		else
		{
			pszDest->Append(*szPath);
		}
		szPath++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Break When Value Changes, for the selected value if it can be watched;
// Remove Data Breakpoint if it has one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::MenuOpening(CMenuContainer* pcMenu)
{
	SCPP65DebugValue*	psValue;
	bool				bWatched;
	Ptr<CMenuItem>		pItem;

	psValue = GetWatchable(mpTable->GetTree()->GetSelectedNode());
	bWatched = psValue && mpcStop->FindDataBreakpoint(psValue);
	mpMenu->RemoveAllItems();
	pItem = mpMenu->AddItem(bWatched ? "Remove Data Breakpoint" : "Break When Value Changes");
	pItem->SetTag(IDE_VALUES_BREAK_WHEN_CHANGED);
	pItem->SetEnabled(psValue != NULL);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::MenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem->GetTag() == IDE_VALUES_BREAK_WHEN_CHANGED)
	{
		ToggleDataBreakpoint(mpTable->GetTree()->GetSelectedNode());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The nodes made again from the roots, the open ones' children read, the
// selected one selected again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Rebuild(void)
{
	Ptr<CTreeContainer>	pTree;
	SIDEDebugRoot*		psRoot;
	CChars				szSelected;
	int32				iSelected;
	size				ui;

	pTree = mpTable->GetTree();
	KeepOpen();
	iSelected = pTree->GetSelectedNode();
	szSelected.Init(((iSelected >= 0) && ((size)iSelected < maszPaths.NumElements())) ? maszPaths.GetText(iSelected) : "");
	mpTable->EndEdit(false);

	mbShowing = true;
	pTree->RemoveAllNodes();
	maszPaths.ReInit();
	masValues.ReInit();
	maiRootNodes.ReInit();
	for (ui = 0; ui < mcRoots.NumRoots(); ui++)
	{
		psRoot = mcRoots.GetRoot(ui);
		maiRootNodes.Add(AddValue(TREE_NO_NODE, psRoot->szName.Text(), psRoot->szName.Text(), psRoot->bValue ? &psRoot->sValue : NULL, psRoot->szError.Text()));
	}
	if (mbWatch)
	{
		maiRootNodes.Add(AddValue(TREE_NO_NODE, "", "", NULL, ""));
	}
	iSelected = szSelected.Empty() ? -1 : FindPath(szSelected.Text());
	if (iSelected >= 0)
	{
		pTree->SelectNode(iSelected);
	}
	mbShowing = false;
	szSelected.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A node for a value (or for psValue NULL, none: szError in its Value),
// its children under it if it is open, or one empty child if it has
// children and isn't.  Returns the node.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEValuesPane::AddValue(int32 iParent, const char* szName, const char* szPath, SCPP65DebugValue* psValue, const char* szError)
{
	Ptr<CTreeContainer>	pTree;
	SCPP65DebugValue*	psNodeValue;
	SCPP65DebugValue	sChild;
	CChars				szText;
	CChars				szChildName;
	CChars				szChildPath;
	char*				szBefore;
	int32				iNode;
	size				uiChildren;
	size				ui;

	pTree = mpTable->GetTree();
	iNode = pTree->AddNode(iParent, szName);
	maszPaths.Add(szPath);
	psNodeValue = masValues.Add();
	memset(psNodeValue, 0, sizeof(SCPP65DebugValue));
	if (!psValue)
	{
		mpTable->SetCellText(iNode, 1, szError);
		return iNode;
	}
	*psNodeValue = *psValue;

	szText.Init();
	mpcStop->GetValues()->Format(psValue, &szText);
	mpTable->SetCellText(iNode, 1, szText.Text());
	if (mpcStop->IsStopped())
	{
		szBefore = macShown[1 - muiShown].Get(szPath);
		if (szBefore && (strcmp(szBefore, szText.Text()) != 0))
		{
			mpTable->SetCellFont(iNode, 1, mpChangedFont);
		}
		macShown[muiShown].Put(szPath, szText.Text());
	}
	szText.Clear();
	mpcStop->GetValues()->AppendType(psValue, &szText);
	mpTable->SetCellText(iNode, 2, szText.Text());
	szText.Kill();

	uiChildren = mpcStop->GetValues()->NumChildren(psValue);
	if ((uiChildren > 0) && (FindOpen(szPath) == ARRAY_ELEMENT_NOT_FOUND))
	{
		AddValue(iNode, "", "", NULL, "");
	}
	else if (uiChildren > 0)
	{
		szChildName.Init();
		szChildPath.Init();
		for (ui = 0; ui < uiChildren; ui++)
		{
			szChildName.Clear();
			if (mpcStop->GetValues()->GetChild(psValue, ui, &szChildName, &sChild))
			{
				szChildPath.Clear();
				szChildPath.Append(szPath)->Append(IDE_VALUES_SEPARATOR)->Append(szChildName);
				AddValue(iNode, szChildName.Text(), szChildPath.Text(), &sChild, "");
			}
		}
		szChildPath.Kill();
		szChildName.Kill();
		pTree->SetExpanded(iNode, true);
	}
	return iNode;
}


//////////////////////////////////////////////////////////////////////////
//
// The open values are the ones open in the tree now: one closed since is
// forgotten, as is one opened (Expanded) remembered.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::KeepOpen(void)
{
	Ptr<CTreeContainer>	pTree;
	const char*			szPath;
	size				uiOpen;
	int32				iNode;

	pTree = mpTable->GetTree();
	for (iNode = 0; (iNode < pTree->NumNodes()) && ((size)iNode < maszPaths.NumElements()); iNode++)
	{
		szPath = maszPaths.GetText(iNode);
		if ((szPath[0] == '\0') || !pTree->HasChildren(iNode))
		{
			continue;
		}
		uiOpen = FindOpen(szPath);
		if (pTree->IsExpanded(iNode) && (uiOpen == ARRAY_ELEMENT_NOT_FOUND))
		{
			maszOpen.Add(szPath);
		}
		else if (!pTree->IsExpanded(iNode) && (uiOpen != ARRAY_ELEMENT_NOT_FOUND))
		{
			maszOpen.Remove(uiOpen);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CIDEValuesPane::FindOpen(const char* szPath)
{
	size	ui;

	for (ui = 0; ui < maszOpen.NumElements(); ui++)
	{
		if (strcmp(maszOpen.GetText(ui), szPath) == 0)
		{
			return ui;
		}
	}
	return ARRAY_ELEMENT_NOT_FOUND;
}


//////////////////////////////////////////////////////////////////////////
//
// The node showing the value at szPath, -1 if none is.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEValuesPane::FindPath(const char* szPath)
{
	size	ui;

	for (ui = 0; ui < maszPaths.NumElements(); ui++)
	{
		if (strcmp(maszPaths.GetText(ui), szPath) == 0)
		{
			return (int32)ui;
		}
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// A value's Value typed into is written; a Watch's name typed is its
// owner's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Edited(CTreeTable* pcTable)
{
	int32	iNode;
	size	ui;

	iNode = pcTable->GetEditedNode();
	if (pcTable->GetEditedColumn() == 1)
	{
		if (((size)iNode < masValues.NumElements()) && masValues.Get(iNode)->pcRecords && mpcStop->Write(masValues.Get(iNode), pcTable->GetEditedText()))
		{
			mcWritten.Call(this);
		}
		return;
	}
	for (ui = 0; ui < maiRootNodes.NumElements(); ui++)
	{
		if (maiRootNodes.GetValue(ui) == iNode)
		{
			miEditedRoot = (int32)ui;
			mszEditedName.Set(pcTable->GetEditedText());
			mcRenamed.Call(this);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A value opened by its chevron: its children are read at the next Poll,
// not while the tree is telling of it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Expanded(CTreeContainer* pcTree, int32 iNode)
{
	if (!mbShowing)
	{
		mbOpened = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: each value shown, a line each, "name = value [type]",
// indented two spaces a depth, "*" after a changed value.
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::Print(CChars* pszDest)
{
	int32	iNode;

	for (iNode = 0; iNode < mpTable->GetTree()->NumNodes(); iNode++)
	{
		if (mpTable->GetTree()->GetNodeParent(iNode) == TREE_NO_NODE)
		{
			PrintNode(iNode, 0, pszDest);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEValuesPane::PrintNode(int32 iNode, size uiDepth, CChars* pszDest)
{
	Ptr<CTreeContainer>	pTree;
	int32				iChild;

	pTree = mpTable->GetTree();
	pszDest->Append(' ', uiDepth * 2);
	pszDest->Append(mpTable->GetCellText(iNode, 0))->Append(" = ")->Append(mpTable->GetCellText(iNode, 1));
	pszDest->Append(" [")->Append(mpTable->GetCellText(iNode, 2))->Append(']');
	if (&pTree->GetCellFont(iNode, 0) == &mpChangedFont)
	{
		pszDest->Append(" *");
	}
	pszDest->AppendNewLine();
	if (!pTree->IsExpanded(iNode))
	{
		return;
	}
	for (iChild = pTree->GetFirstChild(iNode); iChild != TREE_NO_NODE; iChild = pTree->GetNextSibling(iChild))
	{
		PrintNode(iChild, uiDepth + 1, pszDest);
	}
}


Ptr<CComponent> CIDEValuesPane::GetComponent(void) const { return mpTable; }
Ptr<CTreeTable> CIDEValuesPane::GetTable(void) const { return mpTable; }
CComponentEvent<CIDEValuesPane>* CIDEValuesPane::GetWrittenEvent(void) { return &mcWritten; }
CComponentEvent<CIDEValuesPane>* CIDEValuesPane::GetRenamedEvent(void) { return &mcRenamed; }
int32 CIDEValuesPane::GetEditedRoot(void) const { return miEditedRoot; }
const char* CIDEValuesPane::GetEditedName(void) { return mszEditedName.Text(); }
