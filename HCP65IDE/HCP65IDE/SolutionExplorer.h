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
#ifndef __SOLUTION_EXPLORER_H__
#define __SOLUTION_EXPLORER_H__
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/TreeContainer.h"
#include "WindowLib/ScrollPane.h"
#include "VisualStudioLib/VcxSolution.h"


enum ESolutionExplorerNodeType
{
	SENT_Solution,
	SENT_SolutionFolder,
	SENT_SolutionItem,		//A solution folder's file; its index is the item's.
	SENT_Project,
	SENT_Filter,			//Its index is the filter's in its project.
	SENT_File,				//Its index is the item's in its project.
};


//What a node of Solution Explorer is.  A tree node's data is the index of its SSolutionExplorerNode.
struct SSolutionExplorerNode
{
	ESolutionExplorerNodeType	eType;
	size						uiProject;	//The solution's project (or folder) the node is in; unused for the solution.
	size						uiIndex;
};


typedef CArrayTemplate<SSolutionExplorerNode>	CArraySolutionExplorerNode;


#define SOLUTION_EXPLORER_NO_PROJECT	MAX_SIZE


//A folder, project or file to be shown, while the ones in a folder are sorted.
struct SSolutionExplorerEntry
{
	const char*	szName;
	bool		bFolder;
	size		uiIndex;	//The project's, filter's or item's.
};


typedef CArrayTemplate<SSolutionExplorerEntry>	CArraySolutionExplorerEntry;


//Solution Explorer: the solution, its solution folders and projects, each project's filters as folders and its files in
//them, as Visual Studio shows them.  In each folder the folders come first and then the projects or files, each in
//alphabetical order.  It is an object held by the IDE's content.
class CSolutionExplorer : public CObject
{
CONSTRUCTABLE(CSolutionExplorer);
DESTRUCTABLE(CSolutionExplorer);
protected:
	Ptr<CTreeContainer>			mpTree;
	Ptr<CScrollPane>			mpScrollPane;
	Ptr<CImageCel>				mpFolderIcon;
	Ptr<CImageCel>				mpFileIcon;
	Ptr<CImageCel>				mpProjectIcon;
	Ptr<CImageCel>				mpSolutionIcon;
	CVcxSolution*				mpcSolution;
	CArraySolutionExplorerNode	masNodes;

public:
	void					Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme);
	void					Class(void) override;
	void					Free(void) override;

	bool					Save(CObjectWriter* pcFile) override;
	bool					Load(CObjectReader* pcFile) override;

	void					ShowSolution(CVcxSolution* pcSolution);
	Ptr<CTreeContainer>		GetTree(void);
	Ptr<CComponent>			GetComponent(void);
	SSolutionExplorerNode*	GetNode(int32 iNode);
	bool					GetNodeFilePath(int32 iNode, CChars* pszDest);
	bool					GetSelectedFilePath(CChars* pszDest);
	size					GetNodeProject(int32 iNode);
	bool					GetNodeFileName(int32 iNode, CChars* pszDest);
	int32					FindFileNode(const char* szFileName);

protected:
	int32					AddNode(int32 iParentNode, const char* szText, Ptr<CImageCel> pIcon, ESolutionExplorerNodeType eType, size uiProject, size uiIndex);
	void					AddSolutionContents(int32 iParentNode, const char* szParentGuid);
	void					AddSolutionItems(int32 iParentNode, size uiFolder);
	void					AddProject(int32 iParentNode, size uiProject);
	void					AddFilterContents(int32 iParentNode, size uiProject, const char* szFilter);
};


int  CompareSolutionExplorerEntries(const void* pv1, const void* pv2);
void AddSolutionExplorerEntry(CArraySolutionExplorerEntry* pasEntries, const char* szName, bool bFolder, size uiIndex);


#endif // __SOLUTION_EXPLORER_H__

