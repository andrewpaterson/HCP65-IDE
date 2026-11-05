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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/NativeThread.h"
#include "GitRepository.h"


//////////////////////////////////////////////////////////////////////////
//
// szRoot is the repository's top folder (FindGitRoot finds it).
//
//////////////////////////////////////////////////////////////////////////
void CGitRepository::Init(const char* szRoot)
{
	mszRoot.Init(szRoot);
	MakeFullGitPath(&mszRoot);
	mcStatusCommand.Init();
	mbReadingStatus = false;
	mbStatusAgain = false;
	mcStatus.Init();
	muiStatusVersion = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// A status still being read is stopped.
//
//////////////////////////////////////////////////////////////////////////
void CGitRepository::Kill(void)
{
	mcStatus.Kill();
	mcStatusCommand.Kill();
	mszRoot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Appends the path git knows szFullPath by: relative to the top folder,
// with '/'s (nothing for the top folder).  False if it isn't in the
// repository.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::GetRelativePath(const char* szFullPath, CChars* pszDest)
{
	CChars	szPath;
	size	uiStart;
	bool	bResult;

	szPath.Init(szFullPath);
	MakeFullGitPath(&szPath);
	bResult = IsGitPathIn(szPath.Text(), mszRoot.Text());
	uiStart = mszRoot.EndsWith('/') ? mszRoot.Length() : mszRoot.Length() + 1;
	if (bResult && (uiStart < szPath.Length()))
	{
		pszDest->Append(szPath.Text(uiStart));
	}
	szPath.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Starts reading the status again.  The last status read is kept until
// the new one has been (PollStatus).  False if it couldn't be started or
// is already being read.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::StartStatus(void)
{
	CArrayChars	aszArguments;

	if (mbReadingStatus)
	{
		return false;
	}
	aszArguments.Init();
	AddStatusArguments(&aszArguments);
	mbReadingStatus = mcStatusCommand.Start(mszRoot.Text(), &aszArguments);
	aszArguments.Kill();
	return mbReadingStatus;
}


//////////////////////////////////////////////////////////////////////////
//
// True while the status is being read.  When it has been, it replaces the
// last one and the status version goes up; if git failed the last status
// is kept.  A refresh asked for while it was read starts it again.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::PollStatus(void)
{
	if (!mbReadingStatus)
	{
		return false;
	}
	if (mcStatusCommand.Poll())
	{
		return true;
	}
	FinishStatus();
	if (mbStatusAgain)
	{
		mbStatusAgain = false;
		return StartStatus();
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The files have changed (one was saved): the status is read again.  If
// it is being read already it is read again after, as what is being read
// may be from before the change.
//
//////////////////////////////////////////////////////////////////////////
void CGitRepository::RefreshStatus(void)
{
	if (mbReadingStatus)
	{
		mbStatusAgain = true;
	}
	else
	{
		StartStatus();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the status and waits for it.  True if it was read.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::ReadStatus(void)
{
	size	uiVersion;

	uiVersion = muiStatusVersion;
	if (!mbReadingStatus && !StartStatus())
	{
		return false;
	}
	while (PollStatus())
	{
		ThreadSleep(2);
	}
	return muiStatusVersion != uiVersion;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::FinishStatus(void)
{
	CChars*	pszOutput;
	bool	bResult;

	mbReadingStatus = false;
	if (!mcStatusCommand.Succeeded())
	{
		return false;
	}
	pszOutput = mcStatusCommand.GetOutput();
	bResult = mcStatus.Parse(pszOutput->Text(), pszOutput->Length());
	muiStatusVersion++;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Untracked folders are listed whole, as are ignored ones (--ignored
// lists them the traditional way).
//
//////////////////////////////////////////////////////////////////////////
void CGitRepository::AddStatusArguments(CArrayChars* paszArguments)
{
	paszArguments->Add("status");
	paszArguments->Add("--porcelain=v2");
	paszArguments->Add("-z");
	paszArguments->Add("--branch");
	paszArguments->Add("--ignored");
	paszArguments->Add("--untracked-files=normal");
}


//////////////////////////////////////////////////////////////////////////
//
// szFullPath's state in the last status read, unmodified if it isn't in
// the repository.
//
//////////////////////////////////////////////////////////////////////////
EGitFileState CGitRepository::GetFileState(const char* szFullPath)
{
	CChars			szRelative;
	EGitFileState	eState;

	szRelative.Init();
	eState = GFS_Unmodified;
	if (GetRelativePath(szFullPath, &szRelative))
	{
		eState = mcStatus.GetFileState(szRelative.Text());
	}
	szRelative.Kill();
	return eState;
}


//////////////////////////////////////////////////////////////////////////
//
// Replaces pszDest with szFullPath's text in the last commit, as it would
// be checked out (line ends converted as the repository's attributes
// say).  False if it isn't in the last commit, or there are no commits.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::ReadHeadText(const char* szFullPath, CChars* pszDest)
{
	return ReadRevisionText(szFullPath, "HEAD", pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// As ReadHeadText, for any revision git knows: a commit, a branch, a tag,
// "HEAD~2".  False if the file isn't in it or there is no such revision.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepository::ReadRevisionText(const char* szFullPath, const char* szRevision, CChars* pszDest)
{
	CGitCommand	cCommand;
	CArrayChars	aszArguments;
	CChars		szObject;
	bool		bResult;

	szObject.Init(szRevision);
	szObject.Append(':');
	if (!GetRelativePath(szFullPath, &szObject))
	{
		szObject.Kill();
		return false;
	}

	aszArguments.Init();
	aszArguments.Add("cat-file");
	aszArguments.Add("--filters");
	aszArguments.Add(szObject.Text());
	cCommand.Init();
	bResult = cCommand.Run(mszRoot.Text(), &aszArguments);
	if (bResult)
	{
		pszDest->Clear();
		pszDest->Append(cCommand.GetOutput()->Text(), cCommand.GetOutput()->Length());
	}
	cCommand.Kill();
	aszArguments.Kill();
	szObject.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Finds the top folder of the repository szPath (a file or folder) is in:
// the closest folder up from it with a .git in it (a folder, or a file in
// a work tree or submodule).  False if it isn't in one.
//
//////////////////////////////////////////////////////////////////////////
bool FindGitRoot(const char* szPath, CChars* pszRoot)
{
	CFileUtil	cFileUtil;
	CChars		szFolder;
	CChars		szGit;
	size		uiSlash;
	bool		bFound;

	szFolder.Init(szPath);
	MakeFullGitPath(&szFolder);
	if (!cFileUtil.IsDirectory(szFolder.Text()))
	{
		uiSlash = szFolder.FindFromEnd('/');
		if (uiSlash == ARRAY_ELEMENT_NOT_FOUND)
		{
			szFolder.Kill();
			return false;
		}
		szFolder.SetLength(((uiSlash == 0) || (szFolder.GetChar(uiSlash - 1) == ':')) ? uiSlash + 1 : uiSlash);
	}

	bFound = false;
	szGit.Init();
	for (;;)
	{
		szGit.Clear();
		szGit.Append(szFolder);
		if (!szGit.EndsWith('/'))
		{
			szGit.Append('/');
		}
		szGit.Append(".git");
		if (cFileUtil.Exists(szGit.Text()))
		{
			bFound = true;
			break;
		}

		//Up a folder, stopping after the top of the drive.
		if (szFolder.EndsWith('/'))
		{
			break;
		}
		uiSlash = szFolder.FindFromEnd('/');
		if (uiSlash == ARRAY_ELEMENT_NOT_FOUND)
		{
			break;
		}
		szFolder.SetLength(((uiSlash == 0) || (szFolder.GetChar(uiSlash - 1) == ':')) ? uiSlash + 1 : uiSlash);
	}

	if (bFound)
	{
		pszRoot->Clear();
		pszRoot->Append(szFolder);
	}
	szGit.Kill();
	szFolder.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Separators are '/'s, as git writes them (Windows takes them too), and
// a folder doesn't end with one unless it is the top of a drive.
//
//////////////////////////////////////////////////////////////////////////
void NormaliseGitPath(CChars* pszPath)
{
	size	uiLength;

	pszPath->Replace('\\', '/');
	uiLength = pszPath->Length();
	while ((uiLength > 1) && (pszPath->GetChar(uiLength - 1) == '/') && (pszPath->GetChar(uiLength - 2) != ':'))
	{
		uiLength--;
	}
	pszPath->SetLength(uiLength);
}


//////////////////////////////////////////////////////////////////////////
//
// The path made full (from the current folder if it is relative), without
// "." or ".." folders, and normalised.
//
//////////////////////////////////////////////////////////////////////////
void MakeFullGitPath(CChars* pszPath)
{
	CFileUtil	cFileUtil;

	cFileUtil.FixSeparators(pszPath);
	cFileUtil.FullPath(pszPath);
	NormaliseGitPath(pszPath);
}


//////////////////////////////////////////////////////////////////////////
//
// True if szPath is szFolder or in it; both normalised.  Windows paths
// are compared ignoring case.
//
//////////////////////////////////////////////////////////////////////////
bool IsGitPathIn(const char* szPath, const char* szFolder)
{
	size	uiLength;
	int		iCompare;

	uiLength = strlen(szFolder);
#ifdef _WIN32
	iCompare = _strnicmp(szPath, szFolder, uiLength);
#else
	iCompare = strncmp(szPath, szFolder, uiLength);
#endif
	if (iCompare != 0)
	{
		return false;
	}
	return (szPath[uiLength] == '\0') || (szPath[uiLength] == '/') || ((uiLength > 0) && (szFolder[uiLength - 1] == '/'));
}


char* CGitRepository::GetRoot(void) { return mszRoot.Text(); }
bool CGitRepository::IsReadingStatus(void) { return mbReadingStatus; }
CGitStatus* CGitRepository::GetStatus(void) { return &mcStatus; }
size CGitRepository::GetStatusVersion(void) { return muiStatusVersion; }

