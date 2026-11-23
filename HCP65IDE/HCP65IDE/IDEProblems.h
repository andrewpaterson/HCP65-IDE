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
#ifndef __IDE_PROBLEMS_H__
#define __IDE_PROBLEMS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/TabbedContainer.h"
#include "IDEResultsPane.h"
#include "IDESteppable.h"
#include "IDEProblemList.h"


class CIDEContent;
class CIDEEditorTab;


//The Problems tool window, as IntelliJ IDEA's, beside Build and Indexing.  Its Current File tab is what the code
//model's check found in the active editor's file (CIDECodeModel::CheckEditedFile, done after each pause in its edits):
//errors, then warnings, each with its place.  Its Project tab is what the last build said: its errors and warnings,
//by file (from the Build tab's messages, CIDEMessageOutput), cleared while a build runs.  Each is a results pane: a
//double click, enter or Jump to Source (F4) opens a problem's place, and Next and Previous Occurrence step through
//the tab shown.  The status line's tick keeps them up to date (Update).
class CIDEProblems : public CObject, public CIDEResultsSource, public CIDESteppable
{
CONSTRUCTABLE(CIDEProblems);
DESTRUCTABLE(CIDEProblems);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CTabbedContainer>	mpTabs;
	Ptr<CIDEResultsPane>	mpFilePane;
	Ptr<CIDEResultsPane>	mpProjectPane;
	Ptr<CTabComponent>		mpFileTab;
	Ptr<CTabComponent>		mpProjectTab;
	Ptr<CDockPane>			mpTool;

	CChars					mszShownFile;  //The Current File tab's file, empty for none.
	size					muiShownSet;  //Its tab's problems' set count when shown (CIDEEditorTab::GetProblemsSet).
	bool					mbShownChecked;
	bool					mbShownAny;  //The tab has been shown at least once.
	CIDEProblemList			mcShown;  //Its problems as shown, for their places.

	size					muiShownBuild;  //The build the Project tab shows (CIDEBuilding::NumBuilds).
	bool					mbShownBuilding;  //It says a build is running.
	CArrayChars				maszProjectFiles;  //Each project problem's file.
	CIDEProblemList			mcProject;  //The last build's problems, by file then place.

public:
	void				Init(CIDEContent* pcContent);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				CreatePane(SDockPlace* psPlace);
	bool				Update(void);
	Ptr<CDockPane>		GetTool(void) const;
	Ptr<CIDEResultsPane>	GetFilePane(void) const;
	Ptr<CIDEResultsPane>	GetProjectPane(void) const;
	void				ShowFileTab(bool bFile);
	bool				IsFileTabShown(void);

	bool				GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;

	Ptr<CDockPane>		GetSteppablePane(void) const override;
	const char*			GetSteppableName(void) override;
	bool				HasSteps(void) override;
	bool				Step(bool bNext, bool bKeepKeys) override;

protected:
	void				UpdateFile(void);
	void				UpdateProject(void);
	void				ShowProblems(Ptr<CIDEResultsPane> pPane, CIDEProblemList* pcProblems, CArrayChars* paszFiles, const char* szFileName, const char* szNone);
	Ptr<CIDEResultsPane>	GetShownPane(void);
	static void			AppendCount(CChars* psz, size uiCount, const char* szOne, const char* szMany);
};


#endif // __IDE_PROBLEMS_H__

