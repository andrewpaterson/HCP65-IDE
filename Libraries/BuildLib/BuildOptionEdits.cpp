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
#include "VisualStudioLib/VcxProjectConfigurations.h"
#include "VisualStudioLib/VcxSetting.h"
#include "VisualStudioLib/VcxSolutionRows.h"
#include "BuildOptionEdits.h"


#define BUILD_OPTION_COMPILE	"HCP65Compile"
#define BUILD_OPTION_LINK		"HCP65Link"


//The whole program's compile options, each project's own, then what the program is linked into.
static const SBuildOption gasBuildOptions[] =
{
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_NO_FLOATS, "No floating point", true, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_MATH_TABLES, "Maths tables in memory", true, true, "true" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_PREFER_SIZE, "Prefer size to speed", true, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_EXCEPTIONS, "C++ exceptions", true, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_KEEP_UNUSED_OBJECTS, "Keep unused globals that are made or destroyed", true, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_STUB_MISSING, "A missing function stops the program, not the build", true, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_DEBUG, "Built for the debugger", false, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_UNOPTIMISED, "Compiled without the optimiser", false, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_WARN_MISSING_OVERRIDE, "Warn about overrides not declared override", false, true, "true" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_WARN_STRING_TO_POINTER, "Warn when a string literal becomes a char*", false, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_MULTI_WORD_CASTS, "Accept casts like unsigned int(3)", false, true, "false" },
	{ BUILD_OPTION_COMPILE, BUILD_CONFIGURATION_C_CONVERSIONS, "C sources: void* and numbers convert as in C", false, true, "false" },
	{ BUILD_OPTION_LINK, BUILD_CONFIGURATION_EXECUTABLE, "Executable", true, false, "" },
	{ BUILD_OPTION_LINK, BUILD_CONFIGURATION_MODULE, "Module", true, false, "" },
	{ BUILD_OPTION_LINK, BUILD_CONFIGURATION_IMAGE, "Image", true, false, "" },
	{ BUILD_OPTION_LINK, "Entry", "Entry", true, false, "" },
	{ BUILD_OPTION_LINK, "Limit", "Limit", true, false, "" },
	{ BUILD_OPTION_LINK, "Symbols", "Executable's symbols", true, false, "" },
	{ BUILD_OPTION_LINK, "Size", "Image's size", true, false, "" },
	{ BUILD_OPTION_LINK, "Fill", "Image's fill", true, false, "" },
	{ BUILD_OPTION_LINK, "Load", "Image's load address", true, false, "" },
	{ BUILD_OPTION_LINK, "Text", "Image's text address", true, false, "" },
	{ BUILD_OPTION_LINK, "Regions", "Image's regions", true, false, "" },
};


size NumBuildOptions(void) { return sizeof(gasBuildOptions) / sizeof(SBuildOption); }
const SBuildOption* GetBuildOption(size uiIndex) { return &gasBuildOptions[uiIndex]; }


//////////////////////////////////////////////////////////////////////////
//
// The option called szName ("NoFloats"); MAX_SIZE if none is.
//
//////////////////////////////////////////////////////////////////////////
size FindBuildOption(const char* szName)
{
	size	i;

	for (i = 0; i < NumBuildOptions(); i++)
	{
		if (strcmp(gasBuildOptions[i].szName, szName) == 0)
		{
			return i;
		}
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdit::Init(void)
{
	int	i;

	for (i = 0; i < BOP_NumPlaces; i++)
	{
		mabSet[i] = false;
		maszValues[i].Init();
		maszFiles[i].Init();
		maszConditions[i].Init();
	}
	muiSheetProjects = 0;
	mbConfigurationRead = false;
	mePlace = BOP_Default;
	mszValue.Init();
	mbEdited = false;
}


void CBuildOptionEdit::Kill(void)
{
	int	i;

	mszValue.Kill();
	for (i = 0; i < BOP_NumPlaces; i++)
	{
		maszConditions[i].Kill();
		maszFiles[i].Kill();
		maszValues[i].Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildProjectOptionEdits::Init(void)
{
	mszName.Init();
	mszFile.Init();
	mszGuid.Init();
	mszRow.Init();
	mszConfiguration.Init();
	mbStartup = false;
	masOptions.Init();
}


void CBuildProjectOptionEdits::Kill(void)
{
	size	i;

	for (i = 0; i < masOptions.NumElements(); i++)
	{
		masOptions.Get(i)->Kill();
	}
	masOptions.Kill();
	mszConfiguration.Kill();
	mszRow.Kill();
	mszGuid.Kill();
	mszFile.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurationOptionEdits::Init(void)
{
	mszName.Init();
	mszSource.Init();
	mszPlatform.Init();
	masProjects.Init();
	muiStartup = MAX_SIZE;
}


void CBuildConfigurationOptionEdits::Kill(void)
{
	size	i;

	for (i = 0; i < masProjects.NumElements(); i++)
	{
		masProjects.Get(i)->Kill();
	}
	masProjects.Kill();
	mszPlatform.Kill();
	mszSource.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The solution isn't held.  szStartupProject is the name of the project
// whose options are the whole program's; NULL or empty if there is none.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::Init(CVcxSolution* pcSolution, const char* szStartupProject)
{
	mpcSolution = pcSolution;
	mszStartupProject.Init(szStartupProject ? szStartupProject : "");
	mapcConfigurations.Init();
}


void CBuildOptionEdits::Kill(void)
{
	while (mapcConfigurations.NumElements() > 0)
	{
		Remove(mapcConfigurations.NumElements() - 1);
	}
	mapcConfigurations.Kill();
	mszStartupProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What is before a configuration's |, or all of it if it has none.
//
//////////////////////////////////////////////////////////////////////////
static void GetConfigurationPart(const char* szName, CChars* pszDest)
{
	const char*	szBar;

	szBar = strchr(szName, '|');
	pszDest->Set("");
	if (szBar)
	{
		pszDest->AppendSubString(szName, szBar);
	}
	else
	{
		pszDest->Append(szName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The solution configuration szName, its projects read as szSource's rows
// build them (szName itself, or the one a new configuration copies).
// Only an HCP65 configuration has projects: the options are that
// compiler's.  Returns its index.
//
//////////////////////////////////////////////////////////////////////////
size CBuildOptionEdits::Add(const char* szName, const char* szSource)
{
	CBuildConfigurationOptionEdits*		pcConfiguration;
	CVcxSolutionProject*				pcSolutionProject;
	CVcxSolutionProjectConfiguration*	pcRow;
	EBuildToolchain						eToolchain;
	const char*							szBar;
	size								i;

	pcConfiguration = (CBuildConfigurationOptionEdits*)malloc(sizeof(CBuildConfigurationOptionEdits));
	new(pcConfiguration) CBuildConfigurationOptionEdits();
	pcConfiguration->Init();
	pcConfiguration->mszName.Set(szName);
	pcConfiguration->mszSource.Set(szSource);
	szBar = strchr(szSource, '|');
	pcConfiguration->mszPlatform.Set(szBar ? szBar + 1 : "");
	mapcConfigurations.Add(pcConfiguration);

	if (szBar && CBuildConfiguration::FindToolchainOfPlatform(szBar + 1, &eToolchain) && ((eToolchain == BT_W65C816) || (eToolchain == BT_65C64)))
	{
		for (i = 0; i < mpcSolution->NumProjects(); i++)
		{
			pcSolutionProject = mpcSolution->GetProject(i);
			if (pcSolutionProject->IsVcxProject() && pcSolutionProject->IsLoaded())
			{
				pcRow = pcSolutionProject->FindConfiguration(szSource);
				if ((pcRow == NULL) || pcRow->IsBuild())
				{
					ReadProject(pcConfiguration, pcSolutionProject, pcRow, eToolchain);
				}
			}
		}
		//A project that says nothing of its own has the program's: known once every project is read.
		for (i = 0; i < pcConfiguration->masProjects.NumElements(); i++)
		{
			SetWhereSet(pcConfiguration, pcConfiguration->masProjects.Get(i), NULL, MAX_SIZE);
		}
	}
	return mapcConfigurations.NumElements() - 1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::Remove(size uiConfiguration)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;

	pcConfiguration = mapcConfigurations.GetPtr(uiConfiguration);
	pcConfiguration->Kill();
	free(pcConfiguration);
	mapcConfigurations.RemoveAt(uiConfiguration);
}


void CBuildOptionEdits::SetName(size uiConfiguration, const char* szName) { mapcConfigurations.GetPtr(uiConfiguration)->mszName.Set(szName); }
size CBuildOptionEdits::NumConfigurations(void) const { return mapcConfigurations.NumElements(); }
CBuildConfigurationOptionEdits* CBuildOptionEdits::GetConfiguration(size uiConfiguration) { return mapcConfigurations.GetPtr(uiConfiguration); }
size CBuildOptionEdits::NumProjects(size uiConfiguration) { return mapcConfigurations.GetPtr(uiConfiguration)->masProjects.NumElements(); }
CBuildProjectOptionEdits* CBuildOptionEdits::GetProject(size uiConfiguration, size uiProject) { return mapcConfigurations.GetPtr(uiConfiguration)->masProjects.Get(uiProject); }
size CBuildOptionEdits::GetStartupProject(size uiConfiguration) { return mapcConfigurations.GetPtr(uiConfiguration)->muiStartup; }
CBuildOptionEdit* CBuildOptionEdits::GetEdit(size uiConfiguration, size uiProject, size uiOption) { return GetProject(uiConfiguration, uiProject)->masOptions.Get(uiOption); }


//////////////////////////////////////////////////////////////////////////
//
// The condition of a project's group for one configuration of the
// platform, as the tools write it.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::GetOwnCondition(CBuildConfigurationOptionEdits* pcConfiguration, const char* szConfiguration, CChars* pszDest)
{
	pszDest->Set("'$(Configuration)|$(Platform)'=='");
	pszDest->Append(szConfiguration)->Append('|')->Append(pcConfiguration->mszPlatform)->Append('\'');
}


//////////////////////////////////////////////////////////////////////////
//
// A project as the configuration builds it: the project configuration its
// row names, or the one of the configuration's own name without a row,
// read on the toolchain's platform as a build reads it
// (CBuildConfiguration::FindProjectConfiguration).
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::ReadProject(CBuildConfigurationOptionEdits* pcConfiguration, CVcxSolutionProject* pcSolutionProject, CVcxSolutionProjectConfiguration* pcRow, EBuildToolchain eToolchain)
{
	CBuildProjectOptionEdits*	pcProject;
	CBuildConfiguration			cBuilt;
	CVcxConfiguration*			pcProjectConfiguration;
	const char*					szRow;
	size						i;

	szRow = pcRow ? pcRow->GetProjectConfiguration() : pcConfiguration->mszSource.Text();
	cBuilt.Init(pcConfiguration->mszSource.Text(), eToolchain, pcConfiguration->mszSource.Text());
	pcProjectConfiguration = cBuilt.FindProjectConfiguration(pcSolutionProject->GetProject(), szRow);
	cBuilt.Kill();

	pcProject = pcConfiguration->masProjects.Add();
	pcProject->Init();
	pcProject->mszName.Set(pcSolutionProject->GetName());
	pcProject->mszFile.Set(pcSolutionProject->GetProject()->GetFileName());
	pcProject->mszGuid.Set(pcSolutionProject->GetGuid());
	pcProject->mszRow.Set(pcRow ? szRow : "");
	if (pcProjectConfiguration)
	{
		pcProject->mszConfiguration.Set(pcProjectConfiguration->GetConfiguration());
	}
	else
	{
		GetConfigurationPart(szRow, &pcProject->mszConfiguration);
	}
	pcProject->mbStartup = mszStartupProject.Equals(pcSolutionProject->GetName());
	if (pcProject->mbStartup)
	{
		pcConfiguration->muiStartup = pcConfiguration->masProjects.NumElements() - 1;
	}
	for (i = 0; i < NumBuildOptions(); i++)
	{
		pcProject->masOptions.Add()->Init();
		ReadOption(pcConfiguration, pcProject, pcProjectConfiguration, i);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What each place says of an option, from where the project's
// configuration was given it (CVcxConfiguration's sources): a line of the
// project's own is its one configuration's, its platform's or another's by
// its condition, and anything else's is a sheet's.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::ReadOption(CBuildConfigurationOptionEdits* pcConfiguration, CBuildProjectOptionEdits* pcProject, CVcxConfiguration* pcProjectConfiguration, size uiOption)
{
	CArrayTemplatePtr<CVcxSettingSource>	apcSources;
	CVcxSettingSource*						pcSource;
	CBuildOptionEdit*						pcEdit;
	const SBuildOption*						psOption;
	CChars									szName;
	CChars									szValue;
	CChars									szOwn;
	EVcxCondition							eCondition;
	EBuildOptionPlace						ePlace;
	size									i;

	psOption = GetBuildOption(uiOption);
	pcEdit = pcProject->masOptions.Get(uiOption);
	szOwn.Init(pcProject->mszConfiguration);
	szOwn.Append('|')->Append(pcConfiguration->mszPlatform);

	pcEdit->mabSet[BOP_Default] = true;
	pcEdit->maszValues[BOP_Default].Set(psOption->szDefault);
	pcEdit->maszFiles[BOP_Configuration].Set(pcProject->mszFile);
	GetOwnCondition(pcConfiguration, pcProject->mszConfiguration.Text(), &pcEdit->maszConditions[BOP_Configuration]);
	pcEdit->maszFiles[BOP_Platform].Set(pcProject->mszFile);
	pcEdit->maszConditions[BOP_Platform].Set("'$(Platform)'=='");
	pcEdit->maszConditions[BOP_Platform].Append(pcConfiguration->mszPlatform)->Append('\'');

	szName.Init(psOption->szTool);
	szName.Append('.')->Append(psOption->szName);
	apcSources.Init();
	if (pcProjectConfiguration)
	{
		pcProjectConfiguration->GetSources(szName.Text(), &apcSources);
	}
	for (i = 0; i < apcSources.NumElements(); i++)
	{
		pcSource = apcSources.GetPtr(i);
		ePlace = BOP_Sheet;
		if (pcSource->mszFile.Equals(pcProject->mszFile))
		{
			szValue.Init();
			eCondition = pcSource->mszCondition.Empty() ? VCC_Other : ParseVcxCondition(pcSource->mszCondition.Text(), &szValue);
			if ((eCondition == VCC_Both) && szValue.Equals(szOwn))
			{
				ePlace = BOP_Configuration;
			}
			else if ((eCondition == VCC_Platform) && szValue.Equals(pcConfiguration->mszPlatform))
			{
				ePlace = BOP_Platform;
			}
			else
			{
				ePlace = BOP_Project;
			}
			szValue.Kill();
		}
		pcEdit->mabSet[ePlace] = true;
		pcEdit->maszValues[ePlace].Set(pcSource->mszValue);
		pcEdit->maszFiles[ePlace].Set(pcSource->mszFile);
		pcEdit->maszConditions[ePlace].Set(pcSource->mszCondition);
		pcEdit->mePlace = ePlace;
	}
	if (pcEdit->mabSet[BOP_Sheet])
	{
		pcEdit->muiSheetProjects = CountSheetProjects(pcEdit->maszFiles[BOP_Sheet].Text());
	}
	pcEdit->mbConfigurationRead = pcEdit->mabSet[BOP_Configuration];
	if (apcSources.NumElements() > 0)
	{
		pcEdit->mszValue.Set(pcEdit->maszValues[pcEdit->mePlace]);
	}
	else
	{
		pcEdit->mePlace = BOP_Default;
		pcEdit->mszValue.Set(psOption->szDefault);
	}
	apcSources.Kill();
	szName.Kill();
	szOwn.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// How many of the solution's projects import a sheet.
//
//////////////////////////////////////////////////////////////////////////
size CBuildOptionEdits::CountSheetProjects(const char* szSheet)
{
	CVcxProject*	pcProject;
	size			uiCount;
	size			i;
	size			j;

	uiCount = 0;
	for (i = 0; i < mpcSolution->NumProjects(); i++)
	{
		pcProject = mpcSolution->GetProject(i)->IsLoaded() ? mpcSolution->GetProject(i)->GetProject() : NULL;
		for (j = 0; pcProject && (j < pcProject->NumSheets()); j++)
		{
			if (strcmp(pcProject->GetSheet(j), szSheet) == 0)
			{
				uiCount++;
				break;
			}
		}
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// Where an option's value is from when nothing of the project's own says
// it: a sheet that does; else, for an option of a project's own code in a
// project that isn't the startup project, the program; else its default.
//
//////////////////////////////////////////////////////////////////////////
EBuildOptionPlace CBuildOptionEdits::GetBasePlace(CBuildConfigurationOptionEdits* pcConfiguration, CBuildProjectOptionEdits* pcProject, CBuildOptionEdit* pcEdit, size uiOption)
{
	if (pcEdit->mabSet[BOP_Sheet])
	{
		return BOP_Sheet;
	}
	return ((pcConfiguration->muiStartup != MAX_SIZE) && !pcProject->mbStartup && !GetBuildOption(uiOption)->bProgram) ? BOP_Program : BOP_Default;
}


//////////////////////////////////////////////////////////////////////////
//
// An edit's place and value from what the places say: the most particular
// that is set, else its base.  With no edit, every option of the project
// that hasn't been edited (uiOption is then ignored).
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::SetWhereSet(CBuildConfigurationOptionEdits* pcConfiguration, CBuildProjectOptionEdits* pcProject, CBuildOptionEdit* pcEdit, size uiOption)
{
	static const EBuildOptionPlace	aePlaces[] = { BOP_Configuration, BOP_Platform, BOP_Project };
	size							i;
	int								j;

	if (pcEdit == NULL)
	{
		for (i = 0; i < pcProject->masOptions.NumElements(); i++)
		{
			pcEdit = pcProject->masOptions.Get(i);
			if (!pcEdit->mbEdited && !pcEdit->mabSet[BOP_Configuration] && !pcEdit->mabSet[BOP_Platform] && !pcEdit->mabSet[BOP_Project])
			{
				SetWhereSet(pcConfiguration, pcProject, pcEdit, i);
			}
		}
		return;
	}
	for (j = 0; j < 3; j++)
	{
		if (pcEdit->mabSet[aePlaces[j]])
		{
			pcEdit->mePlace = aePlaces[j];
			pcEdit->mszValue.Set(pcEdit->maszValues[aePlaces[j]]);
			return;
		}
	}
	pcEdit->mePlace = GetBasePlace(pcConfiguration, pcProject, pcEdit, uiOption);
	pcEdit->mszValue.Set((pcEdit->mePlace == BOP_Program) ? "" : pcEdit->maszValues[pcEdit->mePlace].Text());
}


//////////////////////////////////////////////////////////////////////////
//
// An option's value.  One that is the program's is the startup project's,
// as it is now.
//
//////////////////////////////////////////////////////////////////////////
const char* CBuildOptionEdits::GetValue(size uiConfiguration, size uiProject, size uiOption)
{
	CBuildOptionEdit*	pcEdit;
	size				uiStartup;

	pcEdit = GetEdit(uiConfiguration, uiProject, uiOption);
	uiStartup = GetStartupProject(uiConfiguration);
	if ((pcEdit->mePlace == BOP_Program) && (uiStartup != MAX_SIZE) && (uiStartup != uiProject))
	{
		return GetValue(uiConfiguration, uiStartup, uiOption);
	}
	return pcEdit->mszValue.Text();
}


EBuildOptionPlace CBuildOptionEdits::GetPlace(size uiConfiguration, size uiProject, size uiOption) { return GetEdit(uiConfiguration, uiProject, uiOption)->mePlace; }


//////////////////////////////////////////////////////////////////////////
//
// The places an option's value can be from, the most particular first:
// the project's own for the one configuration and for the platform, a
// line of the project's with another condition if it has one, and its
// base (GetBasePlace).  paePlaces has room for BOP_NumPlaces.
//
//////////////////////////////////////////////////////////////////////////
size CBuildOptionEdits::GetPlaces(size uiConfiguration, size uiProject, size uiOption, EBuildOptionPlace* paePlaces)
{
	CBuildOptionEdit*	pcEdit;
	size				uiCount;

	pcEdit = GetEdit(uiConfiguration, uiProject, uiOption);
	uiCount = 0;
	paePlaces[uiCount++] = BOP_Configuration;
	paePlaces[uiCount++] = BOP_Platform;
	if (pcEdit->mabSet[BOP_Project])
	{
		paePlaces[uiCount++] = BOP_Project;
	}
	paePlaces[uiCount++] = GetBasePlace(mapcConfigurations.GetPtr(uiConfiguration), GetProject(uiConfiguration, uiProject), pcEdit, uiOption);
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// A place as the dialog says it: "This project, Release only",
// "HCP65Compile.props, 14 projects".
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::GetPlaceText(size uiConfiguration, size uiProject, size uiOption, EBuildOptionPlace ePlace, CChars* pszDest)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;
	CBuildProjectOptionEdits*		pcProject;
	CBuildOptionEdit*				pcEdit;
	const char*						szSeparator;
	CChars							szValue;
	EVcxCondition					eCondition;

	pcConfiguration = mapcConfigurations.GetPtr(uiConfiguration);
	pcEdit = GetEdit(uiConfiguration, uiProject, uiOption);
	szValue.Init();
	pszDest->Set("");
	if (ePlace == BOP_Configuration)
	{
		//A line the project has for the configuration it is built as, which is called something else and not yet
		//edited, is that one's: said so, as an edit of it gives the project a configuration of this name first.
		GetConfigurationPart(pcConfiguration->mszName.Text(), &szValue);
		pcProject = GetProject(uiConfiguration, uiProject);
		if (pcEdit->mabSet[BOP_Configuration] && !pcEdit->mbEdited && !szValue.Equals(pcProject->mszConfiguration))
		{
			pszDest->Append("This project, ")->Append(pcProject->mszConfiguration)->Append(" only, which ")->Append(szValue)->Append(" is built as");
		}
		else
		{
			pszDest->Append("This project, ")->Append(szValue)->Append(" only");
		}
	}
	else if (ePlace == BOP_Platform)
	{
		pszDest->Append("This project, every ")->Append(pcConfiguration->mszPlatform)->Append(" configuration");
	}
	else if ((ePlace == BOP_Project) || (ePlace == BOP_Sheet))
	{
		eCondition = pcEdit->maszConditions[ePlace].Empty() ? VCC_Other : ParseVcxCondition(pcEdit->maszConditions[ePlace].Text(), &szValue);
		if (ePlace == BOP_Project)
		{
			pszDest->Append("This project");
		}
		else
		{
			szSeparator = strrchr(pcEdit->maszFiles[ePlace].Text(), FILE_SEPARATOR[0]);
			pszDest->Append(szSeparator ? szSeparator + 1 : pcEdit->maszFiles[ePlace].Text());
			pszDest->Append(", ")->Append((uint32)pcEdit->muiSheetProjects)->Append((pcEdit->muiSheetProjects == 1) ? " project" : " projects");
		}
		if (pcEdit->maszConditions[ePlace].Empty())
		{
			pszDest->Append(", every configuration");
		}
		else if ((eCondition == VCC_Both) || (eCondition == VCC_Configuration))
		{
			//A whole configuration's is said by its name: the platform is the one being looked at.
			if (szValue.Contains('|'))
			{
				szValue.RemoveEnd(szValue.Find('|'));
			}
			pszDest->Append(", ")->Append(szValue)->Append(" only");
		}
		else if (eCondition == VCC_NotConfiguration)
		{
			pszDest->Append(", every configuration but ")->Append(szValue);
		}
		else if (eCondition == VCC_Platform)
		{
			pszDest->Append(", every ")->Append(szValue)->Append(" configuration");
		}
	}
	else if (ePlace == BOP_Program)
	{
		pszDest->Append("The program's (")->Append(mszStartupProject)->Append(")");
	}
	else
	{
		pszDest->Append("Default");
	}
	szValue.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A line that an edit sets or takes away is every other edit's that has
// it too: another configuration of the project (its platform's line),
// another project (a sheet's).  Each has what it says now, and one that
// hasn't been edited has its value from wherever that leaves it.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::Changed(CBuildOptionEdit* pcOrigin, size uiOption, EBuildOptionPlace ePlace, bool bSet, const char* szValue)
{
	static const EBuildOptionPlace	aePlaces[] = { BOP_Configuration, BOP_Platform, BOP_Project, BOP_Sheet };
	CBuildProjectOptionEdits*		pcProject;
	CBuildOptionEdit*				pcEdit;
	size							i;
	size							j;
	int								k;
	bool							bSame;

	for (i = 0; i < mapcConfigurations.NumElements(); i++)
	{
		for (j = 0; j < mapcConfigurations.GetPtr(i)->masProjects.NumElements(); j++)
		{
			pcProject = mapcConfigurations.GetPtr(i)->masProjects.Get(j);
			pcEdit = pcProject->masOptions.Get(uiOption);
			bSame = false;
			for (k = 0; (pcEdit != pcOrigin) && (k < 4); k++)
			{
				if (pcEdit->maszFiles[aePlaces[k]].Equals(pcOrigin->maszFiles[ePlace]) && pcEdit->maszConditions[aePlaces[k]].Equals(pcOrigin->maszConditions[ePlace]) && !pcEdit->maszFiles[aePlaces[k]].Empty())
				{
					bSame = true;
					pcEdit->mabSet[aePlaces[k]] = bSet;
					if (bSet)
					{
						pcEdit->maszValues[aePlaces[k]].Set(szValue);
						if (pcEdit->mePlace == aePlaces[k])
						{
							pcEdit->mszValue.Set(szValue);
						}
					}
				}
			}
			if (bSame && !pcEdit->mbEdited)
			{
				SetWhereSet(mapcConfigurations.GetPtr(i), pcProject, pcEdit, uiOption);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A new value, where the option's value is from.  The program's and the
// default are no line to change, so the value becomes the project's own
// for the platform.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::SetValue(size uiConfiguration, size uiProject, size uiOption, const char* szValue)
{
	CBuildOptionEdit*	pcEdit;

	if (strcmp(GetValue(uiConfiguration, uiProject, uiOption), szValue) == 0)
	{
		return;
	}
	pcEdit = GetEdit(uiConfiguration, uiProject, uiOption);
	if ((pcEdit->mePlace == BOP_Program) || (pcEdit->mePlace == BOP_Default))
	{
		pcEdit->mePlace = BOP_Platform;
	}
	pcEdit->mszValue.Set(szValue);
	pcEdit->mabSet[pcEdit->mePlace] = true;
	pcEdit->maszValues[pcEdit->mePlace].Set(szValue);
	pcEdit->mbEdited = true;
	if ((pcEdit->mePlace != BOP_Configuration) || !NeedsOwnConfiguration(uiConfiguration, uiProject))
	{
		Changed(pcEdit, uiOption, pcEdit->mePlace, true, szValue);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where an option's value is to be from.  Each line of the project's own
// that is more particular is taken away.  One of the project's own places
// keeps the line it has, or is given the value as it is now; a sheet's,
// the program's or the default is then the value.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::SetPlace(size uiConfiguration, size uiProject, size uiOption, EBuildOptionPlace ePlace)
{
	CBuildOptionEdit*	pcEdit;
	CChars				szValue;
	int					i;
	bool				bOwn;

	pcEdit = GetEdit(uiConfiguration, uiProject, uiOption);
	if (pcEdit->mePlace == ePlace)
	{
		return;
	}
	szValue.Init(GetValue(uiConfiguration, uiProject, uiOption));
	pcEdit->mbEdited = true;
	pcEdit->mePlace = ePlace;
	bOwn = NeedsOwnConfiguration(uiConfiguration, uiProject);
	for (i = BOP_Configuration; (i < (int)ePlace) && (i <= BOP_Project); i++)
	{
		if (pcEdit->mabSet[i])
		{
			pcEdit->mabSet[i] = false;
			if ((i != BOP_Configuration) || !bOwn)
			{
				Changed(pcEdit, uiOption, (EBuildOptionPlace)i, false, NULL);
			}
		}
	}
	if (ePlace <= BOP_Project)
	{
		if (pcEdit->mabSet[ePlace])
		{
			szValue.Set(pcEdit->maszValues[ePlace]);
		}
		pcEdit->mabSet[ePlace] = true;
		pcEdit->maszValues[ePlace].Set(szValue);
		pcEdit->mszValue.Set(szValue);
		if ((ePlace != BOP_Configuration) || !bOwn)
		{
			Changed(pcEdit, uiOption, ePlace, true, szValue.Text());
		}
	}
	else
	{
		pcEdit->mszValue.Set((ePlace == BOP_Program) ? "" : pcEdit->maszValues[ePlace].Text());
	}
	szValue.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBuildOptionEdits::IsEdited(void)
{
	size	i;
	size	j;
	size	k;

	for (i = 0; i < mapcConfigurations.NumElements(); i++)
	{
		for (j = 0; j < NumProjects(i); j++)
		{
			for (k = 0; k < NumBuildOptions(); k++)
			{
				if (GetEdit(i, j, k)->mbEdited)
				{
					return true;
				}
			}
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether an edit of the project's is one configuration's (or takes such
// a line away) while the solution configuration builds the project as a
// configuration called something else, whose line that would be.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildOptionEdits::NeedsOwnConfiguration(size uiConfiguration, size uiProject)
{
	CBuildProjectOptionEdits*	pcProject;
	CBuildOptionEdit*			pcEdit;
	CChars						szOwn;
	size						i;
	bool						bResult;

	pcProject = GetProject(uiConfiguration, uiProject);
	szOwn.Init();
	GetConfigurationPart(mapcConfigurations.GetPtr(uiConfiguration)->mszName.Text(), &szOwn);
	bResult = false;
	if (!szOwn.Equals(pcProject->mszConfiguration) && !pcProject->mszRow.Empty())
	{
		for (i = 0; i < pcProject->masOptions.NumElements(); i++)
		{
			pcEdit = pcProject->masOptions.Get(i);
			bResult |= pcEdit->mbEdited && ((pcEdit->mePlace == BOP_Configuration) || pcEdit->mbConfigurationRead);
		}
	}
	szOwn.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What OK will do that wasn't asked for in so many words: each project
// that is given a configuration of its own.  Empty if there is none.
//
//////////////////////////////////////////////////////////////////////////
void CBuildOptionEdits::GetNote(size uiConfiguration, CChars* pszDest)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;
	CBuildProjectOptionEdits*		pcProject;
	CChars							szOwn;
	size							i;

	pcConfiguration = mapcConfigurations.GetPtr(uiConfiguration);
	szOwn.Init();
	GetConfigurationPart(pcConfiguration->mszName.Text(), &szOwn);
	pszDest->Set("");
	for (i = 0; i < pcConfiguration->masProjects.NumElements(); i++)
	{
		if (NeedsOwnConfiguration(uiConfiguration, i))
		{
			pcProject = pcConfiguration->masProjects.Get(i);
			if (!pszDest->Empty())
			{
				pszDest->Append("  ");
			}
			pszDest->Append("OK gives ")->Append(pcProject->mszName)->Append(" a ")->Append(szOwn)->Append(" configuration of its own, a copy of its ")->Append(pcProject->mszConfiguration);
			pszDest->Append(", and the solution's ")->Append(pcConfiguration->mszName)->Append(" builds it as that.");
		}
	}
	szOwn.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the startup project's linking, as edited, can be read: false
// with why not in pszError (CBuildConfiguration::ReadProject's words).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildOptionEdits::Check(size uiConfiguration, CChars* pszError)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;
	CVcxConfiguration				cSettings;
	CBuildConfiguration				cBuilt;
	EBuildToolchain					eToolchain;
	CChars							szName;
	const char*						szValue;
	size							i;
	bool							bResult;

	pcConfiguration = mapcConfigurations.GetPtr(uiConfiguration);
	if ((pcConfiguration->muiStartup == MAX_SIZE) || !CBuildConfiguration::FindToolchainOfPlatform(pcConfiguration->mszPlatform.Text(), &eToolchain))
	{
		return true;
	}
	cSettings.Init(pcConfiguration->mszSource.Text());
	for (i = 0; i < NumBuildOptions(); i++)
	{
		szValue = GetValue(uiConfiguration, pcConfiguration->muiStartup, i);
		if ((strcmp(GetBuildOption(i)->szTool, BUILD_OPTION_LINK) == 0) && (szValue[0] != '\0'))
		{
			szName.Init(BUILD_OPTION_LINK ".");
			szName.Append(GetBuildOption(i)->szName);
			cSettings.SetProperty(szName.Text(), szValue);
			szName.Kill();
		}
	}
	cBuilt.Init(pcConfiguration->mszSource.Text(), eToolchain, pcConfiguration->mszSource.Text());
	bResult = cBuilt.ReadProject(&cSettings, pszError);
	cBuilt.Kill();
	cSettings.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What a place's line is to say: the edit's value, or no line at all
// (NULL) for an empty one with nothing less particular under it to
// replace, which says the same.
//
//////////////////////////////////////////////////////////////////////////
static const char* GetWritten(CBuildOptionEdit* pcEdit, EBuildOptionPlace ePlace)
{
	int		i;
	bool	bUnder;

	bUnder = false;
	for (i = ePlace + 1; i <= BOP_Sheet; i++)
	{
		bUnder |= pcEdit->mabSet[i];
	}
	return (pcEdit->mszValue.Empty() && !bUnder) ? NULL : pcEdit->mszValue.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// A project's edits written: first its own configuration if an edit needs
// one (a copy of the one it is built as, which the solution's row then
// names), then each edited option's line set where its value is to be
// from, and the lines more particular than that taken away.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildOptionEdits::ApplyProject(const char* szSolutionFile, size uiConfiguration, size uiProject, CChars* pszNote)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;
	CBuildProjectOptionEdits*		pcProject;
	CBuildOptionEdit*	pcEdit;
	const SBuildOption*	psOption;
	CArrayChars			aszNames;
	CChars				szOwn;
	CChars				szRow;
	CChars				szCondition;
	const char*			szFile;
	const char*			szBar;
	size				i;
	bool				bResult;

	bResult = true;
	pcConfiguration = mapcConfigurations.GetPtr(uiConfiguration);
	pcProject = pcConfiguration->masProjects.Get(uiProject);
	szFile = pcProject->mszFile.Text();
	szOwn.Init();
	szCondition.Init();
	if (NeedsOwnConfiguration(uiConfiguration, uiProject))
	{
		GetConfigurationPart(pcConfiguration->mszName.Text(), &szOwn);
		aszNames.Init();
		bResult = GetVcxProjectConfigurationNames(szFile, &aszNames) && (aszNames.Contains(szOwn.Text()) || AddVcxProjectConfiguration(szFile, szOwn.Text(), pcProject->mszConfiguration.Text()));
		aszNames.Kill();
		szBar = strchr(pcProject->mszRow.Text(), '|');
		szRow.Init(szOwn);
		szRow.Append(szBar ? szBar : "");
		bResult = bResult && SetVcxSolutionProjectConfiguration(szSolutionFile, pcProject->mszGuid.Text(), pcConfiguration->mszName.Text(), szRow.Text());
		szRow.Kill();
		if (bResult)
		{
			if (!pszNote->Empty())
			{
				pszNote->Append("  ");
			}
			pszNote->Append(pcProject->mszName)->Append(" has a ")->Append(szOwn)->Append(" configuration of its own now, a copy of its ")->Append(pcProject->mszConfiguration);
			pszNote->Append(", which ")->Append(pcConfiguration->mszName)->Append(" builds it as.");
		}
		GetOwnCondition(pcConfiguration, szOwn.Text(), &szCondition);
	}

	for (i = 0; bResult && (i < pcProject->masOptions.NumElements()); i++)
	{
		pcEdit = pcProject->masOptions.Get(i);
		if (!pcEdit->mbEdited)
		{
			continue;
		}
		psOption = GetBuildOption(i);
		if (szOwn.Empty())
		{
			szCondition.Set(pcEdit->maszConditions[BOP_Configuration]);
		}
		if (pcEdit->mePlace == BOP_Configuration)
		{
			bResult = SetVcxSetting(szFile, szCondition.Text(), psOption->szTool, psOption->szName, GetWritten(pcEdit, BOP_Configuration));
			continue;
		}
		bResult = SetVcxSetting(szFile, szCondition.Text(), psOption->szTool, psOption->szName, NULL);
		if (bResult && (pcEdit->mePlace == BOP_Platform))
		{
			bResult = SetVcxSetting(szFile, pcEdit->maszConditions[BOP_Platform].Text(), psOption->szTool, psOption->szName, GetWritten(pcEdit, BOP_Platform));
			continue;
		}
		bResult = bResult && SetVcxSetting(szFile, pcEdit->maszConditions[BOP_Platform].Text(), psOption->szTool, psOption->szName, NULL);
		if (bResult && (pcEdit->mePlace == BOP_Project))
		{
			bResult = SetVcxSetting(szFile, pcEdit->maszConditions[BOP_Project].Text(), psOption->szTool, psOption->szName, GetWritten(pcEdit, BOP_Project));
			continue;
		}
		if (bResult && !pcEdit->maszFiles[BOP_Project].Empty())
		{
			bResult = SetVcxSetting(szFile, pcEdit->maszConditions[BOP_Project].Text(), psOption->szTool, psOption->szName, NULL);
		}
		if (bResult && (pcEdit->mePlace == BOP_Sheet))
		{
			bResult = SetVcxSetting(pcEdit->maszFiles[BOP_Sheet].Text(), pcEdit->maszConditions[BOP_Sheet].Text(), psOption->szTool, psOption->szName, GetWritten(pcEdit, BOP_Sheet));
		}
	}
	szCondition.Kill();
	szOwn.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution's configurations made what the edits' are (question 35),
// in its rows alone: one that no edit keeps under its own name is taken
// away, and an edit under another name than its source's (a new one, or
// one renamed) is given its source's rows.  The new ones are first added
// under names nothing has, so two that swap names, or one that takes the
// name of one removed, each get the rows they were read from.  Nothing is
// written if they are as they were.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildOptionEdits::ApplyConfigurations(const char* szSolutionFile)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;
	CChars							szTemporary;
	size							i;
	size							j;
	bool							bKept;
	bool							bResult;

	bResult = true;
	szTemporary.Init();
	for (i = 0; bResult && (i < mapcConfigurations.NumElements()); i++)
	{
		pcConfiguration = mapcConfigurations.GetPtr(i);
		if (!pcConfiguration->mszName.Equals(pcConfiguration->mszSource))
		{
			szTemporary.Set("~");
			szTemporary.Append((uint32)i)->Append('|')->Append(pcConfiguration->mszPlatform);
			bResult = AddVcxSolutionConfiguration(szSolutionFile, szTemporary.Text(), pcConfiguration->mszSource.Text());
		}
	}
	for (i = 0; bResult && (i < mpcSolution->NumConfigurations()); i++)
	{
		bKept = false;
		for (j = 0; j < mapcConfigurations.NumElements(); j++)
		{
			pcConfiguration = mapcConfigurations.GetPtr(j);
			bKept |= pcConfiguration->mszName.Equals(pcConfiguration->mszSource) && pcConfiguration->mszName.Equals(mpcSolution->GetConfiguration(i));
		}
		if (!bKept)
		{
			bResult = RemoveVcxSolutionConfiguration(szSolutionFile, mpcSolution->GetConfiguration(i));
		}
	}
	for (i = 0; bResult && (i < mapcConfigurations.NumElements()); i++)
	{
		pcConfiguration = mapcConfigurations.GetPtr(i);
		if (!pcConfiguration->mszName.Equals(pcConfiguration->mszSource))
		{
			szTemporary.Set("~");
			szTemporary.Append((uint32)i)->Append('|')->Append(pcConfiguration->mszPlatform);
			bResult = RenameVcxSolutionConfiguration(szSolutionFile, szTemporary.Text(), pcConfiguration->mszName.Text());
		}
	}
	szTemporary.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Everything written: the solution's configurations (ApplyConfigurations:
// the edits here are all of them, one for each the solution is to have),
// then every edited option in its file.  pszNote says what was done that
// wasn't asked for in so many words (GetNote).  False if a file couldn't
// be written; what was written before it stays.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildOptionEdits::Apply(const char* szSolutionFile, CChars* pszNote)
{
	CBuildConfigurationOptionEdits*	pcConfiguration;
	size							i;
	size							j;
	bool							bResult;

	bResult = ApplyConfigurations(szSolutionFile);
	for (i = 0; bResult && (i < mapcConfigurations.NumElements()); i++)
	{
		pcConfiguration = mapcConfigurations.GetPtr(i);
		for (j = 0; bResult && (j < pcConfiguration->masProjects.NumElements()); j++)
		{
			bResult = ApplyProject(szSolutionFile, i, j, pszNote);
		}
	}
	return bResult;
}
