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
#ifndef __IDE_SCOPE_H__
#define __IDE_SCOPE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"


class CIDEContent;


//Where Find Usages and Find in Files look, as IntelliJ's scopes.
enum EIDEScope
{
	IS_Solution,		//Every solution of the workspace.
	IS_StartupSolution,	//The startup solution alone (a program without the kernal it runs on).
	IS_Project,			//The active file's project.
	IS_CurrentFile,		//The file the editor shows.
	IS_OpenFiles,		//The files with an editor's tab.
	IS_Directory,		//The files in a directory, and under it.
	IS_Count
};


//A scope, made for where the IDE is when something is looked for (Capture): which it is, with the active file, its
//project, the files that are open and the directory typed, as they were then, so what was found in it stays the same
//whatever is opened after.  It says whether a file is in it (Find Usages leaves out the usages that aren't) and what
//its files are (Find in Files searches them).  The kind can be changed, keeping what was captured (SetScope), as a
//tool window shows what it found in another scope.
//
//A file no project has (a header only included) is in Solution alone, of the scopes that are of projects.
class CIDEScope
{
protected:
	EIDEScope	meScope;
	size		muiProject;		//The workspace's project the active file was in, MAX_SIZE if none.
	size		muiSolution;	//The startup solution.
	CChars		mszFile;		//The active file, empty if none.
	CChars		mszDirectory;	//Without a separator at its end.
	CArrayChars	maszOpenFiles;

public:
	void		Init(void);
	void		Kill(void);
	void		Copy(CIDEScope* pcSource);

	void		Capture(CIDEContent* pcContent, EIDEScope eScope, const char* szDirectory);
	void		SetScope(EIDEScope eScope);
	EIDEScope	GetScope(void);
	char*		GetDirectory(void);
	size		GetProject(void);
	char*		GetFile(void);

	bool		Contains(CIDEContent* pcContent, const char* szFileName);
	void		ListFiles(CIDEContent* pcContent, CArrayChars* paszFiles, CArrayChars* paszDirectories);
	void		AppendName(CIDEContent* pcContent, CChars* pszDest);
	void		AppendDescription(CIDEContent* pcContent, CChars* pszDest);

	static const char*	GetScopeName(EIDEScope eScope);
	static bool			FindScope(const char* szName, EIDEScope* peScope);
	static bool			IsOffered(CIDEContent* pcContent, EIDEScope eScope);
};


#endif // __IDE_SCOPE_H__

