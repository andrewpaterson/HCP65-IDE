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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/TextFile.h"
#include "VisualStudioLib/VcxSolutionProject.h"
#include "Build.h"
#include "BuildWorkspace.h"


#define BUILD_WORKSPACE_TAG				"Workspace"
#define BUILD_WORKSPACE_SOLUTION		"Solution"
#define BUILD_WORKSPACE_FILE			"File"
#define BUILD_WORKSPACE_STARTUP			"Startup"
#define BUILD_WORKSPACE_STARTUP_PROJECT	"StartupProject"
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
	mszStartupProject.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspaceSolution::Kill(void)
{
	mszStartupProject.Kill();
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
	ReadStartupProject();
	return mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution, its projects and its build configurations read again from
// their files, after they were written (Edit Build Configurations), with
// the startup project it had.  Everything read before is gone: nothing
// may keep a project or a configuration of it across this.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspaceSolution::Reopen(void)
{
	CChars	szFile;
	CChars	szStartupProject;
	bool	bResult;

	szFile.Init(mszFile);
	szStartupProject.Init(mszStartupProject);
	Kill();
	Init();
	mszStartupProject.Set(szStartupProject);
	bResult = Open(szFile.Text());
	szStartupProject.Kill();
	szFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Each HCP65 build configuration takes what the startup project says of
// the whole program and its linking in the configuration it is built in
// (CBuildConfiguration::ReadProject), so what it links is known before it
// is built.  A setting that is wrong is the build's to say.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspaceSolution::ReadStartupProject(void)
{
	CVcxSolutionProjectConfiguration*	pcSolutionConfiguration;
	CVcxSolutionProject*				pcSolutionProject;
	CBuildConfiguration*				pcConfiguration;
	CChars								szError;
	size								uiProject;
	size								i;

	uiProject = GetStartupProject();
	if (uiProject == MAX_SIZE)
	{
		return;
	}
	pcSolutionProject = mcSolution.GetProject(uiProject);
	for (i = 0; i < mcConfigurations.NumConfigurations(); i++)
	{
		pcConfiguration = mcConfigurations.GetConfiguration(i);
		if (pcConfiguration->IsHCP65())
		{
			pcSolutionConfiguration = pcSolutionProject->FindConfiguration(pcConfiguration->GetSolutionConfiguration());
			szError.Init();
			pcConfiguration->ReadProject(pcConfiguration->FindProjectConfiguration(pcSolutionProject->GetProject(), pcSolutionConfiguration ? pcSolutionConfiguration->GetProjectConfiguration() : pcConfiguration->GetSolutionConfiguration()), &szError);
			szError.Kill();
		}
	}
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
void FullPathFrom(const char* szDirectory, const char* szPath, CChars* pszDest)
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
	maszSharedFiles.Init();
	mbRunConfigurationApplied = false;
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
	maszSharedFiles.Kill();
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
// The others stay in their order.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::RemoveRunConfiguration(size uiIndex)
{
	masRunConfigurations.Get(uiIndex)->Kill();
	masRunConfigurations.RemoveAt(uiIndex);
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
// The shared run configurations of the solution in szDirectory: each
// .hcp65run file in its .run folder, in name order.  A private one of the
// same name is renamed ("ModPlay (private)"), as the shared one is what
// everyone else has; one that can't be read, or a second shared one of a
// name, is left out.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::ReadSharedRunConfigurations(const char* szDirectory)
{
	CFileUtil			cFileUtil;
	CArrayChars			aszFiles;
	CChars				szFolder;
	CChars				szName;
	CChars				szFileDirectory;
	CXMLFile			cFile;
	CMarkupTag*			pcRoot;
	CRunConfiguration	cRun;
	CRunConfiguration*	pcPrivate;
	size				i;

	szFolder.Init(szDirectory);
	cFileUtil.AppendToPath(&szFolder, RUN_CONFIGURATION_FOLDER);
	aszFiles.Init();
	if (cFileUtil.IsDirectory(szFolder.Text()))
	{
		cFileUtil.FindFilesWithExtension(szFolder.Text(), &RUN_CONFIGURATION_EXTENSION[1], &aszFiles);
	}
	aszFiles.QuickSort();
	szName.Init();
	szFileDirectory.Init();
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		szName.Set("");
		szFileDirectory.Set("");
		cFileUtil.SplitPath(aszFiles.GetText(i), &szName, &szFileDirectory);
		cFile.Init();
		pcRoot = (cFile.Read(szName.Text(), szFileDirectory.Text()) && cFile.mcMarkup.mpcDoc) ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
		cRun.Init();
		if (pcRoot && (StringCompare(pcRoot->GetName(), RUN_CONFIGURATION_TAG) == 0) && cRun.LoadXML(pcRoot))
		{
			pcPrivate = FindRunConfiguration(cRun.GetName());
			if ((pcPrivate != NULL) && !pcPrivate->IsShared())
			{
				szName.Set(cRun.GetName());
				do
				{
					szName.Append(" (private)");
				}
				while (FindRunConfiguration(szName.Text()) != NULL);
				if (StringCompare(mszRunConfiguration.Text(), pcPrivate->GetName()) == 0)
				{
					mszRunConfiguration.Set(szName);
				}
				pcPrivate->SetName(szName.Text());
				pcPrivate = NULL;
			}
			if (pcPrivate == NULL)
			{
				cRun.SetShared(szDirectory);
				masRunConfigurations.Add()->Init(&cRun);
				maszSharedFiles.AddIfUnique(aszFiles.GetText(i));
			}
		}
		cRun.Kill();
		cFile.Kill();
	}
	szFileDirectory.Kill();
	szName.Kill();
	aszFiles.Kill();
	szFolder.Kill();
	RemoveSupersededRunConfigurations();
}


//////////////////////////////////////////////////////////////////////////
//
// A private run configuration that is only a build configuration's
// default (its name and nothing more: AddDefaultRunConfigurations) goes
// when a shared one builds that configuration: it was there because
// nothing else was, and says nothing of how to debug
// (ConfigurationsPlan.txt question 28).
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::RemoveSupersededRunConfigurations(void)
{
	CRunConfiguration*	pcRun;
	size				i;
	size				j;
	bool				bShared;

	for (i = masRunConfigurations.NumElements(); i-- > 0; )
	{
		pcRun = masRunConfigurations.Get(i);
		if (pcRun->IsDefault())
		{
			bShared = false;
			for (j = 0; !bShared && (j < masRunConfigurations.NumElements()); j++)
			{
				bShared = masRunConfigurations.Get(j)->IsShared() && (StringCompare(masRunConfigurations.Get(j)->GetConfiguration(), pcRun->GetConfiguration()) == 0);
			}
			if (bShared)
			{
				RemoveRunConfiguration(i);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The solution a shared run configuration is beside, whose program it
// says how to debug: choosing it makes that the startup solution.
// MAX_SIZE for a private one, which belongs to none.
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspace::FindRunConfigurationSolution(CRunConfiguration* pcRun)
{
	size	i;

	for (i = 0; pcRun && pcRun->IsShared() && (i < mapcSolutions.NumElements()); i++)
	{
		if (StringCompare(GetSolution(i)->mcSolution.GetDirectory(), pcRun->GetShared()) == 0)
		{
			return i;
		}
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The first shared run configuration beside the solution uiSolution that
// builds szConfiguration, or with the same configuration name (Debug of
// "Debug|HCP65C816"), or its first; NULL if it has none.  It is chosen
// when the solution is made the startup one.
//
//////////////////////////////////////////////////////////////////////////
CRunConfiguration* CBuildWorkspace::FindSolutionRunConfiguration(size uiSolution, const char* szConfiguration)
{
	CRunConfiguration*	pcRun;
	CRunConfiguration*	pcNamed;
	CRunConfiguration*	pcFirst;
	const char*			szBar;
	size				uiName;
	size				i;

	szBar = strchr(szConfiguration, '|');
	uiName = szBar ? (size)(szBar - szConfiguration) : strlen(szConfiguration);
	pcNamed = NULL;
	pcFirst = NULL;
	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		pcRun = masRunConfigurations.Get(i);
		if (FindRunConfigurationSolution(pcRun) == uiSolution)
		{
			if (StringCompare(pcRun->GetConfiguration(), szConfiguration) == 0)
			{
				return pcRun;
			}
			if (!pcNamed && (strncmp(pcRun->GetConfiguration(), szConfiguration, uiName) == 0) && (pcRun->GetConfiguration()[uiName] == '|'))
			{
				pcNamed = pcRun;
			}
			if (!pcFirst)
			{
				pcFirst = pcRun;
			}
		}
	}
	return pcNamed ? pcNamed : pcFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// Each shared run configuration written to its own file, unless the file
// says it already (they are committed files), and the files of those that
// have gone, been renamed or made private deleted.  False if one can't be
// written.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::WriteSharedRunConfigurations(void)
{
	CFileUtil			cFileUtil;
	CRunConfiguration*	pcRun;
	CXMLWriter			cWriter;
	CTextFile			cOld;
	CArrayChars			aszWritten;
	CChars				szFile;
	CChars				szFolder;
	size				i;
	bool				bResult;

	bResult = true;
	aszWritten.Init();
	szFile.Init();
	szFolder.Init();
	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		pcRun = masRunConfigurations.Get(i);
		if (pcRun->IsShared())
		{
			pcRun->GetSharedFile(&szFile);
			aszWritten.Add(szFile.Text());
			szFolder.Set(pcRun->GetShared());
			cFileUtil.AppendToPath(&szFolder, RUN_CONFIGURATION_FOLDER);
			cFileUtil.TouchDir(szFolder.Text());
			cWriter.Init();
			pcRun->SaveXML(&cWriter);
			cOld.Init();
			if (!cFileUtil.Exists(szFile.Text()) || !cOld.Read(szFile.Text()) || (StringCompare(cOld.Text(), cWriter.GetText()) != 0))
			{
				bResult &= cWriter.Write(szFile.Text());
			}
			cOld.Kill();
			cWriter.Kill();
		}
	}
	for (i = 0; i < maszSharedFiles.NumElements(); i++)
	{
		if (!aszWritten.Contains(maszSharedFiles.GetText(i)))
		{
			cFileUtil.Delete(maszSharedFiles.GetText(i));
		}
	}
	maszSharedFiles.ReInit();
	for (i = 0; i < aszWritten.NumElements(); i++)
	{
		maszSharedFiles.Add(aszWritten.GetText(i));
	}
	szFolder.Kill();
	szFile.Kill();
	aszWritten.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The folder a run configuration's paths are relative to: its solution's
// if it is shared, else the workspace file's (the startup solution's if
// the workspace has no file yet).
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetRunConfigurationDirectory(CRunConfiguration* pcRun, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;

	if (pcRun->IsShared())
	{
		pszDest->Set(pcRun->GetShared());
	}
	else if (!mszFile.Empty())
	{
		szName.Init();
		pszDest->Set("");
		cFileUtil.SplitPath(mszFile.Text(), &szName, pszDest);
		szName.Kill();
	}
	else
	{
		pszDest->Set((muiStartup != MAX_SIZE) ? GetStartupSolution()->mcSolution.GetDirectory() : "");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// How pcRun says its build configuration is debugged, given to that build
// configuration of every solution, where debugging and hcp65build -card
// read it (ConfigurationsPlan.txt C8): each solution whose program is on
// the run configuration's card is on it at the card's path for it; the
// startup solution's also has the run configuration's loads and breakpoint
// space, and reads as they are linked the line tables of the card's
// programs that say so (Lines), but for one no solution here builds, nor
// builds another file for the same path on the card (the kernal's own
// solution, building Debug, where the card's line says Release's).
//
// A run configuration that says nothing of debugging changes nothing: the
// build configurations keep what a configurations file gave them.  But
// once one that says something has been applied, the next is whatever it
// says, so nothing is left of the one before.  False, with why in
// pszError, if its card file can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::ApplyRunConfiguration(CRunConfiguration* pcRun, CChars* pszError)
{
	CBuildConfiguration*	pcConfiguration;
	CBuildCard				cCard;
	CArrayInt				aiBuilt;
	CChars					szDirectory;
	CChars					szFile;
	CChars					szImage;
	const char*				szConfiguration;
	size					uiProgram;
	size					i;
	bool					bCard;

	if ((pcRun == NULL) || (!pcRun->HasDebugging() && !mbRunConfigurationApplied))
	{
		return true;
	}
	mbRunConfigurationApplied = true;
	szConfiguration = pcRun->GetConfiguration();
	szDirectory.Init();
	GetRunConfigurationDirectory(pcRun, &szDirectory);
	szFile.Init();
	szImage.Init();
	cCard.Init();
	bCard = pcRun->GetCard()[0] != '\0';
	if (bCard)
	{
		FullPathFrom(szDirectory.Text(), pcRun->GetCard(), &szFile);
		if (!cCard.Read(szFile.Text()))
		{
			pszError->Set("The card file [");
			pszError->Append(szFile)->Append("] of the run configuration ")->Append(pcRun->GetName())->Append(" can't be read.");
			cCard.Kill();
			szImage.Kill();
			szFile.Kill();
			szDirectory.Kill();
			return false;
		}
		cCard.GetImage(&szImage);
	}

	aiBuilt.Init();
	for (i = 0; i < mapcSolutions.NumElements(); i++)
	{
		pcConfiguration = FindConfiguration(i, szConfiguration);
		if ((pcConfiguration == NULL) || !pcConfiguration->IsHCP65())
		{
			continue;
		}
		pcConfiguration->ClearDebugging();
		if (bCard && CBuild::GetProgramPath(GetSolution(i)->mcSolution.GetDirectory(), pcConfiguration, &szFile))
		{
			uiProgram = cCard.FindProgram(szFile.Text(), szConfiguration);
			if (uiProgram != ARRAY_ELEMENT_NOT_FOUND)
			{
				pcConfiguration->SetCard(szImage.Text(), cCard.GetProgramPath(uiProgram));
				aiBuilt.Add((int)uiProgram);
			}
		}
	}

	pcConfiguration = (muiStartup != MAX_SIZE) ? FindConfiguration(muiStartup, szConfiguration) : NULL;
	if ((pcConfiguration != NULL) && pcConfiguration->IsHCP65())
	{
		pcConfiguration->SetBreakpointSpace(pcRun->GetBreakpointSpace());
		for (i = 0; i < pcRun->NumLoads(); i++)
		{
			FullPathFrom(szDirectory.Text(), pcRun->GetLoadFile(i), &szFile);
			pcConfiguration->AddLoad(szFile.Text(), pcRun->GetLoadAddress(i));
		}
		for (i = 0; i < cCard.NumPrograms(); i++)
		{
			//Not what a solution here builds, nor another file for the same place on the card as one it does.
			bCard = cCard.GetProgramLines(i)[0] != '\0';
			for (uiProgram = 0; bCard && (uiProgram < aiBuilt.NumElements()); uiProgram++)
			{
				bCard = StringInsensitiveCompare(cCard.GetProgramPath((size)aiBuilt.GetValue(uiProgram)), cCard.GetProgramPath(i)) != 0;
			}
			if (bCard)
			{
				cCard.GetProgramFile(i, szConfiguration, &szFile);
				pcConfiguration->AddLines(szFile.Text(), cCard.GetProgramLines(i), cCard.GetProgramPath(i));
			}
		}
	}
	aiBuilt.Kill();
	cCard.Kill();
	szImage.Kill();
	szFile.Kill();
	szDirectory.Kill();
	return true;
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
	maszSharedFiles.ReInit();
	mbRunConfigurationApplied = false;
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
		SetConfiguration(pcRoot->GetStringAttribute(BUILD_WORKSPACE_CONFIGURATION));
		SetRunConfigurationName(pcRoot->GetStringAttribute(BUILD_WORKSPACE_RUN_CONFIGURATION));
		for (pcTag = pcRoot->GetTag(RUN_CONFIGURATION_TAG, &sIterator); pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
		{
			cRun.Init();
			if (cRun.LoadXML(pcTag) && (FindRunConfiguration(cRun.GetName()) == NULL))
			{
				cRun.SetShared(NULL);
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
			szStartup = pcTag->GetStringAttribute(BUILD_WORKSPACE_STARTUP_PROJECT);
			if ((uiSolution != MAX_SIZE) && szStartup)
			{
				GetSolution(uiSolution)->mszStartupProject.Set(szStartup);
				GetSolution(uiSolution)->ReadStartupProject();
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
		if (!GetSolution(i)->mszStartupProject.Empty())
		{
			cWriter.Attribute(BUILD_WORKSPACE_STARTUP_PROJECT, GetSolution(i)->mszStartupProject.Text());
		}
		cWriter.End();
	}
	for (i = 0; i < masRunConfigurations.NumElements(); i++)
	{
		if (!masRunConfigurations.Get(i)->IsShared())
		{
			masRunConfigurations.Get(i)->SaveXML(&cWriter);
		}
	}
	cWriter.End();
	bResult = cWriter.Write(szFull.Text());
	cWriter.Kill();
	bResult &= WriteSharedRunConfigurations();
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
//////////////////////////////////////////////////////////////////////////
//
// Whether the solution's project is an application: a project that was
// read and isn't a static library (as a build tells them apart, see
// CBuild::GetProjects), by its first configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspaceSolution::IsApplication(size uiProject)
{
	CVcxSolutionProject*	pcSolutionProject;
	CVcxProject*			pcProject;
	char*					szType;

	if (uiProject >= mcSolution.NumProjects())
	{
		return false;
	}
	pcSolutionProject = mcSolution.GetProject(uiProject);
	if (!pcSolutionProject->IsVcxProject() || !pcSolutionProject->IsLoaded())
	{
		return false;
	}
	pcProject = pcSolutionProject->GetProject();
	if (pcProject->NumConfigurations() == 0)
	{
		return true;
	}
	szType = pcProject->GetConfiguration(0)->GetProperty("ConfigurationType");
	return !(szType && (StringInsensitiveCompare(szType, "StaticLibrary") == 0));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspaceSolution::NumApplications(void)
{
	size	i;
	size	uiApplications;

	uiApplications = 0;
	for (i = 0; i < mcSolution.NumProjects(); i++)
	{
		uiApplications += IsApplication(i) ? 1 : 0;
	}
	return uiApplications;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution's startup project, its index in the solution: the
// application chosen or, with none chosen (or it has gone, or is a
// library now), the first application.  MAX_SIZE if the solution has no
// application.
//
//////////////////////////////////////////////////////////////////////////
size CBuildWorkspaceSolution::GetStartupProject(void)
{
	size	i;
	size	uiFirst;

	uiFirst = MAX_SIZE;
	for (i = 0; i < mcSolution.NumProjects(); i++)
	{
		if (IsApplication(i))
		{
			if (!mszStartupProject.Empty() && mszStartupProject.EqualsIgnoreCase(mcSolution.GetProject(i)->GetName()))
			{
				return i;
			}
			if (uiFirst == MAX_SIZE)
			{
				uiFirst = i;
			}
		}
	}
	return uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup project's name, "" if the solution has no application.
//
//////////////////////////////////////////////////////////////////////////
const char* CBuildWorkspaceSolution::GetStartupProjectName(void)
{
	size	uiProject;

	uiProject = GetStartupProject();
	return (uiProject != MAX_SIZE) ? mcSolution.GetProject(uiProject)->GetName() : (char*)"";
}


//////////////////////////////////////////////////////////////////////////
//
// One of the solution's applications is chosen as its startup project.
// False if the project isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspaceSolution::SetStartupProject(size uiProject)
{
	if (!IsApplication(uiProject))
	{
		return false;
	}
	mszStartupProject.Set(mcSolution.GetProject(uiProject)->GetName());
	ReadStartupProject();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
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
	ReadSharedRunConfigurations(pcSolution->mcSolution.GetDirectory());
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


size CBuildWorkspace::GetStartup(void) const										{ return muiStartup; }


//////////////////////////////////////////////////////////////////////////
//
// A solution's startup project (see CBuildWorkspaceSolution): chosen by
// its index in the solution; its index (MAX_SIZE if the solution has no
// application); and its name ("" if it has none).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildWorkspace::SetStartupProject(size uiSolution, size uiProject)
{
	return (uiSolution < mapcSolutions.NumElements()) && GetSolution(uiSolution)->SetStartupProject(uiProject);
}


size CBuildWorkspace::GetStartupProject(size uiSolution)
{
	return (uiSolution < mapcSolutions.NumElements()) ? GetSolution(uiSolution)->GetStartupProject() : MAX_SIZE;
}


const char* CBuildWorkspace::GetStartupProjectName(size uiSolution)
{
	return (uiSolution < mapcSolutions.NumElements()) ? GetSolution(uiSolution)->GetStartupProjectName() : (char*)"";
}
void CBuildWorkspace::SetConfiguration(const char* szConfiguration)		{ mszConfiguration.Set(""); CBuildConfiguration::CurrentName(szConfiguration, &mszConfiguration); }
char* CBuildWorkspace::GetConfiguration(void)								{ return mszConfiguration.Text(); }
void CBuildWorkspace::SetRunConfigurationName(const char* szName)			{ mszRunConfiguration.Set(""); CBuildConfiguration::CurrentName(szName, &mszRunConfiguration); }
char* CBuildWorkspace::GetRunConfigurationName(void)						{ return mszRunConfiguration.Text(); }
size CBuildWorkspace::NumRunConfigurations(void) const							{ return masRunConfigurations.NumElements(); }
CRunConfiguration* CBuildWorkspace::GetRunConfiguration(size uiIndex)		{ return masRunConfigurations.SafeGet(uiIndex); }
size CBuildWorkspace::NumSolutions(void) const									{ return mapcSolutions.NumElements(); }
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


size CBuildWorkspace::NumProjects(void) const									{ return masProjects.NumElements(); }
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
// Every solution's configuration names ("Release|HCP65C816"), each once, in
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
// The same for an image put on a card, which the boot loader loads where
// it is linked (the HCP65's kernal, \OS\OS.SYS): its source breakpoints
// are bound by its line table as it is, in its own breakpoint space.
//
//////////////////////////////////////////////////////////////////////////
void CBuildWorkspace::GetImagePrograms(const char* szConfiguration, CArrayInt* paiDest)
{
	GetCardPrograms(szConfiguration, BP_Image, paiDest);
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

