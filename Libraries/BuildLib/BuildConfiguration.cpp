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
#include "BaseLib/StringHelper.h"
#include "BuildConfiguration.h"


#define BUILD_CONFIGURATION_TAG			"Configuration"
#define BUILD_CONFIGURATION_NAME		"Name"
#define BUILD_CONFIGURATION_TOOLCHAIN	"Toolchain"
#define BUILD_CONFIGURATION_SOLUTION	"Solution"


//The toolchains' names, as saved, by EBuildToolchain.
static const char* gaszBuildToolchainNames[BT_NumToolchains] = { "W65C816", "65C64", "MSVC", "GCC" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::Init(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration)
{
	mszName.Init(szName);
	meToolchain = eToolchain;
	mszSolutionConfiguration.Init(szSolutionConfiguration);
}


//////////////////////////////////////////////////////////////////////////
//
// An empty configuration, to be loaded.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::Init(void)
{
	Init("", BT_MSVC, "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::Kill(void)
{
	mszSolutionConfiguration.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// <Configuration Name="Debug|W65C816" Toolchain="W65C816" Solution="Debug|Win32"/>
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SaveXML(CXMLWriter* pcWriter)
{
	pcWriter->Begin(BUILD_CONFIGURATION_TAG);
	pcWriter->Attribute(BUILD_CONFIGURATION_NAME, mszName.Text());
	pcWriter->Attribute(BUILD_CONFIGURATION_TOOLCHAIN, GetToolchainName(meToolchain));
	pcWriter->Attribute(BUILD_CONFIGURATION_SOLUTION, mszSolutionConfiguration.Text());
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcTag isn't a configuration with a name, a toolchain there is
// and a solution configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::LoadXML(CMarkupTag* pcTag)
{
	char*			szName;
	char*			szToolchain;
	char*			szSolution;
	EBuildToolchain	eToolchain;

	if (StringCompare(pcTag->GetName(), BUILD_CONFIGURATION_TAG) != 0)
	{
		return false;
	}
	szName = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_NAME);
	szToolchain = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_TOOLCHAIN);
	szSolution = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_SOLUTION);
	if (!szName || (szName[0] == '\0') || !szToolchain || !FindToolchain(szToolchain, &eToolchain) || !szSolution || (szSolution[0] == '\0'))
	{
		return false;
	}

	mszName.Set(szName);
	meToolchain = eToolchain;
	mszSolutionConfiguration.Set(szSolution);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether it is built by the HCP65 compiler, which writes listings.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::IsHCP65(void)
{
	return (meToolchain == BT_W65C816) || (meToolchain == BT_65C64);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CBuildConfiguration::GetToolchainName(EBuildToolchain eToolchain)
{
	return ((eToolchain >= 0) && (eToolchain < BT_NumToolchains)) ? gaszBuildToolchainNames[eToolchain] : "";
}


//////////////////////////////////////////////////////////////////////////
//
// The toolchain called szName, whatever its case.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::FindToolchain(const char* szName, EBuildToolchain* peToolchain)
{
	int	i;

	for (i = 0; i < BT_NumToolchains; i++)
	{
		if (StringInsensitiveCompare(szName, gaszBuildToolchainNames[i]) == 0)
		{
			*peToolchain = (EBuildToolchain)i;
			return true;
		}
	}
	return false;
}


char* CBuildConfiguration::GetName(void) { return mszName.Text(); }
EBuildToolchain CBuildConfiguration::GetToolchain(void) { return meToolchain; }
char* CBuildConfiguration::GetSolutionConfiguration(void) { return mszSolutionConfiguration.Text(); }

