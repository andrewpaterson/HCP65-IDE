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
#include "BaseLib/StringHelper.h"
#include "BuildConfiguration.h"


#define BUILD_CONFIGURATION_TAG			"Configuration"
#define BUILD_CONFIGURATION_NAME		"Name"
#define BUILD_CONFIGURATION_TOOLCHAIN	"Toolchain"
#define BUILD_CONFIGURATION_SOLUTION	"Solution"
#define BUILD_CONFIGURATION_EMULATOR	"Emulator"
#define BUILD_CONFIGURATION_EMULATOR_DIRECTORY	"EmulatorDirectory"
#define BUILD_CONFIGURATION_EMULATOR_ARGUMENTS	"EmulatorArguments"
#define BUILD_CONFIGURATION_IMAGE		"Image"


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
	mszEmulator.Init();
	mszEmulatorDirectory.Init();
	mszEmulatorArguments.Init();
	mbImage = false;
	mcImage.Init();
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
	mcImage.Kill();
	mszEmulatorArguments.Kill();
	mszEmulatorDirectory.Kill();
	mszEmulator.Kill();
	mszSolutionConfiguration.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// <Configuration Name="Debug|W65C816" Toolchain="W65C816" Solution="Debug|Win32"/>, with Emulator,
// EmulatorDirectory and EmulatorArguments only where they are set, and its <Image> inside it if it has one.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SaveXML(CXMLWriter* pcWriter)
{
	pcWriter->Begin(BUILD_CONFIGURATION_TAG);
	pcWriter->Attribute(BUILD_CONFIGURATION_NAME, mszName.Text());
	pcWriter->Attribute(BUILD_CONFIGURATION_TOOLCHAIN, GetToolchainName(meToolchain));
	pcWriter->Attribute(BUILD_CONFIGURATION_SOLUTION, mszSolutionConfiguration.Text());
	if (!mszEmulator.Empty())
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_EMULATOR, mszEmulator.Text());
	}
	if (!mszEmulatorDirectory.Empty())
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_EMULATOR_DIRECTORY, mszEmulatorDirectory.Text());
	}
	if (!mszEmulatorArguments.Empty())
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_EMULATOR_ARGUMENTS, mszEmulatorArguments.Text());
	}
	if (mbImage)
	{
		mcImage.SaveXML(pcWriter);
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcTag isn't a configuration with a name, a toolchain there is
// and a solution configuration, or if its <Image> isn't one (an image
// is only for an HCP65 toolchain).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::LoadXML(CMarkupTag* pcTag)
{
	char*			szName;
	char*			szToolchain;
	char*			szSolution;
	EBuildToolchain	eToolchain;
	CMarkupTag*		pcImage;

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
	SetEmulator(pcTag->GetAttribute((char*)BUILD_CONFIGURATION_EMULATOR), pcTag->GetAttribute((char*)BUILD_CONFIGURATION_EMULATOR_DIRECTORY), pcTag->GetAttribute((char*)BUILD_CONFIGURATION_EMULATOR_ARGUMENTS));

	pcImage = pcTag->GetTag(BUILD_CONFIGURATION_IMAGE);
	if (pcImage)
	{
		if (!IsHCP65() || !mcImage.LoadXML(pcImage))
		{
			return false;
		}
		mbImage = true;
	}
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
// The processor what it builds runs on, as an emulator names it ("W65C816",
// "65C64"); NULL for this computer's (MSVC, GCC).
//
//////////////////////////////////////////////////////////////////////////
const char* CBuildConfiguration::GetProcessorName(void)
{
	return IsHCP65() ? GetToolchainName(meToolchain) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// NULL or empty for none.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SetEmulator(const char* szExecutable, const char* szDirectory, const char* szArguments)
{
	mszEmulator.Set(szExecutable ? szExecutable : "");
	mszEmulatorDirectory.Set(szDirectory ? szDirectory : "");
	mszEmulatorArguments.Set(szArguments ? szArguments : "");
}


//////////////////////////////////////////////////////////////////////////
//
// The name's configuration: Debug of Debug|W65C816, all of a name without
// a |.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::GetConfigurationName(CChars* pszDest)
{
	const char*	szBar;

	szBar = strchr(mszName.Text(), '|');
	if (szBar)
	{
		pszDest->Append(mszName.Text(), (size)(szBar - mszName.Text()));
	}
	else
	{
		pszDest->Append(mszName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The name's platform: W65C816 of Debug|W65C816, the toolchain's name for
// a name without a |.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::GetPlatformName(CChars* pszDest)
{
	const char*	szBar;

	szBar = strchr(mszName.Text(), '|');
	pszDest->Append(szBar ? szBar + 1 : GetToolchainName(meToolchain));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether its configuration is szConfiguration and its platform
// szPlatform.  NULL matches any.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::Is(const char* szConfiguration, const char* szPlatform)
{
	CChars	szConfigurationName;
	CChars	szPlatformName;
	bool	bResult;

	szConfigurationName.Init();
	szPlatformName.Init();
	GetConfigurationName(&szConfigurationName);
	GetPlatformName(&szPlatformName);
	bResult = ((szConfiguration == NULL) || szConfigurationName.Equals(szConfiguration)) && ((szPlatform == NULL) || szPlatformName.Equals(szPlatform));
	szPlatformName.Kill();
	szConfigurationName.Kill();
	return bResult;
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
bool CBuildConfiguration::HasEmulator(void) { return !mszEmulator.Empty(); }
char* CBuildConfiguration::GetEmulator(void) { return mszEmulator.Text(); }
char* CBuildConfiguration::GetEmulatorDirectory(void) { return mszEmulatorDirectory.Text(); }
char* CBuildConfiguration::GetEmulatorArguments(void) { return mszEmulatorArguments.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// Whether a build links what it compiles into an image (GetImage).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::HasImage(void)
{
	return mbImage;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CAsm65Image* CBuildConfiguration::GetImage(void)
{
	return &mcImage;
}

