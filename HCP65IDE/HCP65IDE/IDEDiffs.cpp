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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/Timer.h"
#include "StandardLib/Objects.h"
#include "IDEIcons.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEDiffs.h"


#define IDE_DIFF_DIALOG_WIDTH	360


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpGitMenu = NULL;
	mszQueuedCommand.Init();
	mpDialog = NULL;
	mpFileChooser = NULL;
	maDiffTabs.Init();
	mpActiveDiff = NULL;
	muiNextDiff = 0;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpGitMenu);
	M_Pointer(mpDiffMenu);
	U_String(mszQueuedCommand);
	M_Pointer(mpDialog);
	M_Pointer(mpFileChooser);
	M_Embedded(maDiffTabs);
	M_Pointer(mpActiveDiff);
	U_Size(muiNextDiff);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::Free(void)
{
	mszQueuedCommand.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDiffs::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The selected file's text in the last commit on the left and as it is
// now (its editor's, saved or not) on the right.  A file added since the
// last commit is all inserted.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::CompareWithHead(void)
{
	return CompareWithRevision("HEAD");
}


//////////////////////////////////////////////////////////////////////////
//
// The selected file at szRevision (a branch, a tag, a commit, "HEAD~2")
// on the left and as it is now on the right.  A diff's tab compares its
// right side's file.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::CompareWithRevision(const char* szRevision)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = GetComparedTab();
	if (pTab.IsNull())
	{
		return false;
	}
	return OpenDiff(pTab->GetFileName(), szRevision, pTab->GetFileName(), "");
}


//////////////////////////////////////////////////////////////////////////
//
// Another file (szPath) on the left and the selected one as it is now on
// the right.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::CompareWithFile(const char* szPath)
{
	Ptr<CIDEEditorTab>	pTab;
	CChars				szFull;
	bool				bResult;

	pTab = GetComparedTab();
	if (pTab.IsNull())
	{
		return false;
	}
	szFull.Init(szPath);
	MakeFullGitPath(&szFull);
	bResult = OpenDiff(szFull.Text(), "", pTab->GetFileName(), "");
	szFull.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks which of the selected file's repository's branches to compare it
// with, the branch checked out first.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::ShowCompareWithBranchDialog(void)
{
	Ptr<CIDEEditorTab>	pTab;
	CGitRepository*		pcRepository;
	CArrayChars			aszBranches;
	const char**		aszItems;
	CComboBox*			pcBox;
	CChars				szChosen;
	size				i;
	int					iSelected;
	bool				bResult;

	pTab = GetComparedTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(pTab->GetFileName());
	if (pcRepository == NULL)
	{
		return gcLogger.Error2(__METHOD__, " [", pTab->GetFileName(), "] isn't in a git repository.", NULL);
	}
	aszBranches.Init();
	if (!pcRepository->ReadBranches(&aszBranches) || (aszBranches.NumElements() == 0))
	{
		aszBranches.Kill();
		return gcLogger.Error2(__METHOD__, " [", pcRepository->GetRoot(), "] has no branches.", NULL);
	}

	aszItems = (const char**)malloc(aszBranches.NumElements() * sizeof(const char*));
	iSelected = 0;
	for (i = 0; i < aszBranches.NumElements(); i++)
	{
		aszItems[i] = aszBranches.GetText(i);
		if (strcmp(aszItems[i], pcRepository->GetStatus()->GetBranch()) == 0)
		{
			iSelected = (int)i;
		}
	}

	bResult = false;
	szChosen.Init();
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Compare with Branch");
	pcBox = &mpDialog->AddComboBox("Branch", aszItems, aszBranches.NumElements(), iSelected);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	if (mpDialog->Show(IDE_DIFF_DIALOG_WIDTH) && (pcBox->GetSelectedText() != NULL))
	{
		szChosen.Append(pcBox->GetSelectedText());
	}
	mpDialog = NULL;
	free(aszItems);
	aszBranches.Kill();

	if (!szChosen.Empty())
	{
		bResult = CompareWithRevision(szChosen.Text());
	}
	szChosen.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for any revision git knows to compare the selected file with.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::ShowCompareWithRevisionDialog(void)
{
	CTextField*	pcField;
	CChars		szChosen;
	bool		bResult;

	if (GetComparedTab().IsNull())
	{
		return false;
	}
	szChosen.Init();
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Compare with Revision");
	pcField = &mpDialog->AddTextField("Revision", "HEAD~1");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	if (mpDialog->Show(IDE_DIFF_DIALOG_WIDTH))
	{
		szChosen.Append(pcField->GetText());
		szChosen.StripWhitespace();
	}
	mpDialog = NULL;

	bResult = false;
	if (!szChosen.Empty())
	{
		bResult = CompareWithRevision(szChosen.Text());
	}
	szChosen.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for another file to compare the selected file with, starting in
// the selected file's folder.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::ShowCompareWithFileDialog(void)
{
	static SFileChooserType	asTypes[] =
	{
		{ "All Files", NULL },
	};
	Ptr<CIDEEditorTab>	pTab;
	CChars				szChosen;
	bool				bResult;

	pTab = GetComparedTab();
	if (pTab.IsNull())
	{
		return false;
	}
	bResult = false;
	szChosen.Init();
	mpFileChooser = OMalloc<CFileChooser>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Compare with File", false, pTab->GetFileName(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpcContent->GetFolderIcon(), mpcContent->GetFileIcon());
	if (mpFileChooser->Show())
	{
		szChosen.Append(mpFileChooser->GetChosen());
	}
	mpFileChooser = NULL;
	if (!szChosen.Empty())
	{
		bResult = CompareWithFile(szChosen.Text());
	}
	szChosen.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A diff of the left and right sides (each a file at a revision, or as it
// is for an empty revision) in a tab of its own, selected, each side
// highlighted as its file would be.  False if a side couldn't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::OpenDiff(const char* szLeftFile, const char* szLeftRevision, const char* szRightFile, const char* szRightRevision)
{
	return ShowDiffTab(CreateDiffTab(szLeftFile, szLeftRevision, szRightFile, szRightRevision));
}


//////////////////////////////////////////////////////////////////////////
//
// A diff tab of those sides with its view, not yet shown (ShowDiffTab):
// a side can be given a text of its own first (CIDEDiffSide::SetFixed).
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEDiffTab> CIDEDiffs::CreateDiffTab(const char* szLeftFile, const char* szLeftRevision, const char* szRightFile, const char* szRightRevision)
{
	Ptr<CDiffView>	pView;
	int				iSide;

	pView = OMalloc<CDiffView>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetFontColours()->GetFont(mpcContent->GetTheme()->GetColour(TC_Icon)), mpcContent->GetCodeFamily());
	pView->SetAlignment(CSH_Stretch, CSV_Stretch);
	for (iSide = 0; iSide < 2; iSide++)
	{
		pView->GetEditor((ETextDiffSide)iSide)->GetTextArea()->SetTiledText(true);
		pView->GetEditor((ETextDiffSide)iSide)->GetTextArea()->SetColour(mpcContent->GetTheme()->GetColour(TC_Text));
	}
	pView->SetContextMenu(GetDiffMenu());
	return OMalloc<CIDEDiffTab>(pView, szLeftFile, szLeftRevision, mpcContent->GetSolutions()->GetExplorer()->FindFileProject(szLeftFile), szRightFile, szRightRevision, mpcContent->GetSolutions()->GetExplorer()->FindFileProject(szRightFile));
}


//////////////////////////////////////////////////////////////////////////
//
// A diff tab's context menu, the same for them all: Next and Previous
// Difference (F7, Shift+F7), then Copy and Select All of the side with
// the keys.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEDiffs::GetDiffMenu(void)
{
	Ptr<CCommandSet>	pCommands;

	if (mpDiffMenu.IsNull())
	{
		pCommands = mpcContent->GetCommands();
		mpDiffMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
		mpDiffMenu->AddItem(pCommands->Find("Window.NextDifference"));
		mpDiffMenu->AddItem(pCommands->Find("Window.PreviousDifference"));
		mpDiffMenu->AddSeparator();
		mpDiffMenu->AddItem(pCommands->Find("Edit.Copy"));
		mpDiffMenu->AddItem(pCommands->Find("Edit.SelectAll"));
	}
	return mpDiffMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab is shown in the document pane, selected.  False if a side
// couldn't be read: it isn't shown then.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::ShowDiffTab(Ptr<CIDEDiffTab> pDiff)
{
	Ptr<CDiffView>		pView;
	Ptr<CDockPane>		pPane;
	CChars				szID;
	CIDEDiffSide*		pcLeft;
	CIDEDiffSide*		pcRight;
	CChars				szLeftText;
	CChars				szRightText;
	CChars				szLeftTitle;
	CChars				szRightTitle;
	CChars				szTabTitle;
	CFileUtil			cFileUtil;
	bool				bResult;

	pView = pDiff->GetView();
	pcLeft = pDiff->GetSide(TDS_Left);
	pcRight = pDiff->GetSide(TDS_Right);

	szLeftText.Init();
	szRightText.Init();
	bResult = ReadDiffSide(pcLeft, &szLeftText) && ReadDiffSide(pcRight, &szRightText);
	if (bResult)
	{
		szLeftTitle.Init();
		szRightTitle.Init();
		MakeDiffTitle(pcLeft, pcRight, &szLeftTitle);
		MakeDiffTitle(pcRight, pcLeft, &szRightTitle);
		pView->ShowTexts(szLeftTitle.Text(), szLeftText.Text(), szRightTitle.Text(), szRightText.Text());
		HighlightDiffSide(pView, TDS_Left, pcLeft->muiProject, pcLeft->mszFile.Text(), szLeftText.Text());
		HighlightDiffSide(pView, TDS_Right, pcRight->muiProject, pcRight->mszFile.Text(), szRightText.Text());

		//The tab is named as IntelliJ IDEA names it: "Main.cpp (HEAD vs. Your version)", or "Old.cpp vs. New.cpp".
		szTabTitle.Init();
		if (pcLeft->mszFile.Equals(pcRight->mszFile) && !pcLeft->IsFixed() && !pcRight->IsFixed())
		{
			cFileUtil.GetFileName(&szTabTitle, pcLeft->mszFile.Text());
			szTabTitle.Append(" (");
			szTabTitle.Append(pcLeft->IsWorkingCopy() ? "Your version" : pcLeft->mszRevision.Text());
			szTabTitle.Append(" vs. ");
			szTabTitle.Append(pcRight->IsWorkingCopy() ? "Your version" : pcRight->mszRevision.Text());
			szTabTitle.Append(")");
		}
		else
		{
			szTabTitle.Append(szLeftTitle);
			szTabTitle.Append(" vs. ");
			szTabTitle.Append(szRightTitle);
		}
		maDiffTabs.Add(pDiff);
		szID.Init(IDE_DIFF_PREFIX);
		szID.Append((uint64)muiNextDiff);
		muiNextDiff++;
		pPane = mpcContent->GetDock()->AddPane(szID.Text(), szTabTitle.Text(), mpcContent->GetFileTypeIcon(CIDEIcons::GetFileType(pcRight->mszFile.Text())), pView, DPK_Document);
		szID.Kill();
		pDiff->SetDockPane(pPane);
		mpcContent->GetDock()->Show(pPane);
		mpActiveDiff = pDiff;
		mpcContent->GetDocuments()->SetActiveTab(NULL);
		pView->GetWindow()->GetFocus()->SetFocus(pView->GetEditor(TDS_Right)->GetTextArea());
		szTabTitle.Kill();
		szRightTitle.Kill();
		szLeftTitle.Kill();
	}
	szRightText.Kill();
	szLeftText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Compare with Clipboard, as IntelliJ's: the clipboard's text on the left
// and the active editor's selection on the right, each as it is now and
// staying so; with nothing selected the right is the whole file, which
// follows its editor as any diff's file does.  False if there is no
// editor or the clipboard has no text.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::CompareWithClipboard(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEDiffTab>	pDiff;
	CChars				szText;
	CChars				szName;
	size				uiStart;
	size				uiEnd;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	szText.Init();
	if (pTab.IsNull() || !mpcContent->GetWindow()->GetClipboard()->GetText(&szText) || szText.Empty())
	{
		szText.Kill();
		mpcContent->ShowStatus(pTab.IsNull() ? "Compare with Clipboard compares a file's text: open one first." : "The clipboard has no text to compare with.");
		return false;
	}
	pDiff = CreateDiffTab(pTab->GetFileName(), "", pTab->GetFileName(), "");
	pDiff->GetSide(TDS_Left)->SetFixed("Clipboard", szText.Text());
	if (pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		szText.Clear();
		szText.AppendSubString(pTab->GetEditor()->GetText(), uiStart, uiEnd);
		szName.Init("Selection from ");
		szName.Append(GetIDEFileNamePart(pTab->GetFileName()));
		pDiff->GetSide(TDS_Right)->SetFixed(szName.Text(), szText.Text());
		szName.Kill();
	}
	szText.Kill();
	return ShowDiffTab(pDiff);
}


bool CIDEDiffs::CanCompareWithClipboard(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// Replaces pszDest with the side's text: its file at its revision, or as
// it is (its open editor's text, else the file on disk).  A file added
// since the last commit (or not yet in git) has an empty HEAD.  False if
// the text couldn't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDiffs::ReadDiffSide(CIDEDiffSide* pcSide, CChars* pszDest)
{
	CGitRepository*		pcRepository;
	Ptr<CIDEEditorTab>	pTab;
	CTextFile			cFile;
	EGitFileState		eState;
	bool				bResult;

	pszDest->Clear();
	if (pcSide->IsFixed())
	{
		pszDest->Append(pcSide->mszText);
		return true;
	}
	if (pcSide->IsRevision())
	{
		pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(pcSide->mszFile.Text());
		if (pcRepository == NULL)
		{
			return gcLogger.Error2(__METHOD__, " [", pcSide->mszFile.Text(), "] isn't in a git repository.", NULL);
		}
		pcSide->mszHead.Set(pcRepository->GetStatus()->GetHead());
		if (pcRepository->ReadRevisionText(pcSide->mszFile.Text(), pcSide->mszRevision.Text(), pszDest))
		{
			return true;
		}
		eState = pcRepository->GetFileState(pcSide->mszFile.Text());
		if (pcSide->mszRevision.Equals("HEAD") && ((eState == GFS_Added) || (eState == GFS_Untracked)))
		{
			return true;
		}
		return gcLogger.Error2(__METHOD__, " [", pcSide->mszFile.Text(), "] isn't in [", pcSide->mszRevision.Text(), "].", NULL);
	}

	pTab = mpcContent->GetDocuments()->FindTab(pcSide->mszFile.Text());
	if (pTab.IsNotNull())
	{
		pszDest->Append(pTab->GetEditor()->GetText());
		pcSide->muiSeenChanges = pTab->GetEditor()->GetChangeCount();
		return true;
	}
	pcSide->muiSeenChanges = IDE_DIFF_NO_CHANGES;
	cFile.Init();
	bResult = cFile.Read(pcSide->mszFile.Text());
	if (bResult)
	{
		pszDest->Append(cFile.Text(), (size)cFile.Length());
	}
	cFile.Kill();
	if (!bResult)
	{
		return gcLogger.Error2(__METHOD__, " Could not read [", pcSide->mszFile.Text(), "].", NULL);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A side's title: its file's name, with its revision, or "Your version"
// when the other side is the same file at a revision; a fixed side's is
// its name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::MakeDiffTitle(CIDEDiffSide* pcSide, CIDEDiffSide* pcOther, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	if (pcSide->IsFixed())
	{
		pszDest->Append(pcSide->mszName);
		return;
	}
	cFileUtil.GetFileName(pszDest, pcSide->mszFile.Text());
	if (pcSide->IsRevision())
	{
		pszDest->Append(" (");
		pszDest->Append(pcSide->mszRevision);
		pszDest->Append(")");
	}
	else if (pcSide->mszFile.Equals(pcOther->mszFile) && !pcOther->IsFixed())
	{
		pszDest->Append(" (Your version)");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Reads both sides again and shows the diff of them, keeping where it is
// scrolled.  A side whose text changed is highlighted again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::RefreshDiff(Ptr<CIDEDiffTab> pDiff)
{
	Ptr<CDiffView>	pView;
	CIDEDiffSide*	pcLeft;
	CIDEDiffSide*	pcRight;
	CChars			szLeftText;
	CChars			szRightText;
	bool			bLeftChanged;
	bool			bRightChanged;

	pDiff->Refreshed();
	pView = pDiff->GetView();
	pcLeft = pDiff->GetSide(TDS_Left);
	pcRight = pDiff->GetSide(TDS_Right);
	szLeftText.Init();
	szRightText.Init();
	if (ReadDiffSide(pcLeft, &szLeftText) && ReadDiffSide(pcRight, &szRightText))
	{
		bLeftChanged = !szLeftText.Equals(pView->GetDiff()->GetContent1()->Text());
		bRightChanged = !szRightText.Equals(pView->GetDiff()->GetContent2()->Text());
		if (bLeftChanged || bRightChanged)
		{
			pView->SetTexts(szLeftText.Text(), szRightText.Text());
			if (bLeftChanged)
			{
				HighlightDiffSide(pView, TDS_Left, pcLeft->muiProject, pcLeft->mszFile.Text(), szLeftText.Text());
			}
			if (bRightChanged)
			{
				HighlightDiffSide(pView, TDS_Right, pcRight->muiProject, pcRight->mszFile.Text(), szRightText.Text());
			}
		}
	}
	szRightText.Kill();
	szLeftText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A diff whose side's editor has been edited is shown again once the
// edits pause.  Returns the milliseconds until one might need to be, -1
// for none.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEDiffs::PollDiffs(int64 iNow)
{
	size				i;
	int					iSide;
	Ptr<CIDEDiffTab>	pDiff;
	CIDEDiffSide*		pcSide;
	Ptr<CIDEEditorTab>	pTab;
	int64				iWait;
	int64				iSoonest;

	iSoonest = -1;
	for (i = 0; i < maDiffTabs.NumElements(); i++)
	{
		pDiff = maDiffTabs.Get(i);
		for (iSide = 0; iSide < 2; iSide++)
		{
			pcSide = pDiff->GetSide((ETextDiffSide)iSide);
			if (pcSide->IsWorkingCopy())
			{
				pTab = mpcContent->GetDocuments()->FindTab(pcSide->mszFile.Text());
				if (pTab.IsNotNull() && (pTab->GetEditor()->GetChangeCount() != pcSide->muiSeenChanges))
				{
					pcSide->muiSeenChanges = pTab->GetEditor()->GetChangeCount();
					pDiff->Changed(iNow);
				}
			}
		}
		if (pDiff->NeedsRefresh(iNow, &iWait))
		{
			RefreshDiff(pDiff);
		}
		else if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
		{
			iSoonest = iWait;
		}
	}
	return iSoonest;
}


//////////////////////////////////////////////////////////////////////////
//
// A diff with a side at a revision is shown again when its repository's
// last commit has changed since (a commit, a checkout).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::RefreshMovedDiffs(void)
{
	size				i;
	int					iSide;
	Ptr<CIDEDiffTab>	pDiff;
	CIDEDiffSide*		pcSide;
	CGitRepository*		pcRepository;
	bool				bMoved;

	for (i = 0; i < maDiffTabs.NumElements(); i++)
	{
		pDiff = maDiffTabs.Get(i);
		bMoved = false;
		for (iSide = 0; iSide < 2; iSide++)
		{
			pcSide = pDiff->GetSide((ETextDiffSide)iSide);
			if (pcSide->IsRevision())
			{
				pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(pcSide->mszFile.Text());
				if (pcRepository && !pcSide->mszHead.Equals(pcRepository->GetStatus()->GetHead()))
				{
					bMoved = true;
				}
			}
		}
		if (bMoved)
		{
			RefreshDiff(pDiff);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One side of a diff coloured by what the parser finds each part of
// szText to be, parsed as the project's file szFileName.  Only C and C++
// files of a project are highlighted, as in the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::HighlightDiffSide(Ptr<CDiffView> pView, ETextDiffSide eSide, size uiProject, const char* szFileName, const char* szText)
{
	CArrayHighlights	acHighlights;
	CChars				szErrors;

	if ((uiProject == SOLUTION_EXPLORER_NO_PROJECT) || (CIDEIcons::GetFileType(szFileName) == IFT_Other))
	{
		return;
	}
	acHighlights.Init();
	szErrors.Init();
	if (mpcContent->GetCodeModel()->Highlight(uiProject, szFileName, szText, &acHighlights, &szErrors))
	{
		ApplyIDEHighlights(pView->GetEditor(eSide)->GetTextArea(), &acHighlights);
	}
	szErrors.Kill();
	acHighlights.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The file a compare is of: the selected file, or the right side's file
// of the selected diff if it is open.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEDiffs::GetComparedTab(void)
{
	if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
	{
		return mpcContent->GetDocuments()->GetActiveTab();
	}
	if (mpActiveDiff.IsNotNull())
	{
		return mpcContent->GetDocuments()->FindTab(mpActiveDiff->GetSide(TDS_Right)->mszFile.Text());
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The diff shown in the document pane, NULL if it shows something else.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEDiffTab> CIDEDiffs::FindDiffTab(CDockPane* pcPane)
{
	size	i;

	for (i = 0; i < maDiffTabs.NumElements(); i++)
	{
		if (&maDiffTabs.Get(i)->GetDockPane() == pcPane)
		{
			return maDiffTabs.Get(i);
		}
	}
	return NULL;
}


Ptr<CIDEDiffTab> CIDEDiffs::GetActiveDiff(void) const { return mpActiveDiff; }


//////////////////////////////////////////////////////////////////////////
//
// The diffs' tabs.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDiffs::NumDiffs(void) { return maDiffTabs.NumElements(); }
Ptr<CIDEDiffTab> CIDEDiffs::GetDiff(size uiIndex) { return maDiffTabs.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The diff whose tab was selected last, NULL if a file's was.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::SetActiveDiff(Ptr<CIDEDiffTab> pDiff) { mpActiveDiff = pDiff; }


//////////////////////////////////////////////////////////////////////////
//
// pDiff's pane has closed: it is forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::RemoveDiff(Ptr<CIDEDiffTab> pDiff)
{
	if (mpActiveDiff == pDiff)
	{
		mpActiveDiff = NULL;
	}
	maDiffTabs.Remove(pDiff);
}


//////////////////////////////////////////////////////////////////////////
//
// Every diff's tab is closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::CloseAllDiffs(void)
{
	size	i;

	for (i = maDiffTabs.NumElements(); i > 0; i--)
	{
		if (i <= maDiffTabs.NumElements())
		{
			mpcContent->GetDock()->Close(maDiffTabs.Get(i - 1)->GetDockPane());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions changed: each diff's sides are coloured as their files'
// projects now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::FindProjects(void)
{
	size				i;
	Ptr<CIDEDiffTab>	pDiff;

	for (i = 0; i < maDiffTabs.NumElements(); i++)
	{
		pDiff = maDiffTabs.Get(i);
		pDiff->GetSide(TDS_Left)->muiProject = mpcContent->GetSolutions()->GetExplorer()->FindFileProject(pDiff->GetSide(TDS_Left)->mszFile.Text());
		pDiff->GetSide(TDS_Right)->muiProject = mpcContent->GetSolutions()->GetExplorer()->FindFileProject(pDiff->GetSide(TDS_Right)->mszFile.Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Git: Compare with HEAD, a branch or a revision.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEDiffs::CreateGitMenu(void)
{
	Ptr<CCommandSet>	pCommands;

	pCommands = mpcContent->GetCommands();
	mpGitMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpGitMenu->AddItem(pCommands->Find("Git.CompareWithHead"));
	mpGitMenu->AddItem(pCommands->Find("Git.CompareWithBranch"));
	mpGitMenu->AddItem(pCommands->Find("Git.CompareWithRevision"));
	return mpGitMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// File's Compare with File.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::AddFileItems(Ptr<CMenuContainer> pFile)
{
	pFile->AddItem(mpcContent->GetCommands()->Find("File.CompareWithFile"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Git.CompareWithHead", "Compare with HEAD", "", Ptr<CImageCel>(), this, &CIDEDiffs::QueueCommand);
	pCommands->Add("Git.CompareWithBranch", "Compare with Branch...", "", Ptr<CImageCel>(), this, &CIDEDiffs::QueueCommand);
	pCommands->Add("Git.CompareWithRevision", "Compare with Revision...", "", Ptr<CImageCel>(), this, &CIDEDiffs::QueueCommand);
	pCommands->Add("File.CompareWithFile", "Compare with File...", "", Ptr<CImageCel>(), this, &CIDEDiffs::QueueCommand);
	pCommands->Add("Edit.CompareWithClipboard", "Compare with Clipboard", "", Ptr<CImageCel>(), this, &CIDEDiffs::QueueCommand)->SetEnabledCheck(this, &CIDEDiffs::CanCompareWithClipboard);
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' menu's Compare with Clipboard.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::AddEditorItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.CompareWithClipboard"));
}


//////////////////////////////////////////////////////////////////////////
//
// A command's keys or menu item: what it does is done at the next tick,
// outside the input, as it can show a dialog.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::QueueCommand(CCommand* pcCommand)
{
	mszQueuedCommand.Set(pcCommand->GetName());
	mpcContent->GetWindow()->Post(this, &CIDEDiffs::RunQueuedCommand);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDiffs::RunQueuedCommand(void)
{
	if (mszQueuedCommand.Equals("Git.CompareWithHead"))
	{
		CompareWithHead();
	}
	else if (mszQueuedCommand.Equals("Git.CompareWithBranch"))
	{
		ShowCompareWithBranchDialog();
	}
	else if (mszQueuedCommand.Equals("Git.CompareWithRevision"))
	{
		ShowCompareWithRevisionDialog();
	}
	else if (mszQueuedCommand.Equals("File.CompareWithFile"))
	{
		ShowCompareWithFileDialog();
	}
	else if (mszQueuedCommand.Equals("Edit.CompareWithClipboard"))
	{
		CompareWithClipboard();
	}
}
