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
#include "BaseLib/FileUtil.h"
#include "VisualStudioLib/VcxPath.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ObjectReader.h"
#include "StandardLib/ObjectWriter.h"
#include "SupportLib/ColourFormat.h"
#include "WindowLib/Window.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/JewelImageGenerator.h"
#include "SolutionExplorer.h"


//Visual Studio's solution icon is purple.
#define SOLUTION_EXPLORER_SOLUTION_COLOUR	Set32BitColour((uint8)0xA7, (uint8)0x82, (uint8)0xE6)


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme)
{
	CColourFormatHelper	cFormat;

	PreInit();

	mpcSolution = NULL;
	masNodes.Init();
	pWindow->GetColourFormat(&cFormat);
	mpFolderIcon = CJewelImageGenerator::CreateFolder(JEWEL_ICON, &cFormat);
	mpFileIcon = CJewelImageGenerator::CreateFile(JEWEL_ICON, &cFormat);
	mpProjectIcon = CJewelImageGenerator::CreateFolder(JEWEL_BLUE, &cFormat);
	mpSolutionIcon = CJewelImageGenerator::CreateFile(SOLUTION_EXPLORER_SOLUTION_COLOUR, &cFormat);

	mpTree = OMalloc<CTreeContainer>(pWindow, pTheme);
	mpTree->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpScrollPane->SetContent(mpTree);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::Class(void)
{
	M_Pointer(mpTree);
	M_Pointer(mpScrollPane);
	M_Pointer(mpFolderIcon);
	M_Pointer(mpFileIcon);
	M_Pointer(mpProjectIcon);
	M_Pointer(mpSolutionIcon);
	U_Pointer(mpcSolution);
	U_Unknown(CArraySolutionExplorerNode, masNodes);
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionExplorer::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionExplorer::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::Free(void)
{
	masNodes.Kill();
	mpcSolution = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution at the top, open: "Solution 'Name' (2 of 3 projects)".  A
// solution of one project has it open too.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::ShowSolution(CVcxSolution* pcSolution)
{
	CChars	szText;
	int32	iSolution;
	int32	iChild;
	size	uiProjects;
	size	i;

	mpcSolution = pcSolution;
	mpTree->RemoveAllNodes();
	masNodes.ReInit();

	uiProjects = 0;
	for (i = 0; i < pcSolution->NumProjects(); i++)
	{
		if (!pcSolution->GetProject(i)->IsFolder())
		{
			uiProjects++;
		}
	}

	szText.Init("Solution '");
	szText.Append(pcSolution->GetName());
	szText.Append("' (");
	szText.Append((uint32)pcSolution->NumLoadedProjects());
	szText.Append(" of ");
	szText.Append((uint32)uiProjects);
	szText.Append((uiProjects == 1) ? " project)" : " projects)");
	iSolution = AddNode(TREE_NO_NODE, szText.Text(), mpSolutionIcon, SENT_Solution, 0, 0);
	szText.Kill();

	AddSolutionContents(iSolution, "");
	mpTree->SetExpanded(iSolution, true);
	if (uiProjects == 1)
	{
		for (iChild = mpTree->GetFirstChild(iSolution); iChild != TREE_NO_NODE; iChild = mpTree->GetNextSibling(iChild))
		{
			if (GetNode(iChild)->eType == SENT_Project)
			{
				mpTree->SetExpanded(iChild, true);
			}
		}
	}
	mpTree->SelectNode(iSolution);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int32 CSolutionExplorer::AddNode(int32 iParentNode, const char* szText, Ptr<CImageCel> pIcon, ESolutionExplorerNodeType eType, size uiProject, size uiIndex)
{
	SSolutionExplorerNode*	psNode;
	int64					iData;

	iData = (int64)masNodes.NumElements();
	psNode = masNodes.Add();
	psNode->eType = eType;
	psNode->uiProject = uiProject;
	psNode->uiIndex = uiIndex;
	return mpTree->AddNode(iParentNode, szText, pIcon, iData);
}


//////////////////////////////////////////////////////////////////////////
//
// The solution folders and projects in the folder szParentGuid ("" is
// the solution): folders first, then projects.  A folder's own files
// come after what is nested in it.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddSolutionContents(int32 iParentNode, const char* szParentGuid)
{
	CArrayInt				aiProjects;
	CVcxSolutionProject*	pcProject;
	CFileUtil				cFileUtil;
	CChars					szPath;
	CChars					szName;
	int32					iNode;
	size					i;
	size					j;
	size					uiIndex;

	aiProjects.Init();
	mpcSolution->GetChildProjects(szParentGuid, &aiProjects);
	SortProjects(&aiProjects);
	for (i = 0; i < aiProjects.NumElements(); i++)
	{
		uiIndex = (size)aiProjects.GetValue(i);
		pcProject = mpcSolution->GetProject(uiIndex);
		if (pcProject->IsFolder())
		{
			iNode = AddNode(iParentNode, pcProject->GetName(), mpFolderIcon, SENT_SolutionFolder, uiIndex, 0);
			AddSolutionContents(iNode, pcProject->GetGuid());
			for (j = 0; j < pcProject->NumSolutionItems(); j++)
			{
				szPath.Init(pcProject->GetSolutionItem(j));
				szPath.Replace('\\', FILE_SEPARATOR[0]);
				szName.Init();
				cFileUtil.GetFileName(&szName, szPath.Text());
				AddNode(iNode, szName.Text(), mpFileIcon, SENT_SolutionItem, uiIndex, j);
				szName.Kill();
				szPath.Kill();
			}
		}
		else
		{
			AddProject(iParentNode, uiIndex);
		}
	}
	aiProjects.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A project with its filters and files, or one Visual Studio couldn't
// load: "Name (load failed)", or "(incompatible)" if it isn't C++.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddProject(int32 iParentNode, size uiProject)
{
	CVcxSolutionProject*	pcProject;
	CChars					szText;
	int32					iNode;

	pcProject = mpcSolution->GetProject(uiProject);
	szText.Init(pcProject->GetName());
	if (!pcProject->IsVcxProject())
	{
		szText.Append(" (incompatible)");
	}
	else if (!pcProject->IsLoaded())
	{
		szText.Append(" (load failed)");
	}
	iNode = AddNode(iParentNode, szText.Text(), mpProjectIcon, SENT_Project, uiProject, 0);
	szText.Kill();

	if (pcProject->IsLoaded())
	{
		AddFilterContents(iNode, uiProject, "");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The filters directly in szFilter as folders (with what is in them),
// then its files.  "" is the project.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddFilterContents(int32 iParentNode, size uiProject, const char* szFilter)
{
	CVcxProject*	pcProject;
	CArrayInt		aiFilters;
	CArrayInt		aiItems;
	CVcxFilter*		pcFilter;
	CVcxItem*		pcItem;
	int32			iNode;
	size			i;
	size			uiIndex;

	pcProject = mpcSolution->GetProject(uiProject)->GetProject();
	aiFilters.Init();
	pcProject->GetChildFilters(szFilter, &aiFilters);
	SortFilters(pcProject, &aiFilters);
	for (i = 0; i < aiFilters.NumElements(); i++)
	{
		uiIndex = (size)aiFilters.GetValue(i);
		pcFilter = pcProject->GetFilter(uiIndex);
		iNode = AddNode(iParentNode, pcFilter->GetLeafName(), mpFolderIcon, SENT_Filter, uiProject, uiIndex);
		AddFilterContents(iNode, uiProject, pcFilter->GetName());
	}
	aiFilters.Kill();

	aiItems.Init();
	pcProject->GetItemsInFilter(szFilter, &aiItems);
	SortItems(pcProject, &aiItems);
	for (i = 0; i < aiItems.NumElements(); i++)
	{
		uiIndex = (size)aiItems.GetValue(i);
		pcItem = pcProject->GetItem(uiIndex);
		AddNode(iParentNode, pcItem->GetFileName(), mpFileIcon, SENT_File, uiProject, uiIndex);
	}
	aiItems.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Solution folders before projects, each alphabetical whatever its case.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::SortProjects(CArrayInt* paiProjects)
{
	size					i;
	size					j;
	int						iProject;
	CVcxSolutionProject*	pcProject;
	CVcxSolutionProject*	pcBefore;
	int						iCompare;

	for (i = 1; i < paiProjects->NumElements(); i++)
	{
		iProject = paiProjects->GetValue(i);
		pcProject = mpcSolution->GetProject((size)iProject);
		for (j = i; j > 0; j--)
		{
			pcBefore = mpcSolution->GetProject((size)paiProjects->GetValue(j - 1));
			if (pcBefore->IsFolder() != pcProject->IsFolder())
			{
				iCompare = pcBefore->IsFolder() ? -1 : 1;
			}
			else
			{
				iCompare = StringInsensitiveCompare(pcBefore->GetName(), pcProject->GetName());
			}
			if (iCompare <= 0)
			{
				break;
			}
			paiProjects->SetValue(j, paiProjects->GetValue(j - 1));
		}
		paiProjects->SetValue(j, iProject);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Alphabetical by the filters' last names, whatever their case.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::SortFilters(CVcxProject* pcProject, CArrayInt* paiFilters)
{
	size	i;
	size	j;
	int		iFilter;

	for (i = 1; i < paiFilters->NumElements(); i++)
	{
		iFilter = paiFilters->GetValue(i);
		for (j = i; (j > 0) && (StringInsensitiveCompare(pcProject->GetFilter(paiFilters->GetValue(j - 1))->GetLeafName(), pcProject->GetFilter(iFilter)->GetLeafName()) > 0); j--)
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
void CSolutionExplorer::SortItems(CVcxProject* pcProject, CArrayInt* paiItems)
{
	size	i;
	size	j;
	int		iItem;

	for (i = 1; i < paiItems->NumElements(); i++)
	{
		iItem = paiItems->GetValue(i);
		for (j = i; (j > 0) && (StringInsensitiveCompare(pcProject->GetItem(paiItems->GetValue(j - 1))->GetFileName(), pcProject->GetItem(iItem)->GetFileName()) > 0); j--)
		{
			paiItems->SetValue(j, paiItems->GetValue(j - 1));
		}
		paiItems->SetValue(j, iItem);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What a tree node is, NULL for no node.
//
//////////////////////////////////////////////////////////////////////////
SSolutionExplorerNode* CSolutionExplorer::GetNode(int32 iNode)
{
	if ((iNode == TREE_NO_NODE) || (mpcSolution == NULL))
	{
		return NULL;
	}
	return masNodes.SafeGet((size)mpTree->GetNodeData(iNode));
}


//////////////////////////////////////////////////////////////////////////
//
// The full path of a file node (a project's or a solution folder's)
// appended to pszDest.  False for any other node.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionExplorer::GetNodeFilePath(int32 iNode, CChars* pszDest)
{
	SSolutionExplorerNode*	psNode;
	CVcxSolutionProject*	pcProject;
	CVcxProject*			pcVcxProject;

	psNode = GetNode(iNode);
	if (psNode == NULL)
	{
		return false;
	}
	if (psNode->eType == SENT_File)
	{
		pcVcxProject = mpcSolution->GetProject(psNode->uiProject)->GetProject();
		pcVcxProject->GetFullPath(pcVcxProject->GetItem(psNode->uiIndex), pszDest);
		return true;
	}
	else if (psNode->eType == SENT_SolutionItem)
	{
		pcProject = mpcSolution->GetProject(psNode->uiProject);
		ResolveVcxPath(mpcSolution->GetDirectory(), pcProject->GetSolutionItem(psNode->uiIndex), pszDest);
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The file name (without its directory) of a file node appended to
// pszDest.  False for any other node.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionExplorer::GetNodeFileName(int32 iNode, CChars* pszDest)
{
	SSolutionExplorerNode*	psNode;

	psNode = GetNode(iNode);
	if ((psNode == NULL) || ((psNode->eType != SENT_File) && (psNode->eType != SENT_SolutionItem)))
	{
		return false;
	}
	mpTree->GetNodeText(iNode, pszDest);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The node of the first file named szFileName (whatever its case),
// TREE_NO_NODE if there is none.
//
//////////////////////////////////////////////////////////////////////////
int32 CSolutionExplorer::FindFileNode(const char* szFileName)
{
	int32	iNode;
	CChars	szName;
	bool	bFound;

	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		szName.Init();
		bFound = GetNodeFileName(iNode, &szName) && szName.EqualsIgnoreCase(szFileName);
		szName.Kill();
		if (bFound)
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
bool CSolutionExplorer::GetSelectedFilePath(CChars* pszDest)
{
	return GetNodeFilePath(mpTree->GetSelectedNode(), pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The solution's project a project's file node is in, or
// SOLUTION_EXPLORER_NO_PROJECT for any other node (a solution item
// isn't built by a project).
//
//////////////////////////////////////////////////////////////////////////
size CSolutionExplorer::GetNodeProject(int32 iNode)
{
	SSolutionExplorerNode*	psNode;

	psNode = GetNode(iNode);
	if ((psNode == NULL) || (psNode->eType != SENT_File))
	{
		return SOLUTION_EXPLORER_NO_PROJECT;
	}
	return psNode->uiProject;
}


Ptr<CTreeContainer> CSolutionExplorer::GetTree(void) { return mpTree; }
Ptr<CComponent> CSolutionExplorer::GetComponent(void) { return mpScrollPane; }
