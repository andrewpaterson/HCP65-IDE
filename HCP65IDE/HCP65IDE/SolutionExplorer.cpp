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
#include "BaseLib/StringHelper.h"
#include "StandardLib/Objects.h"
#include "SupportLib/ColourFormat.h"
#include "WindowLib/Window.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/JewelImageGenerator.h"
#include "SolutionExplorer.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme)
{
	CColourFormatHelper	cFormat;

	mpcProject = NULL;
	pWindow->GetColourFormat(&cFormat);
	mpFolderIcon = CJewelImageGenerator::CreateFolder(JEWEL_ICON, &cFormat);
	mpFileIcon = CJewelImageGenerator::CreateFile(JEWEL_ICON, &cFormat);
	mpProjectIcon = CJewelImageGenerator::CreateFolder(JEWEL_BLUE, &cFormat);

	mpTree = OMalloc<CTreeContainer>(pWindow, pTheme);
	mpTree->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpScrollPane->SetContent(mpTree);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::Kill(void)
{
	mpScrollPane = NULL;
	mpTree = NULL;
	mpProjectIcon = NULL;
	mpFileIcon = NULL;
	mpFolderIcon = NULL;
	mpcProject = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The project at the top, open, with its filters and files in it.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::ShowProject(CVcxProject* pcProject)
{
	int32	iProject;

	mpcProject = pcProject;
	mpTree->RemoveAllNodes();
	iProject = mpTree->AddNode(TREE_NO_NODE, pcProject->GetName(), mpProjectIcon, SOLUTION_EXPLORER_PROJECT);
	AddFilterContents(iProject, "");
	mpTree->SetExpanded(iProject, true);
	mpTree->SelectNode(iProject);
}


//////////////////////////////////////////////////////////////////////////
//
// The filters directly in szFilter as folders (with what is in them),
// then its files.  "" is the project.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddFilterContents(int32 iParentNode, const char* szFilter)
{
	CArrayInt	aiFilters;
	CArrayInt	aiItems;
	CVcxFilter*	pcFilter;
	CVcxItem*	pcItem;
	int32		iNode;
	size		i;
	int			iIndex;

	aiFilters.Init();
	mpcProject->GetChildFilters(szFilter, &aiFilters);
	SortFilters(&aiFilters);
	for (i = 0; i < aiFilters.NumElements(); i++)
	{
		iIndex = aiFilters.GetValue(i);
		pcFilter = mpcProject->GetFilter(iIndex);
		iNode = mpTree->AddNode(iParentNode, pcFilter->GetLeafName(), mpFolderIcon, -(int64)iIndex - 1);
		AddFilterContents(iNode, pcFilter->GetName());
	}
	aiFilters.Kill();

	aiItems.Init();
	mpcProject->GetItemsInFilter(szFilter, &aiItems);
	SortItems(&aiItems);
	for (i = 0; i < aiItems.NumElements(); i++)
	{
		iIndex = aiItems.GetValue(i);
		pcItem = mpcProject->GetItem(iIndex);
		mpTree->AddNode(iParentNode, pcItem->GetFileName(), mpFileIcon, (int64)iIndex + 1);
	}
	aiItems.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Alphabetical by the filters' last names, whatever their case.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::SortFilters(CArrayInt* paiFilters)
{
	size	i;
	size	j;
	int		iFilter;

	for (i = 1; i < paiFilters->NumElements(); i++)
	{
		iFilter = paiFilters->GetValue(i);
		for (j = i; (j > 0) && (StringInsensitiveCompare(mpcProject->GetFilter(paiFilters->GetValue(j - 1))->GetLeafName(), mpcProject->GetFilter(iFilter)->GetLeafName()) > 0); j--)
		{
			paiFilters->SetValue(j, paiFilters->GetValue(j - 1));
		}
		paiFilters->SetValue(j, iFilter);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Alphabetical by the files' names, whatever their case.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::SortItems(CArrayInt* paiItems)
{
	size	i;
	size	j;
	int		iItem;

	for (i = 1; i < paiItems->NumElements(); i++)
	{
		iItem = paiItems->GetValue(i);
		for (j = i; (j > 0) && (StringInsensitiveCompare(mpcProject->GetItem(paiItems->GetValue(j - 1))->GetFileName(), mpcProject->GetItem(iItem)->GetFileName()) > 0); j--)
		{
			paiItems->SetValue(j, paiItems->GetValue(j - 1));
		}
		paiItems->SetValue(j, iItem);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file of a node, NULL for the project or a filter.
//
//////////////////////////////////////////////////////////////////////////
CVcxItem* CSolutionExplorer::GetNodeItem(int32 iNode)
{
	int64	iData;

	if ((iNode == TREE_NO_NODE) || (mpcProject == NULL))
	{
		return NULL;
	}
	iData = mpTree->GetNodeData(iNode);
	if (iData <= 0)
	{
		return NULL;
	}
	return mpcProject->GetItem((size)(iData - 1));
}


//////////////////////////////////////////////////////////////////////////
//
// The node of the first file named szFileName (whatever its case),
// TREE_NO_NODE if there is none.
//
//////////////////////////////////////////////////////////////////////////
int32 CSolutionExplorer::FindFileNode(const char* szFileName)
{
	int32		iNode;
	CVcxItem*	pcItem;

	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		pcItem = GetNodeItem(iNode);
		if ((pcItem != NULL) && (StringInsensitiveCompare(pcItem->GetFileName(), szFileName) == 0))
		{
			return iNode;
		}
	}
	return TREE_NO_NODE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CVcxItem* CSolutionExplorer::GetSelectedItem(void)
{
	return GetNodeItem(mpTree->GetSelectedNode());
}


Ptr<CTreeContainer> CSolutionExplorer::GetTree(void) { return mpTree; }
Ptr<CComponent> CSolutionExplorer::GetComponent(void) { return mpScrollPane; }

