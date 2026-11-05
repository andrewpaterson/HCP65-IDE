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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "VisualStudioLib/VcxSolutionProject.h"
#include "Build.h"


//////////////////////////////////////////////////////////////////////////
//
// szRoot is where Output\ and Listing\ go, NULL for beside the solution.
// pcSolution and pcConfiguration must last as long as the build.
//
//////////////////////////////////////////////////////////////////////////
void CBuild::Init(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot)
{
	mpcSolution = pcSolution;
	mpcConfiguration = pcConfiguration;
	mszRoot.Init(szRoot ? szRoot : pcSolution->GetDirectory());
	meState = BS_NotStarted;
	mszProgress.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuild::Kill(void)
{
	mszProgress.Kill();
	mszRoot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Root\Output\<toolchain>\<configuration>: Output\W65C816\Debug for
// Debug|W65C816.  A configuration without a | is Root\Output\<name>.
//
//////////////////////////////////////////////////////////////////////////
void CBuild::GetDirectory(const char* szKind, CChars* pszDest)
{
	GetDirectory(mszRoot.Text(), mpcConfiguration, szKind, pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The szKind folder (Output, Listing) of pcConfiguration's builds of the
// solution in szRoot.
//
//////////////////////////////////////////////////////////////////////////
void CBuild::GetDirectory(const char* szRoot, CBuildConfiguration* pcConfiguration, const char* szKind, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szConfiguration;
	const char*	szName;
	const char*	szBar;

	pszDest->Set(szRoot);
	cFileUtil.AppendToPath(pszDest, szKind);
	szName = pcConfiguration->GetName();
	szBar = strchr(szName, '|');
	if (szBar)
	{
		cFileUtil.AppendToPath(pszDest, szBar + 1);
		szConfiguration.Init();
		szConfiguration.Append(szName, (size)(szBar - szName));
		cFileUtil.AppendToPath(pszDest, szConfiguration.Text());
		szConfiguration.Kill();
	}
	else
	{
		cFileUtil.AppendToPath(pszDest, szName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The projects the configuration's solution configuration builds, in the
// solution's order, with the configuration it builds each in.  A project
// that isn't loaded, or hasn't that configuration, isn't built.
//
//////////////////////////////////////////////////////////////////////////
void CBuild::GetProjects(CArrayBuildProjects* pasDest)
{
	size								i;
	CVcxSolutionProject*				pcSolutionProject;
	CVcxSolutionProjectConfiguration*	pcSolutionConfiguration;
	CVcxProject*						pcProject;
	CVcxConfiguration*					pcConfiguration;
	SBuildProject*						psProject;
	char*								szType;

	pasDest->ReInit();
	for (i = 0; i < mpcSolution->NumProjects(); i++)
	{
		pcSolutionProject = mpcSolution->GetProject(i);
		if (!pcSolutionProject->IsVcxProject() || !pcSolutionProject->IsLoaded())
		{
			continue;
		}
		pcProject = pcSolutionProject->GetProject();
		pcSolutionConfiguration = pcSolutionProject->FindConfiguration(mpcConfiguration->GetSolutionConfiguration());
		if (pcSolutionConfiguration)
		{
			if (!pcSolutionConfiguration->IsBuild())
			{
				continue;
			}
			pcConfiguration = pcProject->FindConfiguration(pcSolutionConfiguration->GetProjectConfiguration());
		}
		else
		{
			pcConfiguration = pcProject->FindConfiguration(mpcConfiguration->GetSolutionConfiguration());
		}
		if (!pcConfiguration)
		{
			continue;
		}

		szType = pcConfiguration->GetProperty("ConfigurationType");
		psProject = pasDest->Add();
		psProject->pcProject = pcProject;
		psProject->pcConfiguration = pcConfiguration;
		psProject->bLibrary = szType && (StringInsensitiveCompare(szType, "StaticLibrary") == 0);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The build has finished: its last line says how.
//
//////////////////////////////////////////////////////////////////////////
void CBuild::Finish(EBuildState eState, CArrayChars* paszLines)
{
	CChars	szLine;

	meState = eState;
	szLine.Init(mpcConfiguration->GetName());
	if (eState == BS_Succeeded)
	{
		szLine.Append(" build succeeded.");
	}
	else if (eState == BS_Cancelled)
	{
		szLine.Append(" build cancelled.");
	}
	else
	{
		szLine.Append(" build failed.");
	}
	paszLines->Add(szLine.Text());
	mszProgress.Set(szLine);
	szLine.Kill();
}


EBuildState CBuild::GetState(void) { return meState; }
bool CBuild::IsRunning(void) { return meState == BS_Running; }
char* CBuild::GetProgress(void) { return mszProgress.Text(); }  //How the build is going, a line.
void CBuild::SetProgress(const char* szProgress) { mszProgress.Set(szProgress); }
void CBuild::GetOutputDirectory(CChars* pszDest) { GetDirectory("Output", pszDest); }


//////////////////////////////////////////////////////////////////////////
//
// Where a build of the solution in szRoot writes pcConfiguration's image,
// or its executable.  False if it has neither.
//
//////////////////////////////////////////////////////////////////////////
bool CBuild::GetProgramPath(const char* szRoot, CBuildConfiguration* pcConfiguration, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	if (!pcConfiguration->IsLinked())
	{
		return false;
	}
	GetDirectory(szRoot, pcConfiguration, "Output", pszDest);
	cFileUtil.AppendToPath(pszDest, pcConfiguration->HasImage() ? pcConfiguration->GetImage()->GetFile() : pcConfiguration->GetExecutable()->GetFile());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the build writes its program's line table (CAsm65LineTable, which
// source line each address is for): beside the image or executable, as
// <program>.lines.  False if it builds neither.
//
//////////////////////////////////////////////////////////////////////////
bool CBuild::GetLineTablePath(const char* szRoot, CBuildConfiguration* pcConfiguration, CChars* pszDest)
{
	if (!GetProgramPath(szRoot, pcConfiguration, pszDest))
	{
		return false;
	}
	pszDest->Append(".lines");
	return true;
}
void CBuild::GetListingDirectory(CChars* pszDest) { GetDirectory("Listing", pszDest); }

