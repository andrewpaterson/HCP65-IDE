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
#include "BaseLib/ArrayBit.h"
#include "VisualStudioLib/VcxPath.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ObjectReader.h"
#include "StandardLib/ObjectWriter.h"
#include "SupportLib/ColourFormat.h"
#include "WindowLib/Window.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/JewelImageGenerator.h"
#include "IDEGit.h"
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

	mpcWorkspace = NULL;
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
	M_Pointer(mpBoldColours);
	U_Pointer(mpcWorkspace);
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
	mpcWorkspace = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Each solution at the top, in the workspace's order.  The startup
// solution is selected.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::ShowWorkspace(CBuildWorkspace* pcWorkspace)
{
	size	i;
	int32	iStartup;

	mpcWorkspace = pcWorkspace;
	mpTree->RemoveAllNodes();
	masNodes.ReInit();

	for (i = 0; i < pcWorkspace->NumSolutions(); i++)
	{
		AddSolution(i);
	}
	iStartup = FindSolutionNode(pcWorkspace->GetStartup());
	if (iStartup != TREE_NO_NODE)
	{
		mpTree->SelectNode(iStartup);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A solution open: "Solution 'Name' (2 of 3 projects)", or "(startup, 2
// of 3 projects)" if there's more than one.  A solution of one project has it
// open too.  One that couldn't be read is "(load failed)".
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddSolution(size uiSolution)
{
	CBuildWorkspaceSolution*	pcWorkspaceSolution;
	CVcxSolution*				pcSolution;
	CFileUtil					cFileUtil;
	CChars						szText;
	int32						iSolution;
	int32						iChild;
	size						uiProjects;
	size						i;

	pcWorkspaceSolution = mpcWorkspace->GetSolution(uiSolution);
	pcSolution = &pcWorkspaceSolution->mcSolution;
	if (!pcWorkspaceSolution->mbRead)
	{
		szText.Init("Solution '");
		cFileUtil.GetFileName(&szText, pcWorkspaceSolution->mszFile.Text());
		szText.Append("' (load failed)");
		AddNode(TREE_NO_NODE, szText.Text(), mpSolutionIcon, SENT_Solution, uiSolution, 0, 0);
		szText.Kill();
		return;
	}

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
	iSolution = AddNode(TREE_NO_NODE, szText.Text(), mpSolutionIcon, SENT_Solution, uiSolution, 0, 0);
	szText.Kill();
	if (IsStartupNode(iSolution))
	{
		mpTree->SetNodeFont(iSolution, GetFolderFont(iSolution, false, Ptr<CFontColours>()));
	}

	AddSolutionContents(iSolution, uiSolution, "");
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
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CVcxSolution* CSolutionExplorer::GetSolution(size uiSolution)
{
	return &mpcWorkspace->GetSolution(uiSolution)->mcSolution;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int32 CSolutionExplorer::AddNode(int32 iParentNode, const char* szText, Ptr<CImageCel> pIcon, ESolutionExplorerNodeType eType, size uiSolution, size uiProject, size uiIndex)
{
	SSolutionExplorerNode*	psNode;
	int64					iData;

	iData = (int64)masNodes.NumElements();
	psNode = masNodes.Add();
	psNode->eType = eType;
	psNode->uiSolution = uiSolution;
	psNode->uiProject = uiProject;
	psNode->uiIndex = uiIndex;
	return mpTree->AddNode(iParentNode, szText, pIcon, iData);
}


//////////////////////////////////////////////////////////////////////////
//
// Folders before everything else, then alphabetical whatever the case,
// then in the order they were found.
//
//////////////////////////////////////////////////////////////////////////
int CompareSolutionExplorerEntries(const void* pv1, const void* pv2)
{
	SSolutionExplorerEntry*	ps1;
	SSolutionExplorerEntry*	ps2;
	int						iCompare;

	ps1 = (SSolutionExplorerEntry*)pv1;
	ps2 = (SSolutionExplorerEntry*)pv2;
	if (ps1->bFolder != ps2->bFolder)
	{
		return ps1->bFolder ? -1 : 1;
	}
	iCompare = StringInsensitiveCompare(ps1->szName, ps2->szName);
	if (iCompare != 0)
	{
		return iCompare;
	}
	if (ps1->uiIndex == ps2->uiIndex)
	{
		return 0;
	}
	return (ps1->uiIndex < ps2->uiIndex) ? -1 : 1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void AddSolutionExplorerEntry(CArraySolutionExplorerEntry* pasEntries, const char* szName, bool bFolder, size uiIndex)
{
	SSolutionExplorerEntry*	psEntry;

	psEntry = pasEntries->Add();
	psEntry->szName = szName;
	psEntry->bFolder = bFolder;
	psEntry->uiIndex = uiIndex;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution folders and projects in the folder szParentGuid ("" is
// the solution): folders first, then projects.  A folder's own files
// come after what is nested in it.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddSolutionContents(int32 iParentNode, size uiSolution, const char* szParentGuid)
{
	CVcxSolution*				pcSolution;
	CArrayInt					aiProjects;
	CArraySolutionExplorerEntry	asEntries;
	SSolutionExplorerEntry*		psEntry;
	CVcxSolutionProject*		pcProject;
	int32						iNode;
	size						i;

	pcSolution = GetSolution(uiSolution);
	aiProjects.Init();
	pcSolution->GetChildProjects(szParentGuid, &aiProjects);
	asEntries.Init();
	for (i = 0; i < aiProjects.NumElements(); i++)
	{
		pcProject = pcSolution->GetProject((size)aiProjects.GetValue(i));
		AddSolutionExplorerEntry(&asEntries, pcProject->GetName(), pcProject->IsFolder(), (size)aiProjects.GetValue(i));
	}
	aiProjects.Kill();

	asEntries.QuickSort(&CompareSolutionExplorerEntries);
	for (i = 0; i < asEntries.NumElements(); i++)
	{
		psEntry = asEntries.Get(i);
		if (psEntry->bFolder)
		{
			pcProject = pcSolution->GetProject(psEntry->uiIndex);
			iNode = AddNode(iParentNode, pcProject->GetName(), mpFolderIcon, SENT_SolutionFolder, uiSolution, psEntry->uiIndex, 0);
			AddSolutionContents(iNode, uiSolution, pcProject->GetGuid());
			AddSolutionItems(iNode, uiSolution, psEntry->uiIndex);
		}
		else
		{
			AddProject(iParentNode, uiSolution, psEntry->uiIndex);
		}
	}
	asEntries.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A solution folder's files, by their names without their directories.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddSolutionItems(int32 iParentNode, size uiSolution, size uiFolder)
{
	CVcxSolutionProject*	pcFolder;
	CFileUtil				cFileUtil;
	CChars					szPath;
	CChars					szName;
	size					i;

	pcFolder = GetSolution(uiSolution)->GetProject(uiFolder);
	for (i = 0; i < pcFolder->NumSolutionItems(); i++)
	{
		szPath.Init(pcFolder->GetSolutionItem(i));
		szPath.Replace('\\', FILE_SEPARATOR[0]);
		szName.Init();
		cFileUtil.GetFileName(&szName, szPath.Text());
		AddNode(iParentNode, szName.Text(), mpFileIcon, SENT_SolutionItem, uiSolution, uiFolder, i);
		szName.Kill();
		szPath.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A project with its filters and files, or one Visual Studio couldn't
// load: "Name (load failed)", or "(incompatible)" if it isn't C++.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddProject(int32 iParentNode, size uiSolution, size uiProject)
{
	CVcxSolutionProject*	pcProject;
	CChars					szText;
	int32					iNode;

	pcProject = GetSolution(uiSolution)->GetProject(uiProject);
	szText.Init(pcProject->GetName());
	if (!pcProject->IsVcxProject())
	{
		szText.Append(" (incompatible)");
	}
	else if (!pcProject->IsLoaded())
	{
		szText.Append(" (load failed)");
	}
	iNode = AddNode(iParentNode, szText.Text(), mpProjectIcon, SENT_Project, uiSolution, uiProject, 0);
	szText.Kill();

	if (pcProject->IsLoaded())
	{
		AddFilterContents(iNode, uiSolution, uiProject, "");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The filters directly in szFilter as folders (with what is in them),
// then its files.  "" is the project.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::AddFilterContents(int32 iParentNode, size uiSolution, size uiProject, const char* szFilter)
{
	CVcxProject*				pcProject;
	CArrayInt					aiIndices;
	CArraySolutionExplorerEntry	asEntries;
	SSolutionExplorerEntry*		psEntry;
	CVcxFilter*					pcFilter;
	int32						iNode;
	size						i;

	pcProject = GetSolution(uiSolution)->GetProject(uiProject)->GetProject();
	asEntries.Init();
	aiIndices.Init();
	pcProject->GetChildFilters(szFilter, &aiIndices);
	for (i = 0; i < aiIndices.NumElements(); i++)
	{
		AddSolutionExplorerEntry(&asEntries, pcProject->GetFilter((size)aiIndices.GetValue(i))->GetLeafName(), true, (size)aiIndices.GetValue(i));
	}
	aiIndices.ReInit();
	pcProject->GetItemsInFilter(szFilter, &aiIndices);
	for (i = 0; i < aiIndices.NumElements(); i++)
	{
		AddSolutionExplorerEntry(&asEntries, pcProject->GetItem((size)aiIndices.GetValue(i))->GetFileName(), false, (size)aiIndices.GetValue(i));
	}
	aiIndices.Kill();

	asEntries.QuickSort(&CompareSolutionExplorerEntries);
	for (i = 0; i < asEntries.NumElements(); i++)
	{
		psEntry = asEntries.Get(i);
		if (psEntry->bFolder)
		{
			pcFilter = pcProject->GetFilter(psEntry->uiIndex);
			iNode = AddNode(iParentNode, psEntry->szName, mpFolderIcon, SENT_Filter, uiSolution, uiProject, psEntry->uiIndex);
			AddFilterContents(iNode, uiSolution, uiProject, pcFilter->GetName());
		}
		else
		{
			AddNode(iParentNode, psEntry->szName, mpFileIcon, SENT_File, uiSolution, uiProject, psEntry->uiIndex);
		}
	}
	asEntries.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What a tree node is, NULL for no node.
//
//////////////////////////////////////////////////////////////////////////
SSolutionExplorerNode* CSolutionExplorer::GetNode(int32 iNode)
{
	if ((iNode == TREE_NO_NODE) || (mpcWorkspace == NULL))
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
		pcVcxProject = GetSolution(psNode->uiSolution)->GetProject(psNode->uiProject)->GetProject();
		pcVcxProject->GetFullPath(pcVcxProject->GetItem(psNode->uiIndex), pszDest);
		return true;
	}
	else if (psNode->eType == SENT_SolutionItem)
	{
		pcProject = GetSolution(psNode->uiSolution)->GetProject(psNode->uiProject);
		ResolveVcxPath(GetSolution(psNode->uiSolution)->GetDirectory(), pcProject->GetSolutionItem(psNode->uiIndex), pszDest);
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Each file's name is in the colour of its state in its repository's last
// git status, or the theme's colour if it is unmodified or in none.  A
// folder, project or the solution with a changed file somewhere in it
// (not just untracked or ignored ones) is in the modified colour, as
// IntelliJ IDEA shows directories with changed descendants.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::ColourFiles(CGitRepositories* pcGit, Ptr<CFontColours> pColours)
{
	int32			iNode;
	int32			iParent;
	CChars			szPath;
	EGitFileState	eState;
	CArrayBit		abChanged;

	abChanged.Init();
	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		abChanged.Add(false);
	}

	szPath.Init();
	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		szPath.Clear();
		if (GetNodeFilePath(iNode, &szPath))
		{
			eState = pcGit->GetFileState(szPath.Text());
			mpTree->SetNodeFont(iNode, GetGitStateFont(pColours, eState));
			if ((eState != GFS_Unmodified) && (eState != GFS_Untracked) && (eState != GFS_Ignored))
			{
				for (iParent = mpTree->GetNodeParent(iNode); (iParent != TREE_NO_NODE) && !abChanged.Get(iParent); iParent = mpTree->GetNodeParent(iParent))
				{
					abChanged.Set(iParent, true);
				}
			}
		}
	}
	szPath.Kill();

	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		if (GetNode(iNode)->eType != SENT_File && GetNode(iNode)->eType != SENT_SolutionItem)
		{
			mpTree->SetNodeFont(iNode, GetFolderFont(iNode, abChanged.Get(iNode), pColours));
		}
	}
	abChanged.Kill();
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
// The project of the file at szPath (any path to it), the first if more
// than one has it.  SOLUTION_EXPLORER_NO_PROJECT if none has.
//
//////////////////////////////////////////////////////////////////////////
size CSolutionExplorer::FindFileProject(const char* szPath)
{
	int32	iNode;
	CChars	szWanted;
	CChars	szNodePath;
	size	uiProject;

	szWanted.Init(szPath);
	MakeFullGitPath(&szWanted);
	uiProject = SOLUTION_EXPLORER_NO_PROJECT;
	szNodePath.Init();
	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		szNodePath.Clear();
		if ((GetNode(iNode)->eType == SENT_File) && GetNodeFilePath(iNode, &szNodePath))
		{
			MakeFullGitPath(&szNodePath);
			if (IsGitPathIn(szNodePath.Text(), szWanted.Text()) && (szNodePath.Length() == szWanted.Length()))
			{
				uiProject = GetNodeProject(iNode);
				break;
			}
		}
	}
	szNodePath.Kill();
	szWanted.Kill();
	return uiProject;
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
// The workspace's project a project's file node is in (the code
// model's), or SOLUTION_EXPLORER_NO_PROJECT for any other node (a
// solution item isn't built by a project).
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
	return mpcWorkspace->FindProject(psNode->uiSolution, psNode->uiProject);
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's solution any node is in, SOLUTION_EXPLORER_NO_SOLUTION
// for no node.
//
//////////////////////////////////////////////////////////////////////////
size CSolutionExplorer::GetNodeSolution(int32 iNode)
{
	SSolutionExplorerNode*	psNode;

	psNode = GetNode(iNode);
	return (psNode != NULL) ? psNode->uiSolution : SOLUTION_EXPLORER_NO_SOLUTION;
}


//////////////////////////////////////////////////////////////////////////
//
// The top node of the workspace's solution uiSolution, TREE_NO_NODE if
// it isn't shown.
//
//////////////////////////////////////////////////////////////////////////
int32 CSolutionExplorer::FindSolutionNode(size uiSolution)
{
	int32	iNode;

	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		if ((mpTree->GetNodeParent(iNode) == TREE_NO_NODE) && (GetNodeSolution(iNode) == uiSolution))
		{
			return iNode;
		}
	}
	return TREE_NO_NODE;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's name is drawn in pBoldColours' font (NULL for
// the theme's).
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorer::SetBoldColours(Ptr<CFontColours> pBoldColours)
{
	mpBoldColours = pBoldColours;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's node, when there is more than one solution.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionExplorer::IsStartupNode(int32 iNode)
{
	SSolutionExplorerNode*	psNode;

	psNode = GetNode(iNode);
	if ((psNode == NULL) || (psNode->eType != SENT_Solution) || (mpcWorkspace == NULL))
	{
		return false;
	}
	return (mpcWorkspace->NumSolutions() > 1) && (psNode->uiSolution == mpcWorkspace->GetStartup());
}


//////////////////////////////////////////////////////////////////////////
//
// The font of a node that isn't a file: coloured as modified if bChanged
// (a file in it has changed), bold for the startup solution.  NULL for the
// theme's.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CFont> CSolutionExplorer::GetFolderFont(int32 iNode, bool bChanged, Ptr<CFontColours> pColours)
{
	if (IsStartupNode(iNode) && mpBoldColours.IsNotNull())
	{
		return mpBoldColours->GetFont(bChanged ? GetGitStateColour(GFS_Modified) : JEWEL_TEXT);
	}
	return bChanged ? GetGitStateFont(pColours, GFS_Modified) : Ptr<CFont>();
}


Ptr<CTreeContainer> CSolutionExplorer::GetTree(void) { return mpTree; }
Ptr<CComponent> CSolutionExplorer::GetComponent(void) { return mpScrollPane; }
