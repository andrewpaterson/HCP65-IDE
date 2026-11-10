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
#include "WindowLib/GridContainer.h"
#include "WindowLib/ScrollPane.h"
#include "IDEContent.h"
#include "IDEUsages.h"


#define IDE_USAGES_OPTION_GAP	16	//Between the grouping check boxes.


//A usage, where Find Usages puts it: its usage type's group, its project and the class it is in, for sorting the tree.
struct SIDEUsage
{
	int				iType;
	CChars			szProject;
	CChars			szClass;
	SIDELocation*	psLocation;
	size			uiIndex;
	bool			bByType;
	bool			bByProject;
};


//The usage types' groups, in the order the tree shows them: the declarations first, then how the references use it.
static const int	giIDEUsageTypeDefinition = 0;
static const int	giIDEUsageTypeDeclaration = 1;
static const int	gaiIDEUsageTypeRanks[CPPUT_NumUsageTypes] =
{
	14,		//CPPUT_Other
	2,		//CPPUT_Call
	3,		//CPPUT_Read
	4,		//CPPUT_Write
	12,		//CPPUT_TypeInExpression
	11,		//CPPUT_TemplateArgument
	10,		//CPPUT_BaseClass
	5,		//CPPUT_Field
	7,		//CPPUT_Local
	8,		//CPPUT_Global
	6,		//CPPUT_Parameter
	9,		//CPPUT_Function
	13,		//CPPUT_Alias
};
static const char*	gaszIDEUsageTypeNames[CPPUT_NumUsageTypes] =
{
	"Unclassified usage",
	"Call",
	"Value read",
	"Value write",
	"Usage as a type in an expression",
	"Usage in template argument",
	"Usage in base class list",
	"Usage in field declaration",
	"Usage in local variable declaration",
	"Usage in global variable declaration",
	"Usage in parameter declaration",
	"Usage in function declaration",
	"Usage in type alias",
};


//////////////////////////////////////////////////////////////////////////
//
// The groups that are on, by usage type then project, then the files
// (whatever their case), then each file's lines and columns in order.
//
//////////////////////////////////////////////////////////////////////////
static int __cdecl CompareIDEUsages(const void* pv1, const void* pv2)
{
	SIDEUsage*	ps1;
	SIDEUsage*	ps2;
	int			iResult;

	ps1 = (SIDEUsage*)pv1;
	ps2 = (SIDEUsage*)pv2;
	iResult = 0;
	if (ps1->bByType)
	{
		iResult = (ps1->iType != ps2->iType) ? ((ps1->iType < ps2->iType) ? -1 : 1) : 0;
	}
	if ((iResult == 0) && ps1->bByProject)
	{
		iResult = StringInsensitiveCompare(ps1->szProject.Text(), ps2->szProject.Text());
	}
	if (iResult == 0)
	{
		iResult = StringInsensitiveCompare(ps1->psLocation->szFileName.Text(), ps2->psLocation->szFileName.Text());
	}
	if (iResult == 0)
	{
		iResult = (ps1->psLocation->iLine != ps2->psLocation->iLine) ? ((ps1->psLocation->iLine < ps2->psLocation->iLine) ? -1 : 1) : 0;
	}
	if (iResult == 0)
	{
		iResult = (ps1->psLocation->iColumn != ps2->psLocation->iColumn) ? ((ps1->psLocation->iColumn < ps2->psLocation->iColumn) ? -1 : 1) : 0;
	}
	if (iResult == 0)
	{
		iResult = (ps1->uiIndex != ps2->uiIndex) ? ((ps1->uiIndex < ps2->uiIndex) ? -1 : 1) : 0;
	}
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
// " 3 usages" after a group's name, or " 1 usage".
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDEUsageCount(CChars* pszDest, size uiCount, const char* szWhat)
{
	pszDest->Append("  ");
	pszDest->Append((int)uiCount);
	pszDest->Append(szWhat);
	pszDest->Append(uiCount == 1 ? "" : "s");
}


//////////////////////////////////////////////////////////////////////////
//
// Grouped by usage type, project and class, as IntelliJ IDEA starts.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	masUsages.Init();
	mszName.Init();
	muiOverrides = 0;
	mbDeclarations = false;
	mbFound = false;
	muiGrouping = IDE_USAGES_BY_ALL;
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
	M_Pointer(mpEscapeAction);
	M_Pointer(mpUsagesTool);
	M_Pointer(mpByType);
	M_Pointer(mpByProject);
	M_Pointer(mpByClass);
	U_Unknown(CArrayIDELocations, masUsages);
	U_String(mszName);
	U_Size(muiOverrides);
	U_Bool(mbDeclarations);
	U_Bool(mbFound);
	U_UInt8(muiGrouping);
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
		if (mpEscapeAction.IsNotNull())
		{
			mpUsages->RemoveInputAction(mpEscapeAction);
			mpEscapeAction = NULL;
		}
	}
	if (mpByType.IsNotNull())
	{
		mpByType->GetChangeEvent()->Remove(this);
		mpByProject->GetChangeEvent()->Remove(this);
		mpByClass->GetChangeEvent()->Remove(this);
	}
	KillIDELocations(&masUsages);
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Save(CObjectWriter* pcFile) { return false; }
bool CIDEUsages::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Find Usages tool window: the grouping check boxes over a tree,
// beside Build, as IntelliJ's Find window.  A line's node opens its place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::CreatePane(SDockPlace* psPlace)
{
	Ptr<CScrollPane>	pPane;
	Ptr<CGridContainer>	pOptions;
	Ptr<CGridContainer>	pPanel;

	mpUsages = OMalloc<CTreeContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpUsages->SetAlignment(CSH_Stretch, CSV_Top);
	mpUsages->GetActivationEvent()->Add(this, &CIDEUsages::UsageActivated);

	//As IntelliJ's: only Jump to Source (F4) and double clicking go to a usage, not space or enter, and escape goes back
	//to the editor.
	mpUsages->SetInputActionsEnabled(CIC_Space, false);
	mpUsages->SetInputActionsEnabled(CIC_Enter, false);
	mpEscapeAction = mpUsages->AddInputAction(CIC_ClearSelection, this, &CIDEUsages::ReturnToEditor);
	pPane = OMalloc<CScrollPane>(mpcContent->GetWindow(), mpcContent->GetTheme());
	pPane->SetContent(mpUsages);
	pPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpByType = OMalloc<CCheckBox>(mpcContent->GetWindow(), mpcContent->GetTheme(), "Group by usage type", (muiGrouping & IDE_USAGES_BY_TYPE) != 0);
	mpByProject = OMalloc<CCheckBox>(mpcContent->GetWindow(), mpcContent->GetTheme(), "Group by project", (muiGrouping & IDE_USAGES_BY_PROJECT) != 0);
	mpByClass = OMalloc<CCheckBox>(mpcContent->GetWindow(), mpcContent->GetTheme(), "Group by class", (muiGrouping & IDE_USAGES_BY_CLASS) != 0);
	mpByType->SetAlignment(CSH_Left, CSV_Center);
	mpByProject->SetAlignment(CSH_Left, CSV_Center);
	mpByClass->SetAlignment(CSH_Left, CSV_Center);
	mpByType->SetInsets(0, IDE_USAGES_OPTION_GAP, 0, 0);
	mpByProject->SetInsets(0, IDE_USAGES_OPTION_GAP, 0, 0);
	mpByType->GetChangeEvent()->Add(this, &CIDEUsages::GroupingChanged);
	mpByProject->GetChangeEvent()->Add(this, &CIDEUsages::GroupingChanged);
	mpByClass->GetChangeEvent()->Add(this, &CIDEUsages::GroupingChanged);
	pOptions = OMalloc<CGridContainer>(mpcContent->GetWindow());
	pOptions->SetAlignment(CSH_Stretch, CSV_Top);
	pOptions->SetInsets(2, 4, 2, 4);
	pOptions->AddComponent(mpByType, 0, 0);
	pOptions->AddComponent(mpByProject, 1, 0);
	pOptions->AddComponent(mpByClass, 2, 0, 1, 1, 1, 0);

	pPanel = OMalloc<CGridContainer>(mpcContent->GetWindow());
	pPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	pPanel->AddComponent(pOptions, 0, 0, 1, 1, 1, 0);
	pPanel->AddComponent(pPane, 0, 1, 1, 1, 1, 1);
	mpUsagesTool = mpcContent->GetDock()->AddPane(IDE_USAGES_ID, "Find Usages", NULL, pPanel, DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+F7, IntelliJ's Find Usages: every use of the symbol at the caret
// (not its declarations and definition), and of its overrides and what it
// overrides if it is a virtual member function, in the Find Usages tree
// (ShowUsages).  With bDeclarations (Shift+F12, Visual Studio's Find All
// References) its declarations and definition too.  False if there is no
// symbol at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::FindUsages(bool bDeclarations)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	SIDELocation*		psLocation;
	size				i;

	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	asLocations.Init();
	szKey.Init();
	if (!mpcContent->GetDocuments()->FindSymbolAtCaret(&asLocations, &szKey))
	{
		KillIDELocations(&asLocations);
		szKey.Kill();
		mpcContent->ShowStatus(bDeclarations ? "No references found at the caret." : "No usages found at the caret.");
		return false;
	}

	//A virtual member function's overrides (and what it overrides) are used where it is, as IntelliJ's hierarchy.
	muiOverrides = mpcContent->GetCodeModel()->FindOverrideLocations(szKey.Text(), &asLocations);

	//The usages are kept for opening; the declarations and definitions are only kept if asked for.
	KillIDELocations(&masUsages);
	masUsages.Init();
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (bDeclarations || (psLocation->uiRole == CPPIR_Reference))
		{
			memcpy(masUsages.Add(), psLocation, sizeof(SIDELocation));
			psLocation->szFileName.Init();
		}
	}
	KillIDELocations(&asLocations);

	mszName.Kill();
	mszName.Init();
	GetIDESymbolName(szKey.Text(), &mszName);
	szKey.Kill();
	mbDeclarations = bDeclarations;
	mbFound = true;

	ShowUsages(false);
	mpcContent->GetDock()->Show(mpUsagesTool);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, in the tree: its top says what was looked for, then
// the groups that are on (the usage type, then the project), each file,
// the classes in it if they are grouped, and each line, in the order the
// file has them.  A class's lines are under it, where it first has one.
// The first line is selected, for F4 to go to, or with bKeepSelected the
// usage that was, if one was: the same usages grouped again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ShowUsages(bool bKeepSelected)
{
	CChars			szText;
	CChars			szFile;
	SIDEUsage*		asUsages;
	SIDEUsage*		psUsage;
	size			uiUsages;
	size			i;
	size			j;
	size			uiCount;
	int32			iRoot;
	int32			iParent;
	int32			iType;
	int32			iProject;
	int32			iFile;
	int32			iClass;
	CFileUtil		cFileUtil;
	CArrayChars		aszClasses;
	CArrayInt		aiClassNodes;
	size			uiIndexed;
	size			uiToIndex;
	const char*		szWhat;
	bool			bByType;
	bool			bByProject;
	bool			bByClass;
	bool			bNewType;
	bool			bNewProject;
	bool			bNewFile;
	size			uiClass;
	int64			iSelectedData;
	int32			iNode;
	int32			iFirstLine;
	int32			iSelect;

	iSelectedData = 0;
	if (bKeepSelected && (mpUsages->GetSelectedNode() != TREE_NO_NODE))
	{
		iSelectedData = mpUsages->GetNodeData(mpUsages->GetSelectedNode());
	}
	mpUsages->RemoveAllNodes();
	if (!mbFound)
	{
		return;
	}
	iFirstLine = TREE_NO_NODE;
	iSelect = TREE_NO_NODE;

	bByType = (muiGrouping & IDE_USAGES_BY_TYPE) != 0;
	bByProject = (muiGrouping & IDE_USAGES_BY_PROJECT) != 0;
	bByClass = (muiGrouping & IDE_USAGES_BY_CLASS) != 0;
	uiUsages = masUsages.NumElements();
	asUsages = (SIDEUsage*)malloc((uiUsages > 0 ? uiUsages : 1) * sizeof(SIDEUsage));
	for (i = 0; i < uiUsages; i++)
	{
		psUsage = &asUsages[i];
		psUsage->psLocation = masUsages.Get(i);
		psUsage->uiIndex = i;
		psUsage->iType = GetUsageTypeRank(psUsage->psLocation);
		psUsage->bByType = bByType;
		psUsage->bByProject = bByProject;
		mpcContent->GetCodeModel()->GetUsageGroup(psUsage->psLocation, &psUsage->szProject, &psUsage->szClass);
	}
	qsort(asUsages, uiUsages, sizeof(SIDEUsage), CompareIDEUsages);

	szWhat = mbDeclarations ? " reference" : " usage";
	if (mbDeclarations)
	{
		szText.Init(uiUsages == 0 ? "No references to " : "References to ");
	}
	else
	{
		szText.Init(uiUsages == 0 ? "No usages of " : "Usages of ");
	}
	szText.Append(mszName);
	if (muiOverrides > 0)
	{
		szText.Append(" and ");
		szText.Append((int)muiOverrides);
		szText.Append(muiOverrides == 1 ? " override" : " overrides");
	}
	if (uiUsages > 0)
	{
		AppendIDEUsageCount(&szText, uiUsages, szWhat);
	}

	//Until every file is indexed some may be missing.
	mpcContent->GetCodeModel()->GetIndexingProgress(&uiIndexed, &uiToIndex);
	if (uiIndexed < uiToIndex)
	{
		szText.Append("  (indexing, ");
		szText.Append((uint32)uiIndexed);
		szText.Append(" of ");
		szText.Append((uint32)uiToIndex);
		szText.Append(" files)");
	}
	iRoot = mpUsages->AddNode(TREE_NO_NODE, szText.Text(), NULL, 0);
	szText.Kill();

	iType = iRoot;
	iProject = iRoot;
	iFile = iRoot;
	aszClasses.Init();
	aiClassNodes.Init();
	for (i = 0; i < uiUsages; i++)
	{
		psUsage = &asUsages[i];
		bNewType = bByType && ((i == 0) || (psUsage->iType != asUsages[i - 1].iType));
		bNewProject = bByProject && (bNewType || (i == 0) || !psUsage->szProject.Equals(asUsages[i - 1].szProject));
		bNewFile = bNewType || bNewProject || (i == 0) || !psUsage->psLocation->szFileName.EqualsIgnoreCase(asUsages[i - 1].psLocation->szFileName);

		if (bNewType)
		{
			for (uiCount = 0, j = i; (j < uiUsages) && (asUsages[j].iType == psUsage->iType); j++, uiCount++)
			{
			}
			szText.Init(GetUsageTypeName(psUsage->psLocation));
			AppendIDEUsageCount(&szText, uiCount, szWhat);
			iType = mpUsages->AddNode(iRoot, szText.Text(), NULL, 0);
			szText.Kill();
		}
		if (bNewProject)
		{
			for (uiCount = 0, j = i; (j < uiUsages) && (!bByType || (asUsages[j].iType == psUsage->iType)) && asUsages[j].szProject.Equals(psUsage->szProject); j++, uiCount++)
			{
			}
			szText.Init(psUsage->szProject);
			AppendIDEUsageCount(&szText, uiCount, szWhat);
			iProject = mpUsages->AddNode(bByType ? iType : iRoot, szText.Text(), NULL, 0);
			szText.Kill();
		}
		if (bNewFile)
		{
			for (uiCount = 0, j = i; (j < uiUsages) && (!bByType || (asUsages[j].iType == psUsage->iType)) && (!bByProject || asUsages[j].szProject.Equals(psUsage->szProject)) && asUsages[j].psLocation->szFileName.EqualsIgnoreCase(psUsage->psLocation->szFileName); j++, uiCount++)
			{
			}
			szFile.Init();
			cFileUtil.GetFileName(&szFile, psUsage->psLocation->szFileName.Text());
			szText.Init(szFile);
			szText.Append("  ");
			szText.Append((int)uiCount);
			iFile = mpUsages->AddNode(bByProject ? iProject : (bByType ? iType : iRoot), szText.Text(), NULL, 0);
			szText.Kill();
			szFile.Kill();

			//The file's lines, from its first, under the classes they are in.
			aszClasses.ReInit();
			aiClassNodes.Kill();
			aiClassNodes.Init();
		}

		iParent = iFile;
		if (bByClass && !psUsage->szClass.Empty())
		{
			uiClass = aszClasses.GetIndex(psUsage->szClass.Text());
			if (uiClass == ARRAY_ELEMENT_NOT_FOUND)
			{
				for (uiCount = 0, j = i; (j < uiUsages) && (!bByType || (asUsages[j].iType == psUsage->iType)) && (!bByProject || asUsages[j].szProject.Equals(psUsage->szProject)) && asUsages[j].psLocation->szFileName.EqualsIgnoreCase(psUsage->psLocation->szFileName); j++)
				{
					if (asUsages[j].szClass.Equals(psUsage->szClass))
					{
						uiCount++;
					}
				}
				szText.Init(psUsage->szClass);
				szText.Append("  ");
				szText.Append((int)uiCount);
				iClass = mpUsages->AddNode(iFile, szText.Text(), NULL, 0);
				szText.Kill();
				aszClasses.Add(psUsage->szClass.Text());
				aiClassNodes.Add(iClass);
				uiClass = aszClasses.NumElements() - 1;
			}
			iParent = aiClassNodes.GetValue(uiClass);
		}

		//"36: the line", the usage's index (plus one) its data.
		szText.Init();
		szText.Append(psUsage->psLocation->iLine + 1);
		szText.Append(": ");
		mpcContent->GetDocuments()->GetFileLine(psUsage->psLocation->szFileName.Text(), psUsage->psLocation->iLine, &szText);
		if (!bByType)
		{
			if (psUsage->psLocation->uiRole == CPPIR_Definition)
			{
				szText.Append("  [definition]");
			}
			else if (psUsage->psLocation->uiRole == CPPIR_Declaration)
			{
				szText.Append("  [declaration]");
			}
		}
		if (psUsage->psLocation->uiFlags & CPPIF_AMBIGUOUS)
		{
			szText.Append("  [one of the overloads]");
		}
		iNode = mpUsages->AddNode(iParent, szText.Text(), NULL, (int64)psUsage->uiIndex + 1);
		szText.Kill();
		if (iFirstLine == TREE_NO_NODE)
		{
			iFirstLine = iNode;
		}
		if ((iSelectedData > 0) && ((int64)psUsage->uiIndex + 1 == iSelectedData))
		{
			iSelect = iNode;
		}
	}
	aiClassNodes.Kill();
	aszClasses.Kill();
	for (i = 0; i < uiUsages; i++)
	{
		asUsages[i].szClass.Kill();
		asUsages[i].szProject.Kill();
	}
	free(asUsages);

	mpUsages->ExpandAll();
	mpUsages->SelectNode((iSelect != TREE_NO_NODE) ? iSelect : iFirstLine);
}


//////////////////////////////////////////////////////////////////////////
//
// What a place's usage type group is called: its definition, a
// declaration, or how a reference uses its symbol.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEUsages::GetUsageTypeName(SIDELocation* psLocation)
{
	if (psLocation->uiRole == CPPIR_Definition)
	{
		return "Definition";
	}
	if (psLocation->uiRole == CPPIR_Declaration)
	{
		return "Declaration";
	}
	return (psLocation->uiUsage < CPPUT_NumUsageTypes) ? gaszIDEUsageTypeNames[psLocation->uiUsage] : gaszIDEUsageTypeNames[CPPUT_Other];
}


//////////////////////////////////////////////////////////////////////////
//
// Where a place's usage type group goes among the others: definitions,
// declarations, calls, reads and writes, then the declarations a type is
// named in, and the rest.
//
//////////////////////////////////////////////////////////////////////////
int CIDEUsages::GetUsageTypeRank(SIDELocation* psLocation)
{
	if (psLocation->uiRole == CPPIR_Definition)
	{
		return giIDEUsageTypeDefinition;
	}
	if (psLocation->uiRole == CPPIR_Declaration)
	{
		return giIDEUsageTypeDeclaration;
	}
	return (psLocation->uiUsage < CPPUT_NumUsageTypes) ? gaiIDEUsageTypeRanks[psLocation->uiUsage] : gaiIDEUsageTypeRanks[CPPUT_Other];
}


//////////////////////////////////////////////////////////////////////////
//
// A line's node, double clicked or jumped to (F4), opens its place.
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
		mpcContent->GetDocuments()->OpenLocation(masUsages.Get((size)iData - 1));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Escape in the tree: the keys go back to the active editor, as
// IntelliJ's tool windows give them back.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ReturnToEditor(void)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull())
	{
		mpcContent->GetWindow()->GetFocus()->SetFocus(pTab->GetEditor());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A grouping check box was ticked or cleared: what was found is shown
// again, grouped as they say.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::GroupingChanged(void)
{
	uint8	uiGrouping;

	uiGrouping = 0;
	uiGrouping |= mpByType->IsTicked() ? IDE_USAGES_BY_TYPE : 0;
	uiGrouping |= mpByProject->IsTicked() ? IDE_USAGES_BY_PROJECT : 0;
	uiGrouping |= mpByClass->IsTicked() ? IDE_USAGES_BY_CLASS : 0;
	if (uiGrouping != muiGrouping)
	{
		muiGrouping = uiGrouping;
		ShowUsages(true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The groups (IDE_USAGES_BY_*), as the check boxes would set them: the
// boxes are ticked to match and what was found is shown again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::SetGrouping(uint8 uiGrouping)
{
	muiGrouping = uiGrouping & IDE_USAGES_BY_ALL;
	if (mpByType.IsNotNull())
	{
		mpByType->SetTicked((muiGrouping & IDE_USAGES_BY_TYPE) != 0);
		mpByProject->SetTicked((muiGrouping & IDE_USAGES_BY_PROJECT) != 0);
		mpByClass->SetTicked((muiGrouping & IDE_USAGES_BY_CLASS) != 0);
	}
	ShowUsages(true);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CIDEUsages::GetGrouping(void) { return muiGrouping; }
Ptr<CTreeContainer> CIDEUsages::GetUsagesTree(void) { return mpUsages; }
Ptr<CDockPane> CIDEUsages::GetTool(void) { return mpUsagesTool; }


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so have the places found in them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Clear(void)
{
	KillIDELocations(&masUsages);
	masUsages.Init();
	mbFound = false;
	mpUsages->RemoveAllNodes();
}


//////////////////////////////////////////////////////////////////////////
//
// Find Usages (Alt+F7, as IntelliJ IDEA's), while a symbol is named at the
// caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.FindUsages", "Find Usages", "Alt+F7", Ptr<CImageCel>(), this, &CIDEUsages::FindUsagesOnly)->SetEnabledCheck(this, &CIDEUsages::CanFindUsages);
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' context menu's Find Usages.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::AddEditorItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.FindUsages"));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a symbol is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::CanFindUsages(void)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	bool				bFound;

	asLocations.Init();
	szKey.Init();
	bFound = mpcContent->GetDocuments()->FindSymbolAtCaret(&asLocations, &szKey);
	szKey.Kill();
	KillIDELocations(&asLocations);
	return bFound;
}


void CIDEUsages::FindUsagesOnly(void) { FindUsages(false); }  //Find Usages without the declarations and definition, which Find All References has.
