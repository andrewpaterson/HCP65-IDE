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
#ifndef __IDE_DIFF_TAB_H__
#define __IDE_DIFF_TAB_H__
#include "BaseLib/Chars.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/DiffView.h"
#include "WindowLib/DockPane.h"


#define IDE_DIFF_NO_CHANGES		MAX_SIZE


//Where one side of a diff tab's text comes from: a file at a revision git knows (HEAD, a branch, a commit), or the file
//as it is (its editor's text if it is open, else what is on disk).
class CIDEDiffSide
{
public:
	CChars	mszFile;			//Its full path.
	CChars	mszRevision;		//Empty for the file as it is.
	CChars	mszHead;			//The repository's last commit when the revision's text was read.
	size	muiProject;			//The solution's project it is in, SOLUTION_EXPLORER_NO_PROJECT if none.
	size	muiSeenChanges;		//Its editor's change count when its text was read, IDE_DIFF_NO_CHANGES if not from one.

	void	Init(const char* szFile, const char* szRevision, size uiProject);
	void	Kill(void);

	bool	IsWorkingCopy(void);
};


//A tab showing a diff (CDiffView) of two sides, each from a revision or the file as it is.  It follows them: when a
//side's editor is edited its text is shown again a little after (NeedsRefresh), and a revision is read again when its
//repository's last commit changes.
class CIDEDiffTab : public CObject
{
CONSTRUCTABLE(CIDEDiffTab);
DESTRUCTABLE(CIDEDiffTab);
protected:
	Ptr<CDiffView>		mpView;
	Ptr<CDockPane>		mpDockPane;  //The document pane it is shown in.
	CIDEDiffSide		mcLeft;
	CIDEDiffSide		mcRight;
	int64				miChangedTime;	//When a side's editor was last seen to change.
	bool				mbRefresh;		//A side changed and hasn't been shown since.

public:
	void				Init(Ptr<CDiffView> pView, const char* szLeftFile, const char* szLeftRevision, size uiLeftProject, const char* szRightFile, const char* szRightRevision, size uiRightProject);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				SetDockPane(Ptr<CDockPane> pPane);
	Ptr<CDockPane>		GetDockPane(void);
	Ptr<CDiffView>		GetView(void);
	CIDEDiffSide*		GetSide(ETextDiffSide eSide);

	void				Changed(int64 iNow);
	bool				NeedsRefresh(int64 iNow, int64* piWait);
	void				Refreshed(void);
};


#endif // __IDE_DIFF_TAB_H__

