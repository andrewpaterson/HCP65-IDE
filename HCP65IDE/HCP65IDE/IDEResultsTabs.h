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
#ifndef __IDE_RESULTS_TABS_H__
#define __IDE_RESULTS_TABS_H__
#include "StandardLib/Object.h"
#include "WindowLib/TabbedContainer.h"
#include "IDEResultsPane.h"


class CIDEContent;


#define IDE_RESULTS_PINNED_PREFIX	"* "	//Before a pinned tab's name.


//One search shown in a tab of a results tool window (CIDEResultsTabs): its results pane, its tab, its name and whether
//it is pinned.  What a tool window finds derives from it and adds what it found.  Its Init is InitSearch, called
//between the deriving class's PreInit and PostInit.
class CIDEResultsSearch : public CObject
{
CONSTRUCTABLE(CIDEResultsSearch);
DESTRUCTABLE(CIDEResultsSearch);
public:
	Ptr<CIDEResultsPane>	mpPane;
	Ptr<CTabComponent>		mpTab;
	CChars					mszName;  //Its tab's, without a pin before it; empty until it has found something.
	bool					mbPinned;

	void	Init(Ptr<CIDEResultsPane> pPane);
	void	InitSearch(Ptr<CIDEResultsPane> pPane);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;
};


//Whatever has a results tool window with tabs: it makes each tab's search, and hears when another is chosen.
class CIDEResultsTabsOwner
{
public:
	//A new search with its results pane, its toolbar's commands added, not yet in a tab.
	virtual Ptr<CIDEResultsSearch>	CreateResultsSearch(void) =0;

	//Another tab is the chosen one, by the user or because the chosen one was closed: its tree can be made again if
	//it is behind.
	virtual void					ResultsSearchChosen(Ptr<CIDEResultsSearch> pSearch);
};


//The tabs of a results tool window, as IntelliJ IDEA's Find tool window has them (NavigationPlan.txt's S12): a tab
//for each search, its page the search's results pane, closed by its cross.  A new search is shown in the chosen tab
//(GetSearchForNew) unless that is pinned (Pin Tab, a button AddPinCommand puts on a pane's toolbar): then it is left
//as it is and a new tab is made.  A pinned tab's name has IDE_RESULTS_PINNED_PREFIX before it.  The window always has
//a tab, empty until something is found, and its tab bar is only shown with more than one.  A new tab's pane has the
//settings of the one before it (the preview, its share).
class CIDEResultsTabs : public CObject
{
CONSTRUCTABLE(CIDEResultsTabs);
DESTRUCTABLE(CIDEResultsTabs);
protected:
	CIDEContent*				mpcContent;  //Not held.
	CIDEResultsTabsOwner*		mpcOwner;  //Not held: it holds this.
	Ptr<CTabbedContainer>		mpTabs;
	CArray<CIDEResultsSearch>	maSearches;
	Ptr<CIDEResultsSearch>		mpSearch;  //The chosen tab's.
	CChars						mszEmptyName;  //A tab's name until its search has one.

public:
	void	Init(CIDEContent* pcContent, CIDEResultsTabsOwner* pcOwner, const char* szEmptyName);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	Ptr<CIDEResultsSearch>	AddSearch(void);
	void					RemoveSearch(Ptr<CIDEResultsSearch> pSearch, bool bKeepOne);
	Ptr<CIDEResultsSearch>	GetSearch(void) const;
	Ptr<CIDEResultsSearch>	GetSearchForNew(void);
	Ptr<CIDEResultsSearch>	FindSearch(CIDEResultsPane* pcPane);
	Ptr<CIDEResultsSearch>	FindSearch(CTabComponent* pcTab);
	size					NumSearches(void);
	Ptr<CIDEResultsSearch>	GetSearch(size uiIndex);
	void					Select(Ptr<CIDEResultsSearch> pSearch);
	void					SetName(Ptr<CIDEResultsSearch> pSearch, const char* szName);
	void					SetPinned(bool bPinned);
	void					TogglePinned(void);
	bool					IsPinned(void);
	void					AddPinCommand(Ptr<CIDEResultsPane> pPane);
	void					Detach(void);
	Ptr<CTabbedContainer>	GetTabs(void) const;
	Ptr<CComponent>			GetComponent(void) const;

protected:
	void					TabSelected(CTabComponent* pcTab);
	void					TabCloseRequested(CTabComponent* pcTab);
	void					ShowName(Ptr<CIDEResultsSearch> pSearch);
};


#endif // __IDE_RESULTS_TABS_H__

