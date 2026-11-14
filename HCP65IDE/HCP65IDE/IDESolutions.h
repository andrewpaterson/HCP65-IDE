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
#ifndef __IDE_SOLUTIONS_H__
#define __IDE_SOLUTIONS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/FileChooser.h"
#include "WindowLib/CommandSet.h"
#include "BaseLib/ArrayChars.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/DockManager.h"
#include "SolutionExplorer.h"
#include "BuildLib/BuildWorkspace.h"
#include "BaseLib/RecentFiles.h"


class CIDEContent;


//The solutions open: File's Open Project/Solution, Add Solution, Close Solution and Recent Solutions, Solution
//Explorer's Set as Startup Solution and Remove, the workspace they make (kept beside them), the git repositories their
//files are in, and the settings files kept beside the startup solution.
class CIDESolutions : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDESolutions);
DESTRUCTABLE(CIDESolutions);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>		mpRecentMenu;  //File, Recent Solutions: an item for each, or one saying there are none.
	Ptr<CSolutionExplorer>	mpExplorer;  //The solutions' projects, folders and files.
	Ptr<CDockPane>			mpExplorerTool;
	Ptr<CMenuContainer>		mpExplorerMenu;  //A solution's: Set as Startup Solution and Remove.
	Ptr<CFileChooser>		mpFileChooser;  //The solution to open or add, while it's shown.
	CRecentFiles			mcRecent;			//The solutions and workspaces opened last.
	CChars					mszRecentFile;		//Where they are kept, empty if they aren't.
	CChars					mszQueuedCommand;	//The name of the command to run at the next tick.
	CChars					mszQueuedOpen;		//A recent one chosen, opened at the next tick (OpenQueuedSolution).
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void					AddFileItems(Ptr<CMenuContainer> pFile);
	void					CreateExplorerPane(SDockPlace* psPlace);
	void					ExplorerActivated(void);
	bool					ShowActiveFileInExplorer(void);
	void					AddCommands(Ptr<CCommandSet> pCommands);
	Ptr<CSolutionExplorer>	GetExplorer(void);
	Ptr<CDockPane>			GetExplorerTool(void);
	Ptr<CMenuContainer>		GetExplorerMenu(void);
	bool					OpenSolution(const char* szFileName);
	bool					OpenSolutions(CArrayChars* paszFileNames);
	bool					AddSolution(const char* szFileName);
	bool					RemoveSolution(size uiSolution);
	bool					CloseSolution(size uiSolution);
	bool					CloseSelectedSolution(void);
	bool					OpenLastSolution(void);
	void					RememberOpenSolution(void);
	bool					IsSolutionFile(size uiSolution, size uiProject);
	bool					SetStartupSolution(size uiSolution);
	bool					SetStartupProject(size uiSolution, size uiProject);
	bool					CanSetStartupProject(void);
	bool					ShowOpenSolutionDialog(void);
	bool					ShowAddSolutionDialog(void);
	void					SetRecentFile(const char* szFile);
	void					ClearRecentSolutions(void);
	bool					OpenRecentSolution(size uiIndex);
	bool					IsSolutionOpen(void);
	bool					WriteWorkspace(void);
	void					WorkspaceChanged(bool bReindex);
	bool					GetSettingsBaseFile(CChars* pszDest);
	char*					GetStartupFile(void);
	void					ShowRecentMenu(void);

protected:
	Ptr<CMenuContainer>	CreateExplorerMenu(void);
	void				WriteRecent(void);
	void				LetGoOfWorkspace(void);
	CRecentFiles*		GetRecent(void);
	void				OpenQueuedSolution(void);
	void				QueueCommand(CCommand* pcCommand);
	void				RunQueuedCommand(void);
	void				RecentItemClicked(CMenuItem* pcItem);
};


#endif // __IDE_SOLUTIONS_H__

