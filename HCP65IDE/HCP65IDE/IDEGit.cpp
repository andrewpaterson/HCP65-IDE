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
#include "SupportLib/ColourARGB32.h"
#include "IDEGit.h"


//IntelliJ IDEA's (dark theme) colours for files by what has happened to them since the last commit.  An unmodified
//file is in the theme's text colour (GetGitStateFont gives none for it).
#define IDE_GIT_MODIFIED	Set32BitColour((uint8)0x68, (uint8)0x97, (uint8)0xBB)
#define IDE_GIT_ADDED		Set32BitColour((uint8)0x62, (uint8)0x97, (uint8)0x55)
#define IDE_GIT_DELETED		Set32BitColour((uint8)0x6C, (uint8)0x6C, (uint8)0x6C)
#define IDE_GIT_UNTRACKED	Set32BitColour((uint8)0xD1, (uint8)0x67, (uint8)0x5A)
#define IDE_GIT_IGNORED		Set32BitColour((uint8)0x84, (uint8)0x85, (uint8)0x04)
#define IDE_GIT_CONFLICTED	Set32BitColour((uint8)0xD5, (uint8)0x75, (uint8)0x6C)


//////////////////////////////////////////////////////////////////////////
//
// The colour a file's name is shown in, by its state.
//
//////////////////////////////////////////////////////////////////////////
ARGB32 GetGitStateColour(EGitFileState eState)
{
	switch (eState)
	{
	case GFS_Modified:
	case GFS_Renamed:
		return IDE_GIT_MODIFIED;
	case GFS_Added:
		return IDE_GIT_ADDED;
	case GFS_Deleted:
		return IDE_GIT_DELETED;
	case GFS_Untracked:
		return IDE_GIT_UNTRACKED;
	case GFS_Ignored:
		return IDE_GIT_IGNORED;
	case GFS_Conflicted:
		return IDE_GIT_CONFLICTED;
	default:
		return 0;  //Unmodified files aren't coloured: they are in the text's colour.
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The font a file's name is shown in, by its state: NULL (the theme's) if
// it is unmodified.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CFont> GetGitStateFont(Ptr<CFontColours> pColours, EGitFileState eState)
{
	if (eState == GFS_Unmodified)
	{
		return NULL;
	}
	return pColours->GetFont(GetGitStateColour(eState));
}

