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
#ifndef __IDE_GIT_STATUS_H__
#define __IDE_GIT_STATUS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Button.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "GitLib/GitRepositories.h"
#include "GitLib/GitLineChanges.h"
#include "IDEEditorTab.h"


class CIDEContent;


//Git, as IntelliJ IDEA shows it: the branch on the toolbar, the files' statuses coloured in Solution Explorer, and
//change bars in the editors' gutters, a click on which shows the change's old lines with Rollback and Show Diff.
//The statuses are read again now and then, and when the window is activated.
class CIDEGitStatus : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEGitStatus);
DESTRUCTABLE(CIDEGitStatus);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	CGitRepositories			mcGit;				//The repositories the solution's files are in.
	int64						miGitStatusTime;	//When the statuses were last read, 0 to read them now.
	Ptr<CLabel>					mpBranchLabel;		//On the toolbar.
	Ptr<CIDEEditorTab>			mpChangeTab;		//The tab of the change shown.
	size						muiQueuedChange;	//A change's line to show once the editor is laid out, MAX_SIZE for none.
	int							miQueuedAction;		//Then 0 nothing, 1 Rollback or 2 Show Diff.
	CGitLineChanges				mcLineChanges;		//Worked out for one tab at a time.
	size						muiGitVersion;		//mcGit's status version when it was last shown.
	Ptr<CGridContainer>			mpChangePopup;		//A change's old lines and its buttons, while it is open.
	Ptr<CPopup>					mpChangeHolder;		//The window's popup holding it.
	bool						mbCloseChange;		//Its editor scrolled: it closes at the next tick, not in the layout that scrolled.
	Ptr<CButton>				mpRollbackButton;	//In the change popup.
	Ptr<CButton>				mpShowDiffButton;
	SGitLineChange				msChange;
	size						muiChange;			//Its index in its file's changes.
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	CGitRepositories*	GetGit(void);
	Ptr<CLabel>			CreateBranchLabel(void);
	void				TabClosing(Ptr<CIDEEditorTab> pTab);
	void				FindRepositories(void);
	void				ForgetRepositories(void);
	void				ChangeBarClicked(CCodeEditor* pcEditor, size uiLine);
	void				WindowActivated(void);
	void				QueueChange(size uiLine, int iAction);
	Ptr<CGridContainer>	GetChangePopup(void);
	Ptr<CLabel>			GetBranchLabel(void);
	void				GitFileSaved(const char* szFileName);
	void				ReadGitBase(Ptr<CIDEEditorTab> pTab);
	void				ShowBranch(void);
	void				UpdateChangeBars(Ptr<CIDEEditorTab> pTab);
	int64				PollGit(int64 iNow);

protected:
	void				ShowQueuedChange(void);
	void				ShowGitStatus(void);
	void				CloseChangePopup(void);
	void				ForgetChangePopup(void);
	void				ChangePopupClosed(void);
	void				ChangeEditorEdited(void);
	void				ChangeEditorScrolled(void);
	void				AppendBaseLines(Ptr<CIDEEditorTab> pTab, size uiStart, size uiEnd, CChars* pszDest);
	bool				ShowChange(Ptr<CIDEEditorTab> pTab, size uiLine);
	bool				RollbackChange(void);
	bool				ShowChangeDiff(void);
	void				RollbackClicked(void);
	void				ShowDiffClicked(void);
	void				ReadGitStatus(void);
};


#endif // __IDE_GIT_STATUS_H__

