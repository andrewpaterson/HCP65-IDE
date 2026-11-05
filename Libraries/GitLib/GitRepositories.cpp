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
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "GitRepositories.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitRepositories::Init(void)
{
	mapcRepositories.Init();
	mcFolders.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitRepositories::Kill(void)
{
	RemoveAll();
	mcFolders.Kill();
	mapcRepositories.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Forgets every repository, as when another solution is opened.
//
//////////////////////////////////////////////////////////////////////////
void CGitRepositories::RemoveAll(void)
{
	size			i;
	CGitRepository*	pcRepository;

	for (i = 0; i < mapcRepositories.NumElements(); i++)
	{
		pcRepository = mapcRepositories.GetPtr(i);
		SafeKill(pcRepository);
	}
	mapcRepositories.ReInit();
	mcFolders.Kill();
	mcFolders.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// The repository the file szFileName (a full path) is in, added if it is
// the first file asked about in it.  NULL if it isn't in one.
//
//////////////////////////////////////////////////////////////////////////
CGitRepository* CGitRepositories::FindRepository(const char* szFileName)
{
	CChars			szFolder;
	CChars			szRoot;
	CGitRepository*	pcRepository;
	size			uiSlash;
	int32			iIndex;

	szFolder.Init(szFileName);
	MakeFullGitPath(&szFolder);
	uiSlash = szFolder.FindFromEnd('/');
	if (uiSlash != ARRAY_ELEMENT_NOT_FOUND)
	{
		szFolder.SetLength(uiSlash);
	}

	iIndex = mcFolders.Get(szFolder.Text(), -2);
	if (iIndex == -2)
	{
		szRoot.Init();
		pcRepository = NULL;
		if (FindGitRoot(szFileName, &szRoot))
		{
			pcRepository = AddRepository(szRoot.Text());
		}
		iIndex = pcRepository ? (int32)mapcRepositories.Find(pcRepository) : -1;
		mcFolders.Put(szFolder.Text(), iIndex);
		szRoot.Kill();
	}
	szFolder.Kill();
	return (iIndex >= 0) ? mapcRepositories.GetPtr(iIndex) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The repository with its top folder at szRoot, added if it isn't known.
//
//////////////////////////////////////////////////////////////////////////
CGitRepository* CGitRepositories::AddRepository(const char* szRoot)
{
	size			i;
	CGitRepository*	pcRepository;
	CChars			szNormal;

	szNormal.Init(szRoot);
	MakeFullGitPath(&szNormal);
	for (i = 0; i < mapcRepositories.NumElements(); i++)
	{
		pcRepository = mapcRepositories.GetPtr(i);
		if (szNormal.Equals(pcRepository->GetRoot()))
		{
			szNormal.Kill();
			return pcRepository;
		}
	}
	szNormal.Kill();

	pcRepository = NewMalloc<CGitRepository>();
	pcRepository->Init(szRoot);
	mapcRepositories.Add(pcRepository);
	return pcRepository;
}


//////////////////////////////////////////////////////////////////////////
//
// Starts reading each repository's status that isn't being read already.
//
//////////////////////////////////////////////////////////////////////////
void CGitRepositories::StartStatus(void)
{
	size	i;

	for (i = 0; i < mapcRepositories.NumElements(); i++)
	{
		if (!mapcRepositories.GetPtr(i)->IsReadingStatus())
		{
			mapcRepositories.GetPtr(i)->StartStatus();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file szFileName changed (it was saved): its repository's status is
// read again (see CGitRepository::RefreshStatus).  False if it isn't in a
// repository.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepositories::RefreshStatus(const char* szFileName)
{
	CGitRepository*	pcRepository;

	pcRepository = FindRepository(szFileName);
	if (pcRepository == NULL)
	{
		return false;
	}
	pcRepository->RefreshStatus();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Polls each status being read.  True while any is.
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepositories::PollStatus(void)
{
	size	i;
	bool	bReading;

	bReading = false;
	for (i = 0; i < mapcRepositories.NumElements(); i++)
	{
		if (mapcRepositories.GetPtr(i)->PollStatus())
		{
			bReading = true;
		}
	}
	return bReading;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CGitRepositories::IsReadingStatus(void)
{
	size	i;

	for (i = 0; i < mapcRepositories.NumElements(); i++)
	{
		if (mapcRepositories.GetPtr(i)->IsReadingStatus())
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Changes whenever a status is read or a repository is added, so whatever
// shows the statuses knows to show them again.
//
//////////////////////////////////////////////////////////////////////////
size CGitRepositories::GetStatusVersion(void)
{
	size	i;
	size	uiVersion;

	uiVersion = mapcRepositories.NumElements();
	for (i = 0; i < mapcRepositories.NumElements(); i++)
	{
		uiVersion += mapcRepositories.GetPtr(i)->GetStatusVersion();
	}
	return uiVersion;
}


//////////////////////////////////////////////////////////////////////////
//
// The state of szFileName (a full path) in its repository's last status,
// unmodified if it isn't in a repository.
//
//////////////////////////////////////////////////////////////////////////
EGitFileState CGitRepositories::GetFileState(const char* szFileName)
{
	CGitRepository*	pcRepository;

	pcRepository = FindRepository(szFileName);
	if (pcRepository == NULL)
	{
		return GFS_Unmodified;
	}
	return pcRepository->GetFileState(szFileName);
}


size CGitRepositories::NumRepositories(void) { return mapcRepositories.NumElements(); }
CGitRepository* CGitRepositories::GetRepository(size uiIndex) { return mapcRepositories.SafeGetPtr(uiIndex); }

