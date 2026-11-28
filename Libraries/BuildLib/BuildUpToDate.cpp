/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include <sys/stat.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildUpToDate.h"


#define BUILD_COMMAND_EXTENSION	".cmd"


//////////////////////////////////////////////////////////////////////////
//
// When a file was last written, in nanoseconds where the system keeps
// them; 0 if it isn't there.
//
//////////////////////////////////////////////////////////////////////////
int64 GetBuildFileTime(const char* szFile)
{
	struct stat	sStat;

	if (stat(szFile, &sStat) != 0)
	{
		return 0;
	}
#ifdef LINUX_GNU
	return (int64)sStat.st_mtim.tv_sec * 1000000000LL + (int64)sStat.st_mtim.tv_nsec;
#else
	return (int64)sStat.st_mtime * 1000000000LL;
#endif
}


//////////////////////////////////////////////////////////////////////////
//
// A command as one text: its program, its arguments each on a line, and
// the directory it runs in.
//
//////////////////////////////////////////////////////////////////////////
static void GetBuildCommandText(SBuildCommand* psCommand, CChars* pszDest)
{
	size	i;

	pszDest->Set(psCommand->szExecutable);
	pszDest->Append('\n');
	for (i = 0; i < psCommand->aszArguments.NumElements(); i++)
	{
		pszDest->Append(psCommand->aszArguments.GetText(i));
		pszDest->Append('\n');
	}
	pszDest->Append(psCommand->szDirectory);
	pszDest->Append('\n');
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool IsBuildOutputUpToDate(SBuildCommand* psCommand, const char* szOutput, CArrayChars* paszInputs)
{
	CTextFile	cFile;
	CChars		szName;
	CChars		szCommand;
	int64		iOutput;
	int64		iInput;
	size		i;
	bool		bResult;

	iOutput = GetBuildFileTime(szOutput);
	if (iOutput == 0)
	{
		return false;
	}
	for (i = 0; i < paszInputs->NumElements(); i++)
	{
		iInput = GetBuildFileTime(paszInputs->GetText(i));
		if ((iInput == 0) || (iInput > iOutput))
		{
			return false;
		}
	}

	szName.Init(szOutput);
	szName.Append(BUILD_COMMAND_EXTENSION);
	szCommand.Init();
	GetBuildCommandText(psCommand, &szCommand);
	cFile.Init();
	bResult = (GetBuildFileTime(szName.Text()) != 0) && cFile.Read(szName.Text()) && szCommand.Equals(cFile.Text());
	cFile.Kill();
	szCommand.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void PrepareBuildOutput(SBuildCommand* psCommand, const char* szOutput)
{
	CFileUtil	cFileUtil;
	CTextFile	cFile;
	CChars		szName;
	CChars		szCommand;

	if (GetBuildFileTime(szOutput) != 0)
	{
		cFileUtil.Delete(szOutput);
	}
	szName.Init(szOutput);
	szName.Append(BUILD_COMMAND_EXTENSION);
	szCommand.Init();
	GetBuildCommandText(psCommand, &szCommand);
	cFile.Init(szCommand.Text());
	cFile.Write(szName.Text());
	cFile.Kill();
	szCommand.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The files a compile read, from the Makefile rule g++ -MMD wrote for it
// ("Source.o: Source.cpp Header.h \" and more lines): each file after the
// colon, a space in a name written "\ ".  False if the file isn't there.
//
//////////////////////////////////////////////////////////////////////////
bool ReadBuildDependencies(const char* szDependencyFile, CArrayChars* paszDest)
{
	CTextFile	cFile;
	CChars		szName;
	const char*	sz;

	cFile.Init();
	if ((GetBuildFileTime(szDependencyFile) == 0) || !cFile.Read(szDependencyFile))
	{
		cFile.Kill();
		return false;
	}
	sz = strstr(cFile.Text(), ": ");
	sz = sz ? sz + 2 : "";
	szName.Init();
	for (;; sz++)
	{
		if ((sz[0] == '\\') && (sz[1] == ' '))
		{
			szName.Append(' ');
			sz++;
		}
		else if ((sz[0] == '\\') && ((sz[1] == '\n') || (sz[1] == '\r')))
		{
			//The rule goes on on the next line.
		}
		else if ((sz[0] == ' ') || (sz[0] == '\n') || (sz[0] == '\r') || (sz[0] == '\t') || (sz[0] == '\0'))
		{
			if (!szName.Empty())
			{
				paszDest->Add(szName.Text());
				szName.Clear();
			}
			if (sz[0] == '\0')
			{
				break;
			}
		}
		else
		{
			szName.Append(sz[0]);
		}
	}
	szName.Kill();
	cFile.Kill();
	return true;
}
