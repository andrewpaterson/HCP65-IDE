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
	masSources.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxConfiguration::Kill(void)
{
	size	i;

	for (i = 0; i < masSources.NumElements(); i++)
	{
		masSources.Get(i)->Kill();
	}
	masSources.Kill();
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
// NULL if the project doesn't set it for this configuration.  A toolchain's
// configuration takes from its base what a project says for each of Visual
// Studio's configurations and not for a toolchain's: its include
// directories, which are the base's whatever its own (a sheet gives every
// configuration some, and a project adds its own to Visual Studio's), and
// any other property group's property it hasn't (ConfigurationType).
//
//////////////////////////////////////////////////////////////////////////
char* CVcxConfiguration::GetProperty(const char* szName)
{
	char*	szValue;
	bool	bIncludes;

	bIncludes = (strcmp(szName, "IncludePath") == 0) || (strcmp(szName, "ClCompile.AdditionalIncludeDirectories") == 0);
	szValue = (bIncludes && mpcBase) ? mpcBase->GetProperty(szName) : NULL;
	if (szValue == NULL)
	{
		szValue = mcProperties.Get(szName);
	}
	if ((szValue == NULL) && (mpcBase != NULL) && !strchr(szName, '.'))
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
void CVcxSettingSource::Init(const char* szName, const char* szFile, const char* szCondition, const char* szValue)
{
	mszName.Init(szName);
	mszFile.Init(szFile);
	mszCondition.Init(szCondition ? szCondition : "");
	mszValue.Init(szValue);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSettingSource::Kill(void)
{
	mszValue.Kill();
	mszCondition.Kill();
	mszFile.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A setting ("HCP65Compile.NoFloats") was set for this configuration by a
// group of szFile with szCondition (NULL for none).
//
//////////////////////////////////////////////////////////////////////////
void CVcxConfiguration::AddSource(const char* szName, const char* szFile, const char* szCondition, const char* szValue)
{
	masSources.Add()->Init(szName, szFile, szCondition, szValue);
}


//////////////////////////////////////////////////////////////////////////
//
// Where a setting was set, in the order read: the last is where its value
// is from.
//
//////////////////////////////////////////////////////////////////////////
void CVcxConfiguration::GetSources(const char* szName, CArrayTemplatePtr<CVcxSettingSource>* papcDest)
{
	size	i;

	for (i = 0; i < masSources.NumElements(); i++)
	{
		if (masSources.Get(i)->mszName.Equals(szName))
		{
			papcDest->Add(masSources.Get(i));
		}
	}
}


size CVcxConfiguration::NumSources(void) const { return masSources.NumElements(); }
CVcxSettingSource* CVcxConfiguration::GetSource(size uiIndex) { return masSources.Get(uiIndex); }


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

