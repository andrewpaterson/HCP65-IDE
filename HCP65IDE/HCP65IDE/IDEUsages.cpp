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


//A usage type's group in the tree: where it goes among the others and what it is called.
struct SIDEUsageType
{
	int			iRank;
	const char*	szName;
};


//The groups, the declarations' first, then how the references use the symbol, each by its ECPPUsageType.
static const SIDEUsageType	gsIDEUsageTypeDefinition = { 0, "Definition" };
static const SIDEUsageType	gsIDEUsageTypeDeclaration = { 1, "Declaration" };
static const SIDEUsageType	gasIDEUsageTypes[] =
{
	{ 14,	"Unclassified usage" },						//CPPUT_Other
	{ 2,	"Call" },									//CPPUT_Call
	{ 3,	"Value read" },								//CPPUT_Read
	{ 4,	"Value write" },							//CPPUT_Write
	{ 12,	"Usage as a type in an expression" },		//CPPUT_TypeInExpression
	{ 11,	"Usage in template argument" },				//CPPUT_TemplateArgument
	{ 10,	"Usage in base class list" },				//CPPUT_BaseClass
	{ 5,	"Usage in field declaration" },				//CPPUT_Field
	{ 7,	"Usage in local variable declaration" },	//CPPUT_Local
	{ 8,	"Usage in global variable declaration" },	//CPPUT_Global
	{ 6,	"Usage in parameter declaration" },			//CPPUT_Parameter
	{ 9,	"Usage in function declaration" },			//CPPUT_Function
	{ 13,	"Usage in type alias" },					//CPPUT_Alias
};
static_assert(sizeof(gasIDEUsageTypes) / sizeof(SIDEUsageType) == CPPUT_NumUsageTypes, "Each ECPPUsageType needs its group in gasIDEUsageTypes.");


//////////////////////////////////////////////////////////////////////////
//
// A place's usage type group: its definition's, a declaration's, or that
// of how a reference uses its symbol.
//
//////////////////////////////////////////////////////////////////////////
static const SIDEUsageType* GetIDEUsageType(SIDELocation* psLocation)
{
	if (psLocation->uiRole == CPPIR_Definition)
	{
		return &gsIDEUsageTypeDefinition;
	}
	if (psLocation->uiRole == CPPIR_Declaration)
	{
		return &gsIDEUsageTypeDeclaration;
	}
	return &gasIDEUsageTypes[(psLocation->uiUsage < CPPUT_NumUsageTypes) ? psLocation->uiUsage : CPPUT_Other];
}


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
	masPopupUsages.Init();
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
	M_Pointer(mpPopup);
	U_Unknown(CArrayIDELocations, masPopupUsages);
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
	if (mpPopup.IsNotNull())
	{
		mpPopup->GetItemClickEvent()->Remove(this);
	}
	KillIDELocations(&masPopupUsages);
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
	mpcContent->GetWindow()->GetFocus()->SetFocus(mpUsages);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the active editor's caret is on a declaration or the definition
// of the symbol named there, for Control+B to show its usages, as
// IntelliJ's does, rather than go to the definition it is on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::IsOnOwnDeclaration(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	return mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) && (uiRole != CPPIR_Reference);
}


//////////////////////////////////////////////////////////////////////////
//
// The order of the popup's usages: by file, then line, then column.
//
//////////////////////////////////////////////////////////////////////////
static int __cdecl CompareIDEPopupUsages(const void* pv1, const void* pv2)
{
	SIDELocation*	ps1;
	SIDELocation*	ps2;
	int				iResult;

	ps1 = (SIDELocation*)pv1;
	ps2 = (SIDELocation*)pv2;
	iResult = StringInsensitiveCompare(ps1->szFileName.Text(), ps2->szFileName.Text());
	if (iResult != 0)
	{
		return iResult;
	}
	if (ps1->iLine != ps2->iLine)
	{
		return (ps1->iLine < ps2->iLine) ? -1 : 1;
	}
	return (ps1->iColumn < ps2->iColumn) ? -1 : ((ps1->iColumn > ps2->iColumn) ? 1 : 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Control+B on a declaration or the definition, as IntelliJ's: a small
// popup under the caret of the symbol's usages (and its overrides', as
// Find Usages has), the first selected, "Calculator.cpp:367  the line";
// Enter or a click goes to one.  It is at most a third of the screen's
// height and no taller than the editor's view; more usages scroll.  With only one it is gone to; with none
// the status line says so.  False if no symbol is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::ShowUsagesPopup(void)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	CChars				szName;
	CChars				szText;
	CChars				szFile;
	CChars				szLine;
	CFileUtil			cFileUtil;
	SIDELocation*		psLocation;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CInt32Rect			cBounds;
	CInt32Rect			cCaret;
	CInt32Rect			cScreen;
	SInt32Vec2			sPosition;
	Ptr<CPopup>			pPopup;
	int32				iMaximum;
	int32				iView;
	size				uiOverrides;
	size				i;

	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	asLocations.Init();
	szKey.Init();
	if (!mpcContent->GetDocuments()->FindSymbolAtCaret(&asLocations, &szKey))
	{
		KillIDELocations(&asLocations);
		szKey.Kill();
		return false;
	}
	uiOverrides = mpcContent->GetCodeModel()->FindOverrideLocations(szKey.Text(), &asLocations);
	szName.Init();
	GetIDESymbolName(szKey.Text(), &szName);
	szKey.Kill();

	KillIDELocations(&masPopupUsages);
	masPopupUsages.Init();
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			memcpy(masPopupUsages.Add(), psLocation, sizeof(SIDELocation));
			psLocation->szFileName.Init();
		}
	}
	KillIDELocations(&asLocations);
	qsort(masPopupUsages.GetData(), masPopupUsages.NumElements(), sizeof(SIDELocation), CompareIDEPopupUsages);

	if (masPopupUsages.NumElements() == 0)
	{
		szText.Init("No usages of ");
		szText.Append(szName);
		szText.Append(".");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
		szName.Kill();
		return true;
	}
	if (masPopupUsages.NumElements() == 1)
	{
		szName.Kill();
		mpcContent->GetDocuments()->OpenLocation(masPopupUsages.Get(0));
		mpcContent->GetDocuments()->SelectAtCaret((size)masPopupUsages.Get(0)->iLength);
		return true;
	}

	if (mpPopup.IsNull())
	{
		mpPopup = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
		mpPopup->GetItemClickEvent()->Add(this, &CIDEUsages::PopupUsageClicked);
	}
	mpPopup->RemoveAllItems();
	szText.Init("Usages of ");
	szText.Append(szName);
	if (uiOverrides > 0)
	{
		szText.Append(" and ");
		szText.Append((int)uiOverrides);
		szText.Append(uiOverrides == 1 ? " override" : " overrides");
	}
	AppendIDEUsageCount(&szText, masPopupUsages.NumElements(), " usage");
	mpPopup->AddItem(szText.Text())->SetEnabled(false);
	szText.Kill();
	szName.Kill();
	for (i = 0; i < masPopupUsages.NumElements(); i++)
	{
		psLocation = masPopupUsages.Get(i);
		szFile.Init();
		cFileUtil.GetFileName(&szFile, psLocation->szFileName.Text());
		szText.Init(szFile);
		szText.Append(':');
		szText.Append(psLocation->iLine + 1);
		szText.Append("  ");
		szLine.Init();
		mpcContent->GetDocuments()->GetFileLine(psLocation->szFileName.Text(), psLocation->iLine, &szLine);
		szLine.StripWhitespace();
		szText.Append(szLine);
		mpPopup->AddItem(szText.Text());
		szLine.Kill();
		szText.Kill();
		szFile.Kill();
	}

	//Under the caret, as IntelliJ's.
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pEditor = pTab->GetEditor();
	pEditor->GetActualBounds(&cBounds);
	pEditor->GetCaretBounds(&cCaret);
	sPosition.Init(cBounds.GetLeft() + cCaret.GetLeft(), cBounds.GetTop() + cCaret.GetBottom());
	pPopup = mpPopup->Open(sPosition, pEditor, true);

	//At most a third of the screen's height, and no taller than the editor's view: more usages scroll.
	iMaximum = -1;
	if (mpcContent->GetWindow()->GetScreenArea(&cScreen) && (cScreen.GetHeight() > 0))
	{
		iMaximum = cScreen.GetHeight() / 3;
	}
	iView = pTab->GetPane()->GetActualSize().y;
	if ((iView > 0) && ((iMaximum < 0) || (iView < iMaximum)))
	{
		iMaximum = iView;
	}
	if (iMaximum > 0)
	{
		pPopup->SetMaximumHeight(iMaximum, mpcContent->GetTheme());
	}
	mpPopup->Select(1);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A usage chosen in Control+B's popup is gone to, its name selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::PopupUsageClicked(CMenuItem* pcItem)
{
	size			i;
	SIDELocation*	psLocation;

	for (i = 1; i < mpPopup->NumItems(); i++)
	{
		if (&mpPopup->GetItem(i) == pcItem)
		{
			if (i - 1 < masPopupUsages.NumElements())
			{
				psLocation = masPopupUsages.Get(i - 1);
				mpcContent->GetDocuments()->OpenLocation(psLocation);
				mpcContent->GetDocuments()->SelectAtCaret((size)psLocation->iLength);
			}
			return;
		}
	}
}


Ptr<CMenuContainer> CIDEUsages::GetUsagesPopup(void) { return mpPopup; }


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
		psUsage->iType = GetIDEUsageType(psUsage->psLocation)->iRank;
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
			szText.Init(GetIDEUsageType(psUsage->psLocation)->szName);
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
// Next or Previous Occurrence (CIDEOccurrences): the usage line after (or
// before) the selected one is selected and its place opened in its
// editor, which has the keys unless bKeepKeys (the tree was stepped in).
// Past the last (or first) nothing moves.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Step(bool bNext, bool bKeepKeys)
{
	int32	iNode;
	int32	iStep;
	int64	iData;

	iStep = bNext ? 1 : -1;
	iNode = mpUsages->GetSelectedNode();
	if (iNode == TREE_NO_NODE)
	{
		iNode = bNext ? -1 : mpUsages->NumNodes();
	}
	for (iNode += iStep; (iNode >= 0) && (iNode < mpUsages->NumNodes()); iNode += iStep)
	{
		iData = mpUsages->GetNodeData(iNode);
		if ((iData > 0) && ((size)iData <= masUsages.NumElements()))
		{
			mpUsages->SelectNode(iNode);
			mpUsages->ShowRow(mpUsages->FindNodeRow(iNode));
			mpcContent->GetDocuments()->OpenLocation(masUsages.Get((size)iData - 1));
			mpcContent->GetDocuments()->SelectAtCaret((size)masUsages.Get((size)iData - 1)->iLength);
			if (bKeepKeys)
			{
				mpcContent->GetWindow()->GetFocus()->SetFocus(mpUsages);
			}
			return true;
		}
	}
	mpcContent->ShowStatus(bNext ? "No usages after this one." : "No usages before this one.");
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What Next and Previous Occurrence need of a tool window: the Find Usages
// pane, and whether it has a usage to go to.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CDockPane> CIDEUsages::GetSteppablePane(void) { return mpUsagesTool; }
const char* CIDEUsages::GetSteppableName(void) { return "Find Usages"; }
bool CIDEUsages::HasSteps(void) { return mbFound && (masUsages.NumElements() > 0) && mpUsages.IsNotNull(); }


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
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


void CIDEUsages::FindUsagesOnly(void) { FindUsages(false); }  //Find Usages without the declarations and definition, which Find All References has.
