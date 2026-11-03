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
#include "WindowLib/TreeContainer.h"
#include "WindowLib/ScrollPane.h"
#include "VisualStudioLib/VcxProject.h"


//What a node of Solution Explorer is: its data is the item's index plus one for a file, minus the filter's index
//minus one for a filter, and SOLUTION_EXPLORER_PROJECT for the project.
#define SOLUTION_EXPLORER_PROJECT	0


//Solution Explorer: the project, its filters as folders and its files in them, as Visual Studio shows them.  In each
//folder the folders come first and then the files, each in alphabetical order.
class CSolutionExplorer
{
protected:
	Ptr<CTreeContainer>	mpTree;
	Ptr<CScrollPane>	mpScrollPane;
	Ptr<CImageCel>		mpFolderIcon;
	Ptr<CImageCel>		mpFileIcon;
	Ptr<CImageCel>		mpProjectIcon;
	CVcxProject*		mpcProject;

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme);
	void				Kill(void);

	void				ShowProject(CVcxProject* pcProject);
	Ptr<CTreeContainer>	GetTree(void);
	Ptr<CComponent>		GetComponent(void);
	CVcxItem*			GetSelectedItem(void);
	CVcxItem*			GetNodeItem(int32 iNode);
	int32				FindFileNode(const char* szFileName);

protected:
	void				AddFilterContents(int32 iParentNode, const char* szFilter);
	void				SortFilters(CArrayInt* paiFilters);
	void				SortItems(CArrayInt* paiItems);
};


#endif // __SOLUTION_EXPLORER_H__

