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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "VisualStudioLib/VcxSolutionProject.h"
#include "MSVCBuild.h"


//////////////////////////////////////////////////////////////////////////
//
// MSBuild Solution.sln /p:Configuration=Debug /p:Platform=Win32 /m
// /nologo /v:minimal, in the solution's directory.
//
//////////////////////////////////////////////////////////////////////////
bool CMSVCBuild::AddCommands(CArrayChars* paszLines)
{
	CChars			szMSBuild;
	CChars			szArgument;
	CChars			szDescription;
	CFileUtil		cFileUtil;
	SBuildCommand*	psCommand;
	const char*		szSolutionConfiguration;
	const char*		szBar;

#ifndef _WIN32
	paszLines->Add("MSVC builds need Windows.");
	return false;
#endif

	szMSBuild.Init();
	FindMSBuild(&szMSBuild);
	szDescription.Init("MSBuild ");
	szDescription.Append(mpcConfiguration->GetSolutionConfiguration());
	psCommand = AddCommand(szMSBuild.Text(), mpcSolution->GetDirectory(), szDescription.Text());
	szDescription.Kill();
	szMSBuild.Kill();

	//A project opened on its own is in a solution that isn't on the disk: the project is built.
	if (!cFileUtil.Exists(mpcSolution->GetFileName()) && (mpcSolution->NumProjects() == 1))
	{
		psCommand->aszArguments.Add(mpcSolution->GetProject(0)->GetFullPath());
	}
	else
	{
		psCommand->aszArguments.Add(mpcSolution->GetFileName());
	}
	szSolutionConfiguration = mpcConfiguration->GetSolutionConfiguration();
	szBar = strchr(szSolutionConfiguration, '|');
	szArgument.Init("/p:Configuration=");
	if (szBar)
	{
		szArgument.Append(szSolutionConfiguration, (size)(szBar - szSolutionConfiguration));
		psCommand->aszArguments.Add(szArgument.Text());
		szArgument.Set("/p:Platform=");
		szArgument.Append(szBar + 1);
	}
	else
	{
		szArgument.Append(szSolutionConfiguration);
	}
	psCommand->aszArguments.Add(szArgument.Text());
	szArgument.Kill();
	psCommand->aszArguments.Add("/m");
	psCommand->aszArguments.Add("/nologo");
	psCommand->aszArguments.Add("/v:minimal");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The newest Visual Studio's MSBuild.exe, as vswhere finds it, otherwise
// MSBuild.exe (on the path).  False if vswhere didn't find one.
//
//////////////////////////////////////////////////////////////////////////
bool CMSVCBuild::FindMSBuild(CChars* pszDest)
{
	CProcess	cProcess;
	CArrayChars	aszArguments;
	CArrayChars	aszLines;
	CChars		szVSWhere;
	CFileUtil	cFileUtil;
	const char*	szProgramFiles;
	bool		bFound;

	bFound = false;
	szProgramFiles = getenv("ProgramFiles(x86)");
	szVSWhere.Init(szProgramFiles ? szProgramFiles : "");
	cFileUtil.AppendToPath(&szVSWhere, "Microsoft Visual Studio\\Installer\\vswhere.exe");
	if (szProgramFiles && cFileUtil.Exists(szVSWhere.Text()))
	{
		aszArguments.Init();
		aszArguments.Add("-latest");
		aszArguments.Add("-requires");
		aszArguments.Add("Microsoft.Component.MSBuild");
		aszArguments.Add("-find");
		aszArguments.Add("MSBuild\\**\\Bin\\MSBuild.exe");
		aszLines.Init();
		cProcess.Init();
		if (cProcess.Start(szVSWhere.Text(), &aszArguments))
		{
			cProcess.Wait(&aszLines);
			if ((cProcess.GetExitCode() == 0) && (aszLines.NumElements() > 0) && cFileUtil.Exists(aszLines.GetText(0)))
			{
				pszDest->Set(aszLines.GetText(0));
				bFound = true;
			}
		}
		cProcess.Kill();
		aszLines.Kill();
		aszArguments.Kill();
	}
	szVSWhere.Kill();

	if (!bFound)
	{
		pszDest->Set("MSBuild.exe");
	}
	return bFound;
}

