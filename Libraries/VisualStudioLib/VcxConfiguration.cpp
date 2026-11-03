/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
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
// NULL if the project doesn't set it for this configuration.
//
//////////////////////////////////////////////////////////////////////////
char* CVcxConfiguration::GetProperty(const char* szName)
{
	return mcProperties.Get(szName);
}


char* CVcxConfiguration::GetName(void) { return mszName.Text(); }
char* CVcxConfiguration::GetConfiguration(void) { return mszConfiguration.Text(); }
char* CVcxConfiguration::GetPlatform(void) { return mszPlatform.Text(); }
size CVcxConfiguration::NumProperties(void) { return mcProperties.NumElements(); }

