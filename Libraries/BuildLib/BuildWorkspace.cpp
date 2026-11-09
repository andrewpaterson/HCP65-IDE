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
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "VisualStudioLib/VcxSolutionProject.h"
#include "Build.h"
#include "BuildWorkspace.h"


#define BUILD_WORKSPACE_TAG				"Workspace"
#define BUILD_WORKSPACE_SOLUTION		"Solution"
#define BUILD_WORKSPACE_FILE			"File"
#define BUILD_WORKSPACE_STARTUP			"Startup"
#define BUILD_WORKSPACE_CONFIGURATION	"Configuration"
#define BUILD_WORKSPACE_RUN_CONFIGURATION	"RunConfiguration"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspaceSolution::Init(void)
{
	mszFile.Init();
	mcSolution.Init();
	mbRead = false;
	mcConfigurations.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspaceSolution::Kill(void)
{
	mcConfigurations.Kill();
	mcSolution.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a .sln (and its projects), or a .vcxproj as a solution of its
// own, and the build configurations beside it (or the defaults).  False
// if it can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspaceSolution::Open(const char* szFile)
{
	CChars	szName;

	mszFile.Set(szFile);
	szName.Init(szFile);
	mbRead = szName.EndsWithIgnoreCase(".sln") ? mcSolution.Read(szFile) : mcSolution.ReadProject(szFile);
	szName.Kill();

	szName.Init();
	CBuildConfigurations::GetFileName(szFile, &szName);
	if (!mcConfigurations.Read(szName.Text()))
	{
		mcConfigurations.AddDefaults(&mcSolution);
	}
	szName.Kill();
	return mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// szPath relative to szDirectory (both full), with ".." for each of
// szDirectory's directories it isn't in; szPath itself if they are on
// different drives.
//
//////////////////////////////////////////////////////////////////////////
void MakeRelativePath(const char* szDirectory, const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszDirectory;
	CArrayChars	aszPath;
	size		uiSame;
	size		i;

	aszDirectory.Init();
	aszPath.Init();
	cFileUtil.SplitPath(szDirectory, &aszDirectory);
	cFileUtil.SplitPath(szPath, &aszPath);
	for (uiSame = 0; (uiSame < aszDirectory.NumElements()) && (uiSame < aszPath.NumElements()); uiSame++)
	{
		if (StringInsensitiveCompare(aszDirectory.GetText(uiSame), aszPath.GetText(uiSame)) != 0)
		{
			break;
		}
	}

	pszDest->Set("");
	if (uiSame == 0)
	{
		pszDest->Set(szPath);
	}
	else
	{
		for (i = uiSame; i < aszDirectory.NumElements(); i++)
		{
			cFileUtil.AppendToPath(pszDest, "..");
		}
		for (i = uiSame; i < aszPath.NumElements(); i++)
		{
			cFileUtil.AppendToPath(pszDest, aszPath.GetText(i));
		}
	}
	aszPath.Kill();
	aszDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szPath made full: relative to szDirectory unless it is absolute.  Its
// separators are made this OS's first: the files are written on either
// (a <Load> is "..\Kernel\k.rom"), and FullPath doesn't change an
// absolute path's.
//
//////////////////////////////////////////////////////////////////////////
static void FullPathFrom(const char* szDirectory, const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	if (cFileUtil.IsAbsolutePath(szPath))
	{
		pszDest->Set(szPath);
	}
	else
	{
		pszDest->Set(szDirectory);
		cFileUtil.AppendToPath(pszDest, szPath);
	}
	cFileUtil.FixSeparators(pszDest);
	cFileUtil.FullPath(pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::Init(void)
{
	mszFile.Init();
	mapcSolutions.Init();
	muiStartup = MAX_SIZE;
	mszConfiguration.Init();
	mszRunConfiguration.Init();
	masRunConfigurations.Init();
	masProjects.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::Kill(void)
{
	Clear();
	masProjects.Kill();
	masRunConfigurations.Kill();
	mszRunConfiguration.Kill();
	mszConfiguration.Kill();
	mapcSolutions.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::ClearRunConfigurations(void)
{
	size	i;

	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		masRunConfigurations.Get(i)->Kill();
	}
	masRunConfigurations.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// NULL if there is none called szName.
//
//////////////////////////////////////////////////////////////////////////
CRunConfiguration* CBuildWorkspace::FindRunConfiguration(const char* szName)
{
	size	i;

	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		if (StringCompare(masRunConfigurations.Get(i)->GetName(), szName) == 0)
		{
			return masRunConfigurations.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The first run configuration of the build configuration szConfiguration,
// NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
CRunConfiguration* CBuildWorkspace::FindRunConfigurationBuilding(const char* szConfiguration)
{
	size	i;

	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		if (StringCompare(masRunConfigurations.Get(i)->GetConfiguration(), szConfiguration) == 0)
		{
			return masRunConfigurations.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// NULL if there's already one called szName.
//
//////////////////////////////////////////////////////////////////////////
CRunConfiguration* CBuildWorkspace::AddRunConfiguration(const char* szName, const char* szConfiguration)
{
	CRunConfiguration*	pcConfiguration;

	if (FindRunConfiguration(szName) != NULL)
	{
		return NULL;
	}
	pcConfiguration = masRunConfigurations.Add();
	pcConfiguration->Init(szName, szConfiguration);
	return pcConfiguration;
}


//////////////////////////////////////////////////////////////////////////
//
// Copies of pasConfigurations in place of the run configurations there are.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::SetRunConfigurations(CArrayRunConfigurations* pasConfigurations)
{
	size	i;

	ClearRunConfigurations();
	for (i = 0; i < pasConfigurations->NumElements(); i++)
	{
		masRunConfigurations.Add()->Init(pasConfigurations->Get(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A workspace with no run configurations (one saved before they existed,
// or none saved) is given one for each build configuration, named after
// it.  One that has any is left as it is, so those removed stay removed.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::AddDefaultRunConfigurations(void)
{
	CArrayChars	aszNames;
	size		i;

	if (masRunConfigurations.NumElements() != 0)
	{
		return;
	}
	aszNames.Init();
	GetConfigurationNames(&aszNames);
	for (i = 0; i < aszNames.NumElements(); i++)
	{
		AddRunConfiguration(aszNames.GetText(i), aszNames.GetText(i));
	}
	aszNames.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// No solutions, and no workspace file.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::Clear(void)
{
	size	i;

	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		(*mapcSolutions.Get(i))->Kill();
		free(*mapcSolutions.Get(i));
	}
	mapcSolutions.ReInit();
	masProjects.ReInit();
	muiStartup = MAX_SIZE;
	mszConfiguration.Set("");
	mszRunConfiguration.Set("");
	ClearRunConfigurations();
	mszFile.Set("");
}


//////////////////////////////////////////////////////////////////////////
//
// A workspace file (.hcp65workspace), or a solution or project on its own
// (the startup solution of a workspace with no file).  False if it, or
// one of its solutions, can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::Open(const char* szFile)
{
	CChars	szName;
	bool	bWorkspace;
	size	uiSolution;

	szName.Init(szFile);
	bWorkspace = szName.EndsWithIgnoreCase(BUILD_WORKSPACE_EXTENSION);
	szName.Kill();
	if (bWorkspace)
	{
		return Read(szFile);
	}

	Clear();
	uiSolution = AddSolution(szFile);
	return (uiSolution != MAX_SIZE) && GetSolution(uiSolution)->mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions a workspace file lists (relative to it).  The first is
// the startup solution unless one says it is.  False if the file can't be
// read or a solution can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::Read(const char* szFile)
{
	CXMLFile		cFile;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szDirectory;
	CChars			szSolution;
	CMarkupTag*		pcRoot;
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szPath;
	char*			szStartup;
	char*			szConfiguration;
	size			uiSolution;
	bool			bResult;
	CRunConfiguration	cRun;

	Clear();
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFile, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFileUtil.FullPath(&szDirectory);
	cFile.Init();
	bResult = cFileUtil.Exists(szFile) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = pcRoot && (StringCompare(pcRoot->GetName(), BUILD_WORKSPACE_TAG) == 0);
	szSolution.Init();
	if (bResult)
	{
		szConfiguration = pcRoot->GetStringAttribute(BUILD_WORKSPACE_CONFIGURATION);
		mszConfiguration.Set(szConfiguration ? szConfiguration : "");
		szConfiguration = pcRoot->GetStringAttribute(BUILD_WORKSPACE_RUN_CONFIGURATION);
		mszRunConfiguration.Set(szConfiguration ? szConfiguration : "");
		for (pcTag = pcRoot->GetTag(RUN_CONFIGURATION_TAG, &sIterator); pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
		{
			cRun.Init();
			if (cRun.LoadXML(pcTag) && (FindRunConfiguration(cRun.GetName()) == NULL))
			{
				masRunConfigurations.Add()->Init(&cRun);
			}
			else
			{
				bResult = false;
			}
			cRun.Kill();
		}
		for (pcTag = pcRoot->GetTag(BUILD_WORKSPACE_SOLUTION, &sIterator); pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
		{
			szPath = pcTag->GetStringAttribute(BUILD_WORKSPACE_FILE);
			if (!szPath || (szPath[0] == '\0'))
			{
				bResult = false;
				continue;
			}
			FullPathFrom(szDirectory.Text(), szPath, &szSolution);
			uiSolution = AddSolution(szSolution.Text());
			bResult &= (uiSolution != MAX_SIZE) && GetSolution(uiSolution)->mbRead;
			szStartup = pcTag->GetStringAttribute(BUILD_WORKSPACE_STARTUP);
			if ((uiSolution != MAX_SIZE) && szStartup && (StringInsensitiveCompare(szStartup, "true") == 0))
			{
				muiStartup = uiSolution;
			}
		}
	}
	szSolution.Kill();
	cFile.Kill();

	mszFile.Set(szDirectory);
	cFileUtil.AppendToPath(&mszFile, szName.Text());
	szDirectory.Kill();
	szName.Kill();
	return bResult && (mapcSolutions.NumElements() != 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions to the workspace file szFile, their paths relative to it,
// and it becomes the workspace's file.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::Write(const char* szFile)
{
	CXMLWriter	cWriter;
	CFileUtil	cFileUtil;
	CChars		szFull;
	CChars		szName;
	CChars		szDirectory;
	CChars		szRelative;
	size		i;
	bool		bResult;

	szFull.Init(szFile);
	cFileUtil.FullPath(&szFull);
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFull.Text(), &szName, &szDirectory);
	szRelative.Init();

	cWriter.Init();
	cWriter.Begin(BUILD_WORKSPACE_TAG);
	if (!mszConfiguration.Empty())
	{
		cWriter.Attribute(BUILD_WORKSPACE_CONFIGURATION, mszConfiguration.Text());
	}
	if (!mszRunConfiguration.Empty())
	{
		cWriter.Attribute(BUILD_WORKSPACE_RUN_CONFIGURATION, mszRunConfiguration.Text());
	}
	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		MakeRelativePath(szDirectory.Text(), GetSolution(i)->mszFile.Text(), &szRelative);
		cWriter.Begin(BUILD_WORKSPACE_SOLUTION);
		cWriter.Attribute(BUILD_WORKSPACE_FILE, szRelative.Text());
		if (i == muiStartup)
		{
			cWriter.Attribute(BUILD_WORKSPACE_STARTUP, "true");
		}
		cWriter.End();
	}
	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		masRunConfigurations.Get(i)->SaveXML(&cWriter);
	}
	cWriter.End();
	bResult = cWriter.Write(szFull.Text());
	cWriter.Kill();
	if (bResult)
	{
		mszFile.Set(szFull);
	}

	szRelative.Kill();
	szDirectory.Kill();
	szName.Kill();
	szFull.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace file read or written last, "" if none.
//
//////////////////////////////////////////////////////////////////////////
char* CBuildWorkspace::GetFile(void)
{
	return mszFile.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// Opens the solution (or project) szFile in the workspace: the startup
// solution if it is the first.  Its index (also if it couldn't be read:
// see mbRead), or MAX_SIZE if it is already open.
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspace::AddSolution(const char* szFile)
{
	CBuildWorkspaceSolution*	pcSolution;
	CFileUtil					cFileUtil;
	CChars						szFull;

	szFull.Init(szFile);
	cFileUtil.FullPath(&szFull);
	if (FindSolution(szFull.Text()) != MAX_SIZE)
	{
		szFull.Kill();
		return MAX_SIZE;
	}

	pcSolution = (CBuildWorkspaceSolution*)malloc(sizeof(CBuildWorkspaceSolution));
	pcSolution->Init();
	pcSolution->Open(szFull.Text());
	szFull.Kill();
	*mapcSolutions.Add() = pcSolution;
	if (muiStartup == MAX_SIZE)
	{
		muiStartup = 0;
	}
	IndexProjects();
	return mapcSolutions.NumElements() - 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution goes; the first becomes the startup solution if it was.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::RemoveSolution(size uiSolution)
{
	if (uiSolution >= mapcSolutions.NumElements())
	{
		return false;
	}
	(*mapcSolutions.Get(uiSolution))->Kill();
	free(*mapcSolutions.Get(uiSolution));
	mapcSolutions.RemoveAt(uiSolution);
	if (mapcSolutions.NumElements() == 0)
	{
		muiStartup = MAX_SIZE;
	}
	else if (muiStartup == uiSolution)
	{
		muiStartup = 0;
	}
	else if (muiStartup > uiSolution)
	{
		muiStartup--;
	}
	IndexProjects();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The open solution whose file is szFile (made full, ignoring case),
// MAX_SIZE if none is.
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspace::FindSolution(const char* szFile)
{
	CFileUtil	cFileUtil;
	CChars		szFull;
	size		i;

	szFull.Init(szFile);
	cFileUtil.FullPath(&szFull);
	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		if (GetSolution(i)->mszFile.EqualsIgnoreCase(szFull.Text()))
		{
			szFull.Kill();
			return i;
		}
	}
	szFull.Kill();
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::SetStartup(size uiSolution)
{
	if (uiSolution < mapcSolutions.NumElements())
	{
		muiStartup = uiSolution;
	}
}


size CBuildWorkspace::GetStartup(void)										{ return muiStartup; }
void CBuildWorkspace::SetConfiguration(const char* szConfiguration)		{ mszConfiguration.Set(szConfiguration ? szConfiguration : ""); }
char* CBuildWorkspace::GetConfiguration(void)								{ return mszConfiguration.Text(); }
void CBuildWorkspace::SetRunConfigurationName(const char* szName)			{ mszRunConfiguration.Set(szName ? szName : ""); }
char* CBuildWorkspace::GetRunConfigurationName(void)						{ return mszRunConfiguration.Text(); }
size CBuildWorkspace::NumRunConfigurations(void)							{ return masRunConfigurations.NumElements(); }
CRunConfiguration* CBuildWorkspace::GetRunConfiguration(size uiIndex)		{ return masRunConfigurations.SafeGet(uiIndex); }
size CBuildWorkspace::NumSolutions(void)									{ return mapcSolutions.NumElements(); }
CBuildWorkspaceSolution* CBuildWorkspace::GetSolution(size uiSolution)		{ return (uiSolution < mapcSolutions.NumElements()) ? *mapcSolutions.Get(uiSolution) : NULL; }
CBuildWorkspaceSolution* CBuildWorkspace::GetStartupSolution(void)			{ return GetSolution(muiStartup); }


//////////////////////////////////////////////////////////////////////////
//
// Each solution's Visual Studio projects, a project two solutions have
// (the same file) only the first time.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::IndexProjects(void)
{
	CVcxSolution*			pcSolution;
	CVcxSolutionProject*	pcProject;
	SBuildWorkspaceProject*	psProject;
	size					i;
	size					j;
	size					k;

	masProjects.ReInit();
	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		pcSolution = &GetSolution(i)->mcSolution;
		for (j = 0; j < pcSolution->NumProjects(); j++)
		{
			pcProject = pcSolution->GetProject(j);
			if (!pcProject->IsVcxProject())
			{
				continue;
			}
			for (k = 0; k < masProjects.NumElements(); k++)
			{
				if (StringInsensitiveCompare(GetSolutionProject(k)->GetFullPath(), pcProject->GetFullPath()) == 0)
				{
					break;
				}
			}
			if (k == masProjects.NumElements())
			{
				psProject = masProjects.Add();
				psProject->uiSolution = i;
				psProject->uiProject = j;
			}
		}
	}
}


size CBuildWorkspace::NumProjects(void)									{ return masProjects.NumElements(); }
SBuildWorkspaceProject* CBuildWorkspace::GetProject(size uiIndex)		{ return masProjects.SafeGet(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CVcxSolutionProject* CBuildWorkspace::GetSolutionProject(size uiIndex)
{
	SBuildWorkspaceProject*	psProject;

	psProject = masProjects.SafeGet(uiIndex);
	return psProject ? GetSolution(psProject->uiSolution)->mcSolution.GetProject(psProject->uiProject) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution the project is numbered in (its paths are relative to its
// directory).
//
//////////////////////////////////////////////////////////////////////////
CVcxSolution* CBuildWorkspace::GetProjectSolution(size uiIndex)
{
	SBuildWorkspaceProject*	psProject;

	psProject = masProjects.SafeGet(uiIndex);
	return psProject ? &GetSolution(psProject->uiSolution)->mcSolution : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's number for the solution's project uiProject (a shared
// one's first solution's), MAX_SIZE if it isn't a Visual Studio project.
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspace::FindProject(size uiSolution, size uiProject)
{
	CVcxSolutionProject*	pcProject;
	size					i;

	if ((uiSolution >= mapcSolutions.NumElements()) || (uiProject >= GetSolution(uiSolution)->mcSolution.NumProjects()))
	{
		return MAX_SIZE;
	}
	pcProject = GetSolution(uiSolution)->mcSolution.GetProject(uiProject);
	for (i = 0; i < masProjects.NumElements(); i++)
	{
		if (StringInsensitiveCompare(GetSolutionProject(i)->GetFullPath(), pcProject->GetFullPath()) == 0)
		{
			return i;
		}
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// Every solution's configuration names ("Release|W65C816"), each once, in
// the order the solutions (the startup one first) have them.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetConfigurationNames(CArrayChars* paszDest)
{
	CBuildConfigurations*	pcConfigurations;
	size					i;
	size					j;

	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		pcConfigurations = &GetSolution(GetSolutionInOrder(i))->mcConfigurations;
		for (j = 0; j < pcConfigurations->NumConfigurations(); j++)
		{
			if (!paszDest->Contains(pcConfigurations->GetConfiguration(j)->GetName()))
			{
				paszDest->Add(pcConfigurations->GetConfiguration(j)->GetName());
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every solution's configurations' configuration names ("Release"), or
// (bPlatforms) platform names ("W65C816"), each once, the startup
// solution's first: what the toolbar's combo boxes choose from.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetConfigurationNames(CArrayChars* paszDest, bool bPlatforms)
{
	size	i;

	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		if (bPlatforms)
		{
			GetSolution(GetSolutionInOrder(i))->mcConfigurations.GetPlatformNames(paszDest);
		}
		else
		{
			GetSolution(GetSolutionInOrder(i))->mcConfigurations.GetConfigurationNames(paszDest);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's configuration called szName, or the first other
// solution's that has one.  NULL if none has.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildWorkspace::FindAnyConfiguration(const char* szName)
{
	CBuildConfiguration*	pcConfiguration;
	size					i;

	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		pcConfiguration = FindConfiguration(GetSolutionInOrder(i), szName);
		if (pcConfiguration != NULL)
		{
			return pcConfiguration;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The first configuration with the configuration and platform names (NULL
// for any), the startup solution's searched first.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildWorkspace::FindAnyConfiguration(const char* szConfiguration, const char* szPlatform)
{
	CBuildConfiguration*	pcConfiguration;
	size					i;

	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		pcConfiguration = GetSolution(GetSolutionInOrder(i))->mcConfigurations.FindConfiguration(szConfiguration, szPlatform);
		if (pcConfiguration != NULL)
		{
			return pcConfiguration;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions with the startup one first: uiIndex 0 is the startup
// solution, then the others in the workspace's order.
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspace::GetSolutionInOrder(size uiIndex)
{
	if (uiIndex == 0)
	{
		return muiStartup;
	}
	return (uiIndex <= muiStartup) ? uiIndex - 1 : uiIndex;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution's configuration called szName, NULL if it has none.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildWorkspace::FindConfiguration(size uiSolution, const char* szName)
{
	CBuildWorkspaceSolution*	pcSolution;

	pcSolution = GetSolution(uiSolution);
	return pcSolution ? pcSolution->mcConfigurations.FindConfiguration(szName) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions other than the startup one whose configuration
// szConfiguration builds a program the kernal runs as a process (an
// executable put on a card, CardPath), and that the startup solution's
// doesn't load: debugging the startup solution, their source breakpoints
// are bound by their own line tables, in their own breakpoint space and
// process.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetProcessPrograms(const char* szConfiguration, CArrayInt* paiDest)
{
	GetCardPrograms(szConfiguration, BP_Executable, paiDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The same for the drivers the kernal loads (a module put on a card):
// their breakpoints are bound once the kernal has loaded them, in their
// own breakpoint space.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetModulePrograms(const char* szConfiguration, CArrayInt* paiDest)
{
	GetCardPrograms(szConfiguration, BP_Module, paiDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions other than the startup one whose configuration
// szConfiguration builds an eProgram onto a card, and that the startup
// solution's doesn't load.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetCardPrograms(const char* szConfiguration, EBuildProgram eProgram, CArrayInt* paiDest)
{
	CBuildConfiguration*	pcConfiguration;
	size					i;

	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		if (i == muiStartup)
		{
			continue;
		}
		pcConfiguration = FindConfiguration(i, szConfiguration);
		if ((pcConfiguration != NULL) && (pcConfiguration->GetProgram() == eProgram) && pcConfiguration->IsOnCard() && !Loads(muiStartup, i, szConfiguration))
		{
			paiDest->Add((int)i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether uiSolution's configuration szConfiguration loads (a <Load>)
// what uiOther's of that name builds.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::Loads(size uiSolution, size uiOther, const char* szConfiguration)
{
	CBuildConfiguration*	pcConfiguration;
	CBuildConfiguration*	pcOther;
	CChars					szProgram;
	CChars					szLoad;
	size					i;
	bool					bLoads;

	pcConfiguration = FindConfiguration(uiSolution, szConfiguration);
	pcOther = FindConfiguration(uiOther, szConfiguration);
	if (!pcConfiguration || !pcOther || (uiSolution == uiOther))
	{
		return false;
	}
	szProgram.Init();
	if (!CBuild::GetProgramPath(GetSolution(uiOther)->mcSolution.GetDirectory(), pcOther, &szProgram))
	{
		szProgram.Kill();
		return false;
	}

	//<Load> files are relative to the configurations' file, beside the solution.
	bLoads = false;
	szLoad.Init();
	for (i = 0; (i < pcConfiguration->NumLoads()) && !bLoads; i++)
	{
		FullPathFrom(GetSolution(uiSolution)->mcSolution.GetDirectory(), pcConfiguration->GetLoadFile(i), &szLoad);
		bLoads = szLoad.EqualsIgnoreCase(szProgram.Text());
	}
	szLoad.Kill();
	szProgram.Kill();
	return bLoads;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions that have the configuration szConfiguration, in the order
// they are built: each after those whose programs it loads, otherwise in
// the workspace's order.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetBuildOrder(const char* szConfiguration, CArrayInt* paiDest)
{
	CArrayInt	aiLeft;
	size		i;
	size		j;
	bool		bReady;
	bool		bAdded;

	aiLeft.Init();
	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		if (FindConfiguration(i, szConfiguration) != NULL)
		{
			aiLeft.Add((int)i);
		}
	}

	//Each time round the first solution none of the others left are loaded by is taken; a cycle is taken in order.
	while (aiLeft.NumElements() > 0)
	{
		bAdded = false;
		for (i = 0; (i < aiLeft.NumElements()) && !bAdded; i++)
		{
			bReady = true;
			for (j = 0; (j < aiLeft.NumElements()) && bReady; j++)
			{
				bReady = (i == j) || !Loads((size)aiLeft.GetValue(i), (size)aiLeft.GetValue(j), szConfiguration);
			}
			if (bReady)
			{
				paiDest->Add(aiLeft.GetValue(i));
				aiLeft.RemoveAt(i);
				bAdded = true;
			}
		}
		if (!bAdded)
		{
			paiDest->Add(aiLeft.GetValue(0));
			aiLeft.RemoveAt(0);
		}
	}
	aiLeft.Kill();
}

