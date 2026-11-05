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
#include "StandardLib/ClassDefines.h"
#include "SolutionExplorer.h"
#include "IDEDiffTab.h"


#define IDE_DIFF_REFRESH_DELAY	400		//Milliseconds after the last edit before the diff is made again.


//////////////////////////////////////////////////////////////////////////
//
// szRevision NULL or empty is the file as it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffSide::Init(const char* szFile, const char* szRevision, size uiProject)
{
	mszFile.Init(szFile);
	mszRevision.Init((szRevision != NULL) ? szRevision : "");
	mszHead.Init();
	muiProject = uiProject;
	muiSeenChanges = IDE_DIFF_NO_CHANGES;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffSide::Kill(void)
{
	mszHead.Kill();
	mszRevision.Kill();
	mszFile.Kill();
}


bool CIDEDiffSide::IsWorkingCopy(void) { return mszRevision.Empty(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffTab::Init(Ptr<CDiffView> pView, const char* szLeftFile, const char* szLeftRevision, size uiLeftProject, const char* szRightFile, const char* szRightRevision, size uiRightProject)
{
	PreInit();

	mpView = pView;
	mcLeft.Init(szLeftFile, szLeftRevision, uiLeftProject);
	mcRight.Init(szRightFile, szRightRevision, uiRightProject);
	miChangedTime = 0;
	mbRefresh = false;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffTab::Class(void)
{
	M_Pointer(mpView);
	M_Pointer(mpDockPane);
	U_Unknown(CIDEDiffSide, mcLeft);
	U_Unknown(CIDEDiffSide, mcRight);
	U_Int64(miChangedTime);
	U_Bool(mbRefresh);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffTab::Free(void)
{
	mcRight.Kill();
	mcLeft.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A diff tab isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffTab::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffTab::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A side's editor changed at iNow: the diff is made again once it has
// been left alone for a moment.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffTab::Changed(int64 iNow)
{
	miChangedTime = iNow;
	mbRefresh = true;
}


//////////////////////////////////////////////////////////////////////////
//
// True if a side changed long enough ago to show it now.  Otherwise
// piWait is how many milliseconds until it might be, or -1 for never.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffTab::NeedsRefresh(int64 iNow, int64* piWait)
{
	*piWait = -1;
	if (!mbRefresh)
	{
		return false;
	}
	if (iNow - miChangedTime < IDE_DIFF_REFRESH_DELAY)
	{
		*piWait = IDE_DIFF_REFRESH_DELAY - (iNow - miChangedTime);
		return false;
	}
	return true;
}


void CIDEDiffTab::Refreshed(void) { mbRefresh = false; }
void CIDEDiffTab::SetDockPane(Ptr<CDockPane> pPane) { mpDockPane = pPane; }
Ptr<CDockPane> CIDEDiffTab::GetDockPane(void) { return mpDockPane; }
Ptr<CDiffView> CIDEDiffTab::GetView(void) { return mpView; }
CIDEDiffSide* CIDEDiffTab::GetSide(ETextDiffSide eSide) { return (eSide == TDS_Left) ? &mcLeft : &mcRight; }

