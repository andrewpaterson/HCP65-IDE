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
#include "BaseLib/XMLFile.h"
#include "BuildConfigurations.h"


#define BUILD_CONFIGURATIONS_TAG		"BuildConfigurations"
#define BUILD_CONFIGURATIONS_EXTENSION	".hcp65build"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::Init(void)
{
	masConfigurations.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::Kill(void)
{
	Clear();
	masConfigurations.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::Clear(void)
{
	size	i;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		masConfigurations.Get(i)->Kill();
	}
	masConfigurations.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Each solution configuration ("Debug|Win32") built with MSVC under its
// own name, then for each configuration name ("Debug"), in the order they
// first come: the W65C816 and the 65C64 ("Debug|W65C816") from its Win32
// platform, and GCC from its x64 one (or its first, if it hasn't those).
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::AddDefaults(CVcxSolution* pcSolution)
{
	size		i;
	size		j;
	char*		szSolutionConfiguration;
	CChars		szConfiguration;
	CChars		szEarlier;
	bool		bSeen;

	for (i = 0; i < pcSolution->NumConfigurations(); i++)
	{
		szSolutionConfiguration = pcSolution->GetConfiguration(i);
		Add(szSolutionConfiguration, BT_MSVC, szSolutionConfiguration);
	}

	for (i = 0; i < pcSolution->NumConfigurations(); i++)
	{
		szConfiguration.Init();
		GetConfigurationName(pcSolution->GetConfiguration(i), &szConfiguration);
		bSeen = false;
		for (j = 0; (j < i) && !bSeen; j++)
		{
			szEarlier.Init();
			GetConfigurationName(pcSolution->GetConfiguration(j), &szEarlier);
			bSeen = szEarlier.Equals(szConfiguration);
			szEarlier.Kill();
		}
		if (!bSeen)
		{
			AddDefault(pcSolution, szConfiguration.Text(), BT_W65C816, "Win32");
			AddDefault(pcSolution, szConfiguration.Text(), BT_65C64, "Win32");
			AddDefault(pcSolution, szConfiguration.Text(), BT_GCC, "x64");
		}
		szConfiguration.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szConfiguration|<toolchain> ("Debug|W65C816") built from the solution
// configuration szConfiguration|szPlatform if it has it, otherwise its
// first with szConfiguration.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::AddDefault(CVcxSolution* pcSolution, const char* szConfiguration, EBuildToolchain eToolchain, const char* szPlatform)
{
	size	i;
	CChars	szName;
	CChars	szWanted;
	CChars	szOther;
	char*	szSolutionConfiguration;
	char*	szBase;

	szWanted.Init(szConfiguration);
	szWanted.Append('|');
	szWanted.Append(szPlatform);
	szBase = NULL;
	for (i = 0; i < pcSolution->NumConfigurations(); i++)
	{
		szSolutionConfiguration = pcSolution->GetConfiguration(i);
		if (StringInsensitiveCompare(szSolutionConfiguration, szWanted.Text()) == 0)
		{
			szBase = szSolutionConfiguration;
			break;
		}
		szOther.Init();
		GetConfigurationName(szSolutionConfiguration, &szOther);
		if (!szBase && szOther.Equals(szConfiguration))
		{
			szBase = szSolutionConfiguration;
		}
		szOther.Kill();
	}

	szName.Init(szConfiguration);
	szName.Append('|');
	szName.Append(CBuildConfiguration::GetToolchainName(eToolchain));
	Add(szName.Text(), eToolchain, szBase ? szBase : szWanted.Text());
	szName.Kill();
	szWanted.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration part of a solution configuration: Debug of
// Debug|Win32.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::GetConfigurationName(const char* szSolutionConfiguration, CChars* pszDest)
{
	const char*	szBar;

	szBar = strchr(szSolutionConfiguration, '|');
	if (szBar)
	{
		pszDest->Append(szSolutionConfiguration, (size)(szBar - szSolutionConfiguration));
	}
	else
	{
		pszDest->Append(szSolutionConfiguration);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildConfigurations::Add(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration)
{
	CBuildConfiguration*	pcConfiguration;

	pcConfiguration = masConfigurations.Add();
	pcConfiguration->Init(szName, eToolchain, szSolutionConfiguration);
	return pcConfiguration;
}


//////////////////////////////////////////////////////////////////////////
//
// Replaces the configurations with szFileName's.  False, and none, if it
// can't be read or one of its configurations isn't right.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfigurations::Read(const char* szFileName)
{
	CXMLFile				cFile;
	CFileUtil				cFileUtil;
	CChars					szName;
	CChars					szDirectory;
	CMarkupTag*				pcRoot;
	CMarkupTag*				pcTag;
	STagIterator			sIterator;
	CBuildConfiguration*	pcConfiguration;
	bool					bResult;

	Clear();
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFile.Init();
	bResult = cFileUtil.Exists(szFileName) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = pcRoot && (StringCompare(pcRoot->GetName(), BUILD_CONFIGURATIONS_TAG) == 0);
	if (bResult)
	{
		for (pcTag = pcRoot->GetTag(&sIterator); pcTag && bResult; pcTag = pcRoot->GetNextTag(&sIterator))
		{
			pcConfiguration = masConfigurations.Add();
			pcConfiguration->Init();
			bResult = pcConfiguration->LoadXML(pcTag);
		}
	}
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	if (!bResult)
	{
		Clear();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfigurations::Write(const char* szFileName)
{
	CXMLWriter	cWriter;
	size		i;
	bool		bResult;

	cWriter.Init();
	cWriter.Begin(BUILD_CONFIGURATIONS_TAG);
	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		masConfigurations.Get(i)->SaveXML(&cWriter);
	}
	cWriter.End();
	bResult = cWriter.Write(szFileName);
	cWriter.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration called szName, NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildConfigurations::FindConfiguration(const char* szName)
{
	size	i;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		if (StringCompare(masConfigurations.Get(i)->GetName(), szName) == 0)
		{
			return masConfigurations.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The configurations file of the solution (or project) szSolutionFile:
// its name with .hcp65build for its extension.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::GetFileName(const char* szSolutionFile, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	pszDest->Set(szSolutionFile);
	cFileUtil.RemoveExtension(pszDest);
	pszDest->Append(BUILD_CONFIGURATIONS_EXTENSION);
}


size CBuildConfigurations::NumConfigurations(void) { return masConfigurations.NumElements(); }
CBuildConfiguration* CBuildConfigurations::GetConfiguration(size uiIndex) { return masConfigurations.Get(uiIndex); }

