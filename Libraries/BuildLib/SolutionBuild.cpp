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
#include "BaseLib/FileUtil.h"
#include "BuildConfigurations.h"
#include "SolutionBuild.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::Init(void)
{
	mbRebuild = false;
	mpcBuild = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A build still running is stopped (an HCP65 build is waited for).
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::Kill(void)
{
	End();
}


//////////////////////////////////////////////////////////////////////////
//
// The last build is let go.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::End(void)
{
	if (mpcBuild)
	{
		mpcBuild->Cancel();
		mpcBuild->Kill();
		mpcBuild = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A solution's build configurations were once in a file beside it, which
// isn't read any more (ConfigurationsPlan.txt question 36): a build says
// so when one is there, as what it said no longer happens.
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::AddOldFileLine(CVcxSolution* pcSolution, CArrayChars* paszLines)
{
	CFileUtil	cFileUtil;
	CChars		szFile;

	szFile.Init(pcSolution->GetFileName());
	cFileUtil.RemoveExtension(&szFile);
	szFile.Append(BUILD_CONFIGURATIONS_OLD_EXTENSION);
	if (cFileUtil.Exists(szFile.Text()))
	{
		szFile.Append(" isn't read any more: a solution's build configurations are its own, its projects say how each compiles and links, and a run configuration how it is debugged.");
		paszLines->Add(szFile.Text());
	}
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Starts building pcSolution in pcConfiguration, its output and listings
// in szRoot (NULL for beside the solution); an HCP65 build's system
// headers are szLibc65Directory's.  The lines written so far are added to
// paszLines.  False if it couldn't start or a build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionBuild::Start(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot, const char* szLibc65Directory, CArrayChars* paszLines, const char* szStartupProject, const char* szLayoutsFile)
{
	if (IsRunning())
	{
		return false;
	}
	End();
	AddOldFileLine(pcSolution, paszLines);

	if (pcConfiguration->IsHCP65())
	{
		mcHCP65.Init(pcSolution, pcConfiguration, szRoot);
		mcHCP65.SetLibc65Directory(szLibc65Directory);
		mpcBuild = &mcHCP65;
	}
	else if (pcConfiguration->GetToolchain() == BT_MSVC)
	{
		mcMSVC.Init(pcSolution, pcConfiguration, szRoot);
		mpcBuild = &mcMSVC;
	}
	else
	{
		mcGCC.Init(pcSolution, pcConfiguration, szRoot);
		mpcBuild = &mcGCC;
	}
	mpcBuild->SetStartupProject(szStartupProject);
	mpcBuild->SetRebuild(mbRebuild);
	mpcBuild->SetLayoutsFile(szLayoutsFile);
	return mpcBuild->Start(paszLines);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::SetRebuild(bool bRebuild)
{
	mbRebuild = bRebuild;
}


//////////////////////////////////////////////////////////////////////////
//
// Adds the lines written since; true while the build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionBuild::Poll(CArrayChars* paszLines)
{
	return mpcBuild && mpcBuild->Poll(paszLines);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::Cancel(void)
{
	if (mpcBuild)
	{
		mpcBuild->Cancel();
	}
}


EBuildState CSolutionBuild::GetState(void) const { return mpcBuild ? mpcBuild->GetState() : BS_NotStarted; }
bool CSolutionBuild::IsRunning(void) { return mpcBuild && mpcBuild->IsRunning(); }
char* CSolutionBuild::GetProgress(void) const { return mpcBuild ? mpcBuild->GetProgress() : (char*)""; }
float CSolutionBuild::GetFraction(void) const { return mpcBuild ? mpcBuild->GetFraction() : 0.0f; }
CBuild* CSolutionBuild::GetBuild(void) const { return mpcBuild; }  //The last build, NULL before the first.

