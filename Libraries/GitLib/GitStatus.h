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
#ifndef __GIT_STATUS_H__
#define __GIT_STATUS_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "BaseLib/MapStringInt.h"


//What has happened to a file since the last commit, as IntelliJ IDEA colours it.  A file git doesn't list is unmodified.
enum EGitFileState
{
	GFS_Unmodified,
	GFS_Modified,
	GFS_Added,			//New in the index (staged), whether or not it has changed since.
	GFS_Deleted,
	GFS_Renamed,
	GFS_Untracked,
	GFS_Ignored,
	GFS_Conflicted,		//Unmerged, both sides changed it.

	NUM_GIT_FILE_STATES
};


//A file git listed.  Its path is relative to the repository's top folder with '/'s, as git writes it.
struct SGitFileStatus
{
	EGitFileState	eState;
	char			cIndex;			//git's X: what changed in the index ('.' for nothing, '?' untracked, '!' ignored).
	char			cWorkTree;		//git's Y: what changed in the work tree since the index.
	size			uiPath;			//In the status's paths.
	size			uiOriginalPath;	//The path it was renamed or copied from, MAX_SIZE if it wasn't.
};


typedef CArrayTemplate<SGitFileStatus>	CArrayGitFileStatus;


//A repository's status: its branch and the files that differ from the last commit, read from the output of
//"git status --porcelain=v2 -z --branch --ignored".  git lists a folder of only untracked or only ignored files as the
//folder (a path ending in '/'), so a file's state is found from it or the folders it is in.
class CGitStatus
{
protected:
	CChars				mszPaths;		//Each path ended by '\0'.
	CArrayGitFileStatus	masFiles;
	CMapStringInt		mcFiles;		//Each path's index in masFiles.
	CChars				mszBranch;		//Empty when the head is detached.
	CChars				mszHead;		//The commit checked out, empty if there are none yet.
	CChars				mszUpstream;	//Empty if the branch has none.
	int32				miAhead;
	int32				miBehind;

public:
	void			Init(void);
	void			Kill(void);
	void			Clear(void);

	bool			Parse(const char* szOutput, size uiLength);

	size			NumFiles(void);
	SGitFileStatus*	GetFile(size uiIndex);
	char*			GetPath(SGitFileStatus* psFile);
	char*			GetOriginalPath(SGitFileStatus* psFile);
	SGitFileStatus*	FindFile(const char* szPath);
	EGitFileState	GetFileState(const char* szPath);

	char*			GetBranch(void);
	char*			GetHead(void);
	char*			GetUpstream(void);
	bool			IsDetached(void);
	int32			GetAhead(void);
	int32			GetBehind(void);

protected:
	bool			ParseHeader(const char* szLine);
	bool			ParseChanged(const char* szLine, int iFields, const char** pszNext, const char* szEnd);
	bool			ParseOther(const char* szLine, EGitFileState eState, char cMark);
	const char*		SkipFields(const char* szLine, int iFields);
	void			AddFile(const char* szPath, EGitFileState eState, char cIndex, char cWorkTree, const char* szOriginalPath);
};


EGitFileState GetGitChangedState(char cKind, char cIndex, char cWorkTree);
const char* GetGitFileStateName(EGitFileState eState);


#endif // __GIT_STATUS_H__

