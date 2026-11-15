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
#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "WindowLib/TreeContainer.h"
#include "IDEContent.h"
#include "IDEScript.h"
#include "IDESolutionFileChooser.h"


#define IDE_FILE_CHOOSER_WIDTH	460
#define IDE_FILE_CHOOSER_HEIGHT	520


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutionFileChooser::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszExtensions.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutionFileChooser::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpExplorer);
	U_Unknown(CChars, mszExtensions);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutionFileChooser::Free(void)
{
	mszExtensions.Kill();
}


bool CIDESolutionFileChooser::Save(CObjectWriter* pcFile) { return false; }
bool CIDESolutionFileChooser::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is one that can be chosen: its name ends as asked.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutionFileChooser::IsChoosable(const char* szPath)
{
	CArrayChars	aszExtensions;
	CChars		szName;
	size		i;
	bool		bChoosable;

	if (mszExtensions.Empty())
	{
		return true;
	}
	aszExtensions.Init();
	mszExtensions.Split(&aszExtensions, ';');
	szName.Init(szPath);
	bChoosable = false;
	for (i = 0; i < aszExtensions.NumElements(); i++)
	{
		bChoosable = bChoosable || szName.EndsWithIgnoreCase(aszExtensions.GetText(i));
	}
	szName.Kill();
	aszExtensions.Kill();
	return bChoosable;
}


//////////////////////////////////////////////////////////////////////////
//
// OK closes it on a file that can be chosen.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutionFileChooser::Approve(void)
{
	CChars	szPath;
	bool	bChoosable;

	szPath.Init();
	bChoosable = mpExplorer->GetSelectedFilePath(&szPath) && IsChoosable(szPath.Text());
	szPath.Kill();
	return bChoosable;
}


//////////////////////////////////////////////////////////////////////////
//
// A row double clicked or entered: a file that can be chosen is.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutionFileChooser::Activated(CListContainer* pcList)
{
	if (mpDialog.IsNotNull() && Approve())
	{
		mpDialog->OKClicked();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The chooser shown over pOwner, with szSelected (a file's name or path,
// or NULL) selected in it; szExtensions is what the chosen file's name
// must end with (".h;.hpp"), NULL for any file.  True, and the file's
// path, if one was chosen.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutionFileChooser::Choose(Ptr<CWindow> pOwner, const char* szTitle, const char* szExtensions, const char* szSelected, CChars* pszDest)
{
	Ptr<CTreeContainer>	pTree;
	CChars				szWanted;
	CChars				szPath;
	int32				iNode;
	int32				iParent;
	bool				bResult;

	mszExtensions.Set(szExtensions ? szExtensions : "");
	mpDialog = OMalloc<CDialog>(pOwner, mpcContent->GetDialogLook(), szTitle);
	mpExplorer = OMalloc<CSolutionExplorer>(mpDialog->GetWindow(), mpcContent->GetDialogLook()->pcTheme);
	mpExplorer->ShowWorkspace(mpcContent->GetWorkspace());
	pTree = mpExplorer->GetTree();

	//The file to begin on: by its path, or failing that its name.
	iNode = TREE_NO_NODE;
	szWanted.Init((szSelected != NULL) ? szSelected : "");
	szWanted.Replace('\\', '/');
	for (iParent = 0; !szWanted.Empty() && (iNode == TREE_NO_NODE) && (iParent < pTree->NumNodes()); iParent++)
	{
		szPath.Init();
		if (mpExplorer->GetNodeFilePath(iParent, &szPath))
		{
			szPath.Replace('\\', '/');
			if (szPath.EqualsIgnoreCase(szWanted.Text()))
			{
				iNode = iParent;
			}
		}
		szPath.Kill();
	}
	if ((iNode == TREE_NO_NODE) && !szWanted.Empty())
	{
		iNode = mpExplorer->FindFileNode(szWanted.Text());
	}
	szWanted.Kill();
	if (iNode != TREE_NO_NODE)
	{
		for (iParent = pTree->GetNodeParent(iNode); iParent != TREE_NO_NODE; iParent = pTree->GetNodeParent(iParent))
		{
			pTree->SetExpanded(iParent, true);
		}
		pTree->UpdateRows();
		pTree->SelectNode(iNode);
	}
	pTree->GetActivationEvent()->Add(this, &CIDESolutionFileChooser::Activated);
	mpDialog->SetBody(mpExplorer->GetComponent(), pTree);
	mpDialog->SetOKCaption("Choose");
	mpDialog->SetResizable(IDE_FILE_CHOOSER_WIDTH / 2, IDE_FILE_CHOOSER_HEIGHT / 2);
	mpDialog->SetApprover(this, &CIDESolutionFileChooser::Approve);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(IDE_FILE_CHOOSER_WIDTH, IDE_FILE_CHOOSER_HEIGHT);
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	pTree->GetActivationEvent()->Remove(this);

	//However it was closed, only a file that can be chosen is.
	pszDest->Clear();
	bResult = bResult && mpExplorer->GetSelectedFilePath(pszDest) && IsChoosable(pszDest->Text());
	pTree = NULL;
	mpExplorer = NULL;
	mpDialog = NULL;
	return bResult;
}
