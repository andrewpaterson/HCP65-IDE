/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65Build

Codaphela HCP65Build is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65Build is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65Build.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifdef _WIN32
#include "BaseLib/WindowsHeaders.h"
#else
#include <unistd.h>
#include <limits.h>
#endif
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/FileUtil.h"
#include "BuildLib/BuildCommandLine.h"


//////////////////////////////////////////////////////////////////////////
//
// The directory hcp65build.exe is in, empty if it can't be found.
//
//////////////////////////////////////////////////////////////////////////
static void GetToolDirectory(CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	CChars		szPath;
	char		szExecutable[4096];
	size		uiLength;

#ifdef _WIN32
	uiLength = (size)GetModuleFileNameA(NULL, szExecutable, (DWORD)sizeof(szExecutable));
	if (uiLength >= sizeof(szExecutable))
	{
		uiLength = 0;
	}
#else
	ssize_t	iLength;

	iLength = readlink("/proc/self/exe", szExecutable, sizeof(szExecutable) - 1);
	uiLength = (iLength > 0) ? (size)iLength : 0;
#endif
	szExecutable[uiLength] = '\0';

	pszDest->Set("");
	if (uiLength > 0)
	{
		szPath.Init(szExecutable);
		szName.Init();
		cFileUtil.SplitPath(szPath.Text(), &szName, pszDest);
		szName.Kill();
		szPath.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// hcp65build <file.sln | file.vcxproj | file.hcp65workspace> [options]:
// see CBuildCommandLine.
//
//////////////////////////////////////////////////////////////////////////
int __cdecl main(int argc, char* argv[])
{
	CBuildCommandLine	cCommandLine;
	CChars				szTool;
	int					iResult;

	TypesInit();
	TypeConverterInit();
	MemoryInit();

	cCommandLine.Init();
	if (!cCommandLine.Parse(argc, argv))
	{
		printf("Run hcp65build -help for its options.\n");
		iResult = BUILD_EXIT_USAGE;
	}
	else
	{
		szTool.Init();
		GetToolDirectory(&szTool);
		iResult = cCommandLine.Run(szTool.Empty() ? NULL : szTool.Text());
		szTool.Kill();
	}
	cCommandLine.Kill();

	MemoryKill();
	TypeConverterKill();
	TypesKill();
	return iResult;
}

