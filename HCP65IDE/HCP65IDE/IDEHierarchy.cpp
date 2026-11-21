/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDEHierarchy.h"
#include "IDESymbolInfo.h"


static bool IsIDEHierarchyClass(uint8 uiKind) { return (uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate); }
static bool IsIDEHierarchyFunction(uint8 uiKind) { return (uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate); }


//////////////////////////////////////////////////////////////////////////
//
// The scope of a symbol's key, appended: "CFoo" for "CFoo::Init(int)",
// nothing for one with none.
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDEKeyScope(const char* szKey, CChars* pszDest)
{
	CChars		szScoped;
	const char*	szName;

	szScoped.Init();
	GetIDEScopedName(szKey, &szScoped);
	szName = strrchr(szScoped.Text(), ':');
	if (szName && (szName > szScoped.Text()) && (szName[-1] == ':'))
	{
		pszDest->Append(szScoped.Text(), (size)(szName - szScoped.Text()) - 1);
	}
	szScoped.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchySearch::Init(Ptr<CIDEResultsPane> pPane)
{
	PreInit();
	InitSearch(pPane);
	meKind = IHK_Type;
	meMode = IHM_Class;
	mszKey.Init();
	mszClassKey.Init();
	masNodes.Init();
	miSubject = -1;
	mbShown = false;
	PostInit();
}


void CIDEHierarchySearch::Class(void)
{
	CIDEResultsSearch::Class();
	U_Enum(meKind);
	U_Enum(meMode);
	U_String(mszKey);
	U_String(mszClassKey);
	U_Unknown(CArrayIDEHierarchyNodes, masNodes);
	U_Int32(miSubject);
	U_Bool(mbShown);
}


void CIDEHierarchySearch::Free(void)
{
	ClearNodes();
	masNodes.Kill();
	mszClassKey.Kill();
	mszKey.Kill();
	CIDEResultsSearch::Free();
}


bool CIDEHierarchySearch::Save(CObjectWriter* pcFile) { return false; }
bool CIDEHierarchySearch::Load(CObjectReader* pcFile) { return false; }


void CIDEHierarchySearch::ClearNodes(void)
{
	SIDEHierarchyNode*	psNode;
	size				i;

	for (i = 0; i < masNodes.NumElements(); i++)
	{
		psNode = masNodes.Get(i);
		psNode->sPlace.szFileName.Kill();
		psNode->szDetail.Kill();
		psNode->szName.Kill();
		psNode->szKey.Kill();
	}
	masNodes.ReInit();
	miSubject = -1;
}


//////////////////////////////////////////////////////////////////////////
//
// A row under the row iParent (-1 for the top), with nowhere to go and
// nothing said of it yet; its own rows are taken as found.
//
//////////////////////////////////////////////////////////////////////////
SIDEHierarchyNode* CIDEHierarchySearch::AddNode(int32 iParent, const char* szKey, uint8 uiKind)
{
	SIDEHierarchyNode*	psNode;

	psNode = masNodes.Add();
	psNode->szKey.Init(szKey);
	psNode->uiKind = uiKind;
	psNode->szName.Init();
	GetIDEScopedName(szKey, &psNode->szName);
	psNode->szDetail.Init();
	memset(&psNode->sPlace, 0, sizeof(SIDELocation));
	psNode->sPlace.szFileName.Init();
	psNode->sPlace.iContainer = -1;
	psNode->iParent = iParent;
	psNode->bChildrenFound = true;
	psNode->bExpanded = true;
	return psNode;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	miExpandedData = 0;
	mbShowing = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpTabs);
	M_Pointer(mpTool);
	M_Pointer(mpExpandedPane);
	U_Int64(miExpandedData);
	U_Bool(mbShowing);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off the trees still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::Free(void)
{
	size	i;

	for (i = 0; mpTabs.IsNotNull() && (i < mpTabs->NumSearches()); i++)
	{
		if (mpTabs->GetSearch(i)->mpPane.IsNotNull() && mpTabs->GetSearch(i)->mpPane->GetTree().IsNotNull())
		{
			mpTabs->GetSearch(i)->mpPane->GetTree()->GetExpandedEvent()->Remove(this);
			mpTabs->GetSearch(i)->mpPane->GetTree()->GetActivationEvent()->Remove(this);
		}
	}
}


bool CIDEHierarchy::Save(CObjectWriter* pcFile) { return false; }
bool CIDEHierarchy::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Navigate's, with IntelliJ IDEA's keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.TypeHierarchy", "Type Hierarchy", "Control+H", Ptr<CImageCel>(), this, &CIDEHierarchy::ShowTypeHierarchy)->SetEnabledCheck(this, &CIDEHierarchy::HasEditor);
	pCommands->Add("Navigate.MethodHierarchy", "Method Hierarchy", "Control+Shift+H", Ptr<CImageCel>(), this, &CIDEHierarchy::ShowMethodHierarchy)->SetEnabledCheck(this, &CIDEHierarchy::HasEditor);
	pCommands->Add("Navigate.CallHierarchy", "Call Hierarchy", "Control+Alt+H", Ptr<CImageCel>(), this, &CIDEHierarchy::ShowCallHierarchy)->SetEnabledCheck(this, &CIDEHierarchy::HasEditor);
	pCommands->Add("View.Hierarchy", "Hierarchy", "Alt+8", Ptr<CImageCel>(), this, &CIDEHierarchy::ShowHierarchyWindow);
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+8, as IntelliJ IDEA's: the Hierarchy tool window is shown and its
// tree has the keys.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::ShowHierarchyWindow(void)
{
	if (mpTool.IsNull() || GetSearch().IsNull())
	{
		return false;
	}
	mpcContent->GetDock()->Activate(mpTool);
	GetSearch()->mpPane->Focus();
	return true;
}


void CIDEHierarchy::AddNavigateItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.TypeHierarchy"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.MethodHierarchy"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.CallHierarchy"));
}


bool CIDEHierarchy::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// The Hierarchy tool window, put at psPlace: tabs of results panes, as
// the Find tool window's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::CreatePane(SDockPlace* psPlace)
{
	mpTabs = OMalloc<CIDEResultsTabs>(mpcContent, this, "Hierarchy");
	mpTabs->AddSearch();
	mpTool = mpcContent->GetDock()->AddPane(IDE_HIERARCHY_ID, "Hierarchy", NULL, mpTabs->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// The tabs' owner: a new, empty hierarchy with its results pane.  Its
// toolbar has, after the pane's own, how the hierarchy is shown (those
// that aren't its kind's dim), the pane's view commands and Pin Tab.
// Enter on a row goes to its place: its chevron opens and closes it.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEResultsSearch> CIDEHierarchy::CreateResultsSearch(void)
{
	Ptr<CIDEResultsPane>	pPane;

	pPane = OMalloc<CIDEResultsPane>(mpcContent, this, "rows", true);
	pPane->GetTree()->SetActivationToggles(false);
	pPane->GetTree()->GetExpandedEvent()->Add(this, &CIDEHierarchy::NodeExpanded);
	pPane->GetTree()->GetActivationEvent()->Add(this, &CIDEHierarchy::RowActivated);
	pPane->AddToolbarSeparator();
	AddModeCommand(pPane, "Hierarchy.Class", "Class Hierarchy", ITI_ClassHierarchy, &CIDEHierarchy::ShowClassMode, &CIDEHierarchy::IsClassMode, &CIDEHierarchy::IsOfClasses);
	AddModeCommand(pPane, "Hierarchy.Supertypes", "Supertypes Hierarchy", ITI_Supertypes, &CIDEHierarchy::ShowSupertypesMode, &CIDEHierarchy::IsSupertypesMode, &CIDEHierarchy::IsOfClasses);
	AddModeCommand(pPane, "Hierarchy.Subtypes", "Subtypes Hierarchy", ITI_Subtypes, &CIDEHierarchy::ShowSubtypesMode, &CIDEHierarchy::IsSubtypesMode, &CIDEHierarchy::IsOfClasses);
	AddModeCommand(pPane, "Hierarchy.Callers", "Caller Hierarchy", ITI_Callers, &CIDEHierarchy::ShowCallersMode, &CIDEHierarchy::IsCallersMode, &CIDEHierarchy::IsOfCalls);
	AddModeCommand(pPane, "Hierarchy.Callees", "Callee Hierarchy", ITI_Callees, &CIDEHierarchy::ShowCalleesMode, &CIDEHierarchy::IsCalleesMode, &CIDEHierarchy::IsOfCalls);
	pPane->AddToolbarSeparator();
	pPane->AddViewCommands();
	mpTabs->AddPinCommand(pPane);
	return OMalloc<CIDEHierarchySearch>(pPane);
}


void CIDEHierarchy::AddModeCommand(Ptr<CIDEResultsPane> pPane, const char* szName, const char* szCaption, EIDEToolIcon eIcon, void(CIDEHierarchy::*fShow)(void), bool(CIDEHierarchy::*fIsShown)(void), bool(CIDEHierarchy::*fCanShow)(void))
{
	Ptr<CCommand>	pCommand;

	pCommand = pPane->GetCommands()->Add(szName, szCaption, "", mpcContent->GetToolIcon(eIcon), this, fShow);
	pCommand->SetCheckedCheck(this, fIsShown);
	pCommand->SetEnabledCheck(this, fCanShow);
	pPane->AddToolbarCommand(pCommand, eIcon);
}


void CIDEHierarchy::ShowClassMode(void) { SetMode(IHM_Class); }
void CIDEHierarchy::ShowSupertypesMode(void) { SetMode(IHM_Supertypes); }
void CIDEHierarchy::ShowSubtypesMode(void) { SetMode(IHM_Subtypes); }
void CIDEHierarchy::ShowCallersMode(void) { SetMode(IHM_Callers); }
void CIDEHierarchy::ShowCalleesMode(void) { SetMode(IHM_Callees); }
bool CIDEHierarchy::IsClassMode(void) { return IsOfClasses() && (GetSearch()->meMode == IHM_Class); }
bool CIDEHierarchy::IsSupertypesMode(void) { return IsOfClasses() && (GetSearch()->meMode == IHM_Supertypes); }
bool CIDEHierarchy::IsSubtypesMode(void) { return IsOfClasses() && (GetSearch()->meMode == IHM_Subtypes); }
bool CIDEHierarchy::IsCallersMode(void) { return IsOfCalls() && (GetSearch()->meMode == IHM_Callers); }
bool CIDEHierarchy::IsCalleesMode(void) { return IsOfCalls() && (GetSearch()->meMode == IHM_Callees); }
bool CIDEHierarchy::IsOfClasses(void) { return GetSearch().IsNotNull() && GetSearch()->mbShown && (GetSearch()->meKind != IHK_Call); }
bool CIDEHierarchy::IsOfCalls(void) { return GetSearch().IsNotNull() && GetSearch()->mbShown && (GetSearch()->meKind == IHK_Call); }


//////////////////////////////////////////////////////////////////////////
//
// The chosen tab's hierarchy is shown another way, made again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::SetMode(EIDEHierarchyMode eMode)
{
	Ptr<CIDEHierarchySearch>	pSearch;

	pSearch = GetSearch();
	if (pSearch.IsNull() || !pSearch->mbShown || (pSearch->meMode == eMode))
	{
		return;
	}
	pSearch->meMode = eMode;
	Build(pSearch);
	ShowTree(pSearch, (int64)pSearch->miSubject + 1);
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so have the hierarchies of what was in them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::Clear(void)
{
	Ptr<CIDEHierarchySearch>	pSearch;

	mpExpandedPane = NULL;
	while (mpTabs.IsNotNull() && (mpTabs->NumSearches() > 1))
	{
		mpTabs->RemoveSearch(mpTabs->GetSearch(mpTabs->NumSearches() - 1), true);
	}
	pSearch = GetSearch();
	if (pSearch.IsNotNull())
	{
		pSearch->ClearNodes();
		pSearch->mbShown = false;
		pSearch->mszKey.Clear();
		pSearch->mszClassKey.Clear();
		pSearch->mpPane->Clear();
		mpTabs->SetPinned(false);
		mpTabs->SetName(pSearch, "");
	}
}


void CIDEHierarchy::Detach(void)
{
	if (mpTabs.IsNotNull())
	{
		mpTabs->Detach();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The class Type Hierarchy is of: the class named at the caret, the class
// of a member named there, or the class a variable, field or parameter
// named there is declared with.  False if the caret is on none of them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::FindCaretClass(CChars* pszKey)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	CChars				szScope;
	uint8				uiKind;
	uint8				uiScopeKind;
	bool				bFound;

	szKey.Init();
	uiKind = 0;
	bFound = false;
	if (mpcContent->GetNavigator()->FindCaretSymbol(&szKey, &uiKind))
	{
		if (IsIDEHierarchyClass(uiKind))
		{
			pszKey->Set(szKey);
			bFound = true;
		}
		else
		{
			//A member's class.
			szScope.Init();
			AppendIDEKeyScope(szKey.Text(), &szScope);
			asLocations.Init();
			uiScopeKind = 0;
			if (!szScope.Empty() && mpcContent->GetCodeModel()->GetSymbolLocations(szScope.Text(), &asLocations, &uiScopeKind) && IsIDEHierarchyClass(uiScopeKind) &&
				((uiKind == CPPSK_Field) || IsIDEHierarchyFunction(uiKind)))
			{
				pszKey->Set(szScope);
				bFound = true;
			}
			KillIDELocations(&asLocations);
			szScope.Kill();

			//What a variable is declared with.
			if (!bFound && mpcContent->GetNavigator()->FindTypeOf(szKey.Text(), uiKind, pszKey))
			{
				asLocations.Init();
				bFound = mpcContent->GetCodeModel()->GetSymbolLocations(pszKey->Text(), &asLocations, &uiScopeKind) && IsIDEHierarchyClass(uiScopeKind);
				KillIDELocations(&asLocations);
			}
		}
	}
	szKey.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Type Hierarchy (Control+H).  False, the status line saying so, if the
// caret is on no class.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::ShowTypeHierarchy(void)
{
	CChars	szKey;
	bool	bResult;

	if (!HasEditor())
	{
		return false;
	}
	szKey.Init();
	bResult = FindCaretClass(&szKey);
	if (bResult)
	{
		bResult = Show(IHK_Type, IHM_Class, szKey.Text(), szKey.Text());
	}
	else
	{
		mpcContent->ShowStatus("There is no class at the caret to show the hierarchy of.");
	}
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Method Hierarchy (Control+Shift+H): of the member function named at the
// caret, or the one the caret is in.  False, the status line saying so,
// if that is no class's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::ShowMethodHierarchy(void)
{
	CArrayIDELocations	asLocations;
	CChars				szKey;
	CChars				szClass;
	uint8				uiKind;
	uint8				uiClassKind;
	bool				bResult;

	if (!HasEditor())
	{
		return false;
	}
	szKey.Init();
	szClass.Init();
	uiKind = 0;
	bResult = (mpcContent->GetNavigator()->FindCaretSymbol(&szKey, &uiKind) && IsIDEHierarchyFunction(uiKind)) ||
			  (mpcContent->GetNavigator()->FindCaretFunction(&szKey, &uiKind) && IsIDEHierarchyFunction(uiKind));
	if (bResult)
	{
		AppendIDEKeyScope(szKey.Text(), &szClass);
		asLocations.Init();
		uiClassKind = 0;
		bResult = !szClass.Empty() && mpcContent->GetCodeModel()->GetSymbolLocations(szClass.Text(), &asLocations, &uiClassKind) && IsIDEHierarchyClass(uiClassKind);
		KillIDELocations(&asLocations);
	}
	if (bResult)
	{
		bResult = Show(IHK_Method, IHM_Class, szKey.Text(), szClass.Text());
	}
	else
	{
		mpcContent->ShowStatus("There is no member function at the caret to show the hierarchy of.");
	}
	szClass.Kill();
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Call Hierarchy (Control+Alt+H): of the function named at the caret, or
// the one the caret is in.  False, the status line saying so, with none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::ShowCallHierarchy(void)
{
	CChars	szKey;
	uint8	uiKind;
	bool	bResult;

	if (!HasEditor())
	{
		return false;
	}
	szKey.Init();
	uiKind = 0;
	bResult = (mpcContent->GetNavigator()->FindCaretSymbol(&szKey, &uiKind) && IsIDEHierarchyFunction(uiKind)) ||
			  (mpcContent->GetNavigator()->FindCaretFunction(&szKey, &uiKind) && IsIDEHierarchyFunction(uiKind));
	if (bResult)
	{
		bResult = Show(IHK_Call, IHM_Callers, szKey.Text(), "");
	}
	else
	{
		mpcContent->ShowStatus("There is no function at the caret to show the calls of.");
	}
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A hierarchy of szKey is made and shown in the chosen tab (a new one if
// that is pinned), named for it, with what it is of selected; the tool
// window is shown and its tree has the keys.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::Show(EIDEHierarchyKind eKind, EIDEHierarchyMode eMode, const char* szKey, const char* szClassKey)
{
	Ptr<CIDEHierarchySearch>	pSearch;
	CChars						szName;

	if (mpTabs.IsNull())
	{
		return false;
	}
	pSearch = Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->GetSearchForNew());
	pSearch->meKind = eKind;
	pSearch->meMode = eMode;
	pSearch->mszKey.Set(szKey);
	pSearch->mszClassKey.Set(szClassKey);
	pSearch->mbShown = true;
	Build(pSearch);

	szName.Init((eKind == IHK_Type) ? "Type: " : ((eKind == IHK_Method) ? "Method: " : "Calls: "));
	GetIDEScopedName(szKey, &szName);
	mpTabs->SetName(pSearch, szName.Text());
	szName.Kill();

	ShowTree(pSearch, (int64)pSearch->miSubject + 1);
	mpcContent->GetDock()->Activate(mpTool);
	pSearch->mpPane->Focus();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A hierarchy's rows, from the index as it is now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::Build(Ptr<CIDEHierarchySearch> pSearch)
{
	CArrayChars			aszChain;
	CArrayChars			aszBases;
	SIDEHierarchyNode*	psNode;
	CArrayIDETargets	asTargets;
	size				i;
	int32				iNode;
	uint8				uiKind;

	pSearch->ClearNodes();
	if (pSearch->meKind == IHK_Call)
	{
		//The function, and what calls it or what it calls.
		asTargets.Init();
		uiKind = CPPSK_Function;
		if (mpcContent->GetCodeModel()->FindTarget(pSearch->mszKey.Text(), &asTargets))
		{
			uiKind = asTargets.Get(0)->uiKind;
		}
		psNode = pSearch->AddNode(-1, pSearch->mszKey.Text(), uiKind);
		if (asTargets.NumElements() > 0)
		{
			psNode->sPlace.szFileName.Set(asTargets.Get(0)->sLocation.szFileName);
			psNode->sPlace.iLine = asTargets.Get(0)->sLocation.iLine;
			psNode->sPlace.iColumn = asTargets.Get(0)->sLocation.iColumn;
			psNode->sPlace.iLength = asTargets.Get(0)->sLocation.iLength;
		}
		KillIDETargets(&asTargets);
		pSearch->miSubject = 0;
		FindCalls(pSearch, 0);
		return;
	}

	if (pSearch->meMode == IHM_Class)
	{
		//The class's first base class, its first, and so on: the topmost is the first row.
		aszChain.Init();
		aszChain.Add(pSearch->mszClassKey.Text());
		while (aszChain.NumElements() < IDE_HIERARCHY_DEPTH)
		{
			aszBases.Init();
			mpcContent->GetCodeModel()->FindClassBases(aszChain.Tail()->Text(), false, &aszBases);
			if ((aszBases.NumElements() == 0) || aszChain.Contains(aszBases.GetText(0)))
			{
				aszBases.Kill();
				break;
			}
			aszChain.Add(aszBases.GetText(0));
			aszBases.Kill();
		}
		iNode = -1;
		for (i = aszChain.NumElements(); i > 0; i--)
		{
			iNode = AddClass(pSearch, iNode, aszChain.GetText(i - 1));
		}
		aszChain.Kill();
		pSearch->miSubject = iNode;
		AddClasses(pSearch, iNode, true, 0);
	}
	else
	{
		iNode = AddClass(pSearch, -1, pSearch->mszClassKey.Text());
		pSearch->miSubject = iNode;
		AddClasses(pSearch, iNode, pSearch->meMode == IHM_Subtypes, 0);
	}
	if (pSearch->meKind == IHK_Method)
	{
		MarkMethod(pSearch);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A class's row, under the row iParent: its name, the file it is defined
// in after it, and its definition to go to.  Its index.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEHierarchy::AddClass(Ptr<CIDEHierarchySearch> pSearch, int32 iParent, const char* szKey)
{
	CArrayIDETargets	asTargets;
	SIDEHierarchyNode*	psNode;

	psNode = pSearch->AddNode(iParent, szKey, CPPSK_Class);
	asTargets.Init();
	if (mpcContent->GetCodeModel()->FindTarget(szKey, &asTargets))
	{
		psNode->uiKind = asTargets.Get(0)->uiKind;
		psNode->sPlace.szFileName.Set(asTargets.Get(0)->sLocation.szFileName);
		psNode->sPlace.iLine = asTargets.Get(0)->sLocation.iLine;
		psNode->sPlace.iColumn = asTargets.Get(0)->sLocation.iColumn;
		psNode->sPlace.iLength = asTargets.Get(0)->sLocation.iLength;
		psNode->szDetail.Append("  ");
		psNode->szDetail.Append(GetIDEFileNamePart(asTargets.Get(0)->sLocation.szFileName.Text()));
	}
	KillIDETargets(&asTargets);
	return (int32)pSearch->masNodes.NumElements() - 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The classes derived from the row iNode's class (or, not bDerived, its
// base classes) under it, and theirs under them, a level at a time.  A
// class that is over itself (it shouldn't be) isn't followed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::AddClasses(Ptr<CIDEHierarchySearch> pSearch, int32 iNode, bool bDerived, int32 iDepth)
{
	CArrayChars	aszKeys;
	CChars		szKey;
	size		i;
	int32		iChild;
	int32		iAncestor;
	bool		bOver;

	if ((iDepth >= IDE_HIERARCHY_DEPTH) || (pSearch->masNodes.NumElements() >= IDE_HIERARCHY_NODES))
	{
		return;
	}
	szKey.Init(pSearch->masNodes.Get((size)iNode)->szKey);
	aszKeys.Init();
	if (bDerived)
	{
		mpcContent->GetCodeModel()->FindClassDerived(szKey.Text(), false, &aszKeys);
	}
	else
	{
		mpcContent->GetCodeModel()->FindClassBases(szKey.Text(), false, &aszKeys);
	}
	for (i = 0; (i < aszKeys.NumElements()) && (pSearch->masNodes.NumElements() < IDE_HIERARCHY_NODES); i++)
	{
		bOver = false;
		for (iAncestor = iNode; (iAncestor >= 0) && !bOver; iAncestor = pSearch->masNodes.Get((size)iAncestor)->iParent)
		{
			bOver = pSearch->masNodes.Get((size)iAncestor)->szKey.Equals(aszKeys.GetText(i));
		}
		if (!bOver)
		{
			iChild = AddClass(pSearch, iNode, aszKeys.GetText(i));
			AddClasses(pSearch, iChild, bDerived, iDepth + 1);
		}
	}
	aszKeys.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Method Hierarchy: each class's row says what it has of the function.
// The function, what overrides it and what it overrides are one family,
// each a class's; a class with one of them defines it (enter goes to it),
// or with "= 0" only declares it.  A class without inherits it if a base
// class of its defines it, should define it if a base class only declares
// it, and otherwise has nothing to do with it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::MarkMethod(Ptr<CIDEHierarchySearch> pSearch)
{
	CArrayIDETargets	asFamily;
	CArrayChars			aszClasses;
	CArrayChars			aszBases;
	CArrayInt			aiAbstract;
	SIDEHierarchyNode*	psNode;
	SIDESymbolInfo		sInfo;
	CChars				szScope;
	size				i;
	size				j;
	size				k;
	int					iAbstract;
	int					iOwn;
	bool				bDefined;
	bool				bDeclared;

	asFamily.Init();
	mpcContent->GetCodeModel()->FindTarget(pSearch->mszKey.Text(), &asFamily);
	mpcContent->GetCodeModel()->FindImplementations(pSearch->mszKey.Text(), &asFamily);
	mpcContent->GetCodeModel()->FindSuperMethods(pSearch->mszKey.Text(), &asFamily);

	aszClasses.Init();
	aiAbstract.Init();
	for (i = 0; i < asFamily.NumElements(); i++)
	{
		szScope.Init();
		AppendIDEKeyScope(asFamily.Get(i)->szKey.Text(), &szScope);
		aszClasses.Add(szScope.Text());
		szScope.Kill();

		sInfo.Init();
		iAbstract = 0;
		if (ReadIDESymbolInfo(mpcContent, asFamily.Get(i)->szKey.Text(), &sInfo) && sInfo.szDeclaration.EndsWith("= 0"))
		{
			iAbstract = 1;
		}
		aiAbstract.Add(iAbstract);
		sInfo.Kill();
	}

	for (i = 0; i < pSearch->masNodes.NumElements(); i++)
	{
		psNode = pSearch->masNodes.Get(i);
		iOwn = -1;
		for (j = 0; (j < aszClasses.NumElements()) && (iOwn < 0); j++)
		{
			if (aszClasses.Get(j)->Equals(psNode->szKey))
			{
				iOwn = (int)j;
			}
		}
		if (iOwn >= 0)
		{
			//Its own: enter goes to it.
			psNode->szDetail.Insert(0, aiAbstract.GetValue((size)iOwn) ? "  abstract" : "  +");
			psNode->sPlace.szFileName.Set(asFamily.Get((size)iOwn)->sLocation.szFileName);
			psNode->sPlace.iLine = asFamily.Get((size)iOwn)->sLocation.iLine;
			psNode->sPlace.iColumn = asFamily.Get((size)iOwn)->sLocation.iColumn;
			psNode->sPlace.iLength = asFamily.Get((size)iOwn)->sLocation.iLength;
			continue;
		}

		//Its base classes', the nearest first.
		aszBases.Init();
		mpcContent->GetCodeModel()->FindClassBases(psNode->szKey.Text(), true, &aszBases);
		bDefined = false;
		bDeclared = false;
		for (j = 0; (j < aszBases.NumElements()) && !bDefined && !bDeclared; j++)
		{
			for (k = 0; k < aszClasses.NumElements(); k++)
			{
				if (aszClasses.Get(k)->Equals(aszBases.GetText(j)))
				{
					bDefined = bDefined || !aiAbstract.GetValue(k);
					bDeclared = bDeclared || (aiAbstract.GetValue(k) != 0);
				}
			}
		}
		aszBases.Kill();
		if (bDefined)
		{
			psNode->szDetail.Insert(0, "  -");
		}
		else if (bDeclared)
		{
			psNode->szDetail.Insert(0, "  !");
		}
	}
	aiAbstract.Kill();
	aszClasses.Kill();
	KillIDETargets(&asFamily);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a function's first call comes before another's: in a file
// whose name is before the other's, or sooner in the same file.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDECallBefore(SIDECallRelation* psLeft, SIDECallRelation* psRight)
{
	SIDELocation*	psLeftCall;
	SIDELocation*	psRightCall;
	int				iCompare;

	if ((psLeft->asCalls.NumElements() == 0) || (psRight->asCalls.NumElements() == 0))
	{
		return false;
	}
	psLeftCall = psLeft->asCalls.Get(0);
	psRightCall = psRight->asCalls.Get(0);
	iCompare = StringCompare(psLeftCall->szFileName.Text(), psRightCall->szFileName.Text());
	if (iCompare != 0)
	{
		return iCompare < 0;
	}
	return (psLeftCall->iLine < psRightCall->iLine) || ((psLeftCall->iLine == psRightCall->iLine) && (psLeftCall->iColumn < psRightCall->iColumn));
}


//////////////////////////////////////////////////////////////////////////
//
// Call Hierarchy: the rows under the function of the row iNode, found
// now: the functions that call it (or that it calls), each with how many
// calls after its name, and the first of them to go to.  Their own rows
// aren't looked for until each is opened; a function that is already
// over itself here (it calls itself, perhaps through others) says so and
// has none.
//
// A virtual function's callers are also the callers of each function it
// overrides, as a call through a base class may land in it: their rows
// come after its own and say which ("via CShape::Draw").  Only what it
// overrides is followed, not what else overrides that: a call on another
// derived class can't land here.  Nor can a call that names the class
// (CSquare::Draw()), which is of that class's function and no other.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::FindCalls(Ptr<CIDEHierarchySearch> pSearch, int32 iNode)
{
	CArrayIDECallRelations	asRelations;
	CArrayIDETargets		asSupers;
	CChars					szKey;
	CChars					szVia;
	size					i;

	szKey.Init(pSearch->masNodes.Get((size)iNode)->szKey);
	pSearch->masNodes.Get((size)iNode)->bChildrenFound = true;
	asRelations.Init();
	if (pSearch->meMode == IHM_Callees)
	{
		mpcContent->GetCodeModel()->FindCallees(szKey.Text(), &asRelations);
	}
	else
	{
		mpcContent->GetCodeModel()->FindCallers(szKey.Text(), &asRelations);
	}
	AddCallRows(pSearch, iNode, &asRelations, NULL);
	KillIDECallRelations(&asRelations);

	if (pSearch->meMode == IHM_Callers)
	{
		asSupers.Init();
		mpcContent->GetCodeModel()->FindSuperMethods(szKey.Text(), &asSupers);
		for (i = 0; i < asSupers.NumElements(); i++)
		{
			asRelations.Init();
			mpcContent->GetCodeModel()->FindCallers(asSupers.Get(i)->szKey.Text(), &asRelations);
			RemoveQualifiedCalls(&asRelations);
			szVia.Init();
			GetIDEScopedName(asSupers.Get(i)->szKey.Text(), &szVia);
			AddCallRows(pSearch, iNode, &asRelations, szVia.Text());
			szVia.Kill();
			KillIDECallRelations(&asRelations);
		}
		KillIDETargets(&asSupers);
	}
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The calls written with their class before them (CSquare::Draw()) are
// taken out, and the functions left with none: such a call isn't
// virtual, so it is of the function it names alone.  It is told from the
// call's line: "::" before the name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::RemoveQualifiedCalls(CArrayIDECallRelations* pasRelations)
{
	CIDEFileLines		cLines;
	SIDECallRelation*	psRelation;
	SIDELocation*		psCall;
	CChars				szLine;
	size				uiTrimmed;
	size				i;
	size				j;
	int32				iAt;

	cLines.Init(&mpcContent->GetDocuments());
	for (i = pasRelations->NumElements(); i > 0; i--)
	{
		psRelation = pasRelations->Get(i - 1);
		for (j = psRelation->asCalls.NumElements(); j > 0; j--)
		{
			psCall = psRelation->asCalls.Get(j - 1);
			szLine.Init();
			uiTrimmed = 0;
			cLines.AppendLine(psCall->szFileName.Text(), psCall->iLine, &szLine, &uiTrimmed);
			iAt = psCall->iColumn - (int32)uiTrimmed;
			while ((iAt > 0) && ((size)iAt <= szLine.Length()) && ((szLine.GetChar((size)iAt - 1) == ' ') || (szLine.GetChar((size)iAt - 1) == '\t')))
			{
				iAt--;
			}
			if ((iAt >= 2) && ((size)iAt <= szLine.Length()) && (szLine.GetChar((size)iAt - 1) == ':') && (szLine.GetChar((size)iAt - 2) == ':'))
			{
				psCall->szFileName.Kill();
				psRelation->asCalls.RemoveAt(j - 1);
			}
			szLine.Kill();
		}
		if (psRelation->asCalls.NumElements() == 0)
		{
			psRelation->asCalls.Kill();
			psRelation->sPlace.szFileName.Kill();
			psRelation->szKey.Kill();
			pasRelations->RemoveAt(i - 1);
		}
	}
	cLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A row under the row iNode for each function of pasRelations, in the
// order of their first calls (what a function calls, as it calls them).
// With szVia the calls are of that function, one the row's overrides,
// and each row says so.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::AddCallRows(Ptr<CIDEHierarchySearch> pSearch, int32 iNode, CArrayIDECallRelations* pasRelations, const char* szVia)
{
	SIDECallRelation*	psRelation;
	SIDEHierarchyNode*	psNode;
	SIDELocation*		psCall;
	size				i;
	size				j;
	int32				iAncestor;
	bool				bOver;

	for (i = 1; i < pasRelations->NumElements(); i++)
	{
		for (j = i; (j > 0) && IsIDECallBefore(pasRelations->Get(j), pasRelations->Get(j - 1)); j--)
		{
			pasRelations->Swap(j, j - 1);
		}
	}
	for (i = 0; (i < pasRelations->NumElements()) && (pSearch->masNodes.NumElements() < IDE_HIERARCHY_NODES); i++)
	{
		psRelation = pasRelations->Get(i);
		bOver = false;
		for (iAncestor = iNode; (iAncestor >= 0) && !bOver; iAncestor = pSearch->masNodes.Get((size)iAncestor)->iParent)
		{
			bOver = pSearch->masNodes.Get((size)iAncestor)->szKey.Equals(psRelation->szKey);
		}

		psNode = pSearch->AddNode(iNode, psRelation->szKey.Text(), psRelation->uiKind);
		psNode->bChildrenFound = bOver;
		psNode->bExpanded = false;
		if (szVia)
		{
			psNode->szDetail.Append("  via ");
			psNode->szDetail.Append(szVia);
		}
		if (psRelation->asCalls.NumElements() > 0)
		{
			psCall = psRelation->asCalls.Get(0);
			psNode->sPlace.szFileName.Set(psCall->szFileName);
			psNode->sPlace.iLine = psCall->iLine;
			psNode->sPlace.iColumn = psCall->iColumn;
			psNode->sPlace.iLength = psCall->iLength;
			if (psRelation->asCalls.NumElements() > 1)
			{
				psNode->szDetail.Append("  ");
				psNode->szDetail.Append((uint32)psRelation->asCalls.NumElements());
				psNode->szDetail.Append(" calls");
			}
			psNode->szDetail.Append("  ");
			psNode->szDetail.Append(GetIDEFileNamePart(psCall->szFileName.Text()));
			psNode->szDetail.Append(':');
			psNode->szDetail.Append((int32)(psCall->iLine + 1));
		}
		if (bOver)
		{
			psNode->szDetail.Append("  recursive");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The pane's tree is made from the hierarchy's rows, each open or closed
// as it is kept, with the row iSelectedData (its index plus one) selected.
// A row whose own rows haven't been looked for has one saying so under
// it, so it can be opened.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::ShowTree(Ptr<CIDEHierarchySearch> pSearch, int64 iSelectedData)
{
	Ptr<CIDEResultsPane>	pPane;
	Ptr<CTreeContainer>		pTree;
	int32					iNode;
	int64					iData;

	pPane = pSearch->mpPane;
	pTree = pPane->GetTree();
	mbShowing = true;
	pPane->BeginShow(false);
	AddTreeRows(pSearch, -1, TREE_NO_NODE);
	for (iNode = 0; iNode < pTree->NumNodes(); iNode++)
	{
		iData = pTree->GetNodeData(iNode);
		if (iData > 0)
		{
			pTree->SetExpanded(iNode, pSearch->masNodes.Get((size)iData - 1)->bExpanded);
		}
	}
	pPane->EndShow(iSelectedData);
	mbShowing = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The rows under the row iParent (-1 for the top) are added to the tree
// under its node iParentNode, each followed by its own: the tree's nodes
// are in the order they are seen, which is the order they are stepped
// through.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::AddTreeRows(Ptr<CIDEHierarchySearch> pSearch, int32 iParent, int32 iParentNode)
{
	SIDEHierarchyNode*	psNode;
	size				i;
	int32				iTreeNode;

	for (i = (size)(iParent + 1); i < pSearch->masNodes.NumElements(); i++)
	{
		psNode = pSearch->masNodes.Get(i);
		if (psNode->iParent != iParent)
		{
			continue;
		}
		iTreeNode = pSearch->mpPane->AddSymbol(iParentNode, psNode->szName.Text(), psNode->szDetail.Text(), psNode->uiKind, (int64)i + 1);
		if (psNode->bChildrenFound)
		{
			AddTreeRows(pSearch, (int32)i, iTreeNode);
		}
		else
		{
			pSearch->mpPane->AddGroup(iTreeNode, IDE_HIERARCHY_MORE, NULL);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Which rows are open is taken from the tree, before it is made again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::KeepExpanded(Ptr<CIDEHierarchySearch> pSearch)
{
	Ptr<CTreeContainer>	pTree;
	int32				iNode;
	int64				iData;

	pTree = pSearch->mpPane->GetTree();
	for (iNode = 0; iNode < pTree->NumNodes(); iNode++)
	{
		iData = pTree->GetNodeData(iNode);
		if ((iData > 0) && ((size)iData <= pSearch->masNodes.NumElements()))
		{
			pSearch->masNodes.Get((size)iData - 1)->bExpanded = pTree->IsExpanded(iNode);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A row of a hierarchy's tree was opened.  If its own rows haven't been
// looked for they are, at the next tick, once the tree is done with the
// press that opened it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::NodeExpanded(CTreeContainer* pcTree, int32 iNode)
{
	Ptr<CIDEHierarchySearch>	pSearch;
	int64						iData;
	size						i;

	if (mbShowing || mpTabs.IsNull() || mpExpandedPane.IsNotNull())
	{
		return;
	}
	iData = pcTree->GetNodeData(iNode);
	for (i = 0; i < mpTabs->NumSearches(); i++)
	{
		pSearch = Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->GetSearch(i));
		if ((&pSearch->mpPane->GetTree() == pcTree) && (iData > 0) && ((size)iData <= pSearch->masNodes.NumElements()) && !pSearch->masNodes.Get((size)iData - 1)->bChildrenFound)
		{
			mpExpandedPane = pSearch->mpPane;
			miExpandedData = iData;
			mpcContent->GetWindow()->Post(this, &CIDEHierarchy::ExpandPending);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Enter or a double click on a row.  A result's is the pane's (it goes
// to the row's place); on the row that stands for rows not looked for
// yet (Expand All shows them), the row it is under has them found.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::RowActivated(CListContainer* pcList)
{
	CTreeContainer*	pcTree;
	int32			iNode;

	pcTree = (CTreeContainer*)pcList;
	iNode = pcTree->GetSelectedNode();
	if ((iNode != TREE_NO_NODE) && (pcTree->GetNodeData(iNode) == 0) && (pcTree->GetNodeParent(iNode) != TREE_NO_NODE))
	{
		NodeExpanded(pcTree, pcTree->GetNodeParent(iNode));
	}
}


void CIDEHierarchy::ExpandPending(void)
{
	Ptr<CIDEHierarchySearch>	pSearch;
	Ptr<CIDEResultsPane>		pPane;
	int64						iData;

	pPane = mpExpandedPane;
	iData = miExpandedData;
	mpExpandedPane = NULL;
	miExpandedData = 0;
	if (pPane.IsNull() || mpTabs.IsNull())
	{
		return;
	}
	pSearch = Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->FindSearch(&pPane));
	if (pSearch.IsNull() || (iData <= 0) || ((size)iData > pSearch->masNodes.NumElements()) || pSearch->masNodes.Get((size)iData - 1)->bChildrenFound)
	{
		return;
	}
	KeepExpanded(pSearch);
	pSearch->masNodes.Get((size)iData - 1)->bExpanded = true;
	FindCalls(pSearch, (int32)iData - 1);
	ShowTree(pSearch, iData);
}


//////////////////////////////////////////////////////////////////////////
//
// The results' source: a row's place.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest)
{
	Ptr<CIDEHierarchySearch>	pSearch;
	SIDEHierarchyNode*			psNode;

	pSearch = mpTabs.IsNotNull() ? Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->FindSearch(pcPane)) : Ptr<CIDEHierarchySearch>();
	if (pSearch.IsNull() || (iData <= 0) || ((size)iData > pSearch->masNodes.NumElements()))
	{
		return false;
	}
	psNode = pSearch->masNodes.Get((size)iData - 1);
	if (psNode->sPlace.szFileName.Empty())
	{
		return false;
	}
	memcpy(psDest, &psNode->sPlace, sizeof(SIDELocation));
	psDest->szFileName.Init(psNode->sPlace.szFileName);
	return true;
}


bool CIDEHierarchy::CanRerunResults(CIDEResultsPane* pcPane)
{
	Ptr<CIDEHierarchySearch>	pSearch;

	pSearch = mpTabs.IsNotNull() ? Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->FindSearch(pcPane)) : Ptr<CIDEHierarchySearch>();
	return pSearch.IsNotNull() && pSearch->mbShown;
}


void CIDEHierarchy::RerunResults(CIDEResultsPane* pcPane)
{
	Ptr<CIDEHierarchySearch>	pSearch;

	pSearch = mpTabs.IsNotNull() ? Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->FindSearch(pcPane)) : Ptr<CIDEHierarchySearch>();
	if (pSearch.IsNotNull() && pSearch->mbShown)
	{
		mpcContent->GetDocuments()->UpdateEditedTabsNow();
		Build(pSearch);
		ShowTree(pSearch, (int64)pSearch->miSubject + 1);
	}
}


Ptr<CDockPane> CIDEHierarchy::GetSteppablePane(void) const { return mpTool; }
const char* CIDEHierarchy::GetSteppableName(void) { return "Hierarchy"; }
bool CIDEHierarchy::HasSteps(void) { return GetSearch().IsNotNull() && GetSearch()->mbShown && (GetSearch()->masNodes.NumElements() > 0); }
bool CIDEHierarchy::Step(bool bNext, bool bKeepKeys) { return GetSearch()->mpPane->Step(bNext, bKeepKeys); }


Ptr<CDockPane> CIDEHierarchy::GetTool(void) const { return mpTool; }
Ptr<CIDEResultsTabs> CIDEHierarchy::GetTabs(void) const { return mpTabs; }
Ptr<CIDEHierarchySearch> CIDEHierarchy::GetSearch(void) { return mpTabs.IsNotNull() ? Ptr<CIDEHierarchySearch>((CIDEHierarchySearch*)&mpTabs->GetSearch()) : Ptr<CIDEHierarchySearch>(); }
Ptr<CIDEResultsPane> CIDEHierarchy::GetResultsPane(void) { return GetSearch().IsNotNull() ? GetSearch()->mpPane : Ptr<CIDEResultsPane>(); }


//////////////////////////////////////////////////////////////////////////
//
// For checking: the row named szName (as it is shown, without what is
// said after it) is opened, as its chevron's press would, and what that
// asks for is done.  False if there is no such row.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEHierarchy::ExpandRow(const char* szName)
{
	Ptr<CIDEHierarchySearch>	pSearch;
	Ptr<CTreeContainer>			pTree;
	int32						iNode;
	int64						iData;

	pSearch = GetSearch();
	if (pSearch.IsNull())
	{
		return false;
	}
	pTree = pSearch->mpPane->GetTree();
	for (iNode = 0; iNode < pTree->NumNodes(); iNode++)
	{
		iData = pTree->GetNodeData(iNode);
		if ((iData > 0) && ((size)iData <= pSearch->masNodes.NumElements()) && pSearch->masNodes.Get((size)iData - 1)->szName.Equals(szName) && !pTree->IsExpanded(iNode))
		{
			pTree->SetExpanded(iNode, true);
			ExpandPending();
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Hierarchy: Type: CShape (class hierarchy)" and the
// tree's rows that can be seen, each indented by its depth, the selected
// one after a '>', a closed one that has rows after a '+'.
//
//////////////////////////////////////////////////////////////////////////
void CIDEHierarchy::Print(CChars* pszDest)
{
	static const char*			aszModes[] = { "class hierarchy", "supertypes", "subtypes", "callers", "callees" };
	Ptr<CIDEHierarchySearch>	pSearch;
	Ptr<CTreeContainer>			pTree;
	CChars						szNode;
	int32						iRow;
	int32						iNode;
	int32						iDepth;

	pSearch = GetSearch();
	pszDest->Append("Hierarchy: ");
	if (pSearch.IsNull() || !pSearch->mbShown)
	{
		pszDest->Append("none\n");
		return;
	}
	pszDest->Append(pSearch->mszName);
	pszDest->Append(" (");
	pszDest->Append(aszModes[pSearch->meMode]);
	pszDest->Append(")\n");
	pTree = pSearch->mpPane->GetTree();
	pTree->UpdateRows();
	for (iRow = 0; iRow < pTree->NumRows(); iRow++)
	{
		iNode = pTree->GetRowNode(iRow);
		pszDest->Append((iNode == pTree->GetSelectedNode()) ? " >" : "  ");
		for (iDepth = pTree->GetNodeDepth(iNode); iDepth > 0; iDepth--)
		{
			pszDest->Append("  ");
		}
		if (pTree->HasChildren(iNode) && !pTree->IsExpanded(iNode))
		{
			pszDest->Append("+ ");
		}
		szNode.Init();
		pTree->GetNodeText(iNode, &szNode);
		pszDest->Append(szNode);
		szNode.Kill();
		pszDest->Append('\n');
	}
}

