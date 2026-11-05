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
#include <string.h>
#include "BaseLib/Timer.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "WindowLib/JewelColours.h"
#include "IDEGit.h"
#include "IDEContent.h"


#define IDE_GIT_REFRESH			5000	//Milliseconds between reading the statuses.
#define IDE_GIT_POLL			20		//And between looks at git while they are being read.
#define IDE_GIT_SHORT_HEAD		7		//Characters of a detached head's commit shown.


//////////////////////////////////////////////////////////////////////////
//
// Reads the statuses every so often and shows them when they have been
// read.  Returns the milliseconds until it next needs a tick.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEContent::PollGit(int64 iNow)
{
	int64	iWait;

	if (mcGit.NumRepositories() == 0)
	{
		return -1;
	}

	if (mcGit.PollStatus())
	{
		iWait = IDE_GIT_POLL;
	}
	else if ((miGitStatusTime == 0) || (iNow - miGitStatusTime >= IDE_GIT_REFRESH))
	{
		mcGit.StartStatus();
		miGitStatusTime = iNow;
		iWait = IDE_GIT_POLL;
	}
	else
	{
		iWait = IDE_GIT_REFRESH - (iNow - miGitStatusTime);
	}

	if (mcGit.GetStatusVersion() != muiGitVersion)
	{
		ShowGitStatus();
	}
	return iWait;
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName was saved: its repository's status is read again at once,
// and the next reading of them all waits its usual time from now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::GitFileSaved(const char* szFileName)
{
	if (mcGit.RefreshStatus(szFileName))
	{
		miGitStatusTime = GetDebugTickCount();
		if (mpWindow.IsNotNull())
		{
			mpWindow->RequestTick(IDE_GIT_POLL);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Reads every repository's status now, waiting for them, and shows them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReadGitStatus(void)
{
	size	i;

	for (i = 0; i < mcGit.NumRepositories(); i++)
	{
		mcGit.GetRepository(i)->ReadStatus();
	}
	ShowGitStatus();
}


//////////////////////////////////////////////////////////////////////////
//
// The files are coloured by their states in the statuses just read.  An
// open file whose state, or whose repository's last commit, has changed
// since its text in the last commit was read reads it again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowGitStatus(void)
{
	size				i;
	Ptr<CIDEEditorTab>	pTab;
	CGitRepository*		pcRepository;
	EGitFileState		eState;
	bool				bHeadMoved;

	muiGitVersion = mcGit.GetStatusVersion();
	if (mpExplorer.IsNotNull())
	{
		mpExplorer->ColourFiles(&mcGit, mpFontColours);
	}

	for (i = 0; i < maTabs.Size(); i++)
	{
		pTab = maTabs.Get(i);
		pcRepository = mcGit.FindRepository(pTab->GetFileName());
		eState = pcRepository ? pcRepository->GetFileState(pTab->GetFileName()) : GFS_Unmodified;
		bHeadMoved = pcRepository && pTab->HasBaseText() && (strcmp(pTab->GetBaseHead(), pcRepository->GetStatus()->GetHead()) != 0);
		if ((eState != pTab->GetGitState()) || bHeadMoved)
		{
			ReadGitBase(pTab);
			UpdateChangeBars(pTab);
		}
		pTab->GetTab()->SetTextFont(GetGitStateFont(mpFontColours, eState));
	}
	ShowBranch();
}


//////////////////////////////////////////////////////////////////////////
//
// The branch of the selected file's repository, or of the solution's if
// no file is open: its name, or its commit if the head is detached.
// Nothing if neither is in a repository or its status hasn't been read.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowBranch(void)
{
	CGitRepository*	pcRepository;
	CGitStatus*		pcStatus;
	CChars			szText;

	if (mpBranchLabel.IsNull())
	{
		return;
	}

	pcRepository = NULL;
	if (mpActive.IsNotNull())
	{
		pcRepository = mcGit.FindRepository(mpActive->GetFileName());
	}
	if ((pcRepository == NULL) && !mszSolutionFile.Empty())
	{
		pcRepository = mcGit.FindRepository(mszSolutionFile.Text());
	}

	szText.Init();
	if (pcRepository && (pcRepository->GetStatusVersion() > 0))
	{
		pcStatus = pcRepository->GetStatus();
		szText.Append("Git: ");
		if (pcStatus->IsDetached())
		{
			szText.Append(pcStatus->GetHead(), IDE_GIT_SHORT_HEAD);
		}
		else
		{
			szText.Append(pcStatus->GetBranch());
		}
	}
	if (!szText.Equals(mpBranchLabel->GetText()))
	{
		mpBranchLabel->SetText(szText.Text());
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab's file's text in the last commit, to mark its changes from.  A
// file that isn't in a repository, or is untracked or ignored, has none;
// one added since the last commit has an empty one, so all its lines are
// marked added.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReadGitBase(Ptr<CIDEEditorTab> pTab)
{
	CGitRepository*	pcRepository;
	EGitFileState	eState;
	CChars			szText;

	pcRepository = mcGit.FindRepository(pTab->GetFileName());
	if (pcRepository == NULL)
	{
		pTab->SetGitState(GFS_Unmodified);
		pTab->ClearBaseText();
		return;
	}

	eState = pcRepository->GetFileState(pTab->GetFileName());
	pTab->SetGitState(eState);
	if ((eState == GFS_Untracked) || (eState == GFS_Ignored))
	{
		pTab->ClearBaseText();
		return;
	}

	szText.Init();
	if (pcRepository->ReadHeadText(pTab->GetFileName(), &szText))
	{
		pTab->SetBaseText(szText.Text(), szText.Length(), pcRepository->GetStatus()->GetHead());
	}
	else if (eState == GFS_Added)
	{
		pTab->SetBaseText("", 0, pcRepository->GetStatus()->GetHead());
	}
	else
	{
		pTab->ClearBaseText();
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab's gutter marks the lines changed from its file's last commit,
// or nothing if it has no last commit's text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::UpdateChangeBars(Ptr<CIDEEditorTab> pTab)
{
	CChars*	pszBase;
	char*	szText;

	if (!pTab->HasBaseText())
	{
		pTab->SetChangeBars(NULL);
		return;
	}
	pszBase = pTab->GetBaseText();
	szText = pTab->GetEditor()->GetText();
	mcLineChanges.Compare(pszBase->Text(), pszBase->Length(), szText, strlen(szText));
	pTab->SetChangeBars(&mcLineChanges);
}


//////////////////////////////////////////////////////////////////////////
//
// The selected file's text in the last commit on the left and as it is
// now in the editor (saved or not) on the right, in a tab of their own,
// each side highlighted as the file would be with that text.
// A file added since the last commit is all inserted.  False if no file
// is selected, or it isn't in a repository or the last commit.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::CompareWithHead(void)
{
	CGitRepository*		pcRepository;
	CChars				szBase;
	CChars				szName;
	CChars				szTitle;
	CFileUtil			cFileUtil;
	Ptr<CDiffView>		pView;
	Ptr<CTabComponent>	pTab;
	EGitFileState		eState;
	bool				bBase;

	if (mpActive.IsNull())
	{
		return false;
	}
	pcRepository = mcGit.FindRepository(mpActive->GetFileName());
	if (pcRepository == NULL)
	{
		return gcLogger.Error2(__METHOD__, " [", mpActive->GetFileName(), "] isn't in a git repository.", NULL);
	}

	szBase.Init();
	bBase = pcRepository->ReadHeadText(mpActive->GetFileName(), &szBase);
	eState = pcRepository->GetFileState(mpActive->GetFileName());
	if (!bBase && (eState != GFS_Added) && (eState != GFS_Untracked))
	{
		szBase.Kill();
		return gcLogger.Error2(__METHOD__, " [", mpActive->GetFileName(), "] isn't in the last commit.", NULL);
	}

	szName.Init();
	cFileUtil.GetFileName(&szName, mpActive->GetFileName());
	pView = OMalloc<CDiffView>(mpWindow, mpTheme, mpFontColours->GetFont(JEWEL_ICON), mpCodeFamily);
	pView->SetAlignment(CSH_Stretch, CSV_Stretch);
	pView->GetEditor(TDS_Left)->GetTextArea()->SetTiledText(true);
	pView->GetEditor(TDS_Left)->GetTextArea()->SetColour(JEWEL_TEXT);
	pView->GetEditor(TDS_Right)->GetTextArea()->SetTiledText(true);
	pView->GetEditor(TDS_Right)->GetTextArea()->SetColour(JEWEL_TEXT);

	szTitle.Init(szName);
	szTitle.Append(" (HEAD)");
	szName.Append(" (Your version)");
	pView->ShowTexts(szTitle.Text(), szBase.Text(), szName.Text(), mpActive->GetEditor()->GetText());
	HighlightDiffSide(pView, TDS_Left, mpActive->GetProject(), mpActive->GetFileName(), szBase.Text());
	HighlightDiffSide(pView, TDS_Right, mpActive->GetProject(), mpActive->GetFileName(), mpActive->GetEditor()->GetText());
	szName.Kill();
	szBase.Kill();

	//The tab is named for the file, as IntelliJ IDEA names it.
	szTitle.Clear();
	cFileUtil.GetFileName(&szTitle, mpActive->GetFileName());
	szTitle.Append(" (HEAD vs. Your version)");
	pTab = mpEditors->AddTab(szTitle.Text(), pView, maFileIcons.Get(CIDEIcons::GetFileType(mpActive->GetFileName())), true);
	szTitle.Kill();
	mpEditors->Select(pTab);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// One side of a diff coloured by what the parser finds each part of
// szText to be, parsed as the project's file szFileName.  Only C and C++
// files of a project are highlighted, as in the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::HighlightDiffSide(Ptr<CDiffView> pView, ETextDiffSide eSide, size uiProject, const char* szFileName, const char* szText)
{
	CArrayHighlights	acHighlights;
	CChars				szErrors;

	if ((uiProject == SOLUTION_EXPLORER_NO_PROJECT) || (CIDEIcons::GetFileType(szFileName) == IFT_Other))
	{
		return;
	}
	acHighlights.Init();
	szErrors.Init();
	if (mcCodeModel.Highlight(uiProject, szFileName, szText, &acHighlights, &szErrors))
	{
		ApplyIDEHighlights(pView->GetEditor(eSide)->GetTextArea(), &acHighlights);
	}
	szErrors.Kill();
	acHighlights.Kill();
}


CGitRepositories* CIDEContent::GetGit(void) { return &mcGit; }
Ptr<CLabel> CIDEContent::GetBranchLabel(void) { return mpBranchLabel; }

