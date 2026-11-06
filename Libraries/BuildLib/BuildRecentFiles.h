/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __BUILD_RECENT_FILES_H__
#define __BUILD_RECENT_FILES_H__
#include "BaseLib/ArrayChars.h"


#define BUILD_RECENT_FILES_DEFAULT	10


//The files opened last (File, Recent Solutions), the latest first, each once (whatever its case), at most a number
//of them, and the one that was open when the IDE was closed (opened again when it starts), if one was.  They are kept
//in a file of the user's:
//
//	<RecentFiles Open="D:\gameengine\HCP65\...\HCP65Kernal.hcp65workspace">
//		<File Path="D:\gameengine\HCP65\...\HCP65ModPlay.sln"/>
//	</RecentFiles>
class CBuildRecentFiles
{
protected:
	CArrayChars	maszFiles;
	size		muiMaxFiles;
	CChars		mszOpen;

public:
	void	Init(size uiMaxFiles = BUILD_RECENT_FILES_DEFAULT);
	void	Kill(void);

	void	Add(const char* szFile);
	bool	Remove(const char* szFile);
	void	Clear(void);
	size	Find(const char* szFile);
	size	NumFiles(void);
	char*	GetFile(size uiIndex);
	void	SetOpen(const char* szFile);
	char*	GetOpen(void);

	bool	Read(const char* szFile);
	bool	Write(const char* szFile);
};


#endif // __BUILD_RECENT_FILES_H__

