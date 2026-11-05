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
#ifndef __GIT_REPOSITORY_H__
#define __GIT_REPOSITORY_H__
#include "BaseLib/Chars.h"
#include "GitCommand.h"
#include "GitStatus.h"


//A git repository on disk, found from its top folder (where its .git is), and what is known of it: its status, read
//again whenever asked (StartStatus then PollStatus, or ReadStatus to wait), and the last commit's text of its files.
//Files are named by their paths, made full (MakeFullGitPath); GetRelativePath gives the path git knows a file by.
class CGitRepository
{
protected:
	CChars		mszRoot;			//The top folder, full, without a separator at the end.
	CGitCommand	mcStatusCommand;	//Reading the status, while IsReadingStatus.
	bool		mbReadingStatus;
	bool		mbStatusAgain;		//Read it again when it has been: the files changed while it was read.
	CGitStatus	mcStatus;
	size		muiStatusVersion;	//Counts the statuses read, 0 before the first.

public:
	void			Init(const char* szRoot);
	void			Kill(void);

	char*			GetRoot(void);
	bool			GetRelativePath(const char* szFullPath, CChars* pszDest);

	bool			StartStatus(void);
	bool			PollStatus(void);
	bool			ReadStatus(void);
	void			RefreshStatus(void);
	bool			IsReadingStatus(void);
	CGitStatus*		GetStatus(void);
	size			GetStatusVersion(void);
	EGitFileState	GetFileState(const char* szFullPath);

	bool			ReadHeadText(const char* szFullPath, CChars* pszDest);
	bool			ReadRevisionText(const char* szFullPath, const char* szRevision, CChars* pszDest);
	bool			ReadBranches(CArrayChars* paszDest);

protected:
	void			AddStatusArguments(CArrayChars* paszArguments);
	bool			FinishStatus(void);
};


bool FindGitRoot(const char* szPath, CChars* pszRoot);
void NormaliseGitPath(CChars* pszPath);
void MakeFullGitPath(CChars* pszPath);
bool IsGitPathIn(const char* szPath, const char* szFolder);


#endif // __GIT_REPOSITORY_H__

