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
#include "StandardLib/TextContent.h"
#include "IDEGit.h"
#include "IDEContent.h"


#define IDE_GIT_REFRESH			5000	//Milliseconds between reading the statuses.
#define IDE_GIT_POLL			20		//And between looks at git while they are being read.
#define IDE_GIT_SHORT_HEAD		7		//Characters of a detached head's commit shown.
#define IDE_CHANGE_POPUP_WIDTH		360		//Narrowest the change popup's old lines are.
#define IDE_CHANGE_POPUP_MAX_WIDTH	800		//And widest, scrolled past that.
#define IDE_CHANGE_POPUP_ROWS		15		//Most rows of old lines shown before they scroll.


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
		pTab->GetDockPane()->SetTabFont(GetGitStateFont(mpFontColours, eState));
	}
	RefreshMovedDiffs();
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
// The window was activated: git may have been used while it wasn't, so
// every repository's status is read again, and diffs of files that
// aren't open (shown from disk) are made again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::WindowActivated(void)
{
	size				i;
	int					iSide;
	Ptr<CIDEDiffTab>	pDiff;
	bool				bFromDisk;

	if (mcGit.NumRepositories() > 0)
	{
		for (i = 0; i < mcGit.NumRepositories(); i++)
		{
			mcGit.GetRepository(i)->RefreshStatus();
		}
		miGitStatusTime = GetDebugTickCount();
		mpWindow->RequestTick(IDE_GIT_POLL);
	}

	for (i = 0; i < maDiffTabs.Size(); i++)
	{
		pDiff = maDiffTabs.Get(i);
		bFromDisk = false;
		for (iSide = 0; iSide < 2; iSide++)
		{
			if (pDiff->GetSide((ETextDiffSide)iSide)->IsWorkingCopy() && (pDiff->GetSide((ETextDiffSide)iSide)->muiSeenChanges == IDE_DIFF_NO_CHANGES))
			{
				bFromDisk = true;
			}
		}
		if (bFromDisk)
		{
			RefreshDiff(pDiff);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A change bar in a file's gutter was clicked: its change is shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ChangeBarClicked(CCodeEditor* pcEditor, size uiLine)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindTab(pcEditor);
	if (pTab.IsNotNull())
	{
		ShowChange(pTab, uiLine);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A popup under the change at uiLine (a changed line, or one either side
// of removed lines), as IntelliJ IDEA shows one: what changed, Show Diff
// and Rollback, and the lines as they were in the last commit.  False if
// there is no change there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowChange(Ptr<CIDEEditorTab> pTab, size uiLine)
{
	CChars*					pszBase;
	char*					szText;
	SGitLineChange*			psChange;
	size					i;
	size					uiLines;
	Ptr<CGridContainer>		pGrid;
	Ptr<CLabel>				pLabel;
	Ptr<CTextArea>			pOld;
	Ptr<CScrollPane>		pPane;
	Ptr<CPopup>				pPopup;
	Ptr<CWindow>			pWindow;
	Ptr<CTextArea>			pEditor;
	CChars					szOld;
	CChars					szSummary;
	SInsets					sInsets;
	SInt32Vec2				sPosition;
	int32					iRow;
	int32					iWidth;

	if (!pTab->HasBaseText())
	{
		return false;
	}
	pszBase = pTab->GetBaseText();
	szText = pTab->GetEditor()->GetText();
	mcLineChanges.Compare(pszBase->Text(), pszBase->Length(), szText, strlen(szText));
	psChange = NULL;
	for (i = 0; i < mcLineChanges.NumChanges(); i++)
	{
		psChange = mcLineChanges.GetChange(i);
		if (psChange->uiStartLine == psChange->uiEndLine)
		{
			if ((uiLine == psChange->uiStartLine) || (uiLine + 1 == psChange->uiStartLine))
			{
				break;
			}
		}
		else if ((uiLine >= psChange->uiStartLine) && (uiLine < psChange->uiEndLine))
		{
			break;
		}
		psChange = NULL;
	}
	if (psChange == NULL)
	{
		return false;
	}

	CloseChangePopup();
	msChange = *psChange;
	muiChange = i;
	mpChangeTab = pTab;
	//The editor may be in a floating window of its own.
	pWindow = pTab->GetCodeEditor()->GetWindow();

	//What changed, then the buttons.
	szSummary.Init();
	uiLines = (msChange.eType == GLC_Removed) ? (msChange.uiBaseEndLine - msChange.uiBaseStartLine) : (msChange.uiEndLine - msChange.uiStartLine);
	szSummary.Append((uint32)uiLines);
	szSummary.Append((uiLines == 1) ? " line " : " lines ");
	szSummary.Append((msChange.eType == GLC_Added) ? "added" : ((msChange.eType == GLC_Removed) ? "removed" : "changed"));
	pGrid = OMalloc<CGridContainer>(pWindow);
	pGrid->SetSizedByContent(true);
	pLabel = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), szSummary.Text());
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	szSummary.Kill();
	mpShowDiffButton = OMalloc<CButton>(pWindow, mpTheme, "Show Diff");
	mpShowDiffButton->SetAlignment(CSH_Right, CSV_Center);
	mpShowDiffButton->GetClickEvent()->Add(this, &CIDEContent::ShowDiffClicked);
	mpRollbackButton = OMalloc<CButton>(pWindow, mpTheme, "Rollback");
	mpRollbackButton->SetAlignment(CSH_Right, CSV_Center);
	mpRollbackButton->GetClickEvent()->Add(this, &CIDEContent::RollbackClicked);
	pGrid->AddComponent(pLabel, 0, 0, 1, 1, 1, 0);
	pGrid->AddComponent(mpShowDiffButton, 1, 0);
	pGrid->AddComponent(mpRollbackButton, 2, 0);

	//The lines as they were, on the deleted lines' colour, scrolled if there are many.
	pEditor = pTab->GetEditor();
	iRow = pEditor->GetFont()->GetLineHeight();
	if (msChange.uiBaseEndLine > msChange.uiBaseStartLine)
	{
		szOld.Init();
		AppendBaseLines(pTab, msChange.uiBaseStartLine, msChange.uiBaseEndLine, &szOld);
		szOld.RemoveLastCharacter();
		pOld = OMalloc<CTextArea>(pWindow, mpCodeFamily, szOld.Text());
		szOld.Kill();
		pOld->SetTiledText(true);
		pOld->SetColour(JEWEL_TEXT);
		pOld->SetEditable(false);
		pOld->SetAlignment(CSH_Left, CSV_Top);
		uiLines = msChange.uiBaseEndLine - msChange.uiBaseStartLine;
		pOld->AddLineBackground(0, uiLines, Set32BitColour((uint8)0x48, (uint8)0x4A, (uint8)0x4A));
		iWidth = pOld->GetDesiredSize().x;
		pPane = OMalloc<CScrollPane>(pWindow, mpTheme);
		pPane->SetContent(pOld);
		pPane->SetDesiredSize((iWidth < IDE_CHANGE_POPUP_WIDTH) ? IDE_CHANGE_POPUP_WIDTH : ((iWidth > IDE_CHANGE_POPUP_MAX_WIDTH) ? IDE_CHANGE_POPUP_MAX_WIDTH : iWidth), (uiLines > IDE_CHANGE_POPUP_ROWS ? IDE_CHANGE_POPUP_ROWS : (int32)uiLines) * iRow);
		pPane->SetAlignment(CSH_Stretch, CSV_Stretch);
		pGrid->AddComponent(pPane, 0, 1, 3, 1, 1, 1);
	}

	//Under the change, at the left of the code.
	sPosition.x = pEditor->GetPosition().x;
	sPosition.y = pEditor->GetPosition().y + (int32)msChange.uiEndLine * iRow;
	pPopup = pWindow->OpenPopup(pGrid, sPosition, pTab->GetCodeEditor());
	sInsets.Init(6, 6);
	pPopup->SetLook(JEWEL_PANEL, JEWEL_SEPARATOR, &sInsets);
	mpChangePopup = pGrid;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The change shown is undone in its editor: its lines become the last
// commit's.  The editor can undo it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::RollbackChange(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CChars				szLines;
	size				uiLines;
	size				uiLength;
	size				uiStart;
	size				uiEnd;
	CTextContent		cBase;
	bool				bBaseEnds;

	pTab = mpChangeTab;
	CloseChangePopup();
	if (pTab.IsNull())
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	uiLines = pEditor->NumLines();
	uiLength = strlen(pEditor->GetText());

	//The last commit's lines, each with a line feed.
	szLines.Init();
	AppendBaseLines(pTab, msChange.uiBaseStartLine, msChange.uiBaseEndLine, &szLines);

	if (msChange.uiEndLine < uiLines)
	{
		//Whole lines, each ending with its line feed.
		uiStart = pEditor->GetLineStart(msChange.uiStartLine);
		uiEnd = pEditor->GetLineStart(msChange.uiEndLine);
	}
	else
	{
		//To the end of text that doesn't end with a line feed: the last line ends as the commit's did.
		cBase.Init("", pTab->GetBaseText()->Text(), pTab->GetBaseText()->Length());
		bBaseEnds = (msChange.uiBaseEndLine > msChange.uiBaseStartLine) && (cBase.GetLineSeparator(msChange.uiBaseEndLine - 1) != TLS_None);
		cBase.Kill();
		if (!szLines.Empty() && !bBaseEnds)
		{
			szLines.RemoveLastCharacter();
		}
		if (msChange.uiStartLine < uiLines)
		{
			uiStart = pEditor->GetLineStart(msChange.uiStartLine);
		}
		else
		{
			uiStart = uiLength;
			if (!szLines.Empty())
			{
				szLines.Insert(0, '\n');
			}
		}
		uiEnd = uiLength;
	}

	pEditor->Select(uiStart, uiEnd);
	if (szLines.Empty())
	{
		pEditor->Delete();
	}
	else
	{
		pEditor->Paste(szLines.Text());
	}
	szLines.Kill();
	pEditor->GetWindow()->GetFocus()->SetFocus(pEditor);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The change shown, in a diff of its file with its last commit.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowChangeDiff(void)
{
	Ptr<CIDEEditorTab>	pTab;
	size				uiChange;

	pTab = mpChangeTab;
	uiChange = muiChange;
	CloseChangePopup();
	if (pTab.IsNull() || !OpenDiff(pTab->GetFileName(), "HEAD", pTab->GetFileName(), ""))
	{
		return false;
	}
	mpActiveDiff->GetView()->ScrollToBlock(uiChange);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::CloseChangePopup(void)
{
	if (mpChangePopup.IsNotNull())
	{
		mpChangePopup->GetWindow()->ClosePopup(mpChangePopup);
	}
	if (mpRollbackButton.IsNotNull())
	{
		mpRollbackButton->GetClickEvent()->Remove(this);
	}
	if (mpShowDiffButton.IsNotNull())
	{
		mpShowDiffButton->GetClickEvent()->Remove(this);
	}
	mpChangePopup = NULL;
	mpRollbackButton = NULL;
	mpShowDiffButton = NULL;
	mpChangeTab = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Appends lines uiStart up to uiEnd of the tab's last commit's text, each
// with a line feed (whatever ended it).
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::AppendBaseLines(Ptr<CIDEEditorTab> pTab, size uiStart, size uiEnd, CChars* pszDest)
{
	CTextContent	cBase;
	size			i;

	cBase.Init("", pTab->GetBaseText()->Text(), pTab->GetBaseText()->Length());
	for (i = uiStart; (i < uiEnd) && (i < cBase.NumLines()); i++)
	{
		pszDest->Append(cBase.GetLineText(i), (size)cBase.GetLineLength(i));
		pszDest->Append('\n');
	}
	cBase.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab whose code editor is pcEditor, NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(CCodeEditor* pcEditor)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetCodeEditor() == pcEditor)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The change at uiLine of the selected file is shown (and with iAction 1
// rolled back, or 2 shown in a diff) at the next tick once the editor has
// been laid out, as clicking its change bar would: for scripted runs.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::QueueChange(size uiLine, int iAction)
{
	muiQueuedChange = uiLine;
	miQueuedAction = iAction;
	mpWindow->RequestTick(0);
}


void CIDEContent::RollbackClicked(void) { RollbackChange(); }
void CIDEContent::ShowDiffClicked(void) { ShowChangeDiff(); }
Ptr<CGridContainer> CIDEContent::GetChangePopup(void) { return mpChangePopup; }
CGitRepositories* CIDEContent::GetGit(void) { return &mcGit; }
Ptr<CLabel> CIDEContent::GetBranchLabel(void) { return mpBranchLabel; }

