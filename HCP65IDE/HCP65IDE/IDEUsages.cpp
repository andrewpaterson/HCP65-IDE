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
#include <stdlib.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEUsages.h"


//A usage, where Find Usages puts it: its project and class (or file), for sorting the tree.
struct SIDEUsage
{
	CChars			szProject;
	CChars			szClass;
	SIDELocation*	psLocation;
	size			uiIndex;
};


//////////////////////////////////////////////////////////////////////////
//
// Projects, then classes, then files, then lines, alphabetically.
//
//////////////////////////////////////////////////////////////////////////
static int CompareIDEUsages(const void* pv1, const void* pv2)
{
	SIDEUsage*	ps1;
	SIDEUsage*	ps2;
	int			iResult;

	ps1 = (SIDEUsage*)pv1;
	ps2 = (SIDEUsage*)pv2;
	iResult = StringInsensitiveCompare(ps1->szProject.Text(), ps2->szProject.Text());
	if (iResult == 0)
	{
		iResult = StringInsensitiveCompare(ps1->szClass.Text(), ps2->szClass.Text());
	}
	if (iResult == 0)
	{
		iResult = StringCompare(ps1->psLocation->szFileName.Text(), ps2->psLocation->szFileName.Text());
	}
	if (iResult == 0)
	{
		iResult = (ps1->psLocation->iLine != ps2->psLocation->iLine) ? ((ps1->psLocation->iLine < ps2->psLocation->iLine) ? -1 : 1) : 0;
	}
	if (iResult == 0)
	{
		iResult = (ps1->psLocation->iColumn != ps2->psLocation->iColumn) ? ((ps1->psLocation->iColumn < ps2->psLocation->iColumn) ? -1 : 1) : 0;
	}
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	masUsages.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpUsages);
	M_Pointer(mpUsagesTool);
	U_Unknown(CArrayIDELocations, masUsages);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Free(void)
{
	if (mpUsages.IsNotNull())
	{
		mpUsages->GetActivationEvent()->Remove(this);
	}
	KillIDELocations(&masUsages);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Save(CObjectWriter* pcFile) { return false; }
bool CIDEUsages::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Find Usages tool window: a tree beside Build, as IntelliJ's Find
// window.  A line's node opens its place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::CreatePane(SDockPlace* psPlace)
{
	Ptr<CScrollPane>	pPane;

	mpUsages = OMalloc<CTreeContainer>(mpcContent->mpWindow, mpcContent->mpTheme);
	mpUsages->SetAlignment(CSH_Stretch, CSV_Top);
	mpUsages->GetActivationEvent()->Add(this, &CIDEUsages::UsageActivated);
	pPane = OMalloc<CScrollPane>(mpcContent->mpWindow, mpcContent->mpTheme);
	pPane->SetContent(mpUsages);
	pPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpUsagesTool = mpcContent->mpDock->AddPane(IDE_USAGES_ID, "Find Usages", NULL, pPane, DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+F7, IntelliJ's Find Usages: every use of the symbol at the caret
// (not its declarations and definition), in the Find Usages tree under
// the project its file is in, then the class (or file) it is in, then
// its line.  False if there is no symbol at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::FindUsages(void)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	CChars				szText;
	CChars				szName;
	CChars				szFile;
	SIDEUsage*			asUsages;
	SIDEUsage*			psUsage;
	size				uiUsages;
	size				i;
	size				j;
	size				uiCount;
	int32				iRoot;
	int32				iProject;
	int32				iClass;
	CFileUtil			cFileUtil;
	SIDELocation*		psLocation;

	asLocations.Init();
	szKey.Init();
	if (!mpcContent->FindSymbolAtCaret(&asLocations, &szKey))
	{
		KillIDELocations(&asLocations);
		szKey.Kill();
		return false;
	}

	//The usages are kept for opening; the declarations and definitions aren't usages.
	KillIDELocations(&masUsages);
	masUsages.Init();
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			memcpy(masUsages.Add(), psLocation, sizeof(SIDELocation));
			psLocation->szFileName.Init();
		}
	}
	KillIDELocations(&asLocations);

	uiUsages = masUsages.NumElements();
	asUsages = (SIDEUsage*)malloc((uiUsages > 0 ? uiUsages : 1) * sizeof(SIDEUsage));
	for (i = 0; i < uiUsages; i++)
	{
		psUsage = &asUsages[i];
		psUsage->psLocation = masUsages.Get(i);
		psUsage->uiIndex = i;
		mpcContent->mcCodeModel.GetUsageGroup(psUsage->psLocation, &psUsage->szProject, &psUsage->szClass);
	}
	qsort(asUsages, uiUsages, sizeof(SIDEUsage), CompareIDEUsages);

	szName.Init();
	GetIDESymbolName(szKey.Text(), &szName);
	szText.Init(uiUsages == 0 ? "No usages of " : "Usages of ");
	szText.Append(szName);
	if (uiUsages > 0)
	{
		szText.Append("  ");
		szText.Append((int)uiUsages);
		szText.Append(uiUsages == 1 ? " usage" : " usages");
	}
	mpUsages->RemoveAllNodes();
	iRoot = mpUsages->AddNode(TREE_NO_NODE, szText.Text(), NULL, 0);
	szText.Kill();
	szName.Kill();

	iProject = TREE_NO_NODE;
	iClass = TREE_NO_NODE;
	for (i = 0; i < uiUsages; i++)
	{
		psUsage = &asUsages[i];
		if ((i == 0) || !psUsage->szProject.Equals(asUsages[i - 1].szProject))
		{
			for (uiCount = 0, j = i; (j < uiUsages) && asUsages[j].szProject.Equals(psUsage->szProject); j++, uiCount++)
			{
			}
			szText.Init(psUsage->szProject);
			szText.Append("  ");
			szText.Append((int)uiCount);
			szText.Append(uiCount == 1 ? " usage" : " usages");
			iProject = mpUsages->AddNode(iRoot, szText.Text(), NULL, 0);
			szText.Kill();
			iClass = TREE_NO_NODE;
		}
		if ((iClass == TREE_NO_NODE) || !psUsage->szClass.Equals(asUsages[i - 1].szClass))
		{
			for (uiCount = 0, j = i; (j < uiUsages) && asUsages[j].szProject.Equals(psUsage->szProject) && asUsages[j].szClass.Equals(psUsage->szClass); j++, uiCount++)
			{
			}
			szText.Init(psUsage->szClass);
			szText.Append("  ");
			szText.Append((int)uiCount);
			iClass = mpUsages->AddNode(iProject, szText.Text(), NULL, 0);
			szText.Kill();
		}

		//"File.cpp 36: the line", the usage's index (plus one) its data.
		szFile.Init();
		cFileUtil.GetFileName(&szFile, psUsage->psLocation->szFileName.Text());
		szText.Init(szFile);
		szText.Append(' ');
		szText.Append(psUsage->psLocation->iLine + 1);
		szText.Append(": ");
		mpcContent->GetFileLine(psUsage->psLocation->szFileName.Text(), psUsage->psLocation->iLine, &szText);
		mpUsages->AddNode(iClass, szText.Text(), NULL, (int64)psUsage->uiIndex + 1);
		szText.Kill();
		szFile.Kill();
	}
	for (i = 0; i < uiUsages; i++)
	{
		asUsages[i].szClass.Kill();
		asUsages[i].szProject.Kill();
	}
	free(asUsages);
	szKey.Kill();

	mpUsages->ExpandAll();
	mpcContent->mpDock->Show(mpUsagesTool);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A line's node, double clicked or entered, opens its place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::UsageActivated(void)
{
	int64	iData;

	if (mpUsages->GetSelectedNode() == TREE_NO_NODE)
	{
		return;
	}
	iData = mpUsages->GetNodeData(mpUsages->GetSelectedNode());
	if ((iData > 0) && ((size)iData <= masUsages.NumElements()))
	{
		mpcContent->OpenLocation(masUsages.Get((size)iData - 1));
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTreeContainer> CIDEUsages::GetUsagesTree(void) { return mpUsages; }
