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
#ifndef __IDE_DIFFS_H__
#define __IDE_DIFFS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/FileChooser.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/DiffView.h"
#include "StandardLib/Array.h"
#include "IDEDiffTab.h"
#include "IDEEditorTab.h"


class CIDEContent;


//IntelliJ IDEA's diff viewer: a file compared with its last commit (Git, Compare with HEAD), a branch or a revision,
//or with another file (File, Compare with File), side by side in a document tab of its own.  A diff follows its
//files: edits in their editors, and new commits.
class CIDEDiffs : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEDiffs);
DESTRUCTABLE(CIDEDiffs);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>	mpGitMenu;
	CChars			mszQueuedCommand;  //The name of the command to run at the next tick.
	Ptr<CDialog>	mpDialog;  //The branch or revision asked for, while it's shown.
	Ptr<CFileChooser>	mpFileChooser;  //The file to compare with, while it's shown.
	CArray<CIDEDiffTab>			maDiffTabs;			//A tab for each diff shown.
	Ptr<CIDEDiffTab>			mpActiveDiff;		//The selected tab's diff, NULL if it isn't one.
	size						muiNextDiff;		//Each diff's pane's ID is numbered.
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	Ptr<CMenuContainer>	CreateGitMenu(void);
	void				AddFileItems(Ptr<CMenuContainer> pFile);
	void				AddCommands(Ptr<CCommandSet> pCommands);
	bool				CompareWithHead(void);
	bool				CompareWithRevision(const char* szRevision);
	bool				CompareWithFile(const char* szPath);
	bool				ShowCompareWithBranchDialog(void);
	bool				ShowCompareWithRevisionDialog(void);
	bool				ShowCompareWithFileDialog(void);
	bool				OpenDiff(const char* szLeftFile, const char* szLeftRevision, const char* szRightFile, const char* szRightRevision);
	Ptr<CIDEDiffTab>	GetActiveDiff(void);
	void				SetActiveDiff(Ptr<CIDEDiffTab> pDiff);
	size				NumDiffs(void);
	Ptr<CIDEDiffTab>	GetDiff(size uiIndex);
	void				RemoveDiff(Ptr<CIDEDiffTab> pDiff);
	void				CloseAllDiffs(void);
	void				FindProjects(void);
	void				RefreshDiff(Ptr<CIDEDiffTab> pDiff);
	void				RefreshMovedDiffs(void);
	int64				PollDiffs(int64 iNow);
	Ptr<CIDEDiffTab>	FindDiffTab(CDockPane* pcPane);

protected:
	void				HighlightDiffSide(Ptr<CDiffView> pView, ETextDiffSide eSide, size uiProject, const char* szFileName, const char* szText);
	Ptr<CIDEEditorTab>	GetComparedTab(void);
	bool				ReadDiffSide(CIDEDiffSide* pcSide, CChars* pszDest);
	void				MakeDiffTitle(CIDEDiffSide* pcSide, CIDEDiffSide* pcOther, CChars* pszDest);
	void				QueueCommand(CCommand* pcCommand);
	void				RunQueuedCommand(void);
};


#endif // __IDE_DIFFS_H__

