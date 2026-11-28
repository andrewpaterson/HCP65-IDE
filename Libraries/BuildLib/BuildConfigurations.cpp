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
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BuildConfigurations.h"


#define BUILD_CONFIGURATIONS_TAG		"BuildConfigurations"


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
// Each solution configuration under its own name: one whose platform is a
// toolchain's ("Release|GCC-x64") is that toolchain's, and any other
// ("Debug|Win32") MSVC's.  A solution that lists a toolchain's has said
// what it builds, and that is all.  One that lists none is given each
// toolchain's for each configuration name ("Debug"), in the order they
// first come: the W65C816's and the 65C64's ("Debug|HCP65C816") from the
// one that builds its projects' Win32, and GCC's from the one that builds
// their x64 (or its first, if it hasn't those).
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::AddDefaults(CVcxSolution* pcSolution)
{
	size		i;
	size		j;
	char*			szSolutionConfiguration;
	const char*		szBar;
	EBuildToolchain	eToolchain;
	CChars			szConfiguration;
	CChars			szEarlier;
	bool			bSeen;
	bool			bListed;

	bListed = false;
	for (i = 0; i < pcSolution->NumConfigurations(); i++)
	{
		szSolutionConfiguration = pcSolution->GetConfiguration(i);
		eToolchain = BT_MSVC;
		szBar = strchr(szSolutionConfiguration, '|');
		if (szBar && CBuildConfiguration::FindToolchainOfPlatform(szBar + 1, &eToolchain))
		{
			bListed = true;
		}
		Add(szSolutionConfiguration, eToolchain, szSolutionConfiguration);
	}

	for (i = 0; !bListed && (i < pcSolution->NumConfigurations()); i++)
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
// Whether a solution configuration is szPlatform's ("Win32"): by its own
// name (Debug|Win32), or, a solution's platforms being names of its own
// (Debug|MSVC-x86: ConfigurationsPlan.txt C11-2), by the platform of the
// project configuration its first project's row names (Debug|Win32).
//
//////////////////////////////////////////////////////////////////////////
static bool IsSolutionConfigurationOfPlatform(CVcxSolution* pcSolution, const char* szSolutionConfiguration, const char* szPlatform)
{
	CVcxSolutionProjectConfiguration*	pcRow;
	const char*							szBar;
	size								i;

	szBar = strchr(szSolutionConfiguration, '|');
	if (szBar && (StringInsensitiveCompare(szBar + 1, szPlatform) == 0))
	{
		return true;
	}
	for (i = 0; i < pcSolution->NumProjects(); i++)
	{
		pcRow = pcSolution->GetProject(i)->IsVcxProject() ? pcSolution->GetProject(i)->FindConfiguration(szSolutionConfiguration) : NULL;
		if (pcRow != NULL)
		{
			szBar = strchr(pcRow->GetProjectConfiguration(), '|');
			return szBar && (StringInsensitiveCompare(szBar + 1, szPlatform) == 0);
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// szConfiguration|<toolchain> ("Debug|HCP65C816") built from the solution
// configuration of that configuration that is szPlatform's
// (IsSolutionConfigurationOfPlatform) if it has one, otherwise its first
// with szConfiguration.
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
		szOther.Init();
		GetConfigurationName(szSolutionConfiguration, &szOther);
		if (szOther.Equals(szConfiguration))
		{
			if (IsSolutionConfigurationOfPlatform(pcSolution, szSolutionConfiguration, szPlatform))
			{
				szBase = szSolutionConfiguration;
				szOther.Kill();
				break;
			}
			if (!szBase)
			{
				szBase = szSolutionConfiguration;
			}
		}
		szOther.Kill();
	}

	szName.Init(szConfiguration);
	szName.Append('|');
	szName.Append(CBuildConfiguration::GetToolchainPlatform(eToolchain));
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
// The configuration called szName, NULL if there is none.  A name with
// one of Visual Studio's own platforms that the solution hasn't
// (Release|x64, in a workspace written before its solution's platforms were
// renamed: ConfigurationsPlan.txt C11-2) finds the one of the solution's
// own name for it (Release|MSVC-x64).
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildConfigurations::FindConfiguration(const char* szName)
{
	size					i;
	size					uiPass;
	CChars					szCurrent;
	CBuildConfiguration*	pcFound;

	szCurrent.Init();
	CBuildConfiguration::CurrentName(szName, &szCurrent);
	pcFound = NULL;
	for (uiPass = 0; (uiPass < 2) && !pcFound; uiPass++)
	{
		for (i = 0; (i < masConfigurations.NumElements()) && !pcFound; i++)
		{
			if (StringCompare(masConfigurations.Get(i)->GetName(), szCurrent.Text()) == 0)
			{
				pcFound = masConfigurations.Get(i);
			}
		}
		if (szCurrent.EndsWith("|Win32"))
		{
			szCurrent.RemoveFromEnd(5);
			szCurrent.Append("MSVC-x86");
		}
		else if (szCurrent.EndsWith("|x64"))
		{
			szCurrent.RemoveFromEnd(3);
			szCurrent.Append("MSVC-x64");
		}
		else
		{
			break;
		}
	}
	szCurrent.Kill();
	return pcFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The first configuration whose configuration is szConfiguration and
// platform szPlatform, NULL if there is none.  NULL matches any.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CBuildConfigurations::FindConfiguration(const char* szConfiguration, const char* szPlatform)
{
	size	i;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		if (masConfigurations.Get(i)->Is(szConfiguration, szPlatform))
		{
			return masConfigurations.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The configurations' configuration names (Debug, Release), each once in
// the order they first come.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::GetConfigurationNames(CArrayChars* paszDest)
{
	GetNames(paszDest, false);
}


//////////////////////////////////////////////////////////////////////////
//
// The configurations' platforms (Win32, x64, W65C816), each once in the
// order they first come.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::GetPlatformNames(CArrayChars* paszDest)
{
	GetNames(paszDest, true);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurations::GetNames(CArrayChars* paszDest, bool bPlatforms)
{
	size	i;
	CChars	szName;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		szName.Init();
		if (bPlatforms)
		{
			masConfigurations.Get(i)->GetPlatformName(&szName);
		}
		else
		{
			masConfigurations.Get(i)->GetConfigurationName(&szName);
		}
		if (!paszDest->Contains(&szName))
		{
			paszDest->Add(szName.Text());
		}
		szName.Kill();
	}
}


size CBuildConfigurations::NumConfigurations(void) const { return masConfigurations.NumElements(); }
CBuildConfiguration* CBuildConfigurations::GetConfiguration(size uiIndex) { return masConfigurations.Get(uiIndex); }

