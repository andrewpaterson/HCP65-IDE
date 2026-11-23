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
#include "BaseLib/ArrayChars.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDEProblems.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszShownFile.Init();
	muiShownSet = 0;
	mbShownChecked = false;
	mbShownAny = false;
	mcShown.Init();
	muiShownBuild = 0;
	mbShownBuilding = false;
	maszProjectFiles.Init();
	mcProject.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpTabs);
	M_Pointer(mpFilePane);
	M_Pointer(mpProjectPane);
	M_Pointer(mpFileTab);
	M_Pointer(mpProjectTab);
	M_Pointer(mpTool);
	U_String(mszShownFile);
	U_Size(muiShownSet);
	U_Bool(mbShownChecked);
	U_Bool(mbShownAny);
	U_Unknown(CIDEProblemList, mcShown);
	U_Size(muiShownBuild);
	U_Bool(mbShownBuilding);
	U_Unknown(CArrayChars, maszProjectFiles);
	U_Unknown(CIDEProblemList, mcProject);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::Free(void)
{
	if (mpFilePane.IsNotNull())
	{
		mpFilePane->Detach();
	}
	if (mpProjectPane.IsNotNull())
	{
		mpProjectPane->Detach();
	}
	mcProject.Kill();
	maszProjectFiles.Kill();
	mcShown.Kill();
	mszShownFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab isn't saved: it is made from the editors and the build.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEProblems::Save(CObjectWriter* pcFile) { return false; }
bool CIDEProblems::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Problems tool window, put at psPlace: Current File's and Project's
// results panes, each with the view commands, in tabs.  Their previews
// are off to start with, as IntelliJ IDEA's are: a preview's file is
// coloured on the edits thread, which the check is busy on already.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::CreatePane(SDockPlace* psPlace)
{
	mpFilePane = OMalloc<CIDEResultsPane>(mpcContent, this, "problems", false);
	mpFilePane->AddViewCommands();
	mpFilePane->SetPreviewShown(false);
	mpProjectPane = OMalloc<CIDEResultsPane>(mpcContent, this, "problems", false);
	mpProjectPane->AddViewCommands();
	mpProjectPane->SetPreviewShown(false);
	mpTabs = OMalloc<CTabbedContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpTabs->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpFileTab = mpTabs->AddTab("Current File", mpFilePane->GetComponent());
	mpProjectTab = mpTabs->AddTab("Project", mpProjectPane->GetComponent());
	mpTabs->Select(mpFileTab);
	mpTool = mpcContent->GetDock()->AddPane(IDE_PROBLEMS_ID, "Problems", NULL, mpTabs, DPK_Tool, psPlace);
	Update();
}


//////////////////////////////////////////////////////////////////////////
//
// Each tick of the status line: the Current File tab shows the active
// editor's problems if they have changed (it is another file, or its
// check has found them again), and the Project tab the last build's
// once it has finished.  False: nothing needs looking at sooner.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEProblems::Update(void)
{
	if (mpTool.IsNull())
	{
		return false;
	}
	UpdateFile();
	UpdateProject();
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::UpdateFile(void)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDEProblem*		psProblem;
	const char*			szFileName;
	CChars				szNone;
	bool				bChecked;
	size				uiSet;
	size				i;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	szFileName = pTab.IsNotNull() ? pTab->GetFileName() : "";
	bChecked = pTab.IsNotNull() && pTab->HasProblems();
	uiSet = pTab.IsNotNull() ? pTab->GetProblemsSet() : 0;
	if (mbShownAny && mszShownFile.Equals(szFileName) && (mbShownChecked == bChecked) && (muiShownSet == uiSet))
	{
		return;
	}
	mbShownAny = true;
	mszShownFile.Set(szFileName);
	mbShownChecked = bChecked;
	muiShownSet = uiSet;
	mcShown.Clear();

	szNone.Init();
	if (pTab.IsNull())
	{
		szNone.Append("No file is open");
	}
	else if (!bChecked)
	{
		szNone.Append(GetIDEFileNamePart(szFileName))->Append(" hasn't been checked yet");
	}
	else
	{
		//A copy: the tab keeps its own.
		for (i = 0; i < pTab->GetProblems()->NumProblems(); i++)
		{
			psProblem = pTab->GetProblems()->Get(i);
			mcShown.Add(psProblem->iLine, psProblem->iColumn, psProblem->bWarning, psProblem->szMessage.Text());
		}
		szNone.Append("No problems in ")->Append(GetIDEFileNamePart(szFileName));
	}
	ShowProblems(mpFilePane, &mcShown, NULL, szFileName, szNone.Text());
	szNone.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The last build's errors and warnings, read from the Build tab's
// messages once it has finished, by file in the order the build gave
// them.  A GCC note isn't a problem.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::UpdateProject(void)
{
	Ptr<CIDEBuilding>		pBuilding;
	CArrayIDEOutputMessages	asMessages;
	SIDEOutputMessage*		psMessage;
	CArrayChars				aszLines;
	CArrayChars				aszFiles;
	CIDEProblemList			cFileProblems;
	const char*				szLine;
	const char*				szMessage;
	size					i;
	size					j;
	size					k;
	bool					bBuilding;

	pBuilding = mpcContent->GetBuilding();
	bBuilding = pBuilding->IsBuilding();
	if (bBuilding)
	{
		if (!mbShownBuilding)
		{
			mbShownBuilding = true;
			mcProject.Clear();
			maszProjectFiles.ReInit();
			ShowProblems(mpProjectPane, &mcProject, &maszProjectFiles, NULL, "Building...");
		}
		return;
	}
	if (!mbShownBuilding && (muiShownBuild == pBuilding->NumBuilds()) && mpProjectPane->GetTree()->NumNodes() > 0)
	{
		return;
	}
	mbShownBuilding = false;
	muiShownBuild = pBuilding->NumBuilds();
	mcProject.Clear();
	maszProjectFiles.ReInit();

	asMessages.Init();
	aszLines.Init();
	aszFiles.Init();
	if (muiShownBuild > 0)
	{
		pBuilding->GetMessages()->FindMessages(&asMessages);
		aszLines.Split(pBuilding->GetBuildPane()->GetOutput(), '\n');
		for (i = 0; i < asMessages.NumElements(); i++)
		{
			aszFiles.AddIfUnique(asMessages.Get(i)->szFile.Text());
		}
	}
	for (j = 0; j < aszFiles.NumElements(); j++)
	{
		cFileProblems.Init();
		for (i = 0; i < asMessages.NumElements(); i++)
		{
			psMessage = asMessages.Get(i);
			if ((psMessage->eKind == IMK_Note) || !psMessage->szFile.Equals(aszFiles.GetText(j)))
			{
				continue;
			}
			szLine = aszLines.GetText(psMessage->uiOutputLine);
			szMessage = strstr(szLine, "): ");
			szMessage = szMessage ? szMessage + 3 : szLine;
			if ((strncmp(szMessage, "Warning: ", 9) == 0) || (strncmp(szMessage, "warning: ", 9) == 0))
			{
				szMessage += 9;
			}
			else if (strncmp(szMessage, "error: ", 7) == 0)
			{
				szMessage += 7;
			}
			cFileProblems.Add(psMessage->iLine - 1, (psMessage->iColumn > 0) ? psMessage->iColumn - 1 : 0, psMessage->eKind == IMK_Warning, szMessage);
		}
		cFileProblems.Sort();
		for (k = 0; k < cFileProblems.NumProblems(); k++)
		{
			mcProject.Add(cFileProblems.Get(k)->iLine, cFileProblems.Get(k)->iColumn, cFileProblems.Get(k)->bWarning, cFileProblems.Get(k)->szMessage.Text());
			maszProjectFiles.Add(aszFiles.GetText(j));
		}
		cFileProblems.Kill();
	}
	ShowProblems(mpProjectPane, &mcProject, &maszProjectFiles, NULL, (muiShownBuild == 0) ? "Nothing has been built yet" : "No problems in the last build");
	aszFiles.Kill();
	aszLines.Kill();
	CIDEMessageOutput::KillMessages(&asMessages);
}


//////////////////////////////////////////////////////////////////////////
//
// pcProblems shown in pPane: a group for each file (szFileName's, or
// each of paszFiles', which says each problem's), "Kernel.cpp  2 errors,
// 1 warning", and under it a row for each, its icon, its message and
// where it is.  szNone alone if there are none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::ShowProblems(Ptr<CIDEResultsPane> pPane, CIDEProblemList* pcProblems, CArrayChars* paszFiles, const char* szFileName, const char* szNone)
{
	Ptr<CTreeContainer>	pTree;
	SIDEProblem*		psProblem;
	CChars				szDetail;
	CChars				szPlace;
	const char*			szFile;
	int64				iSelected;
	int32				iGroup;
	size				i;
	size				j;
	size				k;
	size				uiErrors;
	size				uiWarnings;

	pTree = pPane->GetTree();
	iSelected = pPane->BeginShow(true);
	if (pcProblems->NumProblems() == 0)
	{
		pTree->AddNode(TREE_NO_NODE, szNone, NULL, 0);
		pPane->EndShow(0);
		return;
	}
	for (i = 0; i < pcProblems->NumProblems(); i = j)
	{
		szFile = paszFiles ? paszFiles->GetText(i) : szFileName;
		uiErrors = 0;
		uiWarnings = 0;
		for (j = i; (j < pcProblems->NumProblems()) && (!paszFiles || (strcmp(paszFiles->GetText(j), szFile) == 0)); j++)
		{
			uiErrors += pcProblems->Get(j)->bWarning ? 0 : 1;
			uiWarnings += pcProblems->Get(j)->bWarning ? 1 : 0;
		}
		szDetail.Init("  ");
		AppendCount(&szDetail, uiErrors, "error", "errors");
		if ((uiErrors > 0) && (uiWarnings > 0))
		{
			szDetail.Append(", ");
		}
		AppendCount(&szDetail, uiWarnings, "warning", "warnings");
		iGroup = pPane->AddGroup(TREE_NO_NODE, GetIDEFileNamePart(szFile), szDetail.Text(), szFile);
		pTree->SetExpanded(iGroup, true);
		szDetail.Kill();
		for (k = i; k < j; k++)
		{
			psProblem = pcProblems->Get(k);
			szPlace.Init("  ");
			szPlace.Append((int32)(psProblem->iLine + 1))->Append(':')->Append((int32)(psProblem->iColumn + 1));
			pPane->AddMessage(iGroup, psProblem->szMessage.Text(), szPlace.Text(), mpcContent->GetProblemIcon(psProblem->bWarning), (int64)k + 1);
			szPlace.Kill();
		}
	}
	pPane->EndShow(iSelected);
}


//////////////////////////////////////////////////////////////////////////
//
// "2 errors", nothing for none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblems::AppendCount(CChars* psz, size uiCount, const char* szOne, const char* szMany)
{
	if (uiCount > 0)
	{
		psz->Append((uint32)uiCount)->Append(' ')->Append((uiCount == 1) ? szOne : szMany);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where a problem (a row's data, from 1) is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEProblems::GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest)
{
	CIDEProblemList*	pcProblems;
	SIDEProblem*		psProblem;
	const char*			szFile;

	pcProblems = (pcPane == &mpFilePane) ? &mcShown : &mcProject;
	if ((iData <= 0) || ((size)iData > pcProblems->NumProblems()))
	{
		return false;
	}
	psProblem = pcProblems->Get((size)iData - 1);
	szFile = (pcPane == &mpFilePane) ? mszShownFile.Text() : maszProjectFiles.GetText((size)iData - 1);
	memset(psDest, 0, sizeof(SIDELocation));
	psDest->szFileName.Init(szFile);
	psDest->iLine = psProblem->iLine;
	psDest->iColumn = psProblem->iColumn;
	psDest->iContainer = -1;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What Next and Previous Occurrence need of a tool window: the tab shown.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEResultsPane> CIDEProblems::GetShownPane(void) { return IsFileTabShown() ? mpFilePane : mpProjectPane; }
Ptr<CDockPane> CIDEProblems::GetSteppablePane(void) const { return mpTool; }
const char* CIDEProblems::GetSteppableName(void) { return "Problems"; }
bool CIDEProblems::HasSteps(void) { return mpTool.IsNotNull() && GetShownPane()->HasResults(); }
bool CIDEProblems::Step(bool bNext, bool bKeepKeys) { return GetShownPane()->Step(bNext, bKeepKeys); }


Ptr<CDockPane> CIDEProblems::GetTool(void) const { return mpTool; }
Ptr<CIDEResultsPane> CIDEProblems::GetFilePane(void) const { return mpFilePane; }
Ptr<CIDEResultsPane> CIDEProblems::GetProjectPane(void) const { return mpProjectPane; }
void CIDEProblems::ShowFileTab(bool bFile) { mpTabs->Select(bFile ? mpFileTab : mpProjectTab); }
bool CIDEProblems::IsFileTabShown(void) { return mpTabs->GetSelected() == mpFileTab; }

