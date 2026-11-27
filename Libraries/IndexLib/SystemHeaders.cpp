/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "SystemHeaders.h"


#ifdef LINUX_GNU
//The Windows SDK and MSVC library headers aren't on Linux; the MinGW ones stand in for them: the C++ library's in the
//folder of the newest MinGW GCC installed, then its C library's, then GCC's own.  GCC's own come last, unlike GCC's
//order: they expect __GNUC__, which the index (being MSVC) doesn't define, and found first their stdarg.h defines
//va_start again differently to the _mingw_stdarg.h it includes.  Last, only those the C library hasn't are found there
//(stdbool.h, the intrinsics), as MSVC's own are.
#define MINGW_GCC_DIRECTORY		"/usr/lib/gcc/x86_64-w64-mingw32"
#define MINGW_INCLUDE_DIRECTORY	"/usr/share/mingw-w64/include"
const char* gaszMinGWGCCLibraryDirectories[] =
{
	"include/c++",
	"include/c++/x86_64-w64-mingw32",
	NULL
};
const char* gaszMinGWGCCOwnDirectories[] =
{
	"include",
	NULL
};


//A GCC configuration is read with Linux's headers, where g++ looks for them (% is the newest GCC's version).
#define LINUX_GCC_DIRECTORY		"/usr/lib/gcc/x86_64-linux-gnu"
const char* gaszLinuxGCCDirectories[] =
{
	"/usr/include/c++/%",
	"/usr/include/x86_64-linux-gnu/c++/%",
	"/usr/lib/gcc/x86_64-linux-gnu/%/include",
	"/usr/include/x86_64-linux-gnu",
	"/usr/include",
	NULL
};


//////////////////////////////////////////////////////////////////////////
//
// Whether the MinGW GCC folder called szName ("13-win32") is newer than
// the one called szOther, NULL for none.  A folder whose name doesn't start
// with a version isn't.  Of one version, win32 threads are preferred.
//
//////////////////////////////////////////////////////////////////////////
static bool IsNewerMinGWVersion(const char* szName, const char* szOther)
{
	int	aiName[3] = { 0, 0, 0 };
	int	aiOther[3] = { 0, 0, 0 };
	int	i;

	if (sscanf(szName, "%d.%d.%d", &aiName[0], &aiName[1], &aiName[2]) < 1)
	{
		return false;
	}
	if (szOther == NULL)
	{
		return true;
	}
	sscanf(szOther, "%d.%d.%d", &aiOther[0], &aiOther[1], &aiOther[2]);
	for (i = 0; i < 3; i++)
	{
		if (aiName[i] != aiOther[i])
		{
			return aiName[i] > aiOther[i];
		}
	}
	return (strstr(szName, "win32") != NULL) && (strstr(szOther, "win32") == NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// The folders called paszDirectories (to NULL) in the MinGW GCC folder
// szGCC, added to paszDest; false if any isn't there (it is logged).
//
//////////////////////////////////////////////////////////////////////////
static bool AddMinGWDirectories(const char* szGCC, const char** paszDirectories, CArrayChars* paszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	size		i;
	bool		bFound;

	bFound = true;
	szName.Init();
	for (i = 0; paszDirectories[i] != NULL; i++)
	{
		szName.Set(szGCC);
		szName.Append('/');
		szName.Append(paszDirectories[i]);
		if (cFileUtil.IsDirectory(szName.Text()))
		{
			paszDest->Add(szName.Text());
		}
		else
		{
			bFound = false;
			gcLogger.Info2(__METHOD__, " System headers [", szName.Text(), "] aren't there.", NULL);
		}
	}
	szName.Kill();
	return bFound;
}
#endif


#ifdef _WIN32
//The folders Visual Studio is installed in, each holding <version>\<edition>\VC\Tools\MSVC\<toolset>.
static const char* gaszVisualStudioDirectories[] = { "C:/Program Files (x86)/Microsoft Visual Studio", "C:/Program Files/Microsoft Visual Studio", NULL };
#define WINDOWS_KITS_INCLUDE_DIRECTORY	"C:/Program Files (x86)/Windows Kits/10/Include"
//The Windows SDK's folders MSVC's INCLUDE has, in its order (after the toolset's own include).
static const char* gaszWindowsKitsDirectories[] = { "ucrt", "shared", "um", NULL };


//////////////////////////////////////////////////////////////////////////
//
// Whether the version szName ("14.29.30133", "10.0.19041.0") is newer
// than szOther, NULL for none.  A name that doesn't start with a number
// isn't.
//
//////////////////////////////////////////////////////////////////////////
static bool IsNewerVersion(const char* szName, const char* szOther)
{
	int	aiName[4] = { 0, 0, 0, 0 };
	int	aiOther[4] = { 0, 0, 0, 0 };
	int	i;

	if (sscanf(szName, "%d.%d.%d.%d", &aiName[0], &aiName[1], &aiName[2], &aiName[3]) < 1)
	{
		return false;
	}
	if (szOther == NULL)
	{
		return true;
	}
	sscanf(szOther, "%d.%d.%d.%d", &aiOther[0], &aiOther[1], &aiOther[2], &aiOther[3]);
	for (i = 0; i < 4; i++)
	{
		if (aiName[i] != aiOther[i])
		{
			return aiName[i] > aiOther[i];
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The full path of szDirectory's folder with the newest version name
// that has szInside in it (szInside may be ""), appended to pszDest.
// False if it has none.
//
//////////////////////////////////////////////////////////////////////////
static bool FindNewestVersionDirectory(const char* szDirectory, const char* szInside, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszFound;
	CChars		szName;
	CChars		szParent;
	CChars		szNewest;
	CChars		szPath;
	size		i;
	bool		bFound;

	aszFound.Init();
	szName.Init();
	szParent.Init();
	szNewest.Init();
	szPath.Init();
	if (cFileUtil.IsDirectory(szDirectory))
	{
		cFileUtil.FindAllDirectories(szDirectory, &aszFound);
	}
	for (i = 0; i < aszFound.NumElements(); i++)
	{
		szName.Set("");
		szParent.Set("");
		cFileUtil.SplitPath(aszFound.GetText(i), &szName, &szParent);
		szPath.Set(aszFound.GetText(i));
		szPath.Append('/');
		szPath.Append(szInside);
		if (cFileUtil.IsDirectory(szPath.Text()) && IsNewerVersion(szName.Text(), szNewest.Empty() ? NULL : szNewest.Text()))
		{
			szNewest.Set(szName);
		}
	}
	bFound = !szNewest.Empty();
	if (bFound)
	{
		pszDest->Append(szDirectory);
		pszDest->Append('/');
		pszDest->Append(szNewest);
	}
	szPath.Kill();
	szNewest.Kill();
	szParent.Kill();
	szName.Kill();
	aszFound.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The newest MSVC toolset's include folder (VC\Tools\MSVC\<toolset>\
// include) of any Visual Studio installed, set in pszDest.  False if
// there is none.
//
//////////////////////////////////////////////////////////////////////////
static bool FindVisualStudioInclude(CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszVersions;
	CArrayChars	aszEditions;
	CChars		szTools;
	CChars		szToolset;
	CChars		szNewest;
	CChars		szName;
	CChars		szParent;
	size		i;
	size		j;
	size		k;

	szNewest.Init();
	szName.Init();
	szParent.Init();
	for (i = 0; gaszVisualStudioDirectories[i] != NULL; i++)
	{
		aszVersions.Init();
		if (cFileUtil.IsDirectory(gaszVisualStudioDirectories[i]))
		{
			cFileUtil.FindAllDirectories(gaszVisualStudioDirectories[i], &aszVersions);
		}
		for (j = 0; j < aszVersions.NumElements(); j++)
		{
			aszEditions.Init();
			cFileUtil.FindAllDirectories(aszVersions.GetText(j), &aszEditions);
			for (k = 0; k < aszEditions.NumElements(); k++)
			{
				szTools.Init(aszEditions.GetText(k));
				szTools.Append("/VC/Tools/MSVC");
				szToolset.Init();
				if (FindNewestVersionDirectory(szTools.Text(), "include", &szToolset))
				{
					szName.Set("");
					szParent.Set("");
					cFileUtil.SplitPath(szToolset.Text(), &szName, &szParent);
					if (szNewest.Empty() || IsNewerVersion(szName.Text(), szNewest.Text()))
					{
						szNewest.Set(szName);
						pszDest->Set(szToolset);
						pszDest->Append("/include");
					}
				}
				szToolset.Kill();
				szTools.Kill();
			}
			aszEditions.Kill();
		}
		aszVersions.Kill();
	}
	szParent.Kill();
	szName.Kill();
	szNewest.Kill();
	return !pszDest->Empty();
}
#endif


//////////////////////////////////////////////////////////////////////////
//
// The compiler's own headers on this machine, those that exist, added to
// paszDest; those that don't are logged and the result is false, as
// without them almost nothing can be parsed: MinGW's on Linux, and on
// Windows the newest Visual Studio MSVC toolset's and Windows SDK's, in
// MSVC's INCLUDE order.  For checking, HCP65_IDE_SYSTEM_ROOT is where
// MinGW's are looked for in place of the root folder.
//
//////////////////////////////////////////////////////////////////////////
bool FindSystemHeaderDirectories(CArrayChars* paszDest)
{
	bool		bFound;

	bFound = true;
#ifdef LINUX_GNU
	CFileUtil	cFileUtil;
	CArrayChars	aszFound;
	CChars		szRoot;
	CChars		szPath;
	CChars		szName;
	CChars		szNewest;
	CChars		szParent;
	size		i;

	szRoot.Init((getenv("HCP65_IDE_SYSTEM_ROOT") != NULL) ? getenv("HCP65_IDE_SYSTEM_ROOT") : "");
	szPath.Init();
	szName.Init();
	szParent.Init();
	szNewest.Init();
	aszFound.Init();
	szPath.Set(szRoot);
	szPath.Append(MINGW_GCC_DIRECTORY);
	if (cFileUtil.IsDirectory(szPath.Text()))
	{
		cFileUtil.FindAllDirectories(szPath.Text(), &aszFound);
	}
	for (i = 0; i < aszFound.NumElements(); i++)
	{
		szName.Set("");
		szParent.Set("");
		cFileUtil.SplitPath(aszFound.GetText(i), &szName, &szParent);
		if (IsNewerMinGWVersion(szName.Text(), szNewest.Empty() ? NULL : szNewest.Text()))
		{
			szNewest.Set(szName);
		}
	}
	if (!szNewest.Empty())
	{
		szPath.Append('/');
		szPath.Append(szNewest);
		bFound &= AddMinGWDirectories(szPath.Text(), gaszMinGWGCCLibraryDirectories, paszDest);
	}
	else
	{
		bFound = false;
		gcLogger.Info2(__METHOD__, " No MinGW GCC is in [", szPath.Text(), "].", NULL);
	}
	szName.Set(szRoot);
	szName.Append(MINGW_INCLUDE_DIRECTORY);
	if (cFileUtil.IsDirectory(szName.Text()))
	{
		paszDest->Add(szName.Text());
	}
	else
	{
		bFound = false;
		gcLogger.Info2(__METHOD__, " System headers [", szName.Text(), "] aren't there.", NULL);
	}
	if (!szNewest.Empty())
	{
		bFound &= AddMinGWDirectories(szPath.Text(), gaszMinGWGCCOwnDirectories, paszDest);
	}
	aszFound.Kill();
	szNewest.Kill();
	szParent.Kill();
	szName.Kill();
	szPath.Kill();
	szRoot.Kill();
#endif
#ifdef _WIN32
	//As MSVC's INCLUDE has them: the toolset's own headers, then the Windows SDK's.
	CFileUtil	cFileUtil;
	CChars		szInclude;
	CChars		szKits;
	CChars		szName;
	size		i;

	szInclude.Init();
	if (FindVisualStudioInclude(&szInclude))
	{
		szInclude.Replace('\\', '/');
		paszDest->Add(szInclude.Text());
	}
	else
	{
		bFound = false;
		gcLogger.Info2(__METHOD__, " No Visual Studio MSVC toolset was found.", NULL);
	}
	szInclude.Kill();

	szKits.Init();
	if (FindNewestVersionDirectory(WINDOWS_KITS_INCLUDE_DIRECTORY, "ucrt", &szKits))
	{
		szKits.Replace('\\', '/');
		szName.Init();
		for (i = 0; gaszWindowsKitsDirectories[i] != NULL; i++)
		{
			szName.Set(szKits);
			szName.Append('/');
			szName.Append(gaszWindowsKitsDirectories[i]);
			if (cFileUtil.IsDirectory(szName.Text()))
			{
				paszDest->Add(szName.Text());
			}
			else
			{
				bFound = false;
				gcLogger.Info2(__METHOD__, " System headers [", szName.Text(), "] aren't there.", NULL);
			}
		}
		szName.Kill();
	}
	else
	{
		bFound = false;
		gcLogger.Info2(__METHOD__, " No Windows SDK is in [", WINDOWS_KITS_INCLUDE_DIRECTORY, "].", NULL);
	}
	szKits.Kill();
#endif
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// GCC's own headers for Linux on this machine, added to paszDest in g++'s
// order: the newest GCC's C++ library, GCC's own, then the C library's.
// False if GCC isn't here (as on Windows, where a GCC configuration isn't
// built) or one of them isn't.  HCP65_IDE_SYSTEM_ROOT is where they are
// looked for in place of the root folder.
//
//////////////////////////////////////////////////////////////////////////
bool FindGCCHeaderDirectories(CArrayChars* paszDest)
{
#ifdef LINUX_GNU
	CFileUtil	cFileUtil;
	CArrayChars	aszFound;
	CChars		szRoot;
	CChars		szName;
	CChars		szParent;  //Then the version.
	CChars		szPath;
	int			iNewest;
	size		i;
	bool		bFound;

	szRoot.Init((getenv("HCP65_IDE_SYSTEM_ROOT") != NULL) ? getenv("HCP65_IDE_SYSTEM_ROOT") : "");
	szPath.Init(szRoot);
	szPath.Append(LINUX_GCC_DIRECTORY);
	szName.Init();
	szParent.Init();
	aszFound.Init();
	if (cFileUtil.IsDirectory(szPath.Text()))
	{
		cFileUtil.FindAllDirectories(szPath.Text(), &aszFound);
	}
	iNewest = 0;
	for (i = 0; i < aszFound.NumElements(); i++)
	{
		szName.Set("");
		szParent.Set("");
		cFileUtil.SplitPath(aszFound.GetText(i), &szName, &szParent);
		if (atoi(szName.Text()) > iNewest)
		{
			iNewest = atoi(szName.Text());
		}
	}
	aszFound.Kill();
	szParent.Set("");
	szParent.Append(iNewest);

	bFound = iNewest > 0;
	for (i = 0; bFound && (gaszLinuxGCCDirectories[i] != NULL); i++)
	{
		szName.Set(gaszLinuxGCCDirectories[i]);
		szName.Replace("%", szParent.Text());
		szName.Insert(0, szRoot.Text());
		if (cFileUtil.IsDirectory(szName.Text()))
		{
			paszDest->Add(szName.Text());
		}
		else
		{
			bFound = false;
			gcLogger.Info2(__METHOD__, " GCC's headers [", szName.Text(), "] aren't there.", NULL);
		}
	}
	szParent.Kill();
	szName.Kill();
	szPath.Kill();
	szRoot.Kill();
	return bFound;
#else
	(void)paszDest;
	return false;
#endif
}
