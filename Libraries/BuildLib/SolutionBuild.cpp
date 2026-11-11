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
#include "SolutionBuild.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionBuild::Init(void)
{
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
// Starts building pcSolution in pcConfiguration, its output and listings
// in szRoot (NULL for beside the solution); an HCP65 build's system
// headers are szLibc65Directory's.  The lines written so far are added to
// paszLines.  False if it couldn't start or a build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CSolutionBuild::Start(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot, const char* szLibc65Directory, CArrayChars* paszLines, const char* szStartupProject)
{
	if (IsRunning())
	{
		return false;
	}
	End();

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
	return mpcBuild->Start(paszLines);
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


EBuildState CSolutionBuild::GetState(void) { return mpcBuild ? mpcBuild->GetState() : BS_NotStarted; }
bool CSolutionBuild::IsRunning(void) { return mpcBuild && mpcBuild->IsRunning(); }
char* CSolutionBuild::GetProgress(void) { return mpcBuild ? mpcBuild->GetProgress() : (char*)""; }
float CSolutionBuild::GetFraction(void) { return mpcBuild ? mpcBuild->GetFraction() : 0.0f; }
CBuild* CSolutionBuild::GetBuild(void) { return mpcBuild; }  //The last build, NULL before the first.

