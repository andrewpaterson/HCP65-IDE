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
#include <stdlib.h>
#include <string.h>
#include "GitStatus.h"


const char* gaszGitFileStateNames[NUM_GIT_FILE_STATES] =
{
	"Unmodified",
	"Modified",
	"Added",
	"Deleted",
	"Renamed",
	"Untracked",
	"Ignored",
	"Conflicted",
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool GitStartsWith(const char* szString, const char* szStart)
{
	return strncmp(szString, szStart, strlen(szStart)) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitStatus::Init(void)
{
	mszPaths.Init();
	masFiles.Init();
	mcFiles.Init();
	mszBranch.Init();
	mszHead.Init();
	mszUpstream.Init();
	miAhead = 0;
	miBehind = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitStatus::Kill(void)
{
	mszUpstream.Kill();
	mszHead.Kill();
	mszBranch.Kill();
	mcFiles.Kill();
	masFiles.Kill();
	mszPaths.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitStatus::Clear(void)
{
	mszPaths.Clear();
	masFiles.ReInit();
	mcFiles.Kill();
	mcFiles.Init();
	mszBranch.Clear();
	mszHead.Clear();
	mszUpstream.Clear();
	miAhead = 0;
	miBehind = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Replaces the status with the one in szOutput, uiLength bytes of
// "git status --porcelain=v2 -z --branch" output: entries each ended by
// a '\0', a renamed file's entry followed by the path it was renamed
// from.  Entries of kinds it doesn't know are skipped.  False (and the
// status is left cleared) if an entry is cut short.
//
//////////////////////////////////////////////////////////////////////////
bool CGitStatus::Parse(const char* szOutput, size uiLength)
{
	const char*	szLine;
	const char*	szNext;
	const char*	szEnd;
	CChars		szCopy;
	bool		bResult;

	Clear();

	//Entries are used as strings: a copy ends with a '\0' even if git's output was cut short.
	szCopy.Init();
	szCopy.Append(szOutput, uiLength);
	szLine = szCopy.Text();
	szEnd = szLine + uiLength;
	bResult = true;
	while (bResult && (szLine < szEnd))
	{
		szNext = szLine + strlen(szLine) + 1;
		switch (szLine[0])
		{
		case '#':
			bResult = ParseHeader(szLine);
			break;
		case '1':
			bResult = ParseChanged(szLine, 8, &szNext, szEnd);
			break;
		case '2':
			bResult = ParseChanged(szLine, 9, &szNext, szEnd);
			break;
		case 'u':
			bResult = ParseChanged(szLine, 10, &szNext, szEnd);
			break;
		case '?':
			bResult = ParseOther(szLine, GFS_Untracked, '?');
			break;
		case '!':
			bResult = ParseOther(szLine, GFS_Ignored, '!');
			break;
		}
		szLine = szNext;
	}
	szCopy.Kill();

	if (!bResult)
	{
		Clear();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// "# branch.oid <commit>" ("(initial)" before the first commit),
// "# branch.head <branch>" ("(detached)"), "# branch.upstream <branch>"
// and "# branch.ab +<ahead> -<behind>".  Other headers are skipped.
//
//////////////////////////////////////////////////////////////////////////
bool CGitStatus::ParseHeader(const char* szLine)
{
	const char*	szValue;

	if (GitStartsWith(szLine, "# branch.oid "))
	{
		szValue = szLine + strlen("# branch.oid ");
		mszHead.Clear();
		if (strcmp(szValue, "(initial)") != 0)
		{
			mszHead.Append(szValue);
		}
	}
	else if (GitStartsWith(szLine, "# branch.head "))
	{
		szValue = szLine + strlen("# branch.head ");
		mszBranch.Clear();
		if (strcmp(szValue, "(detached)") != 0)
		{
			mszBranch.Append(szValue);
		}
	}
	else if (GitStartsWith(szLine, "# branch.upstream "))
	{
		mszUpstream.Clear();
		mszUpstream.Append(szLine + strlen("# branch.upstream "));
	}
	else if (GitStartsWith(szLine, "# branch.ab +"))
	{
		szValue = szLine + strlen("# branch.ab +");
		miAhead = atoi(szValue);
		szValue = strchr(szValue, '-');
		if (szValue == NULL)
		{
			return false;
		}
		miBehind = atoi(szValue + 1);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// An entry of a changed file: its kind ('1' ordinary, '2' renamed or
// copied, 'u' unmerged), its XY, then fields up to iFields, then its path
// to the end.  A renamed file's original path is the next entry, which
// *pszNext is moved past.
//
//////////////////////////////////////////////////////////////////////////
bool CGitStatus::ParseChanged(const char* szLine, int iFields, const char** pszNext, const char* szEnd)
{
	const char*		szPath;
	const char*		szOriginal;
	EGitFileState	eState;

	if ((strlen(szLine) < 4) || (szLine[1] != ' '))
	{
		return false;
	}
	szPath = SkipFields(szLine, iFields);
	if (szPath == NULL)
	{
		return false;
	}

	szOriginal = NULL;
	if (szLine[0] == '2')
	{
		if (*pszNext >= szEnd)
		{
			return false;
		}
		szOriginal = *pszNext;
		*pszNext = szOriginal + strlen(szOriginal) + 1;
	}

	eState = GetGitChangedState(szLine[0], szLine[2], szLine[3]);
	AddFile(szPath, eState, szLine[2], szLine[3], szOriginal);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// "? <path>" or "! <path>".
//
//////////////////////////////////////////////////////////////////////////
bool CGitStatus::ParseOther(const char* szLine, EGitFileState eState, char cMark)
{
	if ((szLine[1] != ' ') || (szLine[2] == '\0'))
	{
		return false;
	}
	AddFile(&szLine[2], eState, cMark, cMark, NULL);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What follows the first iFields fields of szLine (each ended by a
// space), NULL if it has fewer.
//
//////////////////////////////////////////////////////////////////////////
const char* CGitStatus::SkipFields(const char* szLine, int iFields)
{
	int		i;

	for (i = 0; i < iFields; i++)
	{
		szLine = strchr(szLine, ' ');
		if (szLine == NULL)
		{
			return NULL;
		}
		szLine++;
	}
	return (*szLine != '\0') ? szLine : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitStatus::AddFile(const char* szPath, EGitFileState eState, char cIndex, char cWorkTree, const char* szOriginalPath)
{
	SGitFileStatus*	psFile;

	mcFiles.Put(szPath, (int32)masFiles.NumElements());
	psFile = masFiles.Add();
	psFile->eState = eState;
	psFile->cIndex = cIndex;
	psFile->cWorkTree = cWorkTree;
	psFile->uiPath = mszPaths.Length();
	mszPaths.Append(szPath, strlen(szPath) + 1);
	psFile->uiOriginalPath = MAX_SIZE;
	if (szOriginalPath)
	{
		psFile->uiOriginalPath = mszPaths.Length();
		mszPaths.Append(szOriginalPath, strlen(szOriginalPath) + 1);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file listed as szPath, NULL if it isn't (it may be in a folder that
// is listed).
//
//////////////////////////////////////////////////////////////////////////
SGitFileStatus* CGitStatus::FindFile(const char* szPath)
{
	int32	iIndex;

	iIndex = mcFiles.Get(szPath, -1);
	return (iIndex >= 0) ? masFiles.Get(iIndex) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// szPath's state: its own if it is listed, otherwise that of the closest
// folder it is in that is listed as untracked or ignored, otherwise
// unmodified.
//
//////////////////////////////////////////////////////////////////////////
EGitFileState CGitStatus::GetFileState(const char* szPath)
{
	SGitFileStatus*	psFile;
	CChars			szFolder;
	EGitFileState	eState;
	size			uiLength;

	psFile = FindFile(szPath);
	if (psFile)
	{
		return psFile->eState;
	}

	eState = GFS_Unmodified;
	szFolder.Init(szPath);
	for (uiLength = szFolder.Length(); uiLength > 0; uiLength--)
	{
		if (szFolder.GetChar(uiLength - 1) == '/')
		{
			szFolder.SetLength(uiLength);
			psFile = FindFile(szFolder.Text());
			if (psFile && ((psFile->eState == GFS_Untracked) || (psFile->eState == GFS_Ignored)))
			{
				eState = psFile->eState;
				break;
			}
		}
	}
	szFolder.Kill();
	return eState;
}


//////////////////////////////////////////////////////////////////////////
//
// A changed file's state from its entry's kind ('1', '2' or 'u') and XY:
// unmerged is conflicted; renamed (R) and copied (C, a new file) by their
// kind; added to the index is added even if changed since; deleted on
// either side is deleted; anything else is modified.
//
//////////////////////////////////////////////////////////////////////////
EGitFileState GetGitChangedState(char cKind, char cIndex, char cWorkTree)
{
	if (cKind == 'u')
	{
		return GFS_Conflicted;
	}
	if (cIndex == 'R')
	{
		return GFS_Renamed;
	}
	if ((cIndex == 'A') || (cIndex == 'C'))
	{
		return GFS_Added;
	}
	if ((cIndex == 'D') || (cWorkTree == 'D'))
	{
		return GFS_Deleted;
	}
	return GFS_Modified;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* GetGitFileStateName(EGitFileState eState)
{
	if ((eState < 0) || (eState >= NUM_GIT_FILE_STATES))
	{
		return "Unknown";
	}
	return gaszGitFileStateNames[eState];
}


size CGitStatus::NumFiles(void) { return masFiles.NumElements(); }
SGitFileStatus* CGitStatus::GetFile(size uiIndex) { return masFiles.SafeGet(uiIndex); }
char* CGitStatus::GetPath(SGitFileStatus* psFile) { return mszPaths.Text(psFile->uiPath); }
char* CGitStatus::GetOriginalPath(SGitFileStatus* psFile) { return (psFile->uiOriginalPath != MAX_SIZE) ? mszPaths.Text(psFile->uiOriginalPath) : NULL; }
char* CGitStatus::GetBranch(void) { return mszBranch.Text(); }
char* CGitStatus::GetHead(void) { return mszHead.Text(); }
char* CGitStatus::GetUpstream(void) { return mszUpstream.Text(); }
bool CGitStatus::IsDetached(void) { return mszBranch.Empty() && !mszHead.Empty(); }
int32 CGitStatus::GetAhead(void) { return miAhead; }
int32 CGitStatus::GetBehind(void) { return miBehind; }

