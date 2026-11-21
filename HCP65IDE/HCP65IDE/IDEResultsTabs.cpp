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
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDEResultsTabs.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsSearch::Init(Ptr<CIDEResultsPane> pPane)
{
	PreInit();
	InitSearch(pPane);
	PostInit();
}


void CIDEResultsSearch::InitSearch(Ptr<CIDEResultsPane> pPane)
{
	mpPane = pPane;
	mszName.Init();
	mbPinned = false;
}


void CIDEResultsSearch::Class(void)
{
	M_Pointer(mpPane);
	M_Pointer(mpTab);
	U_String(mszName);
	U_Bool(mbPinned);
}


void CIDEResultsSearch::Free(void)
{
	mszName.Kill();
}


bool CIDEResultsSearch::Save(CObjectWriter* pcFile) { return false; }
bool CIDEResultsSearch::Load(CObjectReader* pcFile) { return false; }


void CIDEResultsTabsOwner::ResultsSearchChosen(Ptr<CIDEResultsSearch> pSearch) {}


//////////////////////////////////////////////////////////////////////////
//
// The tabs, with none: the owner adds the first (AddSearch) once it can
// make one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::Init(CIDEContent* pcContent, CIDEResultsTabsOwner* pcOwner, const char* szEmptyName)
{
	PreInit();
	mpcContent = pcContent;
	mpcOwner = pcOwner;
	maSearches.Init();
	mszEmptyName.Init(szEmptyName);
	mpTabs = OMalloc<CTabbedContainer>(pcContent->GetWindow(), pcContent->GetTheme());
	mpTabs->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpTabs->SetHideSingleTab(true);
	mpTabs->GetSelectionEvent()->Add(this, &CIDEResultsTabs::TabSelected);
	mpTabs->GetCloseRequestEvent()->Add(this, &CIDEResultsTabs::TabCloseRequested);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::Class(void)
{
	U_Pointer(mpcContent);
	U_Pointer(mpcOwner);
	M_Pointer(mpTabs);
	M_Embedded(maSearches);
	M_Pointer(mpSearch);
	U_String(mszEmptyName);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::Free(void)
{
	if (mpTabs.IsNotNull())
	{
		mpTabs->GetSelectionEvent()->Remove(this);
		mpTabs->GetCloseRequestEvent()->Remove(this);
	}
	mszEmptyName.Kill();
}


bool CIDEResultsTabs::Save(CObjectWriter* pcFile) { return false; }
bool CIDEResultsTabs::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// A new, empty search in a tab of its own, which is chosen: its owner
// makes it, and its pane has the settings of the pane chosen before it.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEResultsSearch> CIDEResultsTabs::AddSearch(void)
{
	Ptr<CIDEResultsSearch>	pSearch;
	Ptr<CIDEResultsPane>	pLast;

	pLast = mpSearch.IsNotNull() ? mpSearch->mpPane : Ptr<CIDEResultsPane>();
	pSearch = mpcOwner->CreateResultsSearch();
	maSearches.Add(pSearch);
	mpSearch = pSearch;
	if (pLast.IsNotNull())
	{
		pSearch->mpPane->CopySettings(pLast);
	}

	//The tab is chosen without TabSelected being told: the search is the chosen one already.
	mpTabs->GetSelectionEvent()->Remove(this);
	pSearch->mpTab = mpTabs->AddTab(mszEmptyName.Text(), pSearch->mpPane->GetComponent(), Ptr<CImageCel>(), true);
	mpTabs->Select(pSearch->mpTab);
	mpTabs->GetSelectionEvent()->Add(this, &CIDEResultsTabs::TabSelected);
	return pSearch;
}


//////////////////////////////////////////////////////////////////////////
//
// A search and its tab go.  With bKeepOne, as closing a tab does, the
// last to go is replaced by an empty one, so the tool window always has
// a tab; the tab left of it (or the first) is chosen, and the owner told
// which is chosen now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::RemoveSearch(Ptr<CIDEResultsSearch> pSearch, bool bKeepOne)
{
	size	uiIndex;
	bool	bChosen;

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
	bChosen = false;
	if (mpSearch == pSearch)
	{
		mpSearch = NULL;
		if (maSearches.NumElements() > 0)
		{
			mpSearch = maSearches.Get((uiIndex > 0) ? uiIndex - 1 : 0);
			mpTabs->Select(mpSearch->mpTab);
			bChosen = true;
		}
	}
	mpTabs->GetSelectionEvent()->Add(this, &CIDEResultsTabs::TabSelected);
	if (mpSearch.IsNull() && bKeepOne)
	{
		//The empty one that takes its place is the chosen one: an owner that keeps the chosen search hears of it.
		AddSearch();
		mpcOwner->ResultsSearchChosen(mpSearch);
	}
	else if (bChosen)
	{
		mpcOwner->ResultsSearchChosen(mpSearch);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The search a new find is shown in: the chosen tab's, or a new tab's if
// that one is pinned.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEResultsSearch> CIDEResultsTabs::GetSearchForNew(void)
{
	if (mpSearch.IsNull() || mpSearch->mbPinned)
	{
		return AddSearch();
	}
	return mpSearch;
}


//////////////////////////////////////////////////////////////////////////
//
// The search whose pane or tab it is, NULL if none's.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEResultsSearch> CIDEResultsTabs::FindSearch(CIDEResultsPane* pcPane)
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


Ptr<CIDEResultsSearch> CIDEResultsTabs::FindSearch(CTabComponent* pcTab)
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
// Another tab was chosen: its search is the chosen one, and its owner is
// told, to make its tree again if it is behind.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::TabSelected(CTabComponent* pcTab)
{
	Ptr<CIDEResultsSearch>	pSearch;

	pSearch = FindSearch(mpTabs->GetSelected().IsNotNull() ? &mpTabs->GetSelected() : pcTab);
	if (pSearch.IsNull() || (pSearch == mpSearch))
	{
		return;
	}
	mpSearch = pSearch;
	mpcOwner->ResultsSearchChosen(mpSearch);
	mpSearch->mpPane->RefreshToolbar();
}


//////////////////////////////////////////////////////////////////////////
//
// The search's tab chosen, as if clicked.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::Select(Ptr<CIDEResultsSearch> pSearch)
{
	if (pSearch.IsNotNull() && (pSearch != mpSearch))
	{
		mpTabs->Select(pSearch->mpTab);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A tab's cross was pressed: its search goes, pinned or not.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::TabCloseRequested(CTabComponent* pcTab)
{
	Ptr<CIDEResultsSearch>	pSearch;

	pSearch = FindSearch(pcTab);
	if (pSearch.IsNotNull())
	{
		RemoveSearch(pSearch, true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A search's name, which its tab shows: with a pin before it if it is
// pinned, and the tabs' own name while it has none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::SetName(Ptr<CIDEResultsSearch> pSearch, const char* szName)
{
	pSearch->mszName.Set(szName);
	ShowName(pSearch);
}


void CIDEResultsTabs::ShowName(Ptr<CIDEResultsSearch> pSearch)
{
	CChars	szName;

	szName.Init(pSearch->mbPinned ? IDE_RESULTS_PINNED_PREFIX : "");
	szName.Append(pSearch->mszName.Empty() ? mszEmptyName.Text() : pSearch->mszName.Text());
	pSearch->mpTab->SetText(szName.Text());
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Pin Tab, as IntelliJ's: the next find leaves a pinned tab as it is and
// shows what it finds in a new one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::SetPinned(bool bPinned)
{
	if (mpSearch.IsNull())
	{
		return;
	}
	mpSearch->mbPinned = bPinned;
	ShowName(mpSearch);
	mpSearch->mpPane->RefreshToolbar();
}


void CIDEResultsTabs::TogglePinned(void) { SetPinned(!IsPinned()); }
bool CIDEResultsTabs::IsPinned(void) { return mpSearch.IsNotNull() && mpSearch->mbPinned; }


//////////////////////////////////////////////////////////////////////////
//
// Pin Tab on a pane's toolbar: on while the chosen tab is pinned.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::AddPinCommand(Ptr<CIDEResultsPane> pPane)
{
	Ptr<CCommand>	pCommand;

	pCommand = pPane->GetCommands()->Add("Results.PinTab", "Pin Tab", "", mpcContent->GetToolIcon(ITI_Pin), this, &CIDEResultsTabs::TogglePinned);
	pCommand->SetCheckedCheck(this, &CIDEResultsTabs::IsPinned);
	pPane->AddToolbarCommand(pCommand, ITI_Pin);
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE is closing: each pane lets go of what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsTabs::Detach(void)
{
	size	i;

	for (i = 0; i < maSearches.NumElements(); i++)
	{
		maSearches.Get(i)->mpPane->Detach();
	}
}


Ptr<CIDEResultsSearch> CIDEResultsTabs::GetSearch(void) const { return mpSearch; }
size CIDEResultsTabs::NumSearches(void) { return maSearches.NumElements(); }
Ptr<CIDEResultsSearch> CIDEResultsTabs::GetSearch(size uiIndex) { return maSearches.Get(uiIndex); }
Ptr<CTabbedContainer> CIDEResultsTabs::GetTabs(void) const { return mpTabs; }
Ptr<CComponent> CIDEResultsTabs::GetComponent(void) const { return mpTabs; }

