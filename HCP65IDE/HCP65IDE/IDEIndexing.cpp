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
#include "BaseLib/ArrayChars.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEIndexing.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIndexing::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	muiShown = 0;
	mbFinished = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIndexing::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpPane);
	M_Pointer(mpTool);
	U_Size(muiShown);
	U_Bool(mbFinished);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIndexing::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// The tab isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIndexing::Save(CObjectWriter* pcFile) { return false; }
bool CIDEIndexing::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Indexing tab, put at psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIndexing::CreatePane(SDockPlace* psPlace)
{
	mpPane = OMalloc<CIDEBuildPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetCodeFamily(), mpcContent->GetTheme()->GetColour(TC_Text));
	mpTool = mpcContent->GetDock()->AddPane(IDE_INDEXING_ID, "Indexing", NULL, mpPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Each tick of the status line: an indexing the code model has started
// since is shown from its start, then what its units couldn't include as
// they find it, its progress, and when it is done how long it took.  True
// while it is being built, so it is looked at again soon.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIndexing::Update(void)
{
	CIDECodeModel*	pcModel;
	CArrayChars		aszLines;
	CChars			szLine;
	size			uiIndexed;
	size			uiToIndex;
	size			uiProblemUnits;
	int64			iMilliseconds;
	ARGB32			sColour;

	pcModel = mpcContent->GetCodeModel();
	pcModel->GetIndexingProgress(&uiIndexed, &uiToIndex);
	aszLines.Init();
	if (pcModel->NumIndexings() != muiShown)
	{
		//A new indexing: what the last wrote goes.
		muiShown = pcModel->NumIndexings();
		mbFinished = false;
		mpPane->Clear();
		szLine.Init("Indexing ");
		szLine.Append((uint32)uiToIndex);
		szLine.Append(uiToIndex == 1 ? " file" : " files");
		szLine.Append(" (");
		szLine.Append(pcModel->GetConfiguration());
		szLine.Append(")");
		aszLines.Add(szLine.Text());
		szLine.Kill();
	}
	if ((muiShown == 0) || mbFinished)
	{
		aszLines.Kill();
		return false;
	}

	pcModel->TakeIndexingProblems(&aszLines);
	iMilliseconds = pcModel->GetIndexingMilliseconds();
	if (iMilliseconds < 0)
	{
		mpPane->AppendLines(&aszLines);
		mpPane->SetProgress((uiToIndex > 0) ? ((float)uiIndexed / (float)uiToIndex) : 0.0f, mpcContent->GetTheme()->GetColour(TC_Accent));
		aszLines.Kill();
		return true;
	}

	//Done: how long it took, and green, or red if some units couldn't be read whole.
	mbFinished = true;
	uiProblemUnits = pcModel->NumIndexingProblemUnits();
	szLine.Init("Indexed ");
	szLine.Append((uint32)uiToIndex);
	szLine.Append(uiToIndex == 1 ? " file in " : " files in ");
	szLine.Append((uint32)(iMilliseconds / 1000));
	szLine.Append('.');
	szLine.Append((uint32)((iMilliseconds % 1000) / 100));
	szLine.Append(" s");
	if (uiProblemUnits > 0)
	{
		szLine.Append("; ");
		szLine.Append((uint32)uiProblemUnits);
		szLine.Append(" had errors (above), and the rest of each is indexed");
	}
	szLine.Append('.');
	aszLines.Add(szLine.Text());
	szLine.Kill();
	mpPane->AppendLines(&aszLines);
	sColour = mpcContent->GetTheme()->GetColour((uiProblemUnits > 0) ? TC_Failure : TC_Success);
	mpPane->SetProgress(1.0f, sColour);
	aszLines.Kill();
	return false;
}


Ptr<CIDEBuildPane> CIDEIndexing::GetPane(void) { return mpPane; }
Ptr<CDockPane> CIDEIndexing::GetTool(void) { return mpTool; }

