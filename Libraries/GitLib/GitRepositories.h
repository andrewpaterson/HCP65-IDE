/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela GitLib

Codaphela GitLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela GitLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela GitLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __GIT_REPOSITORIES_H__
#define __GIT_REPOSITORIES_H__
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/MapStringInt.h"
#include "GitRepository.h"


typedef CArrayTemplatePtr<CGitRepository>	CArrayGitRepositoryPtr;


//The repositories the files of a solution are in, which can be several (IntelliJ IDEA's version control roots): a
//solution's projects can come from beside it and from other repositories.  A repository is added the first time a file
//in it is asked about, and which repository each folder is in (or that it is in none) is kept so asking again doesn't
//look at the disk.
class CGitRepositories
{
protected:
	CArrayGitRepositoryPtr	mapcRepositories;
	CMapStringInt			mcFolders;		//Each folder looked up, and its repository's index, -1 for none.

public:
	void			Init(void);
	void			Kill(void);
	void			RemoveAll(void);

	CGitRepository*	FindRepository(const char* szFileName);
	size			NumRepositories(void);
	CGitRepository*	GetRepository(size uiIndex);

	void			StartStatus(void);
	bool			RefreshStatus(const char* szFileName);
	bool			PollStatus(void);
	bool			IsReadingStatus(void);
	size			GetStatusVersion(void);
	EGitFileState	GetFileState(const char* szFileName);

protected:
	CGitRepository*	AddRepository(const char* szRoot);
};


#endif // __GIT_REPOSITORIES_H__

