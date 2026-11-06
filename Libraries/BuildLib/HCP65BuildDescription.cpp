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
#include "HCP65BuildDescription.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildIncludes::Init(void)
{
	maszDirectories.Init();
	maiSubDirectories.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildIncludes::Kill(void)
{
	maiSubDirectories.Kill();
	maszDirectories.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildIncludes::Add(const char* szDirectory, bool bSubDirectories)
{
	maszDirectories.Add(szDirectory);
	maiSubDirectories.Add(bSubDirectories ? 1 : 0);
}


size CHCP65BuildIncludes::NumIncludes(void) { return maszDirectories.NumElements(); }
char* CHCP65BuildIncludes::GetDirectory(size uiIndex) { return maszDirectories.GetText(uiIndex); }
bool CHCP65BuildIncludes::HasSubDirectories(size uiIndex) { return maiSubDirectories.GetValue(uiIndex) != 0; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildProject::Init(const char* szName, const char* szDirectory, const char* szLabel, bool bLibrary)
{
	mszName.Init(szName);
	mszDirectory.Init(szDirectory);
	mszLabel.Init(szLabel ? szLabel : "");
	mszUnreadable.Init();
	mcIncludes.Init();
	maszSources.Init();
	maszDefines.Init();
	mbLibrary = bLibrary;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildProject::Kill(void)
{
	maszDefines.Kill();
	maszSources.Kill();
	mcIncludes.Kill();
	mszUnreadable.Kill();
	mszLabel.Kill();
	mszDirectory.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildDescription::Init(void)
{
	mcIncludes.Init();
	macProjects.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildDescription::Kill(void)
{
	Clear();
	macProjects.Kill();
	mcIncludes.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing to compile.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BuildDescription::Clear(void)
{
	size	i;

	for (i = 0; i < macProjects.NumElements(); i++)
	{
		macProjects.Get(i)->Kill();
	}
	macProjects.ReInit();
	mcIncludes.Kill();
	mcIncludes.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// A project added after the others.  The pointer is good until the next
// is added.
//
//////////////////////////////////////////////////////////////////////////
CHCP65BuildProject* CHCP65BuildDescription::AddProject(const char* szName, const char* szDirectory, const char* szLabel, bool bLibrary)
{
	CHCP65BuildProject*	pcProject;

	pcProject = macProjects.Add();
	pcProject->Init(szName, szDirectory, szLabel, bLibrary);
	return pcProject;
}


size CHCP65BuildDescription::NumProjects(void) { return macProjects.NumElements(); }
CHCP65BuildProject* CHCP65BuildDescription::GetProject(size uiIndex) { return macProjects.Get(uiIndex); }
CHCP65BuildProject* CHCP65BuildDescription::GetLastProject(void) { return (macProjects.NumElements() > 0) ? macProjects.Tail() : NULL; }

