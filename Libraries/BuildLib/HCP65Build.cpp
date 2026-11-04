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
#include "HCP65Build.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::Init(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot)
{
	CBuild::Init(pcSolution, pcConfiguration, szRoot);
	mszLibc65.Init();
	mcBuilder.Init();
	mcJobs.Init();
	mcThread.Init();
	mbCompiled = false;
	mbResult = false;
	mbCancel = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A compile still running is waited for.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::Kill(void)
{
	Wait();
	mcThread.Kill();
	mcJobs.Kill();
	mcBuilder.Kill();
	mszLibc65.Kill();
	CBuild::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Libc65's directory: the system headers (<stdint.h>) and the runtime.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::SetLibc65Directory(const char* szDirectory)
{
	mszLibc65.Set(szDirectory);
}


//////////////////////////////////////////////////////////////////////////
//
// Adds the projects to the builder and starts compiling them on the
// build's thread.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Build::Start(CArrayChars* paszLines)
{
	CChars		szListings;
	CFileUtil	cFileUtil;

	meState = BS_Running;
	mbCompiled = false;
	mbCancel = false;
	mcBuilder.SetTarget((mpcConfiguration->GetToolchain() == BT_W65C816) ? CPPT_W65C816 : CPPT_65C64);
	mcBuilder.SetJobs(&mcJobs);
	mcBuilder.SetHeaderCache(true);
	mcBuilder.SetLineTable(true);
	szListings.Init();
	GetListingDirectory(&szListings);
	cFileUtil.RemoveDir(szListings.Text());
	cFileUtil.TouchDir(szListings.Text());
	mcBuilder.SetListingDirectory(szListings.Text());
	szListings.Kill();
	if (!mszLibc65.Empty())
	{
		mcBuilder.AddIncludeDirectory(mszLibc65.Text(), false, true);
	}
	AddProjects(paszLines);
	if (mcBuilder.NumUnitsToCompile() == 0)
	{
		paszLines->Add("Nothing to build.");
		Finish(BS_Succeeded, paszLines);
		return true;
	}

	SetProgress("Compiling.");
	if (!mcThread.Start(CompileThread, this))
	{
		paszLines->Add("Could not start the build's thread.");
		Finish(BS_Failed, paszLines);
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each project's sources as a library of the builder, with the project's
// include directories (and its own directory) and defines.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::AddProjects(CArrayChars* paszLines)
{
	CArrayBuildProjects	asProjects;
	SBuildProject*		psProject;
	CArrayChars			aszIncludes;
	CArrayChars			aszDefines;
	CChars				szLine;
	CFileUtil			cFileUtil;
	size				i;
	size				j;

	asProjects.Init();
	GetProjects(&asProjects);
	for (i = 0; i < asProjects.NumElements(); i++)
	{
		psProject = asProjects.Get(i);
		aszIncludes.Init();
		psProject->pcProject->GetIncludeDirectories(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszIncludes);
		//With their sub directories, which includes like "BaseLib/Chars.h" are in.
		mcBuilder.AddIncludeDirectory(psProject->pcProject->GetDirectory(), true, false);
		for (j = 0; j < aszIncludes.NumElements(); j++)
		{
			//A directory that isn't there has nothing to include, as Visual Studio has it.
			if (cFileUtil.IsDirectory(aszIncludes.GetText(j)))
			{
				mcBuilder.AddIncludeDirectory(aszIncludes.GetText(j), true, false);
			}
		}
		aszIncludes.Kill();

		szLine.Init(psProject->pcProject->GetName());
		szLine.Append(": ");
		szLine.Append(psProject->pcConfiguration->GetName());
		paszLines->Add(szLine.Text());
		szLine.Kill();
		if (!mcBuilder.AddVisualStudioProject(psProject->pcProject->GetName(), psProject->pcProject->GetFileName(), psProject->bLibrary))
		{
			AddLines(mcBuilder.GetErrors(), paszLines);
		}

		aszDefines.Init();
		psProject->pcProject->GetPreprocessorDefinitions(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszDefines);
		for (j = 0; j < aszDefines.NumElements(); j++)
		{
			mcBuilder.AddDefine(psProject->pcProject->GetName(), aszDefines.GetText(j));
		}
		aszDefines.Kill();
	}
	asProjects.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The build's thread: compiles the units.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::CompileThread(void* pvBuild)
{
	CHCP65Build*	pcBuild;

	pcBuild = (CHCP65Build*)pvBuild;
	pcBuild->mbResult = pcBuild->mcBuilder.Compile();
	pcBuild->mbCompiled = true;
}


//////////////////////////////////////////////////////////////////////////
//
// Says how many units have compiled.  Once they all have, adds the errors
// and warnings.  Returns true while the build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Build::Poll(CArrayChars* paszLines)
{
	CChars	szProgress;
	size	uiFinished;
	size	uiUnits;

	if (meState != BS_Running)
	{
		return false;
	}
	if (!mbCompiled)
	{
		uiUnits = mcBuilder.NumUnitsToCompile();
		uiFinished = mcBuilder.NumUnitsFinished();
		szProgress.Init("Compiling ");
		szProgress.Append((uint32)((uiFinished < uiUnits) ? uiFinished : uiUnits));
		szProgress.Append(" of ");
		szProgress.Append((uint32)uiUnits);
		szProgress.Append(" units.");
		SetProgress(szProgress.Text());
		szProgress.Kill();
		return true;
	}

	Wait();
	AddLines(mcBuilder.GetWarnings(), paszLines);
	AddLines(mcBuilder.GetErrors(), paszLines);
	Finish(mbCancel ? BS_Cancelled : (mbResult ? BS_Succeeded : BS_Failed), paszLines);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The compiler can't be stopped part way: the build says it was cancelled
// when it has finished.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::Cancel(void)
{
	if (meState == BS_Running)
	{
		mbCancel = true;
		SetProgress("Cancelling, once the compile has finished.");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the build's thread, if it was started.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::Wait(void)
{
	if (mcThread.IsStarted())
	{
		mcThread.Join();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szText a line at a time.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::AddLines(const char* szText, CArrayChars* paszLines)
{
	const char*	szLine;
	const char*	szEnd;

	for (szLine = szText; *szLine != '\0'; szLine = (*szEnd == '\n') ? szEnd + 1 : szEnd)
	{
		szEnd = strchr(szLine, '\n');
		if (!szEnd)
		{
			szEnd = szLine + strlen(szLine);
		}
		paszLines->Add(szLine, 0, (size)(szEnd - szLine));
	}
}

