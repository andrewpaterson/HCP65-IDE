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
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/DockGroup.h"
#include "WindowLib/DockManager.h"
#include "IDEContent.h"
#include "IDEToolWindows.h"


//A tool window's pane and the command that shows it, for View's keys.
struct SIDEToolWindowCommand
{
	const char*	szID;
	const char*	szCommand;
};


static SIDEToolWindowCommand	gasIDEToolWindowCommands[] =
{
	{ IDE_EXPLORER_ID,	"View.SolutionExplorer" },
	{ IDE_BOOKMARKS_ID,	"View.Bookmarks" },
	{ IDE_USAGES_ID,	"View.FindUsages" },
	{ IDE_RUN_ID,		"View.Run" },
	{ IDE_DEBUG_ID,		"View.Debug" },
	{ IDE_BUILD_ID,		"View.Build" },
	{ IDE_STRUCTURE_ID,	"View.Structure" },
	{ IDE_HIERARCHY_ID,	"View.Hierarchy" },
	{ IDE_FIND_ID,		"View.Find" },
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	masHidden.Init();
	mszLast.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayIDEHiddenToolWindows, masHidden);
	U_String(mszLast);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::Free(void)
{
	mszLast.Kill();
	ForgetHidden();
	masHidden.Kill();
}


bool CIDEToolWindows::Save(CObjectWriter* pcFile) { return false; }
bool CIDEToolWindows::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// View's, with IntelliJ IDEA's keys where it has the window: its Alt+3 is
// Find (here Find Usages, as its Find tool window is what Find Usages
// fills), its Alt+6 Problems (here Build, which lists them) and its Alt+9
// Git (here Find in Files' window).
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("View.Bookmarks", "Bookmarks", "Alt+2", Ptr<CImageCel>(), this, &CIDEToolWindows::ToggleBookmarks);
	pCommands->Add("View.FindUsages", "Find Usages", "Alt+3", Ptr<CImageCel>(), this, &CIDEToolWindows::ToggleUsages);
	pCommands->Add("View.Run", "Run", "Alt+4", Ptr<CImageCel>(), this, &CIDEToolWindows::ToggleRun);
	pCommands->Add("View.Debug", "Debug", "Alt+5", Ptr<CImageCel>(), this, &CIDEToolWindows::ToggleDebug);
	pCommands->Add("View.Build", "Build", "Alt+6", Ptr<CImageCel>(), this, &CIDEToolWindows::ToggleBuild);
	pCommands->Add("View.Find", "Find", "Alt+9", Ptr<CImageCel>(), this, &CIDEToolWindows::ToggleFind);
	pCommands->Add("View.HideAllToolWindows", "Hide All Tool Windows", "Control+Shift+F12", Ptr<CImageCel>(), this, &CIDEToolWindows::HideAll);
	pCommands->Add("View.JumpToLastToolWindow", "Jump to Last Tool Window", "", Ptr<CImageCel>(), this, &CIDEToolWindows::JumpToLast)->SetEnabledCheck(this, &CIDEToolWindows::HasLast);
}


bool CIDEToolWindows::ToggleBookmarks(void) { return Toggle(IDE_BOOKMARKS_ID); }
bool CIDEToolWindows::ToggleUsages(void) { return Toggle(IDE_USAGES_ID); }
bool CIDEToolWindows::ToggleRun(void) { return Toggle(IDE_RUN_ID); }
bool CIDEToolWindows::ToggleDebug(void) { return Toggle(IDE_DEBUG_ID); }
bool CIDEToolWindows::ToggleBuild(void) { return Toggle(IDE_BUILD_ID); }
bool CIDEToolWindows::ToggleFind(void) { return Toggle(IDE_FIND_ID); }


//////////////////////////////////////////////////////////////////////////
//
// After View's tool windows: the two commands for them all.
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::AddViewItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.HideAllToolWindows"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.JumpToLastToolWindow"));
}


//////////////////////////////////////////////////////////////////////////
//
// The keys of the command that shows the pane, as the keymap has them
// now; NULL if it has no command or the command no keys.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEToolWindows::GetPaneKeys(const char* szID)
{
	Ptr<CCommand>	pCommand;
	size			i;

	for (i = 0; i < sizeof(gasIDEToolWindowCommands) / sizeof(gasIDEToolWindowCommands[0]); i++)
	{
		if (strcmp(gasIDEToolWindowCommands[i].szID, szID) == 0)
		{
			pCommand = mpcContent->GetCommands()->Find(gasIDEToolWindowCommands[i].szCommand);
			if (pCommand.IsNotNull() && (pCommand->GetKeys() != NULL) && (pCommand->GetKeys()[0] != '\0'))
			{
				return pCommand->GetKeys();
			}
			return NULL;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the pane is in front and the keys are in it.  A floating pane's
// window has its own keys: it has them while it is the active pane.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEToolWindows::HasKeys(Ptr<CDockPane> pPane)
{
	Ptr<CComponent>	pFocussed;

	if (pPane.IsNull() || !mpcContent->GetDock()->IsPaneVisible(pPane))
	{
		return false;
	}
	if (pPane->GetState() == DPS_Floating)
	{
		return mpcContent->GetDock()->GetActivePane() == pPane;
	}
	pFocussed = mpcContent->GetWindow()->GetFocus()->GetFocussedComponent();
	return pFocussed.IsNotNull() && pFocussed->IsWithin(pPane->GetContent());
}


//////////////////////////////////////////////////////////////////////////
//
// A tool window's key: the window is shown and has the keys, or, if it
// had them, is hidden (see HideWithKeys).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEToolWindows::Toggle(const char* szID)
{
	Ptr<CDockPane>	pPane;

	pPane = mpcContent->GetDock()->GetPane(szID);
	if (pPane.IsNull())
	{
		return false;
	}
	if (!HideWithKeys(szID))
	{
		mpcContent->GetDock()->Activate(pPane);
		Poll();
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// If the pane has the keys it is hidden (one slid out from the edge slides
// back, and stays on its edge) and the active editor has the keys.  False
// if it hasn't them: the caller shows it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEToolWindows::HideWithKeys(const char* szID)
{
	Ptr<CDockPane>	pPane;

	Poll();
	pPane = mpcContent->GetDock()->GetPane(szID);
	if (!HasKeys(pPane))
	{
		return false;
	}
	if (pPane->GetState() == DPS_AutoHidden)
	{
		mpcContent->GetDock()->CloseFlyout();
	}
	else
	{
		mpcContent->GetDock()->Hide(pPane);
	}
	mpcContent->GetDocuments()->FocusActiveEditor();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Hide All Tool Windows: every docked tool pane is hidden, a group's last
// tab first so each remembers its own place among the tabs, and a pane
// slid out from the edge slides back.  With none docked, the ones it hid
// come back, the last hidden first, and then the tabs that were in front
// are in front again.  Each goes back to where it was docked when it was
// hidden, whatever it has been shown as since.  The editor has the keys
// after both.
// False, the status line saying so, if there is nothing to hide or to put
// back.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEToolWindows::HideAll(void)
{
	CDockManager*			pcDock;
	Ptr<CDockPane>			pPane;
	Ptr<CDockPane>			pLast;
	CArray<CDockPane>		apDocked;
	CArrayChars				aszFront;
	SIDEHiddenToolWindow*	psHidden;
	size					uiIndex;
	size					uiLast;
	size					i;
	size					j;

	Poll();
	pcDock = &mpcContent->GetDock();
	apDocked.Init();
	for (i = 0; i < pcDock->NumPanes(); i++)
	{
		pPane = pcDock->GetPane(i);
		if (pPane->IsTool() && (pPane->GetState() == DPS_Docked) && pPane->GetGroup().IsNotNull())
		{
			apDocked.Add(pPane);
		}
	}

	if (apDocked.NumElements() > 0)
	{
		//What was hidden before and not put back is still to be put back: these are added to it, and the tabs in front
		//then stay the ones to be in front.  The tabs in front now are found before any is hidden, as a group brings
		//another to the front when its own goes.
		aszFront.Init();
		for (i = 0; (masHidden.NumElements() == 0) && (i < apDocked.NumElements()); i++)
		{
			pPane = apDocked.Get(i);
			if (pPane->GetGroup()->GetSelectedPane() == pPane)
			{
				aszFront.Add(pPane->GetID());
			}
		}

		//Group by group, as the dock has their first panes; a group's panes from its last tab to its first.
		while (apDocked.NumElements() > 0)
		{
			pLast = apDocked.Get(0);
			uiLast = 0;
			for (j = 1; j < apDocked.NumElements(); j++)
			{
				pPane = apDocked.Get(j);
				if (pPane->GetGroup() == apDocked.Get(0)->GetGroup())
				{
					uiIndex = pPane->GetGroup()->IndexOfPane(pPane);
					if (uiIndex > pLast->GetGroup()->IndexOfPane(pLast))
					{
						pLast = pPane;
						uiLast = j;
					}
				}
			}
			apDocked.RemoveAt(uiLast);
			pcDock->Hide(pLast);
			if (!IsHidden(pLast->GetID()))
			{
				psHidden = masHidden.Add();
				psHidden->szID.Init(pLast->GetID());
				psHidden->sRestore.Init();
				psHidden->sRestore.Copy(pLast->GetRestore());
				psHidden->bFront = aszFront.GetIndex(pLast->GetID()) != ARRAY_ELEMENT_NOT_FOUND;
			}
		}
		pcDock->CloseFlyout();
		aszFront.Kill();
		apDocked.Kill();
		mpcContent->GetDocuments()->FocusActiveEditor();
		return true;
	}
	apDocked.Kill();

	if (masHidden.NumElements() == 0)
	{
		mpcContent->ShowStatus("No tool windows are docked");
		return false;
	}
	for (i = masHidden.NumElements(); i > 0; i--)
	{
		psHidden = masHidden.Get(i - 1);
		pPane = pcDock->GetPane(psHidden->szID.Text());
		if (pPane.IsNotNull() && (pPane->GetState() == DPS_Hidden))
		{
			pPane->SetRestore(&psHidden->sRestore);
			pcDock->Show(pPane);
		}
	}
	for (i = 0; i < masHidden.NumElements(); i++)
	{
		psHidden = masHidden.Get(i);
		pPane = pcDock->GetPane(psHidden->szID.Text());
		if (psHidden->bFront && pPane.IsNotNull() && (pPane->GetState() == DPS_Docked))
		{
			pcDock->Show(pPane);
		}
	}
	ForgetHidden();
	mpcContent->GetDocuments()->FocusActiveEditor();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Hide All hid the pane and hasn't put it back.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEToolWindows::IsHidden(const char* szID)
{
	size	i;

	for (i = 0; i < masHidden.NumElements(); i++)
	{
		if (masHidden.Get(i)->szID.Equals(szID))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::ForgetHidden(void)
{
	size	i;

	for (i = 0; i < masHidden.NumElements(); i++)
	{
		masHidden.Get(i)->szID.Kill();
		masHidden.Get(i)->sRestore.Kill();
	}
	masHidden.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Jump to Last Tool Window: the tool pane that last had the keys is shown
// and has them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEToolWindows::JumpToLast(void)
{
	Ptr<CDockPane>	pPane;

	Poll();
	pPane = mpcContent->GetDock()->GetPane(mszLast.Text());
	if (mszLast.Empty() || pPane.IsNull())
	{
		return false;
	}
	mpcContent->GetDock()->Activate(pPane);
	return true;
}


bool CIDEToolWindows::HasLast(void) { return !mszLast.Empty(); }


//////////////////////////////////////////////////////////////////////////
//
// The tool pane that has the keys now is the last to have had them.  As
// the content ticks, and before anything here that asks or that moves the
// keys: a pane hidden takes the keys from itself first, and the dock's
// active pane is, for a moment, the tab that came to the front after it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::Poll(void)
{
	Ptr<CDockPane>	pPane;

	pPane = mpcContent->GetDock()->GetActivePane();
	if (pPane.IsNotNull() && pPane->IsTool() && !mszLast.Equals(pPane->GetID()) && HasKeys(pPane))
	{
		mszLast.Set(pPane->GetID());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// For the checks: "Tool windows: Solution Explorer (front), Structure,
// Find Usages (front, keys), Bookmarks (hidden)" in the dock's order, a
// docked pane's tab number after it where its group has more than one,
// then each group's place and size, the last tool window, and what Hide
// All hid.
//
//////////////////////////////////////////////////////////////////////////
void CIDEToolWindows::Print(CChars* pszDest)
{
	CDockManager*	pcDock;
	Ptr<CDockPane>	pPane;
	Ptr<CDockPane>	pLast;
	Ptr<CDockGroup>	pGroup;
	SInt32Vec2		sPosition;
	SInt32Vec2		sSize;
	bool			bFirst;
	size			i;

	Poll();
	mpcContent->GetWindow()->Layout();
	pcDock = &mpcContent->GetDock();
	pszDest->Append("Tool windows:");
	bFirst = true;
	for (i = 0; i < pcDock->NumPanes(); i++)
	{
		pPane = pcDock->GetPane(i);
		if (pPane->IsTool())
		{
			pszDest->Append(bFirst ? " " : ", ");
			bFirst = false;
			pszDest->Append(pPane->GetTitle());
			pGroup = pPane->GetGroup();
			if ((pPane->GetState() == DPS_Docked) && pGroup.IsNotNull() && (pGroup->NumPanes() > 1))
			{
				pszDest->Append(' ');
				pszDest->Append((uint32)(pGroup->IndexOfPane(pPane) + 1));
			}
			if (pPane->GetState() == DPS_Hidden)
			{
				pszDest->Append(" (hidden)");
			}
			else if (pPane->GetState() == DPS_Floating)
			{
				pszDest->Append(HasKeys(pPane) ? " (floating, keys)" : " (floating)");
			}
			else if (pPane->GetState() == DPS_AutoHidden)
			{
				pszDest->Append(HasKeys(pPane) ? " (edge, out, keys)" : (pcDock->IsPaneVisible(pPane) ? " (edge, out)" : " (edge)"));
			}
			else if (HasKeys(pPane))
			{
				pszDest->Append(" (front, keys)");
			}
			else if (pcDock->IsPaneVisible(pPane))
			{
				pszDest->Append(" (front)");
			}
		}
	}
	pszDest->Append("\nTool window areas:");
	bFirst = true;
	for (i = 0; i < pcDock->NumPanes(); i++)
	{
		pPane = pcDock->GetPane(i);
		pGroup = pPane->GetGroup();
		if (pPane->IsTool() && (pPane->GetState() == DPS_Docked) && pGroup.IsNotNull() && (pGroup->GetSelectedPane() == pPane))
		{
			sPosition = pGroup->GetPosition();
			sSize = pGroup->GetActualSize();
			pszDest->Append(bFirst ? " " : ", ");
			bFirst = false;
			pszDest->Append(pPane->GetTitle());
			pszDest->Append(' ');
			pszDest->Append(sPosition.x);
			pszDest->Append(',');
			pszDest->Append(sPosition.y);
			pszDest->Append(' ');
			pszDest->Append(sSize.x);
			pszDest->Append('x');
			pszDest->Append(sSize.y);
		}
	}
	if (bFirst)
	{
		pszDest->Append(" none");
	}
	pszDest->Append("\nLast tool window: ");
	pLast = pcDock->GetPane(mszLast.Text());
	pszDest->Append((mszLast.Empty() || pLast.IsNull()) ? "none" : pLast->GetTitle());
	pszDest->Append("\nHidden by Hide All:");
	for (i = 0; i < masHidden.NumElements(); i++)
	{
		pPane = pcDock->GetPane(masHidden.Get(i)->szID.Text());
		pszDest->Append((i > 0) ? ", " : " ");
		pszDest->Append(pPane.IsNotNull() ? pPane->GetTitle() : masHidden.Get(i)->szID.Text());
	}
	if (masHidden.NumElements() == 0)
	{
		pszDest->Append(" none");
	}
	pszDest->Append('\n');
}
