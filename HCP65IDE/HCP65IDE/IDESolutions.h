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
#include "BaseLib/ArrayChars.h"
#include "WindowLib/MenuContainer.h"
#include "BuildLib/BuildWorkspace.h"
#include "BuildLib/BuildRecentFiles.h"


class CIDEContent;


//The solutions open: File's Open Project/Solution, Add Solution, Close Solution and Recent Solutions, Solution
//Explorer's Set as Startup Solution and Remove, the workspace they make (kept beside them), the git repositories their
//files are in, and the settings files kept beside the startup solution.
class CIDESolutions : public CObject
{
friend class CIDEContent;
friend class CIDEScript;
CONSTRUCTABLE(CIDESolutions);
DESTRUCTABLE(CIDESolutions);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CFileChooser>	mpFileChooser;  //The solution to open or add, while it's shown.
	CBuildRecentFiles			mcRecent;			//The solutions and workspaces opened last.
	CChars						mszRecentFile;		//Where they are kept, empty if they aren't.
	CChars						mszQueuedOpen;		//A recent one chosen, opened at the next tick (OpenQueuedSolution).
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool			OpenSolution(const char* szFileName);
	bool			OpenSolutions(CArrayChars* paszFileNames);
	bool			AddSolution(const char* szFileName);
	bool			RemoveSolution(size uiSolution);
	bool			CloseSolution(size uiSolution);
	bool			CloseSelectedSolution(void);
	bool			OpenLastSolution(void);
	void			RememberOpenSolution(void);
	bool			IsSolutionFile(size uiSolution, size uiProject);
	bool			SetStartupSolution(size uiSolution);
	bool			ShowOpenSolutionDialog(void);
	bool			ShowAddSolutionDialog(void);
	CBuildWorkspace*	GetWorkspace(void);
	void			SetRecentFile(const char* szFile);
	void			ClearRecentSolutions(void);
	bool			OpenRecentSolution(size uiIndex);
	CBuildRecentFiles*	GetRecent(void);
	void				OpenQueuedSolution(void);
	bool				IsSolutionOpen(void);
	bool				WriteWorkspace(void);
	bool				GetSettingsBaseFile(CChars* pszDest);
	char*				GetStartupFile(void);

protected:
	void				ShowRecentMenu(void);
	void				WriteRecent(void);
	bool				RecentItemClicked(CMenuItem* pcItem);
	void				LetGoOfWorkspace(void);
	void				WorkspaceChanged(bool bReindex);
};


#endif // __IDE_SOLUTIONS_H__

