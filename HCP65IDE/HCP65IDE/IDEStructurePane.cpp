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
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDEStructurePane.h"


#define IDE_STRUCTURE_SORT		0	//The menu's items' tags.
#define IDE_STRUCTURE_FIELDS	1


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	maiEntryNodes.Init();
	muiShownChanges = MAX_SIZE;
	miShownEntry = -1;
	mbSortByName = false;
	mbShowFields = true;
	mbDirty = false;

	mpTree = OMalloc<CTreeContainer>(pcContent->GetWindow(), pcContent->GetTheme());
	mpTree->SetAlignment(CSH_Stretch, CSV_Top);
	mpTree->SetTypeToFind(true);
	mpTree->GetActivationEvent()->Add(this, &CIDEStructurePane::Activated);
	mpScrollPane = OMalloc<CScrollPane>(pcContent->GetWindow(), pcContent->GetTheme());
	mpScrollPane->SetContent(mpTree);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpMenu = OMalloc<CMenuContainer>(pcContent->GetWindow(), pcContent->GetTheme());
	mpMenu->GetOpeningEvent()->Add(this, &CIDEStructurePane::MenuOpening);
	mpMenu->GetItemClickEvent()->Add(this, &CIDEStructurePane::MenuItemClicked);
	mpTree->SetContextMenu(mpMenu);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpTree);
	M_Pointer(mpScrollPane);
	M_Pointer(mpMenu);
	M_Pointer(mpShownTab);
	U_Unknown(CArrayInt, maiEntryNodes);
	U_Size(muiShownChanges);
	U_Int32(miShownEntry);
	U_Bool(mbSortByName);
	U_Bool(mbShowFields);
	U_Bool(mbDirty);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Free(void)
{
	mpTree->GetActivationEvent()->Remove(this);
	mpMenu->GetOpeningEvent()->Remove(this);
	mpMenu->GetItemClickEvent()->Remove(this);
	maiEntryNodes.Kill();
}


bool CIDEStructurePane::Save(CObjectWriter* pcFile) { return false; }
bool CIDEStructurePane::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// At each tick, with the active editor (NULL for none): the tree is made
// again if it shows another editor's outline, or this one's as it was
// before it was last parsed, or sorted or filtered another way; then the
// node of the entry the caret is in is selected and shown, if the caret
// has gone to another entry.  With fields left out a caret in one selects
// the class it is in.  The keys stay where they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Update(Ptr<CIDEEditorTab> pTab)
{
	CIDEOutline*	pcOutline;
	int32			iEntry;
	int32			iNode;
	int				iLine;
	int				iColumn;
	bool			bRebuilt;

	if (pTab.IsNull())
	{
		if (mpShownTab.IsNotNull() || mbDirty)
		{
			mpShownTab = NULL;
			mpTree->RemoveAllNodes();
			maiEntryNodes.ReInit();
			muiShownChanges = MAX_SIZE;
			miShownEntry = -1;
			mbDirty = false;
		}
		return;
	}

	pcOutline = pTab->GetLastOutline();
	bRebuilt = false;
	if ((pTab != mpShownTab) || (pTab->GetOutlineChanges() != muiShownChanges) || mbDirty)
	{
		mpShownTab = pTab;
		muiShownChanges = pTab->GetOutlineChanges();
		mbDirty = false;
		Rebuild(pcOutline);
		bRebuilt = true;
	}

	iEntry = pTab->GetCaretPlace(&iLine, &iColumn) ? pcOutline->FindEntryAt(iLine, iColumn) : -1;
	if (!bRebuilt && (iEntry == miShownEntry))
	{
		return;
	}
	miShownEntry = iEntry;
	for (iNode = TREE_NO_NODE; (iEntry >= 0) && (iNode == TREE_NO_NODE); iEntry = pcOutline->Get((size)iEntry)->iParent)
	{
		iNode = maiEntryNodes.GetValue((size)iEntry);
	}
	if (iNode != TREE_NO_NODE)
	{
		mpTree->SelectNode(iNode);
		mpTree->ShowRow(mpTree->FindNodeRow(iNode));
	}
	else
	{
		mpTree->Select(-1);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The tree made again from pcOutline, every node open.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Rebuild(CIDEOutline* pcOutline)
{
	size	i;

	mpTree->StopFinding();
	mpTree->RemoveAllNodes();
	maiEntryNodes.ReInit();
	for (i = 0; i < pcOutline->NumEntries(); i++)
	{
		maiEntryNodes.Add((int)TREE_NO_NODE);
	}
	AddEntries(pcOutline, -1, TREE_NO_NODE);
	mpTree->ExpandAll();
	mpcContent->GetWindow()->InvalidateLayout();
}


//////////////////////////////////////////////////////////////////////////
//
// A node under iParentNode for each entry that is in entry iParentEntry
// (-1 for those in nothing), in the file's order or by name, and under
// each the entries in it.  The entries in an entry follow it, deeper than
// it is, until the next that isn't.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::AddEntries(CIDEOutline* pcOutline, int32 iParentEntry, int32 iParentNode)
{
	SIDEOutlineEntry*	psEntry;
	Ptr<CImageCel>		pIcon;
	CArrayInt			aiChildren;
	CChars				szText;
	size				i;
	size				j;
	int32				iDepth;
	int32				iNode;
	int					iChild;
	int					iType;

	iDepth = (iParentEntry < 0) ? -1 : pcOutline->Get((size)iParentEntry)->iDepth;
	aiChildren.Init();
	for (i = (size)(iParentEntry + 1); i < pcOutline->NumEntries(); i++)
	{
		psEntry = pcOutline->Get(i);
		if (psEntry->iDepth <= iDepth)
		{
			break;
		}
		if ((psEntry->iParent != iParentEntry) || (!mbShowFields && (psEntry->eKind == IOK_Field)))
		{
			continue;
		}

		//By name: before the first already there whose name is after its own.
		j = aiChildren.NumElements();
		if (mbSortByName)
		{
			for (j = 0; j < aiChildren.NumElements(); j++)
			{
				if (StringInsensitiveCompare(psEntry->szName.Text(), pcOutline->Get((size)aiChildren.GetValue(j))->szName.Text()) < 0)
				{
					break;
				}
			}
		}
		aiChildren.InsertAt((int)i, j);
	}

	for (i = 0; i < aiChildren.NumElements(); i++)
	{
		iChild = aiChildren.GetValue(i);
		psEntry = pcOutline->Get((size)iChild);
		szText.Init(psEntry->szName);
		szText.Append(psEntry->szDetail);
		iType = GetIDESymbolIconType(mpcContent->GetStructure()->GetEntrySymbolKind(psEntry));
		pIcon = (iType < 0) ? Ptr<CImageCel>() : mpcContent->GetSymbolIcon((size)iType);
		iNode = mpTree->AddNode(iParentNode, szText.Text(), pIcon, iChild);
		szText.Kill();
		*maiEntryNodes.Get((size)iChild) = (int)iNode;
		AddEntries(pcOutline, iChild, iNode);
	}
	aiChildren.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A row clicked, or Enter on it: the caret goes to its entry's name, and
// the editor has the keys.  Not if the file has been parsed again since
// the tree was made: the next tick makes it again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Activated(void)
{
	CIDEOutline*	pcOutline;
	int32			iNode;
	int64			iEntry;

	iNode = mpTree->GetSelectedNode();
	if (mpShownTab.IsNull() || (iNode == TREE_NO_NODE) || (mpShownTab->GetOutlineChanges() != muiShownChanges))
	{
		return;
	}
	pcOutline = mpShownTab->GetLastOutline();
	iEntry = mpTree->GetNodeData(iNode);
	if ((iEntry >= 0) && ((size)iEntry < pcOutline->NumEntries()))
	{
		mpcContent->GetStructure()->GoToEntry(mpShownTab, pcOutline->Get((size)iEntry), false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// As a click on row iRow.  False if there is no such row.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEStructurePane::ActivateRow(int32 iRow)
{
	if (!mpTree->Select(iRow))
	{
		return false;
	}
	mpTree->Activate();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The tree's menu, made as it opens: how the entries are ordered and
// whether fields are shown, each item saying what choosing it does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::MenuOpening(CMenuContainer* pcMenu)
{
	mpMenu->RemoveAllItems();
	mpMenu->AddItem(mbSortByName ? "Sort by Place in the File" : "Sort by Name")->SetTag(IDE_STRUCTURE_SORT);
	mpMenu->AddItem(mbShowFields ? "Hide Fields" : "Show Fields")->SetTag(IDE_STRUCTURE_FIELDS);
}


void CIDEStructurePane::MenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem->GetTag() == IDE_STRUCTURE_SORT)
	{
		mpcContent->GetStructure()->SetStructureSettings(!mbSortByName, mbShowFields);
	}
	else if (pcItem->GetTag() == IDE_STRUCTURE_FIELDS)
	{
		mpcContent->GetStructure()->SetStructureSettings(mbSortByName, !mbShowFields);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Structure: 3 rows", then a line for each row, two spaces
// in for each node it is under, with "> " before the selected one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEStructurePane::Print(CChars* pszDest)
{
	CChars	szText;
	int32	iRow;
	int32	iNode;
	int32	i;

	mpTree->UpdateRows();
	pszDest->Append("Structure: ");
	pszDest->Append(mpTree->NumRows());
	pszDest->Append((mpTree->NumRows() == 1) ? " row" : " rows");
	if (mbSortByName)
	{
		pszDest->Append(", by name");
	}
	if (!mbShowFields)
	{
		pszDest->Append(", no fields");
	}
	pszDest->Append('\n');
	for (iRow = 0; iRow < mpTree->NumRows(); iRow++)
	{
		iNode = mpTree->GetRowNode(iRow);
		pszDest->Append((iRow == mpTree->GetSelected()) ? "> " : "  ");
		for (i = 0; i < mpTree->GetNodeDepth(iNode); i++)
		{
			pszDest->Append("  ");
		}
		szText.Init();
		pszDest->Append(mpTree->GetNodeText(iNode, &szText));
		szText.Kill();
		pszDest->Append('\n');
	}
}


void CIDEStructurePane::SetSortByName(bool bSortByName) { mbDirty = mbDirty || (bSortByName != mbSortByName); mbSortByName = bSortByName; }
bool CIDEStructurePane::IsSortByName(void) const { return mbSortByName; }
void CIDEStructurePane::SetShowFields(bool bShowFields) { mbDirty = mbDirty || (bShowFields != mbShowFields); mbShowFields = bShowFields; }
bool CIDEStructurePane::IsShowFields(void) const { return mbShowFields; }
Ptr<CComponent> CIDEStructurePane::GetComponent(void) const { return mpScrollPane; }
Ptr<CTreeContainer> CIDEStructurePane::GetTree(void) const { return mpTree; }

