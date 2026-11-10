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
#include "BaseLib/StringHelper.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLWriter.h"
#include "StandardLib/ClassDefines.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/ScrollPane.h"
#include "IDEContent.h"
#include "IDEUsages.h"


#define IDE_USAGES_OPTION_GAP			16		//Between the grouping check boxes.
#define IDE_USAGES_POPUP_MAXIMUM		100		//The most usages Control+B's popup shows; Find Usages shows the rest.
#define IDE_USAGES_OPEN_FILES_MAXIMUM	1000	//With more usages than this the tree's files start closed.
#define IDE_USAGES_REFRESH				500		//Milliseconds after the last edit that moved a usage until the tree shows it.

//Find Usages' part of the settings' file.
#define IDE_USAGES_SETTINGS_TAG			"FindUsages"
#define IDE_USAGES_BY_TYPE_ATTRIBUTE	"GroupByType"
#define IDE_USAGES_BY_PROJECT_ATTRIBUTE	"GroupByProject"
#define IDE_USAGES_BY_CLASS_ATTRIBUTE	"GroupByClass"


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


//The tree's groups above a file's lines, the outermost first: a row is in a group with those before it that are the
//same in it and in the groups outside it.
enum EIDEUsageLevel
{
	IUL_Type,
	IUL_Project,
	IUL_File,
};


//A usage as the tree sorts it: by its usage type's rank and its project, each only if the tree groups by it (0 and ""
//if not), then in the order it was found in, which is by file and line.
struct SIDEUsageRow
{
	SIDEUsage*	psUsage;
	size		uiIndex;	//Its usage's, in what was found.
	int			iType;
	const char*	szProject;
	bool		bShown;		//Its line is in the tree.
};


typedef CArrayTemplate<SIDEUsageRow>	CArrayIDEUsageRows;


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
// The order usages are kept in, and the popup shows them in: by their
// files' names (whatever their case), files of one name by their paths,
// then by line and column.
//
//////////////////////////////////////////////////////////////////////////
static int __cdecl CompareIDEUsages(const void* pv1, const void* pv2)
{
	SIDELocation*	ps1;
	SIDELocation*	ps2;
	int				iResult;

	ps1 = &((SIDEUsage*)pv1)->sLocation;
	ps2 = &((SIDEUsage*)pv2)->sLocation;
	iResult = StringInsensitiveCompare(GetIDEFileNamePart(ps1->szFileName.Text()), GetIDEFileNamePart(ps2->szFileName.Text()));
	if (iResult == 0)
	{
		iResult = StringInsensitiveCompare(ps1->szFileName.Text(), ps2->szFileName.Text());
	}
	if ((iResult == 0) && (ps1->iLine != ps2->iLine))
	{
		iResult = (ps1->iLine < ps2->iLine) ? -1 : 1;
	}
	if ((iResult == 0) && (ps1->iColumn != ps2->iColumn))
	{
		iResult = (ps1->iColumn < ps2->iColumn) ? -1 : 1;
	}
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The order of the tree's rows: by the groups above the files that are
// on, then as the usages were found.
//
//////////////////////////////////////////////////////////////////////////
static int __cdecl CompareIDEUsageRows(const void* pv1, const void* pv2)
{
	SIDEUsageRow*	ps1;
	SIDEUsageRow*	ps2;
	int				iResult;

	ps1 = (SIDEUsageRow*)pv1;
	ps2 = (SIDEUsageRow*)pv2;
	if (ps1->iType != ps2->iType)
	{
		return (ps1->iType < ps2->iType) ? -1 : 1;
	}
	iResult = StringInsensitiveCompare(ps1->szProject, ps2->szProject);
	if (iResult != 0)
	{
		return iResult;
	}
	return (ps1->uiIndex < ps2->uiIndex) ? -1 : ((ps1->uiIndex > ps2->uiIndex) ? 1 : 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether two rows are in the same group at eLevel: the same usage type,
// and for a project's group the same project, and for a file's the same
// file.  A group the tree doesn't have is the same for every row.
//
//////////////////////////////////////////////////////////////////////////
static bool IsSameIDEUsageGroup(SIDEUsageRow* ps1, SIDEUsageRow* ps2, EIDEUsageLevel eLevel)
{
	if (ps1->iType != ps2->iType)
	{
		return false;
	}
	if ((eLevel >= IUL_Project) && (StringInsensitiveCompare(ps1->szProject, ps2->szProject) != 0))
	{
		return false;
	}
	return (eLevel < IUL_File) || ps1->psUsage->sLocation.szFileName.EqualsIgnoreCase(ps2->psUsage->sLocation.szFileName);
}


//////////////////////////////////////////////////////////////////////////
//
// How many of pasRows, from its first, are in the first's group at eLevel.
//
//////////////////////////////////////////////////////////////////////////
static size CountIDEUsageGroup(SIDEUsageRow* pasRows, size uiRows, EIDEUsageLevel eLevel)
{
	size	uiCount;

	for (uiCount = 1; (uiCount < uiRows) && IsSameIDEUsageGroup(&pasRows[0], &pasRows[uiCount], eLevel); uiCount++)
	{
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether two rows' files are different files of one name under the same
// usage type and project, where only their folders tell them apart.
//
//////////////////////////////////////////////////////////////////////////
static bool IsSameIDEUsageFileName(SIDEUsageRow* ps1, SIDEUsageRow* ps2)
{
	return IsSameIDEUsageGroup(ps1, ps2, IUL_Project) && !IsSameIDEUsageGroup(ps1, ps2, IUL_File) &&
		(StringInsensitiveCompare(GetIDEFileNamePart(ps1->psUsage->sLocation.szFileName.Text()), GetIDEFileNamePart(ps2->psUsage->sLocation.szFileName.Text())) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// "  3 usages" after a group's name, or "  1 usage"; "  3" alone if
// szWhat is NULL.
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDEUsageCount(CChars* pszDest, size uiCount, const char* szWhat)
{
	pszDest->Append("  ");
	pszDest->Append((int)uiCount);
	if (szWhat)
	{
		pszDest->Append(szWhat);
		pszDest->Append(uiCount == 1 ? "" : "s");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFoundUsages::Init(void)
{
	szName.Init();
	uiOverrides = 0;
	bDeclarations = false;
	asUsages.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFoundUsages::Kill(void)
{
	size		i;
	SIDEUsage*	psUsage;

	for (i = 0; i < asUsages.NumElements(); i++)
	{
		psUsage = asUsages.Get(i);
		psUsage->szClass.Kill();
		psUsage->szProject.Kill();
		psUsage->sLocation.szFileName.Kill();
	}
	asUsages.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is found.
//
//////////////////////////////////////////////////////////////////////////
void SIDEFoundUsages::Clear(void)
{
	Kill();
	Init();
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of the open file szFile: the usages in it move with their text
// (AdjustIDEPlace).  True if any moved.
//
//////////////////////////////////////////////////////////////////////////
bool SIDEFoundUsages::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	size			i;
	SIDELocation*	psLocation;
	bool			bMoved;

	bMoved = false;
	for (i = 0; i < asUsages.NumElements(); i++)
	{
		psLocation = &asUsages.Get(i)->sLocation;
		if (psLocation->szFileName.Equals(szFile) && AdjustIDEPlace(&psLocation->iLine, &psLocation->iColumn, psChange))
		{
			bMoved = true;
		}
	}
	return bMoved;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, as the tree's top and the popup's first row say it:
// "Usages of IsParentheses and 1 override  3 usages", or "No usages of
// IsParentheses"; "References to" with the declarations.
//
//////////////////////////////////////////////////////////////////////////
void SIDEFoundUsages::AppendTitle(CChars* pszDest)
{
	if (bDeclarations)
	{
		pszDest->Append(asUsages.NumElements() == 0 ? "No references to " : "References to ");
	}
	else
	{
		pszDest->Append(asUsages.NumElements() == 0 ? "No usages of " : "Usages of ");
	}
	pszDest->Append(szName);
	if (uiOverrides > 0)
	{
		pszDest->Append(" and ");
		pszDest->Append((int)uiOverrides);
		pszDest->Append(uiOverrides == 1 ? " override" : " overrides");
	}
	if (asUsages.NumElements() > 0)
	{
		AppendIDEUsageCount(pszDest, asUsages.NumElements(), bDeclarations ? " reference" : " usage");
	}
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
	msFound.Init();
	mbFound = false;
	mbMoved = false;
	miMoved = 0;
	msPopupFound.Init();
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
	U_Unknown(SIDEFoundUsages, msFound);
	U_Bool(mbFound);
	U_Bool(mbMoved);
	U_Int64(miMoved);
	M_Pointer(mpPopup);
	U_Unknown(SIDEFoundUsages, msPopupFound);
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
	msPopupFound.Kill();
	msFound.Kill();
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
// Every use of the symbol at the caret (not its declarations and
// definition, unless bDeclarations), and of its overrides and what it
// overrides if it is a virtual member function, as IntelliJ's hierarchy
// has them, in psDest, each with the project and class the tree groups it
// by, in the order they are kept in (CompareIDEUsages).  False, with
// psDest as it was, if no symbol is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Find(bool bDeclarations, SIDEFoundUsages* psDest)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	SIDELocation*		psLocation;
	SIDEUsage*			psUsage;
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

	psDest->Kill();
	psDest->uiOverrides = mpcContent->GetCodeModel()->FindOverrideLocations(szKey.Text(), &asLocations);
	psDest->bDeclarations = bDeclarations;
	GetIDESymbolName(szKey.Text(), &psDest->szName);
	szKey.Kill();

	psDest->asUsages.Init();
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (bDeclarations || (psLocation->uiRole == CPPIR_Reference))
		{
			psUsage = psDest->asUsages.Add();
			MoveIDELocation(&psUsage->sLocation, psLocation);
			mpcContent->GetCodeModel()->GetUsageGroup(&psUsage->sLocation, &psUsage->szProject, &psUsage->szClass);
		}
	}
	KillIDELocations(&asLocations);
	qsort(psDest->asUsages.GetData(), psDest->asUsages.NumElements(), sizeof(SIDEUsage), CompareIDEUsages);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+F7, IntelliJ's Find Usages: what Find finds of the symbol at the
// caret, in the Find Usages tree (ShowUsages), which is given the keys.
// With bDeclarations (Shift+F12, Visual Studio's Find All References) its
// declarations and definition too.  False if there is no symbol at the
// caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::FindUsages(bool bDeclarations)
{
	if (!Find(bDeclarations, &msFound))
	{
		mpcContent->ShowStatus(bDeclarations ? "No references found at the caret." : "No usages found at the caret.");
		return false;
	}
	mbFound = true;
	mbMoved = false;

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
// Control+B on a declaration or the definition, as IntelliJ's: a small
// popup under the caret of the symbol's usages (and its overrides', as
// Find Usages has), the first selected, "Calculator.cpp:367  the line";
// Enter or a click goes to one.  It shows IDE_USAGES_POPUP_MAXIMUM of
// them: with more, its last row says how many more there are, and opens
// them all in Find Usages.  With only one usage it is gone to; with none
// the status line says so.  False if no symbol is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::ShowUsagesPopup(void)
{
	CIDEFileLines	cLines;
	CChars			szText;
	SIDEUsage*		psUsage;
	size			uiUsages;
	size			i;

	if (!Find(false, &msPopupFound))
	{
		return false;
	}
	if (msPopupFound.asUsages.NumElements() == 0)
	{
		szText.Init();
		msPopupFound.AppendTitle(&szText);
		szText.Append(".");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
		return true;
	}
	if (msPopupFound.asUsages.NumElements() == 1)
	{
		mpcContent->GetDocuments()->OpenLocation(&msPopupFound.asUsages.Get(0)->sLocation, true);
		return true;
	}

	if (mpPopup.IsNull())
	{
		mpPopup = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
		mpPopup->GetItemClickEvent()->Add(this, &CIDEUsages::PopupUsageClicked);
	}
	mpPopup->RemoveAllItems();
	szText.Init();
	msPopupFound.AppendTitle(&szText);
	mpPopup->AddItem(szText.Text())->SetEnabled(false);
	szText.Kill();
	uiUsages = msPopupFound.asUsages.NumElements();
	cLines.Init(&mpcContent->GetDocuments());
	for (i = 0; (i < uiUsages) && (i < IDE_USAGES_POPUP_MAXIMUM); i++)
	{
		psUsage = msPopupFound.asUsages.Get(i);
		szText.Init(GetIDEFileNamePart(psUsage->sLocation.szFileName.Text()));
		szText.Append(':');
		szText.Append(psUsage->sLocation.iLine + 1);
		szText.Append("  ");
		cLines.AppendLine(psUsage->sLocation.szFileName.Text(), psUsage->sLocation.iLine, &szText);
		mpPopup->AddItem(szText.Text());
		szText.Kill();
	}
	cLines.Kill();
	if (uiUsages > IDE_USAGES_POPUP_MAXIMUM)
	{
		szText.Init("... and ");
		szText.Append((int)(uiUsages - IDE_USAGES_POPUP_MAXIMUM));
		szText.Append(" more: show them all in Find Usages");
		mpPopup->AddItem(szText.Text());
		szText.Kill();
	}
	OpenPopup();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The popup opened under the active editor's caret, as IntelliJ's, its
// first usage selected.  It is at most a third of the screen's height and
// no taller than the editor's view: more usages scroll.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::OpenPopup(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	Ptr<CPopup>			pPopup;
	CInt32Rect			cBounds;
	CInt32Rect			cCaret;
	CInt32Rect			cScreen;
	SInt32Vec2			sPosition;
	int32				iMaximum;
	int32				iView;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pEditor = pTab->GetEditor();
	pEditor->GetActualBounds(&cBounds);
	pEditor->GetCaretBounds(&cCaret);
	sPosition.Init(cBounds.GetLeft() + cCaret.GetLeft(), cBounds.GetTop() + cCaret.GetBottom());
	pPopup = mpPopup->Open(sPosition, pEditor, true);

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
}


//////////////////////////////////////////////////////////////////////////
//
// A usage chosen in Control+B's popup is gone to, its name selected.  The
// row after the last usage it shows opens them all in Find Usages: the
// caret is still on the declaration they were found from.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::PopupUsageClicked(CMenuItem* pcItem)
{
	size	i;

	for (i = 1; i < mpPopup->NumItems(); i++)
	{
		if (&mpPopup->GetItem(i) == pcItem)
		{
			if (i - 1 == IDE_USAGES_POPUP_MAXIMUM)
			{
				FindUsages(false);
			}
			else if (i - 1 < msPopupFound.asUsages.NumElements())
			{
				mpcContent->GetDocuments()->OpenLocation(&msPopupFound.asUsages.Get(i - 1)->sLocation, true);
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
// file has them.  The nodes are added in the order the tree shows them,
// so a step to the next node is a step down the tree.  The first line is
// selected, for F4 to go to, or with bKeepSelected the usage that was, if
// one was: the same usages grouped again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ShowUsages(bool bKeepSelected)
{
	CArrayIDEUsageRows	asRows;
	SIDEUsageRow*		psRow;
	CIDEFileLines		cLines;
	CChars				szText;
	const char*			szWhat;
	size				uiIndexed;
	size				uiToIndex;
	size				uiRows;
	size				uiFile;
	size				i;
	int64				iSelectedData;
	int32				iRoot;
	int32				iType;
	int32				iProject;
	int32				iFile;
	int32				iNode;
	bool				bByType;
	bool				bByProject;
	bool				bOpenFiles;

	if (mpUsages.IsNull())
	{
		return;
	}
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

	bByType = (muiGrouping & IDE_USAGES_BY_TYPE) != 0;
	bByProject = (muiGrouping & IDE_USAGES_BY_PROJECT) != 0;
	uiRows = msFound.asUsages.NumElements();
	asRows.Init();
	for (i = 0; i < uiRows; i++)
	{
		psRow = asRows.Add();
		psRow->psUsage = msFound.asUsages.Get(i);
		psRow->uiIndex = i;
		psRow->iType = bByType ? GetIDEUsageType(&psRow->psUsage->sLocation)->iRank : 0;
		psRow->szProject = bByProject ? psRow->psUsage->szProject.Text() : "";
		psRow->bShown = false;
	}
	qsort(asRows.GetData(), uiRows, sizeof(SIDEUsageRow), CompareIDEUsageRows);

	//Until every file is indexed some may be missing.
	szText.Init();
	msFound.AppendTitle(&szText);
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
	mpUsages->SetExpanded(iRoot, true);
	szText.Kill();

	//The tree has a row for each line that can be seen, which thousands of make it slow: past
	//IDE_USAGES_OPEN_FILES_MAXIMUM usages the files start closed, but for the selected line's.
	bOpenFiles = (uiRows <= IDE_USAGES_OPEN_FILES_MAXIMUM);
	szWhat = msFound.bDeclarations ? " reference" : " usage";
	iType = iRoot;
	iProject = iRoot;
	cLines.Init(&mpcContent->GetDocuments());
	for (i = 0; i < uiRows; i += uiFile)
	{
		psRow = asRows.Get(i);
		if (bByType && ((i == 0) || !IsSameIDEUsageGroup(psRow, asRows.Get(i - 1), IUL_Type)))
		{
			iType = AddGroupNode(iRoot, GetIDEUsageType(&psRow->psUsage->sLocation)->szName, CountIDEUsageGroup(psRow, uiRows - i, IUL_Type), szWhat);
			iProject = iType;
		}
		if (bByProject && ((i == 0) || !IsSameIDEUsageGroup(psRow, asRows.Get(i - 1), IUL_Project)))
		{
			iProject = AddGroupNode(iType, psRow->szProject, CountIDEUsageGroup(psRow, uiRows - i, IUL_Project), szWhat);
		}
		uiFile = CountIDEUsageGroup(psRow, uiRows - i, IUL_File);
		iFile = AddFileNode(iProject, asRows.GetData(), uiRows, i, uiFile);
		mpUsages->SetExpanded(iFile, bOpenFiles);
		AddFileUsages(iFile, psRow, uiFile, &cLines);
	}
	cLines.Kill();
	asRows.Kill();

	iNode = (iSelectedData > 0) ? FindUsageNode(iSelectedData) : TREE_NO_NODE;
	mpUsages->SelectNode((iNode != TREE_NO_NODE) ? iNode : FindUsageNode(0));
}


//////////////////////////////////////////////////////////////////////////
//
// A group's node under iParent, open: "Call  3 usages", or
// "CCalculator  3" if szWhat is NULL.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEUsages::AddGroupNode(int32 iParent, const char* szName, size uiCount, const char* szWhat)
{
	CChars	szText;
	int32	iNode;

	szText.Init(szName);
	AppendIDEUsageCount(&szText, uiCount, szWhat);
	iNode = mpUsages->AddNode(iParent, szText.Text(), NULL, 0);
	mpUsages->SetExpanded(iNode, true);
	szText.Kill();
	return iNode;
}


//////////////////////////////////////////////////////////////////////////
//
// The node of the file whose rows are the uiCount from uiFirst of
// pasRows: "Calculator.cpp  3", with its folder after it if the file
// before or after it in the group is another of that name.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEUsages::AddFileNode(int32 iParent, SIDEUsageRow* pasRows, size uiRows, size uiFirst, size uiCount)
{
	CChars		szText;
	const char*	szFile;
	const char*	szName;
	int32		iNode;
	bool		bShared;

	szFile = pasRows[uiFirst].psUsage->sLocation.szFileName.Text();
	szName = GetIDEFileNamePart(szFile);
	szText.Init(szName);
	AppendIDEUsageCount(&szText, uiCount, NULL);
	bShared = (uiFirst > 0) && IsSameIDEUsageFileName(&pasRows[uiFirst], &pasRows[uiFirst - 1]);
	bShared = bShared || ((uiFirst + uiCount < uiRows) && IsSameIDEUsageFileName(&pasRows[uiFirst], &pasRows[uiFirst + uiCount]));
	if (bShared && (szName > szFile + 1))
	{
		szText.Append("  ");
		szText.AppendSubString(szFile, szName - 1);
	}
	iNode = mpUsages->AddNode(iParent, szText.Text(), NULL, 0);
	szText.Kill();
	return iNode;
}


//////////////////////////////////////////////////////////////////////////
//
// A file's lines under its node, from its first.  If classes are grouped
// a class's node goes where the file first has a line in it, with all its
// lines; lines in no class are under the file.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::AddFileUsages(int32 iFile, SIDEUsageRow* pasRows, size uiCount, CIDEFileLines* pcLines)
{
	CChars*	pszClass;
	size	uiLines;
	size	i;
	size	j;
	int32	iClass;
	bool	bByClass;

	bByClass = (muiGrouping & IDE_USAGES_BY_CLASS) != 0;
	for (i = 0; i < uiCount; i++)
	{
		pszClass = &pasRows[i].psUsage->szClass;
		if (pasRows[i].bShown)
		{
			continue;
		}
		if (!bByClass || pszClass->Empty())
		{
			AddUsageLine(iFile, &pasRows[i], pcLines);
			continue;
		}

		uiLines = 0;
		for (j = i; j < uiCount; j++)
		{
			uiLines += pasRows[j].psUsage->szClass.Equals(*pszClass) ? 1 : 0;
		}
		iClass = AddGroupNode(iFile, pszClass->Text(), uiLines, NULL);
		for (j = i; j < uiCount; j++)
		{
			if (pasRows[j].psUsage->szClass.Equals(*pszClass))
			{
				AddUsageLine(iClass, &pasRows[j], pcLines);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A usage's line under iParent, "36: the line", its usage's index (plus
// one) its data.  Without the usage type groups a declaration or the
// definition says what it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::AddUsageLine(int32 iParent, SIDEUsageRow* psRow, CIDEFileLines* pcLines)
{
	CChars			szText;
	SIDELocation*	psLocation;

	psLocation = &psRow->psUsage->sLocation;
	szText.Init();
	szText.Append(psLocation->iLine + 1);
	szText.Append(": ");
	pcLines->AppendLine(psLocation->szFileName.Text(), psLocation->iLine, &szText);
	if ((muiGrouping & IDE_USAGES_BY_TYPE) == 0)
	{
		if (psLocation->uiRole == CPPIR_Definition)
		{
			szText.Append("  [definition]");
		}
		else if (psLocation->uiRole == CPPIR_Declaration)
		{
			szText.Append("  [declaration]");
		}
	}
	if (psLocation->uiFlags & CPPIF_AMBIGUOUS)
	{
		szText.Append("  [one of the overloads]");
	}
	mpUsages->AddNode(iParent, szText.Text(), NULL, (int64)psRow->uiIndex + 1);
	szText.Kill();
	psRow->bShown = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The node of the line whose data is iData, or with 0 the first line's.
// TREE_NO_NODE if there is none.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEUsages::FindUsageNode(int64 iData)
{
	int32	iNode;
	int64	iNodeData;

	for (iNode = 0; iNode < mpUsages->NumNodes(); iNode++)
	{
		iNodeData = mpUsages->GetNodeData(iNode);
		if ((iNodeData > 0) && ((iData == 0) || (iNodeData == iData)))
		{
			return iNode;
		}
	}
	return TREE_NO_NODE;
}


//////////////////////////////////////////////////////////////////////////
//
// The usage a node is the line of, NULL if it is a group's node or none.
//
//////////////////////////////////////////////////////////////////////////
SIDEUsage* CIDEUsages::GetNodeUsage(int32 iNode)
{
	int64	iData;

	if ((iNode < 0) || (iNode >= mpUsages->NumNodes()))
	{
		return NULL;
	}
	iData = mpUsages->GetNodeData(iNode);
	if ((iData <= 0) || ((size)iData > msFound.asUsages.NumElements()))
	{
		return NULL;
	}
	return msFound.asUsages.Get((size)iData - 1);
}


//////////////////////////////////////////////////////////////////////////
//
// A line's node, double clicked or jumped to (F4), opens its place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::UsageActivated(void)
{
	SIDEUsage*	psUsage;

	psUsage = GetNodeUsage(mpUsages->GetSelectedNode());
	if (psUsage)
	{
		mpcContent->GetDocuments()->OpenLocation(&psUsage->sLocation);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Next or Previous Occurrence (CIDEOccurrences): the usage line below (or
// above) the selected one in the tree is selected and its place opened in
// its editor, which has the keys unless bKeepKeys (the tree was stepped
// in).  Past the last (or first) nothing moves.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Step(bool bNext, bool bKeepKeys)
{
	SIDEUsage*	psUsage;
	int32		iNode;
	int32		iStep;

	iStep = bNext ? 1 : -1;
	iNode = mpUsages->GetSelectedNode();
	if (iNode == TREE_NO_NODE)
	{
		iNode = bNext ? -1 : mpUsages->NumNodes();
	}
	for (iNode += iStep; (iNode >= 0) && (iNode < mpUsages->NumNodes()); iNode += iStep)
	{
		psUsage = GetNodeUsage(iNode);
		if (psUsage)
		{
			mpUsages->SelectNode(iNode);
			mpUsages->ShowRow(mpUsages->FindNodeRow(iNode));
			mpcContent->GetDocuments()->OpenLocation(&psUsage->sLocation, true);
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
bool CIDEUsages::HasSteps(void) { return mbFound && (msFound.asUsages.NumElements() > 0) && mpUsages.IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// Escape in the tree: the keys go back to the active editor, as
// IntelliJ's tool windows give them back.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ReturnToEditor(void)
{
	mpcContent->GetDocuments()->FocusActiveEditor();
}


//////////////////////////////////////////////////////////////////////////
//
// A grouping check box was ticked or cleared: the groups are as the
// boxes say.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::GroupingChanged(void)
{
	uint8	uiGrouping;

	uiGrouping = 0;
	uiGrouping |= mpByType->IsTicked() ? IDE_USAGES_BY_TYPE : 0;
	uiGrouping |= mpByProject->IsTicked() ? IDE_USAGES_BY_PROJECT : 0;
	uiGrouping |= mpByClass->IsTicked() ? IDE_USAGES_BY_CLASS : 0;
	SetGrouping(uiGrouping);
}


//////////////////////////////////////////////////////////////////////////
//
// The groups (IDE_USAGES_BY_*) changed, as the check boxes change them:
// they are shown (ShowGrouping) and kept for the next time the IDE is run
// (CIDESettings::WriteSettings).
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::SetGrouping(uint8 uiGrouping)
{
	uiGrouping &= IDE_USAGES_BY_ALL;
	if (uiGrouping != muiGrouping)
	{
		ShowGrouping(uiGrouping);
		mpcContent->GetSettings()->WriteSettings();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The groups are uiGrouping: the boxes are ticked to match and what was
// found is shown again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ShowGrouping(uint8 uiGrouping)
{
	muiGrouping = uiGrouping;
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
// Find Usages' part of how the IDE was left, in the settings' file (see
// CIDESettings): its groups, <FindUsages GroupByType="true" .../>.  One
// the file doesn't say stays as it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ReadSettings(CMarkupTag* pcSettings)
{
	static const char*	aszAttributes[] = { IDE_USAGES_BY_TYPE_ATTRIBUTE, IDE_USAGES_BY_PROJECT_ATTRIBUTE, IDE_USAGES_BY_CLASS_ATTRIBUTE };
	static const uint8	auiGroups[] = { IDE_USAGES_BY_TYPE, IDE_USAGES_BY_PROJECT, IDE_USAGES_BY_CLASS };
	CMarkupTag*			pcTag;
	char*				szValue;
	uint8				uiGrouping;
	size				i;

	pcTag = pcSettings->GetTag(IDE_USAGES_SETTINGS_TAG);
	if (pcTag == NULL)
	{
		return;
	}
	uiGrouping = muiGrouping;
	for (i = 0; i < sizeof(auiGroups); i++)
	{
		szValue = pcTag->GetStringAttribute(aszAttributes[i]);
		if (szValue)
		{
			uiGrouping = (StringCompare(szValue, "true") == 0) ? (uiGrouping | auiGroups[i]) : (uiGrouping & ~auiGroups[i]);
		}
	}
	ShowGrouping(uiGrouping);
}


void CIDEUsages::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin(IDE_USAGES_SETTINGS_TAG);
	pcWriter->Attribute(IDE_USAGES_BY_TYPE_ATTRIBUTE, (muiGrouping & IDE_USAGES_BY_TYPE) != 0);
	pcWriter->Attribute(IDE_USAGES_BY_PROJECT_ATTRIBUTE, (muiGrouping & IDE_USAGES_BY_PROJECT) != 0);
	pcWriter->Attribute(IDE_USAGES_BY_CLASS_ATTRIBUTE, (muiGrouping & IDE_USAGES_BY_CLASS) != 0);
	pcWriter->End();
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
// An edit of the open file szFile (CIDEDocuments::DocumentChanged): the
// usages in it, the tree's and the popup's, move with their text, so one
// gone to after lines were put in above it is still where its name is.
// The tree's lines say where they were until Poll shows them again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	msPopupFound.AdjustPlaces(szFile, psChange);
	if (msFound.AdjustPlaces(szFile, psChange))
	{
		mbMoved = true;
		miMoved = GetDebugTickCount();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: once the edits that moved the tree's usages have
// stopped for IDE_USAGES_REFRESH, the tree is shown again with the lines
// as they are now, the selected usage still selected and in view.
// Milliseconds until it should be looked at again, -1 if not until
// something changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEUsages::Poll(int64 iNow)
{
	if (!mbMoved)
	{
		return -1;
	}
	if (iNow - miMoved < IDE_USAGES_REFRESH)
	{
		return IDE_USAGES_REFRESH - (iNow - miMoved);
	}
	mbMoved = false;
	ShowUsages(true);
	if (mpUsages.IsNotNull() && (mpUsages->GetSelected() >= 0))
	{
		mpUsages->ShowRow(mpUsages->GetSelected());
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so have the places found in them, from the
// tree and the popup, which is closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Clear(void)
{
	msFound.Clear();
	mbFound = false;
	mbMoved = false;
	msPopupFound.Clear();
	if (mpPopup.IsNotNull() && mpPopup->IsOpen())
	{
		mpPopup->Close();
	}
	if (mpUsages.IsNotNull())
	{
		mpUsages->RemoveAllNodes();
	}
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
