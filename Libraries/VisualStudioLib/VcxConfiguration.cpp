/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "VcxConfiguration.h"


//////////////////////////////////////////////////////////////////////////
//
// szName is the configuration and the platform: "Debug|Win32".
//
//////////////////////////////////////////////////////////////////////////
void CVcxConfiguration::Init(const char* szName)
{
	const char*	szBar;

	mszName.Init(szName);
	mszConfiguration.Init();
	mszPlatform.Init();
	szBar = strchr(szName, '|');
	if (szBar != NULL)
	{
		mszConfiguration.Append(szName, (int)(szBar - szName));
		mszPlatform.Append(szBar + 1);
	}
	else
	{
		mszConfiguration.Append(szName);
	}
	mcProperties.Init();
	mpcBase = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxConfiguration::Kill(void)
{
	mcProperties.Kill();
	mszPlatform.Kill();
	mszConfiguration.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A property set again replaces its value, as MSBuild does.
//
//////////////////////////////////////////////////////////////////////////
void CVcxConfiguration::SetProperty(const char* szName, const char* szValue)
{
	mcProperties.Put(szName, szValue);
}


//////////////////////////////////////////////////////////////////////////
//
// NULL if the project doesn't set it for this configuration, nor, for a
// toolchain's configuration, for its base where the base's is taken: a
// property group's property (ConfigurationType, IncludePath) and the
// compiler's include directories.
//
//////////////////////////////////////////////////////////////////////////
char* CVcxConfiguration::GetProperty(const char* szName)
{
	char*	szValue;

	szValue = mcProperties.Get(szName);
	if ((szValue == NULL) && (mpcBase != NULL) && (!strchr(szName, '.') || (strcmp(szName, "ClCompile.AdditionalIncludeDirectories") == 0)))
	{
		szValue = mpcBase->GetProperty(szName);
	}
	return szValue;
}


//////////////////////////////////////////////////////////////////////////
//
// What the configuration itself was given, NULL if nothing.
//
//////////////////////////////////////////////////////////////////////////
char* CVcxConfiguration::GetOwnProperty(const char* szName)
{
	return mcProperties.Get(szName);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* GetVcxToolchainBasePlatform(const char* szPlatform)
{
	if (strcmp(szPlatform, VCX_PLATFORM_GCC_X64) == 0)
	{
		return "x64";
	}
	if ((strcmp(szPlatform, VCX_PLATFORM_HCP65C816) == 0) || (strcmp(szPlatform, VCX_PLATFORM_HCP65C64) == 0))
	{
		return "Win32";
	}
	return NULL;
}


bool CVcxConfiguration::IsToolchain(void) const { return GetVcxToolchainBasePlatform(mszPlatform.Text()) != NULL; }
void CVcxConfiguration::SetBase(CVcxConfiguration* pcBase) { mpcBase = pcBase; }


const char* CVcxConfiguration::GetName(void) const { return mszName.Text(); }
const char* CVcxConfiguration::GetConfiguration(void) const { return mszConfiguration.Text(); }
const char* CVcxConfiguration::GetPlatform(void) const { return mszPlatform.Text(); }
size CVcxConfiguration::NumProperties(void) { return mcProperties.NumElements(); }

