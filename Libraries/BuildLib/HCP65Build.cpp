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
#include <stdio.h>
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
	muiReported = 0;
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
	uint32		uiLimit;

	meState = BS_Running;
	mbCompiled = false;
	mbCancel = false;
	mcBuilder.SetTarget((mpcConfiguration->GetToolchain() == BT_W65C816) ? CPPT_W65C816 : CPPT_65C64);
	mcBuilder.SetJobs(&mcJobs);
	mcBuilder.SetHeaderCache(true);
	mcBuilder.SetLineTable(true);
	mcBuilder.SetUnitReports(true);
	muiReported = 0;
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
	if (mpcConfiguration->IsLinked())
	{
		//A program: the runtime after the projects, only what is used kept.
		mcBuilder.SetRemoveUnused(true);
		mcBuilder.SetNoFloats(mpcConfiguration->IsNoFloats());
		mcBuilder.SetMathTables(mpcConfiguration->IsMathTables());
		uiLimit = mpcConfiguration->HasImage() ? mpcConfiguration->GetImage()->GetLimit() : mpcConfiguration->GetExecutable()->GetLimit();
		if (uiLimit != 0)
		{
			mcBuilder.SetLimit(uiLimit);
		}
		if (!mszLibc65.Empty())
		{
			mcBuilder.AddLibc65(mszLibc65.Text());
		}
	}
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
// The build's thread: compiles the units, and links them if there is an
// image to make.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::CompileThread(void* pvBuild)
{
	CHCP65Build*	pcBuild;

	pcBuild = (CHCP65Build*)pvBuild;
	if (pcBuild->mpcConfiguration->IsLinked())
	{
		pcBuild->mbResult = pcBuild->mcBuilder.Link();
	}
	else
	{
		pcBuild->mbResult = pcBuild->mcBuilder.Compile();
	}
	pcBuild->mbCompiled = true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each unit that has compiled since the last poll is a line, its name and
// how many have compiled, followed by its warnings and errors.  Once they
// all have, adds the errors and warnings that aren't a unit's (the
// link's).  Returns true while the build is running.
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
		AddUnitReports(paszLines);
		uiUnits = mcBuilder.NumUnitsToCompile();
		uiFinished = mcBuilder.NumUnitsFinished();
		SetSteps(uiFinished, uiUnits);
		if ((uiFinished >= uiUnits) && mpcConfiguration->IsLinked())
		{
			SetProgress("Linking.");
			return true;
		}
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
	AddUnitReports(paszLines);
	AddLines(mcBuilder.GetWarnings(), paszLines);
	AddLines(mcBuilder.GetErrors(), paszLines);
	if (mbResult && !mbCancel && mpcConfiguration->IsLinked())
	{
		mbResult = WriteProgram(paszLines);
	}
	Finish(mbCancel ? BS_Cancelled : (mbResult ? BS_Succeeded : BS_Failed), paszLines);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The linked program as the configuration's image or executable, in the
// Output folder.  Says where each linked segment went and where it was
// written, or why it couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65Build::WriteProgram(CArrayChars* paszLines)
{
	CAsm65Image*		pcImage;
	CAsm65Executable*	pcExecutable;
	CAsm65Output*	pcOutput;
	SAsm65Segment*	psSegment;
	CChars			szErrors;
	CChars			szPath;
	CFileUtil		cFileUtil;
	char			szLine[128];
	size			i;
	bool			bResult;

	pcImage = mpcConfiguration->HasImage() ? mpcConfiguration->GetImage() : NULL;
	pcExecutable = mpcConfiguration->HasExecutable() ? mpcConfiguration->GetExecutable() : NULL;
	pcOutput = mcBuilder.GetOutput();
	for (i = 0; i < pcOutput->NumSegments(); i++)
	{
		psSegment = pcOutput->GetSegment(i);
		snprintf(szLine, sizeof(szLine), "Segment $%06X, %u bytes.", (unsigned int)psSegment->uiAddress, (unsigned int)psSegment->cBytes.NumElements());
		paszLines->Add(szLine);
	}

	szErrors.Init();
	bResult = pcImage ? pcImage->Make(pcOutput, &szErrors) : pcExecutable->Make(pcOutput, mcBuilder.GetLinker(), &szErrors);
	AddLines(szErrors.Text(), paszLines);
	szErrors.Kill();
	if (!bResult)
	{
		return false;
	}

	szPath.Init();
	GetOutputDirectory(&szPath);
	cFileUtil.TouchDir(szPath.Text());
	GetProgramPath(mszRoot.Text(), mpcConfiguration, &szPath);
	bResult = pcImage ? pcImage->Write(szPath.Text()) : pcExecutable->Write(szPath.Text());
	snprintf(szLine, sizeof(szLine), bResult ? "Wrote %u bytes to " : "Could not write %u bytes to ", (unsigned int)(pcImage ? pcImage->GetSize() : pcExecutable->GetSize()));
	szErrors.Init(szLine);
	szErrors.Append(szPath);
	szErrors.Append(".");
	paszLines->Add(szErrors.Text());
	szErrors.Kill();

	//Beside it, which source line each address is for, so a debugger can stop at a line.
	if (bResult)
	{
		GetLineTablePath(mszRoot.Text(), mpcConfiguration, &szPath);
		bResult = mcBuilder.GetLineTable()->WriteFile(szPath.Text());
		szErrors.Init(bResult ? "Wrote its line table to " : "Could not write its line table to ");
		szErrors.Append(szPath);
		szErrors.Append(".");
		paszLines->Add(szErrors.Text());
		szErrors.Kill();
	}
	szPath.Kill();
	return bResult;
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
// The units that have compiled since the last time: each a line, its name
// and how many of the units have compiled ("Main.cpp (12/40)"), or that
// it was compiled again (a W65C816 unit that calls a function exported
// after it compiled), then its warnings and errors.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65Build::AddUnitReports(CArrayChars* paszLines)
{
	CArrayCPP65UnitReports	asReports;
	SCPP65UnitReport*		psReport;
	CChars					szLine;
	size					i;

	asReports.Init();
	mcBuilder.TakeUnitReports(&asReports);
	for (i = 0; i < asReports.NumElements(); i++)
	{
		psReport = asReports.Get(i);
		szLine.Init(psReport->szName);
		if (psReport->bAgain)
		{
			szLine.Append(" (compiled again)");
		}
		else
		{
			muiReported++;
			szLine.Append(" (");
			szLine.Append((uint32)muiReported);
			szLine.Append('/');
			szLine.Append((uint32)mcBuilder.NumUnitsToCompile());
			szLine.Append(')');
		}
		paszLines->Add(szLine.Text());
		szLine.Kill();
		AddLines(psReport->szWarnings.Text(), paszLines);
		AddLines(psReport->szErrors.Text(), paszLines);
	}
	CCPP65Builder::KillUnitReports(&asReports);
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

