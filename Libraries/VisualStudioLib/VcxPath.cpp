/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/WindowsHeaders.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/ArrayChars.h"
#include "VcxPath.h"


//////////////////////////////////////////////////////////////////////////
//
// The full path of szRelative (a path from a Visual Studio file) from
// szDirectory appended to pszDest, with the case of the files on disk.
// szRelative may also be absolute.  False if it doesn't exist, in which
// case the path is appended as it was written.
//
//////////////////////////////////////////////////////////////////////////
bool ResolveVcxPath(const char* szDirectory, const char* szRelative, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szPath;
	bool		bResult;

	szPath.Init(szRelative);
	szPath.Replace('\\', FILE_SEPARATOR[0]);
	if (!cFileUtil.IsAbsolutePath(szPath.Text()))
	{
		szPath.Insert(0, FILE_SEPARATOR[0]);
		szPath.Insert(0, szDirectory);
	}
	cFileUtil.CollapsePath(&szPath);
	bResult = FindVcxPathCaseInsensitive(&szPath);
	pszDest->Append(szPath);
	szPath.Kill();
	return bResult;
}


#ifdef _WIN32
//////////////////////////////////////////////////////////////////////////
//
// The existing full path pszPath changed to the case of its files and
// directories on disk (Windows finds them in any case, but shows them in
// theirs).  The drive is left as it is.
//
//////////////////////////////////////////////////////////////////////////
static void VcxPathDiskCase(CChars* pszPath)
{
	CFileUtil			cFileUtil;
	CArrayChars			aszComponents;
	CChars				szFound;
	CChars				szName;
	WIN32_FIND_DATA		sFindData;
	HANDLE				hFind;
	const char*			szComponent;
	size				i;

	aszComponents.Init();
	cFileUtil.SplitPath(pszPath->Text(), &aszComponents);
	if (aszComponents.NumElements() == 0)
	{
		aszComponents.Kill();
		return;
	}

	szFound.Init(aszComponents.GetText(0));
	for (i = 1; i < aszComponents.NumElements(); i++)
	{
		szComponent = aszComponents.GetText(i);
		szName.Init(szFound);
		cFileUtil.AppendToPath(&szName, szComponent);
		hFind = INVALID_HANDLE_VALUE;
		if ((strchr(szComponent, '*') == NULL) && (strchr(szComponent, '?') == NULL))
		{
			hFind = FindFirstFile(szName.Text(), &sFindData);
		}
		if (hFind != INVALID_HANDLE_VALUE)
		{
			FindClose(hFind);
			cFileUtil.AppendToPath(&szFound, sFindData.cFileName);
		}
		else
		{
			szFound.Set(szName);
		}
		szName.Kill();
	}
	pszPath->Set(szFound);
	szFound.Kill();
	aszComponents.Kill();
}
#endif // _WIN32


//////////////////////////////////////////////////////////////////////////
//
// Changes the full path pszPath to the case of the file or directory on
// disk when it only exists in another case.  Windows finds it in any case
// (its case on disk is looked up); Linux file names are case sensitive.
// False (and pszPath unchanged) if there is no such file in any case.
//
//////////////////////////////////////////////////////////////////////////
bool FindVcxPathCaseInsensitive(CChars* pszPath)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszComponents;
	CArrayChars	aszEntries;
	CChars		szFound;
	CChars		szName;
	CChars		szEntryName;
	CChars		szEntryDirectory;
	size		i;
	size		j;
	bool		bFound;

	if (cFileUtil.Exists(pszPath->Text()))
	{
#ifdef _WIN32
		VcxPathDiskCase(pszPath);
#endif // _WIN32
		return true;
	}

	aszComponents.Init();
	cFileUtil.SplitPath(pszPath->Text(), &aszComponents);
	if (aszComponents.NumElements() == 0)
	{
		aszComponents.Kill();
		return false;
	}

	szFound.Init(aszComponents.GetText(0));
	bFound = cFileUtil.Exists(szFound.Text());
	for (i = 1; bFound && (i < aszComponents.NumElements()); i++)
	{
		szName.Init(szFound);
		cFileUtil.AppendToPath(&szName, aszComponents.GetText(i));
		if (cFileUtil.Exists(szName.Text()))
		{
			szFound.Set(szName);
		}
		else
		{
			bFound = false;
			aszEntries.Init();
			cFileUtil.FindAllDirectories(szFound.Text(), &aszEntries);
			cFileUtil.FindAllFiles(szFound.Text(), &aszEntries);
			for (j = 0; j < aszEntries.NumElements(); j++)
			{
				szEntryName.Init();
				szEntryDirectory.Init();
				cFileUtil.SplitPath(aszEntries.GetText(j), &szEntryName, &szEntryDirectory);
				if (szEntryName.EqualsIgnoreCase(aszComponents.GetText(i)))
				{
					szFound.Set(aszEntries.GetText(j));
					bFound = true;
				}
				szEntryDirectory.Kill();
				szEntryName.Kill();
				if (bFound)
				{
					break;
				}
			}
			aszEntries.Kill();
		}
		szName.Kill();
	}

	if (bFound)
	{
		pszPath->Set(szFound);
	}
	szFound.Kill();
	aszComponents.Kill();
	return bFound;
}



//////////////////////////////////////////////////////////////////////////
//
// A project's or solution's file szFileName as its full path, the
// directory it is in and its name without the extension, as Solution
// Explorer shows it.  Each replaces what was in it.
//
//////////////////////////////////////////////////////////////////////////
void SplitVcxFileName(const char* szFileName, CChars* pszFullPath, CChars* pszDirectory, CChars* pszName)
{
	CFileUtil	cFileUtil;

	pszFullPath->Set(szFileName);
	cFileUtil.FullPath(pszFullPath);
	pszDirectory->Clear();
	pszName->Clear();
	cFileUtil.SplitPath(pszFullPath->Text(), pszName, pszDirectory);
	cFileUtil.RemoveExtension(pszName);
}
