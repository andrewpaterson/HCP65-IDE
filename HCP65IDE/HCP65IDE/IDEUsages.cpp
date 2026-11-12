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


#define IDE_USAGES_POPUP_MAXIMUM		100		//The most usages Control+B's popup shows; Find Usages shows the rest.
#define IDE_USAGES_OPEN_FILES_MAXIMUM	1000	//With more usages than this the tree's files start closed.
#define IDE_USAGES_REFRESH				500		//Milliseconds after the last edit that moved a usage until the tree shows it.

//Find Usages' part of the settings' file.
#define IDE_USAGES_SETTINGS_TAG			"FindUsages"
#define IDE_USAGES_PINNED_PREFIX			"* "	//Before a pinned tab's name.
#define IDE_USAGES_BY_TYPE_ATTRIBUTE	"GroupByType"
#define IDE_USAGES_BY_PROJECT_ATTRIBUTE	"GroupByProject"
#define IDE_USAGES_BY_CLASS_ATTRIBUTE	"GroupByClass"
#define IDE_USAGES_READ_ATTRIBUTE		"ShowReadAccess"
#define IDE_USAGES_WRITE_ATTRIBUTE		"ShowWriteAccess"


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
	szKey.Init();
	bDeclared = false;
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
	if (bDeclared)
	{
		sDeclaration.szFileName.Kill();
	}
	szKey.Kill();
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
	if (bDeclared && sDeclaration.szFileName.Equals(szFile))
	{
		AdjustIDEPlace(&sDeclaration.iLine, &sDeclaration.iColumn, psChange);
	}
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
	maSearches.Init();
	msPopupFound.Init();
	muiGrouping = IDE_USAGES_BY_ALL;
	mbShowRead = true;
	mbShowWrite = true;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpTabs);
	M_Embedded(maSearches);
	M_Pointer(mpSearch);
	M_Pointer(mpUsagesTool);
	M_Pointer(mpPopup);
	U_Unknown(SIDEFoundUsages, msPopupFound);
	U_UInt8(muiGrouping);
	U_Bool(mbShowRead);
	U_Bool(mbShowWrite);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::Free(void)
{
	if (mpTabs.IsNotNull())
	{
		mpTabs->GetSelectionEvent()->Remove(this);
		mpTabs->GetCloseRequestEvent()->Remove(this);
	}
	if (mpPopup.IsNotNull())
	{
		mpPopup->GetItemClickEvent()->Remove(this);
	}
	msPopupFound.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Save(CObjectWriter* pcFile) { return false; }
bool CIDEUsages::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Find Usages tool window, beside Build, as IntelliJ's Find window: a
// results pane, with the groupings and the access filters on its
// toolbar.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::CreatePane(SDockPlace* psPlace)
{
	mpTabs = OMalloc<CTabbedContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpTabs->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpTabs->SetHideSingleTab(true);
	mpTabs->GetSelectionEvent()->Add(this, &CIDEUsages::TabSelected);
	mpTabs->GetCloseRequestEvent()->Add(this, &CIDEUsages::TabCloseRequested);
	AddSearch();
	mpUsagesTool = mpcContent->GetDock()->AddPane(IDE_USAGES_ID, "Find Usages", NULL, mpTabs, DPK_Tool, psPlace);
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
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole))
	{
		return false;
	}
	return FindAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, bDeclarations, psDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The usages of the symbol named at a place in a file (a line and column
// from 0), as Find's are.  False if no symbol is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::FindAt(const char* szFileName, int iLine, int iColumn, bool bDeclarations, SIDEFoundUsages* psDest)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	SIDELocation*		psLocation;
	SIDELocation*		psDeclared;
	SIDEUsage*			psUsage;
	size				i;

	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	asLocations.Init();
	szKey.Init();
	if (!mpcContent->GetCodeModel()->FindReferences(szFileName, iLine, iColumn, &asLocations, &szKey))
	{
		KillIDELocations(&asLocations);
		szKey.Kill();
		return false;
	}

	psDest->Kill();
	psDest->Init();
	psDest->szKey.Set(szKey);

	//Where it is declared: its definition, else its first declaration, before the overrides' are with its own.
	psDeclared = NULL;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Definition) || ((psLocation->uiRole == CPPIR_Declaration) && (psDeclared == NULL)))
		{
			psDeclared = psLocation;
			if (psLocation->uiRole == CPPIR_Definition)
			{
				break;
			}
		}
	}
	if (psDeclared)
	{
		memcpy(&psDest->sDeclaration, psDeclared, sizeof(SIDELocation));
		psDest->sDeclaration.szFileName.Init(psDeclared->szFileName);
		psDest->bDeclared = true;
	}

	psDest->uiOverrides = mpcContent->GetCodeModel()->FindOverrideLocations(szKey.Text(), &asLocations);
	psDest->bDeclarations = bDeclarations;
	psDest->szName.Kill();
	GetIDESymbolName(szKey.Text(), &psDest->szName);
	szKey.Kill();

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
	SIDEFoundUsages	sFound;

	sFound.Init();
	if (!Find(bDeclarations, &sFound))
	{
		sFound.Kill();
		mpcContent->ShowStatus(bDeclarations ? "No references found at the caret." : "No usages found at the caret.");
		return false;
	}

	//In the tab shown, unless it is pinned: then in a new one.
	if (mpSearch->mbPinned)
	{
		AddSearch();
	}
	mpSearch->SetFound(&sFound);
	mpSearch->mpTab->SetText(mpSearch->msFound.szName.Text());

	ShowUsages(false);
	mpcContent->GetDock()->Show(mpUsagesTool);
	mpSearch->mpPane->Focus();
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
// no taller than the editor's view: more usages scroll.  A press anywhere
// outside it closes it, in the editor too.
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

	//Its owner is the editor, most of the window: a press there closes it, as one anywhere else outside it does.
	pPopup->SetOwnerPressKeepsOpen(false);

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
	bool				bByType;
	bool				bByProject;
	bool				bOpenFiles;

	if (mpSearch.IsNull())
	{
		return;
	}
	mpSearch->mbMoved = false;
	mpSearch->mbStale = false;
	iSelectedData = mpSearch->mpPane->BeginShow(bKeepSelected);
	if (!mpSearch->mbFound)
	{
		return;
	}

	bByType = (muiGrouping & IDE_USAGES_BY_TYPE) != 0;
	bByProject = (muiGrouping & IDE_USAGES_BY_PROJECT) != 0;
	asRows.Init();
	for (i = 0; i < mpSearch->msFound.asUsages.NumElements(); i++)
	{
		if (!IsUsageShown(mpSearch->msFound.asUsages.Get(i)))
		{
			continue;
		}
		psRow = asRows.Add();
		psRow->psUsage = mpSearch->msFound.asUsages.Get(i);
		psRow->uiIndex = i;
		psRow->iType = bByType ? GetIDEUsageType(&psRow->psUsage->sLocation)->iRank : 0;
		psRow->szProject = bByProject ? psRow->psUsage->szProject.Text() : "";
		psRow->bShown = false;
	}
	uiRows = asRows.NumElements();
	qsort(asRows.GetData(), uiRows, sizeof(SIDEUsageRow), CompareIDEUsageRows);

	//Until every file is indexed some may be missing.
	szText.Init();
	mpSearch->msFound.AppendTitle(&szText);
	mpcContent->GetCodeModel()->GetIndexingProgress(&uiIndexed, &uiToIndex);
	if (uiIndexed < uiToIndex)
	{
		szText.Append("  (indexing, ");
		szText.Append((uint32)uiIndexed);
		szText.Append(" of ");
		szText.Append((uint32)uiToIndex);
		szText.Append(" files)");
	}
	if (uiRows < mpSearch->msFound.asUsages.NumElements())
	{
		szText.Append("  (");
		szText.Append((uint32)(mpSearch->msFound.asUsages.NumElements() - uiRows));
		szText.Append(" hidden)");
	}
	iRoot = mpSearch->mpPane->GetTree()->AddNode(TREE_NO_NODE, szText.Text(), NULL, 0);
	mpSearch->mpPane->GetTree()->SetExpanded(iRoot, true);
	szText.Kill();

	//The tree has a row for each line that can be seen, which thousands of make it slow: past
	//IDE_USAGES_OPEN_FILES_MAXIMUM usages the files start closed, but for the selected line's.
	bOpenFiles = (uiRows <= IDE_USAGES_OPEN_FILES_MAXIMUM);
	szWhat = mpSearch->msFound.bDeclarations ? " reference" : " usage";
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
		mpSearch->mpPane->GetTree()->SetExpanded(iFile, bOpenFiles);
		AddFileUsages(iFile, psRow, uiFile, &cLines);
	}
	cLines.Kill();
	asRows.Kill();

	mpSearch->mpPane->EndShow(iSelectedData);
}


//////////////////////////////////////////////////////////////////////////
//
// A group's node under iParent, open: "Call  3 usages", or
// "CCalculator  3" if szWhat is NULL.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEUsages::AddGroupNode(int32 iParent, const char* szName, size uiCount, const char* szWhat)
{
	CChars	szDetail;
	int32	iNode;

	szDetail.Init();
	AppendIDEUsageCount(&szDetail, uiCount, szWhat);
	iNode = mpSearch->mpPane->AddGroup(iParent, szName, szDetail.Text());
	mpSearch->mpPane->GetTree()->SetExpanded(iNode, true);
	szDetail.Kill();
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
	CChars		szDetail;
	const char*	szFile;
	const char*	szName;
	int32		iNode;
	bool		bShared;

	szFile = pasRows[uiFirst].psUsage->sLocation.szFileName.Text();
	szName = GetIDEFileNamePart(szFile);
	szDetail.Init();
	AppendIDEUsageCount(&szDetail, uiCount, NULL);
	bShared = (uiFirst > 0) && IsSameIDEUsageFileName(&pasRows[uiFirst], &pasRows[uiFirst - 1]);
	bShared = bShared || ((uiFirst + uiCount < uiRows) && IsSameIDEUsageFileName(&pasRows[uiFirst], &pasRows[uiFirst + uiCount]));
	if (bShared && (szName > szFile + 1))
	{
		szDetail.Append("  ");
		szDetail.AppendSubString(szFile, szName - 1);
	}
	iNode = mpSearch->mpPane->AddGroup(iParent, szName, szDetail.Text(), szFile);
	szDetail.Kill();
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
	CChars			szNote;
	SIDELocation*	psLocation;

	psLocation = &psRow->psUsage->sLocation;
	szNote.Init();
	if ((muiGrouping & IDE_USAGES_BY_TYPE) == 0)
	{
		if (psLocation->uiRole == CPPIR_Definition)
		{
			szNote.Append("  [definition]");
		}
		else if (psLocation->uiRole == CPPIR_Declaration)
		{
			szNote.Append("  [declaration]");
		}
	}
	if (psLocation->uiFlags & CPPIF_AMBIGUOUS)
	{
		szNote.Append("  [one of the overloads]");
	}
	mpSearch->mpPane->AddResult(iParent, pcLines, psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, psLocation->iLength, szNote.Text(), (int64)psRow->uiIndex + 1);
	szNote.Kill();
	psRow->bShown = true;
}


//////////////////////////////////////////////////////////////////////////
//
// Next or Previous Occurrence (CIDEOccurrences) steps through the tree's
// usages (CIDEResultsPane::Step).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::Step(bool bNext, bool bKeepKeys)
{
	return mpSearch->mpPane->Step(bNext, bKeepKeys);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsagesSearch::Init(Ptr<CIDEResultsPane> pPane)
{
	PreInit();
	mpPane = pPane;
	msFound.Init();
	mbFound = false;
	mbMoved = false;
	miMoved = 0;
	mbStale = false;
	mbPinned = false;
	PostInit();
}


void CIDEUsagesSearch::Class(void)
{
	M_Pointer(mpPane);
	M_Pointer(mpTab);
	U_Unknown(SIDEFoundUsages, msFound);
	U_Bool(mbFound);
	U_Bool(mbMoved);
	U_Int64(miMoved);
	U_Bool(mbStale);
	U_Bool(mbPinned);
}


void CIDEUsagesSearch::Free(void)
{
	msFound.Kill();
}


bool CIDEUsagesSearch::Save(CObjectWriter* pcFile) { return false; }
bool CIDEUsagesSearch::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// What was found is the search's, in place of what it had: psFound is
// taken, not copied.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsagesSearch::SetFound(SIDEFoundUsages* psFound)
{
	msFound.Kill();
	memcpy(&msFound, psFound, sizeof(SIDEFoundUsages));
	mbFound = true;
	mbMoved = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A new, empty search in a tab of its own, which is chosen: a results
// pane with the groupings, the filters and Pin Tab on its toolbar, and
// the settings of the pane before it.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEUsagesSearch> CIDEUsages::AddSearch(void)
{
	Ptr<CIDEUsagesSearch>	pSearch;
	Ptr<CIDEResultsPane>	pPane;
	Ptr<CIDEResultsPane>	pLast;

	pLast = mpSearch.IsNotNull() ? mpSearch->mpPane : Ptr<CIDEResultsPane>();
	pPane = OMalloc<CIDEResultsPane>(mpcContent, this, "usages", false);
	pSearch = OMalloc<CIDEUsagesSearch>(pPane);
	maSearches.Add(pSearch);
	mpSearch = pSearch;

	pPane->AddToolbarSeparator();
	AddPaneCommand(pPane, "Usages.GroupByType", "Group by Usage Type", ITI_GroupByType, &CIDEUsages::ToggleByType, &CIDEUsages::IsByType);
	AddPaneCommand(pPane, "Usages.GroupByProject", "Group by Project", ITI_GroupByProject, &CIDEUsages::ToggleByProject, &CIDEUsages::IsByProject);
	AddPaneCommand(pPane, "Usages.GroupByClass", "Group by Class", ITI_GroupByClass, &CIDEUsages::ToggleByClass, &CIDEUsages::IsByClass);
	pPane->AddToolbarSeparator();
	AddPaneCommand(pPane, "Usages.ShowReadAccess", "Show Read Access", ITI_ReadAccess, &CIDEUsages::ToggleShowRead, &CIDEUsages::IsShowRead);
	AddPaneCommand(pPane, "Usages.ShowWriteAccess", "Show Write Access", ITI_WriteAccess, &CIDEUsages::ToggleShowWrite, &CIDEUsages::IsShowWrite);
	pPane->AddViewCommands();
	AddPaneCommand(pPane, "Usages.PinTab", "Pin Tab", ITI_Pin, &CIDEUsages::TogglePinned, &CIDEUsages::IsPinned);
	if (pLast.IsNotNull())
	{
		pPane->CopySettings(pLast);
	}

	//The tab is chosen without TabSelected being told: the search is the one shown already.
	mpTabs->GetSelectionEvent()->Remove(this);
	pSearch->mpTab = mpTabs->AddTab("Find Usages", pPane->GetComponent(), Ptr<CImageCel>(), true);
	mpTabs->Select(pSearch->mpTab);
	mpTabs->GetSelectionEvent()->Add(this, &CIDEUsages::TabSelected);
	return pSearch;
}


//////////////////////////////////////////////////////////////////////////
//
// A search and its tab go.  With bKeepOne, as closing a tab does, the
// last to go is replaced by an empty one, so the tool window always has
// a tab; the tab left of it (or the first) is chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::RemoveSearch(Ptr<CIDEUsagesSearch> pSearch, bool bKeepOne)
{
	size	uiIndex;

	for (uiIndex = 0; (uiIndex < maSearches.NumElements()) && (maSearches.Get(uiIndex) != pSearch); uiIndex++)
	{
	}
	if (uiIndex == maSearches.NumElements())
	{
		return;
	}
	pSearch->mpPane->Detach();
	mpTabs->GetSelectionEvent()->Remove(this);
	mpTabs->RemoveTab(pSearch->mpTab);
	maSearches.RemoveAt(uiIndex);
	if (mpSearch == pSearch)
	{
		mpSearch = NULL;
		if (maSearches.NumElements() > 0)
		{
			mpSearch = maSearches.Get((uiIndex > 0) ? uiIndex - 1 : 0);
			mpTabs->Select(mpSearch->mpTab);
		}
	}
	mpTabs->GetSelectionEvent()->Add(this, &CIDEUsages::TabSelected);
	if (mpSearch.IsNull() && bKeepOne)
	{
		AddSearch();
	}
	else if (mpSearch.IsNotNull() && (mpSearch->mbStale || mpSearch->mbMoved))
	{
		ShowUsages(true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The search whose pane or tab it is, NULL if none's.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEUsagesSearch> CIDEUsages::FindSearch(CIDEResultsPane* pcPane)
{
	size	i;

	for (i = 0; i < maSearches.NumElements(); i++)
	{
		if (&maSearches.Get(i)->mpPane == pcPane)
		{
			return maSearches.Get(i);
		}
	}
	return NULL;
}


Ptr<CIDEUsagesSearch> CIDEUsages::FindSearch(CTabComponent* pcTab)
{
	size	i;

	for (i = 0; i < maSearches.NumElements(); i++)
	{
		if (&maSearches.Get(i)->mpTab == pcTab)
		{
			return maSearches.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Another tab was chosen: its search is the one shown, with its tree made
// again if the groupings or filters changed, or edits moved its usages,
// while it wasn't.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::TabSelected(CTabComponent* pcTab)
{
	Ptr<CIDEUsagesSearch>	pSearch;

	pSearch = FindSearch(mpTabs->GetSelected().IsNotNull() ? &mpTabs->GetSelected() : pcTab);
	if (pSearch.IsNull() || (pSearch == mpSearch))
	{
		return;
	}
	mpSearch = pSearch;
	if (mpSearch->mbStale || mpSearch->mbMoved)
	{
		ShowUsages(true);
	}
	mpSearch->mpPane->RefreshToolbar();
}


//////////////////////////////////////////////////////////////////////////
//
// A tab's cross was pressed: its search goes, pinned or not.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::TabCloseRequested(CTabComponent* pcTab)
{
	Ptr<CIDEUsagesSearch>	pSearch;

	pSearch = FindSearch(pcTab);
	if (pSearch.IsNotNull())
	{
		RemoveSearch(pSearch, true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Pin Tab, as IntelliJ's: the next Find Usages leaves a pinned tab as it
// is and shows what it finds in a new one.  A pinned tab's name has a pin
// before it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::TogglePinned(void)
{
	SetPinned(!mpSearch->mbPinned);
}


void CIDEUsages::SetPinned(bool bPinned)
{
	CChars	szName;

	mpSearch->mbPinned = bPinned;
	szName.Init(bPinned ? IDE_USAGES_PINNED_PREFIX : "");
	szName.Append(mpSearch->mbFound ? mpSearch->msFound.szName.Text() : "Find Usages");
	mpSearch->mpTab->SetText(szName.Text());
	szName.Kill();
	mpSearch->mpPane->RefreshToolbar();
}


bool CIDEUsages::IsPinned(void) { return mpSearch.IsNotNull() && mpSearch->mbPinned; }
size CIDEUsages::NumSearches(void) { return maSearches.NumElements(); }
Ptr<CTabbedContainer> CIDEUsages::GetTabs(void) { return mpTabs; }


//////////////////////////////////////////////////////////////////////////
//
// The groupings or the filters changed: the tab shown is made again, and
// the others are when they are chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::ShowAllUsages(void)
{
	size	i;

	for (i = 0; i < maSearches.NumElements(); i++)
	{
		maSearches.Get(i)->mbStale = true;
	}
	if (mpSearch.IsNotNull())
	{
		ShowUsages(true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's source: the place of the usage a line's node is of
// (its index in what was found, and one).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest)
{
	Ptr<CIDEUsagesSearch>	pSearch;
	SIDELocation*			psLocation;

	pSearch = FindSearch(pcPane);
	if (pSearch.IsNull() || (iData <= 0) || ((size)iData > pSearch->msFound.asUsages.NumElements()))
	{
		return false;
	}
	psLocation = &pSearch->msFound.asUsages.Get((size)iData - 1)->sLocation;
	memcpy(psDest, psLocation, sizeof(SIDELocation));
	psDest->szFileName.Init(psLocation->szFileName);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's source: where the symbol the usages are of is
// defined or declared, for Jump to Source.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::GetDeclarationLocation(CIDEResultsPane* pcPane, SIDELocation* psDest)
{
	Ptr<CIDEUsagesSearch>	pSearch;

	pSearch = FindSearch(pcPane);
	if (pSearch.IsNull() || !pSearch->mbFound || !pSearch->msFound.bDeclared)
	{
		return false;
	}
	memcpy(psDest, &pSearch->msFound.sDeclaration, sizeof(SIDELocation));
	psDest->szFileName.Init(pSearch->msFound.sDeclaration.szFileName);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A grouping's, a filter's or Pin Tab's button on a pane's toolbar: its
// command, the pane's, turns it on and off and says which it is.  What
// they turn on is the tool window's (or, for Pin Tab, the tab shown's),
// so each tab's buttons show the same.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::AddPaneCommand(Ptr<CIDEResultsPane> pPane, const char* szName, const char* szCaption, EIDEToolIcon eIcon, void(CIDEUsages::*fToggle)(void), bool(CIDEUsages::*fIsOn)(void))
{
	Ptr<CCommand>	pCommand;

	pCommand = pPane->GetCommands()->Add(szName, szCaption, "", mpcContent->GetToolIcon(eIcon), this, fToggle);
	pCommand->SetCheckedCheck(this, fIsOn);
	pPane->AddToolbarCommand(pCommand, eIcon);
}


void CIDEUsages::ToggleByType(void) { SetGrouping(muiGrouping ^ IDE_USAGES_BY_TYPE); }
void CIDEUsages::ToggleByProject(void) { SetGrouping(muiGrouping ^ IDE_USAGES_BY_PROJECT); }
void CIDEUsages::ToggleByClass(void) { SetGrouping(muiGrouping ^ IDE_USAGES_BY_CLASS); }
bool CIDEUsages::IsByType(void) { return (muiGrouping & IDE_USAGES_BY_TYPE) != 0; }
bool CIDEUsages::IsByProject(void) { return (muiGrouping & IDE_USAGES_BY_PROJECT) != 0; }
bool CIDEUsages::IsByClass(void) { return (muiGrouping & IDE_USAGES_BY_CLASS) != 0; }
void CIDEUsages::ToggleShowRead(void) { SetAccessShown(!mbShowRead, mbShowWrite); }
void CIDEUsages::ToggleShowWrite(void) { SetAccessShown(mbShowRead, !mbShowWrite); }
bool CIDEUsages::IsShowRead(void) { return mbShowRead; }
bool CIDEUsages::IsShowWrite(void) { return mbShowWrite; }


//////////////////////////////////////////////////////////////////////////
//
// Whether usages that read a value, and those that write one, are in the
// tree: it is shown again, and they are kept for the next time the IDE is
// run.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUsages::SetAccessShown(bool bRead, bool bWrite)
{
	if ((bRead != mbShowRead) || (bWrite != mbShowWrite))
	{
		mbShowRead = bRead;
		mbShowWrite = bWrite;
		ShowAllUsages();
		mpcContent->GetSettings()->WriteSettings();
	}
	if (mpSearch.IsNotNull())
	{
		mpSearch->mpPane->RefreshToolbar();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a usage is in the tree: not one that reads a value with read
// access hidden, nor one that writes one with write access hidden.  Its
// declarations always are.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::IsUsageShown(SIDEUsage* psUsage)
{
	if (psUsage->sLocation.uiRole != CPPIR_Reference)
	{
		return true;
	}
	if (psUsage->sLocation.uiUsage == CPPUT_Read)
	{
		return mbShowRead;
	}
	if (psUsage->sLocation.uiUsage == CPPUT_Write)
	{
		return mbShowWrite;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's Rerun: the symbol's usages are found again, from
// where it is declared (which has followed its file's edits), with its
// declarations if they were there before.  It can be while the symbol's
// declaration is known.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUsages::CanRerunResults(CIDEResultsPane* pcPane)
{
	Ptr<CIDEUsagesSearch>	pSearch;

	pSearch = FindSearch(pcPane);
	return pSearch.IsNotNull() && pSearch->mbFound && pSearch->msFound.bDeclared;
}


void CIDEUsages::RerunResults(CIDEResultsPane* pcPane)
{
	SIDEFoundUsages	sFound;
	CChars			szFileName;

	if (!CanRerunResults(pcPane) || (FindSearch(pcPane) != mpSearch))
	{
		return;
	}
	sFound.Init();
	szFileName.Init(mpSearch->msFound.sDeclaration.szFileName);
	if (FindAt(szFileName.Text(), mpSearch->msFound.sDeclaration.iLine, mpSearch->msFound.sDeclaration.iColumn, mpSearch->msFound.bDeclarations, &sFound))
	{
		mpSearch->SetFound(&sFound);
		ShowUsages(true);
	}
	else
	{
		sFound.Kill();
		mpcContent->ShowStatus("The symbol is no longer where it was declared: find its usages again from the editor.");
	}
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Next and Previous Occurrence need of a tool window: the Find Usages
// pane, and whether it has a usage to go to.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CDockPane> CIDEUsages::GetSteppablePane(void) { return mpUsagesTool; }
const char* CIDEUsages::GetSteppableName(void) { return "Find Usages"; }
bool CIDEUsages::HasSteps(void) { return mpSearch->mbFound && (mpSearch->msFound.asUsages.NumElements() > 0) && mpSearch.IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// The groups (IDE_USAGES_BY_*) changed, as the toolbar's buttons change
// them:
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
	if (mpSearch.IsNotNull())
	{
		mpSearch->mpPane->RefreshToolbar();
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
	ShowAllUsages();
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
	szValue = pcTag->GetStringAttribute(IDE_USAGES_READ_ATTRIBUTE);
	mbShowRead = !szValue || (StringCompare(szValue, "true") == 0);
	szValue = pcTag->GetStringAttribute(IDE_USAGES_WRITE_ATTRIBUTE);
	mbShowWrite = !szValue || (StringCompare(szValue, "true") == 0);
	ShowGrouping(uiGrouping);
	if (mpSearch.IsNotNull())
	{
		mpSearch->mpPane->ReadSettings(pcTag);
		mpSearch->mpPane->RefreshToolbar();
	}
}


void CIDEUsages::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin(IDE_USAGES_SETTINGS_TAG);
	pcWriter->Attribute(IDE_USAGES_BY_TYPE_ATTRIBUTE, (muiGrouping & IDE_USAGES_BY_TYPE) != 0);
	pcWriter->Attribute(IDE_USAGES_BY_PROJECT_ATTRIBUTE, (muiGrouping & IDE_USAGES_BY_PROJECT) != 0);
	pcWriter->Attribute(IDE_USAGES_BY_CLASS_ATTRIBUTE, (muiGrouping & IDE_USAGES_BY_CLASS) != 0);
	pcWriter->Attribute(IDE_USAGES_READ_ATTRIBUTE, mbShowRead);
	pcWriter->Attribute(IDE_USAGES_WRITE_ATTRIBUTE, mbShowWrite);
	if (mpSearch.IsNotNull())
	{
		mpSearch->mpPane->WriteSettings(pcWriter);
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CIDEUsages::GetGrouping(void) { return muiGrouping; }
Ptr<CTreeContainer> CIDEUsages::GetUsagesTree(void) { return mpSearch.IsNotNull() ? mpSearch->mpPane->GetTree() : Ptr<CTreeContainer>(); }
Ptr<CIDEResultsPane> CIDEUsages::GetResultsPane(void) { return mpSearch->mpPane; }
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
	Ptr<CIDEUsagesSearch>	pSearch;
	size					i;

	msPopupFound.AdjustPlaces(szFile, psChange);
	for (i = 0; i < maSearches.NumElements(); i++)
	{
		pSearch = maSearches.Get(i);
		if (pSearch->msFound.AdjustPlaces(szFile, psChange))
		{
			pSearch->mbMoved = true;
			pSearch->miMoved = GetDebugTickCount();
		}
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
	if (mpSearch.IsNull() || !mpSearch->mbMoved)
	{
		return -1;
	}
	if (iNow - mpSearch->miMoved < IDE_USAGES_REFRESH)
	{
		return IDE_USAGES_REFRESH - (iNow - mpSearch->miMoved);
	}
	ShowUsages(true);
	mpSearch->mpPane->ShowSelected();
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
	msPopupFound.Clear();
	if (mpPopup.IsNotNull() && mpPopup->IsOpen())
	{
		mpPopup->Close();
	}
	if (mpTabs.IsNotNull())
	{
		//Every tab goes, pinned or not, but for an empty one.
		while (maSearches.NumElements() > 0)
		{
			RemoveSearch(maSearches.Get(maSearches.NumElements() - 1), false);
		}
		AddSearch();
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
