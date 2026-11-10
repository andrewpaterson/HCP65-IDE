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
#include "BaseLib/Chars.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEOccurrences.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOccurrences::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOccurrences::Class(void)
{
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOccurrences::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEOccurrences::Save(CObjectWriter* pcFile) { return false; }
bool CIDEOccurrences::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Next and Previous Occurrence, as IntelliJ's, while there is somewhere
// they could step from: a steppable tool window or a source editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOccurrences::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.NextOccurrence", "Next Occurrence", "Control+Alt+Cursor Down", Ptr<CImageCel>(), this, &CIDEOccurrences::NextOccurrence)->SetEnabledCheck(this, &CIDEOccurrences::CanStep);
	pCommands->Add("Navigate.PreviousOccurrence", "Previous Occurrence", "Control+Alt+Cursor Up", Ptr<CImageCel>(), this, &CIDEOccurrences::PreviousOccurrence)->SetEnabledCheck(this, &CIDEOccurrences::CanStep);
}


void CIDEOccurrences::NextOccurrence(void) { Step(true); }
void CIDEOccurrences::PreviousOccurrence(void) { Step(false); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOccurrences::Step(bool bNext)
{
	CIDESteppable*	pcSteppable;
	bool			bKeepKeys;

	pcSteppable = FindSteppable(&bKeepKeys, true);
	if (pcSteppable)
	{
		pcSteppable->Step(bNext, bKeepKeys);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The tool window that steps: the steppable one with the keys (which it
// keeps, *pbKeepKeys), or, from a source editor, the one steppable tool
// window that can be seen and has something to step to.  NULL if there
// isn't one; with bSay the status line says why.
//
//////////////////////////////////////////////////////////////////////////
CIDESteppable* CIDEOccurrences::FindSteppable(bool* pbKeepKeys, bool bSay)
{
	CIDESteppable*	apcSteppables[IDE_NUM_STEPPABLES];
	CIDESteppable*	pcFound;
	size			uiSteppables;
	size			uiFound;
	size			i;
	CChars			szNames;

	pcFound = FindFocussedSteppable();
	*pbKeepKeys = (pcFound != NULL);
	if (pcFound || !IsInSourceEditor(mpcContent->GetWindow()->GetFocus()->GetFocussedComponent()))
	{
		return pcFound;
	}

	uiSteppables = GetSteppables(apcSteppables);
	uiFound = 0;
	szNames.Init();
	for (i = 0; i < uiSteppables; i++)
	{
		if (mpcContent->GetDock()->IsPaneVisible(apcSteppables[i]->GetSteppablePane()) && apcSteppables[i]->HasSteps())
		{
			if (uiFound > 0)
			{
				szNames.Append(" and ");
			}
			szNames.Append(apcSteppables[i]->GetSteppableName());
			pcFound = apcSteppables[i];
			uiFound++;
		}
	}
	if (bSay && (uiFound == 0))
	{
		mpcContent->ShowStatus("Nothing to step through: no tool window that can be seen has anything to go to.");
	}
	else if (bSay && (uiFound > 1))
	{
		szNames.Append(" can both step: click in the one to step through.");
		mpcContent->ShowStatus(szNames.Text());
	}
	szNames.Kill();
	return (uiFound == 1) ? pcFound : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The steppable tool window the keys are in, NULL if they are in none.
//
//////////////////////////////////////////////////////////////////////////
CIDESteppable* CIDEOccurrences::FindFocussedSteppable(void)
{
	CIDESteppable*	apcSteppables[IDE_NUM_STEPPABLES];
	Ptr<CDockPane>	pPane;
	size			uiSteppables;
	size			i;

	pPane = mpcContent->GetDock()->FindPane(mpcContent->GetWindow()->GetFocus()->GetFocussedComponent());
	uiSteppables = GetSteppables(apcSteppables);
	for (i = 0; i < uiSteppables; i++)
	{
		if (pPane.IsNotNull() && (apcSteppables[i]->GetSteppablePane() == pPane))
		{
			return apcSteppables[i];
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the keys are in a steppable tool window or a source editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEOccurrences::CanStep(void)
{
	return (FindFocussedSteppable() != NULL) || IsInSourceEditor(mpcContent->GetWindow()->GetFocus()->GetFocussedComponent());
}


//////////////////////////////////////////////////////////////////////////
//
// The tool windows that can be stepped through, in the order the status
// line names them.
//
//////////////////////////////////////////////////////////////////////////
size CIDEOccurrences::GetSteppables(CIDESteppable** apcDest)
{
	Ptr<CIDEUsages>		pUsages;
	Ptr<CIDEBuilding>	pBuilding;

	pUsages = mpcContent->GetUsages();
	pBuilding = mpcContent->GetBuilding();
	apcDest[0] = &pUsages;
	apcDest[1] = &pBuilding;
	return IDE_NUM_STEPPABLES;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pComponent is an open file's editor (not a diff, a tool
// window's text or a dialog's).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEOccurrences::IsInSourceEditor(Ptr<CComponent> pComponent)
{
	size	i;

	if (pComponent.IsNull())
	{
		return false;
	}
	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		if (pComponent == mpcContent->GetDocuments()->GetTab(i)->GetEditor())
		{
			return true;
		}
	}
	return false;
}
