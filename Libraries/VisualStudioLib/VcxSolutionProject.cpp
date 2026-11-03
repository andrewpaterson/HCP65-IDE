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
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "BaseLib/StringHelper.h"
#include "VcxPath.h"
#include "VcxSolutionProject.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolutionProjectConfiguration::Init(const char* szSolutionConfiguration)
{
	mszSolutionConfiguration.Init(szSolutionConfiguration);
	mszProjectConfiguration.Init();
	mbBuild = false;
	mbDeploy = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolutionProjectConfiguration::Kill(void)
{
	mszProjectConfiguration.Kill();
	mszSolutionConfiguration.Kill();
}


char* CVcxSolutionProjectConfiguration::GetSolutionConfiguration(void) { return mszSolutionConfiguration.Text(); }
char* CVcxSolutionProjectConfiguration::GetProjectConfiguration(void) { return mszProjectConfiguration.Text(); }
void CVcxSolutionProjectConfiguration::SetProjectConfiguration(const char* szProjectConfiguration) { mszProjectConfiguration.Set(szProjectConfiguration); }
bool CVcxSolutionProjectConfiguration::IsBuild(void) { return mbBuild; }
void CVcxSolutionProjectConfiguration::SetBuild(bool bBuild) { mbBuild = bBuild; }
bool CVcxSolutionProjectConfiguration::IsDeploy(void) { return mbDeploy; }
void CVcxSolutionProjectConfiguration::SetDeploy(bool bDeploy) { mbDeploy = bDeploy; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolutionProject::Init(const char* szTypeGuid, const char* szName, const char* szPath, const char* szGuid)
{
	mszTypeGuid.Init(szTypeGuid);
	mszName.Init(szName);
	mszPath.Init(szPath);
	mszFullPath.Init();
	mszGuid.Init(szGuid);
	mszParentGuid.Init();
	maszSolutionItems.Init();
	maszDependencies.Init();
	masConfigurations.Init();
	mpcProject = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolutionProject::Kill(void)
{
	size	i;

	SafeKill(mpcProject);
	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		masConfigurations.Get(i)->Kill();
	}
	masConfigurations.Kill();
	maszDependencies.Kill();
	maszSolutionItems.Kill();
	mszParentGuid.Kill();
	mszGuid.Kill();
	mszFullPath.Kill();
	mszPath.Kill();
	mszName.Kill();
	mszTypeGuid.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the .vcxproj.  A folder or another kind of project has nothing to
// read.  A project that can't be read stays unloaded, as Visual Studio
// shows it, and the solution is still read.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolutionProject::Load(const char* szSolutionDirectory)
{
	CVcxProject*	pcProject;

	if (IsFolder())
	{
		return true;
	}

	mszFullPath.Clear();
	ResolveVcxPath(szSolutionDirectory, mszPath.Text(), &mszFullPath);
	if (!IsVcxProject())
	{
		return true;
	}

	SafeKill(mpcProject);
	pcProject = NewMalloc<CVcxProject>();
	pcProject->Init();
	if (!pcProject->Read(mszFullPath.Text()))
	{
		SafeKill(pcProject);
		return false;
	}
	mpcProject = pcProject;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A project of an unknown type is still a .vcxproj if its file is one.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolutionProject::IsVcxProject(void)
{
	return mszTypeGuid.EqualsIgnoreCase(VCX_CPP_PROJECT_TYPE) || mszPath.EndsWithIgnoreCase(".vcxproj");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolutionProject::IsFolder(void)
{
	return mszTypeGuid.EqualsIgnoreCase(VCX_SOLUTION_FOLDER_TYPE);
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration of this project szSolutionConfiguration builds,
// added if the solution hasn't named it yet.
//
//////////////////////////////////////////////////////////////////////////
CVcxSolutionProjectConfiguration* CVcxSolutionProject::GetOrAddConfiguration(const char* szSolutionConfiguration)
{
	CVcxSolutionProjectConfiguration*	pcConfiguration;

	pcConfiguration = FindConfiguration(szSolutionConfiguration);
	if (pcConfiguration == NULL)
	{
		pcConfiguration = masConfigurations.Add();
		pcConfiguration->Init(szSolutionConfiguration);
	}
	return pcConfiguration;
}


//////////////////////////////////////////////////////////////////////////
//
// Solution configuration names aren't case sensitive.
//
//////////////////////////////////////////////////////////////////////////
CVcxSolutionProjectConfiguration* CVcxSolutionProject::FindConfiguration(const char* szSolutionConfiguration)
{
	size								i;
	CVcxSolutionProjectConfiguration*	pcConfiguration;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		pcConfiguration = masConfigurations.Get(i);
		if (StringInsensitiveCompare(pcConfiguration->GetSolutionConfiguration(), szSolutionConfiguration) == 0)
		{
			return pcConfiguration;
		}
	}
	return NULL;
}


char* CVcxSolutionProject::GetTypeGuid(void) { return mszTypeGuid.Text(); }
char* CVcxSolutionProject::GetName(void) { return mszName.Text(); }
char* CVcxSolutionProject::GetPath(void) { return mszPath.Text(); }
char* CVcxSolutionProject::GetFullPath(void) { return mszFullPath.Text(); }
char* CVcxSolutionProject::GetGuid(void) { return mszGuid.Text(); }
void CVcxSolutionProject::SetGuid(const char* szGuid) { mszGuid.Set(szGuid); }
char* CVcxSolutionProject::GetParentGuid(void) { return mszParentGuid.Text(); }
void CVcxSolutionProject::SetParentGuid(const char* szParentGuid) { mszParentGuid.Set(szParentGuid); }
bool CVcxSolutionProject::IsLoaded(void) { return mpcProject != NULL; }
CVcxProject* CVcxSolutionProject::GetProject(void) { return mpcProject; }
void CVcxSolutionProject::AddSolutionItem(const char* szItem) { maszSolutionItems.Add(szItem); }
size CVcxSolutionProject::NumSolutionItems(void) { return maszSolutionItems.NumElements(); }
char* CVcxSolutionProject::GetSolutionItem(size uiIndex) { return maszSolutionItems.GetText(uiIndex); }
void CVcxSolutionProject::AddDependency(const char* szGuid) { maszDependencies.Add(szGuid); }
size CVcxSolutionProject::NumDependencies(void) { return maszDependencies.NumElements(); }
char* CVcxSolutionProject::GetDependency(size uiIndex) { return maszDependencies.GetText(uiIndex); }
size CVcxSolutionProject::NumConfigurations(void) { return masConfigurations.NumElements(); }
CVcxSolutionProjectConfiguration* CVcxSolutionProject::GetConfiguration(size uiIndex) { return masConfigurations.Get(uiIndex); }

