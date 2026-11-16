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
#ifndef __IDE_SOLUTION_FILE_CHOOSER_H__
#define __IDE_SOLUTION_FILE_CHOOSER_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/ListContainer.h"
#include "SolutionExplorer.h"


class CIDEContent;


//A dialog to choose one of the solutions' files from: the tree the Solution Explorer shows (the solutions, their
//projects, the projects' filters and files), as IntelliJ's choosers of a class or a file show the project.  WindowLib's
//file chooser shows the disk; a refactoring that moves something wants a file the solution builds.
//
//Choose shows it over a window (a dialog's, where a dialog's button opens it), with a file selected to begin with if
//one is given.  OK, or a file double clicked or entered, chooses the selected file: only a file, and only one whose
//name ends as asked (".h" for a header), will do.
class CIDESolutionFileChooser : public CObject
{
CONSTRUCTABLE(CIDESolutionFileChooser);
DESTRUCTABLE(CIDESolutionFileChooser);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>			mpDialog;  //While it is shown.
	Ptr<CSolutionExplorer>	mpExplorer;
	CChars					mszExtensions;  //What a file's name must end with, separated by ';'; empty for any file.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Choose(Ptr<CWindow> pOwner, const char* szTitle, const char* szExtensions, const char* szSelected, CChars* pszDest);

protected:
	bool	Approve(void);
	bool	IsChoosable(const char* szPath);
	void	Activated(CListContainer* pcList);
};


#endif // __IDE_SOLUTION_FILE_CHOOSER_H__
