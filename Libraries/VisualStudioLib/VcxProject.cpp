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
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "VcxPath.h"
#include "VcxProject.h"


#define VCX_MAX_MACRO_DEPTH		16


//////////////////////////////////////////////////////////////////////////
//
// Reads the XML file szFileName (a path, with or without a directory).
//
//////////////////////////////////////////////////////////////////////////
bool ReadVcxXMLFile(CXMLFile* pcFile, const char* szFileName)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	CChars		szDirectory;
	bool		bResult;

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	bResult = pcFile->Read(szName.Text(), szDirectory.Text());
	szDirectory.Kill();
	szName.Kill();
	return bResult && (pcFile->mcMarkup.mpcDoc != NULL) && (pcFile->mcMarkup.mpcDoc->GetRootTag() != NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// The text of pcTag's child szChild (or of pcTag itself when szChild is
// NULL) appended to pszDest.  False if there is no such child.
//
//////////////////////////////////////////////////////////////////////////
bool GetVcxTagText(CMarkupTag* pcTag, const char* szChild, CChars* pszDest)
{
	CMarkupTag*	pcChild;

	if (szChild != NULL)
	{
		pcChild = pcTag->GetTag(szChild);
		if (pcChild == NULL)
		{
			return false;
		}
		pcTag = pcChild;
	}
	pcTag->GetText(pszDest);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What a condition is on, and the value it wants appended to pszValue:
// '$(Configuration)|$(Platform)'=='Debug|Win32' a whole configuration,
// '$(Configuration)'=='Debug' every platform's Debug and
// '$(Platform)'=='GCC-x64' every configuration of a platform.
// '$(Configuration)'!='Debug' is every configuration not called that
// (VCC_NotConfiguration): what Release has, a configuration built as it is
// has too (Reference).  Anything else (exists('...'), a condition on another
// property) is VCC_Other, which is true of no configuration.
//
//////////////////////////////////////////////////////////////////////////
EVcxCondition ParseVcxCondition(const char* szCondition, CChars* pszValue)
{
	const char*		szEquals;
	const char*		szStart;
	const char*		szEnd;
	bool			bConfiguration;
	bool			bPlatform;
	bool			bNot;

	szEquals = strstr(szCondition, "==");
	bNot = szEquals == NULL;
	if (bNot)
	{
		szEquals = strstr(szCondition, "!=");
	}
	szStart = szEquals ? strchr(szEquals, '\'') : NULL;
	szEnd = szStart ? strchr(szStart + 1, '\'') : NULL;
	if (szEnd == NULL)
	{
		return VCC_Other;
	}
	bConfiguration = FindVcxConditionProperty(szCondition, szEquals, "$(Configuration)");
	bPlatform = FindVcxConditionProperty(szCondition, szEquals, "$(Platform)");
	if ((!bConfiguration && !bPlatform) || (bNot && bPlatform))
	{
		return VCC_Other;
	}
	pszValue->Append(szStart + 1, (size)(szEnd - szStart - 1));
	if (bNot)
	{
		return VCC_NotConfiguration;
	}
	return bConfiguration ? (bPlatform ? VCC_Both : VCC_Configuration) : VCC_Platform;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szProperty is on the left of a condition's == (or !=).
//
//////////////////////////////////////////////////////////////////////////
bool FindVcxConditionProperty(const char* szCondition, const char* szEquals, const char* szProperty)
{
	const char*	szFound;

	szFound = strstr(szCondition, szProperty);
	return (szFound != NULL) && (szFound < szEquals);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::Init(void)
{
	mszFileName.Init();
	mszDirectory.Init();
	mszName.Init();
	mszGuid.Init();
	mszRootNamespace.Init();
	mbFilters = false;
	masConfigurations.Init();
	masItems.Init();
	masFilters.Init();
	masReferences.Init();
	maszSheets.Init();
	mszReadingDirectory = NULL;
	mszReadingFile = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::Kill(void)
{
	size	i;

	for (i = 0; i < masReferences.NumElements(); i++)
	{
		masReferences.Get(i)->Kill();
	}
	masReferences.Kill();
	for (i = 0; i < masFilters.NumElements(); i++)
	{
		masFilters.Get(i)->Kill();
	}
	masFilters.Kill();
	for (i = 0; i < masItems.NumElements(); i++)
	{
		masItems.Get(i)->Kill();
	}
	masItems.Kill();
	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		masConfigurations.Get(i)->Kill();
	}
	masConfigurations.Kill();
	maszSheets.Kill();
	mszRootNamespace.Kill();
	mszGuid.Kill();
	mszName.Kill();
	mszDirectory.Kill();
	mszFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads szFileName (a .vcxproj), and szFileName.filters if it is there.
// A project without a filters file has no filters: Solution Explorer
// shows its items without folders.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::Read(const char* szFileName)
{
	CFileUtil	cFileUtil;
	CXMLFile	cFile;
	CChars		szFilters;
	bool		bResult;

	SplitVcxFileName(szFileName, &mszFileName, &mszDirectory, &mszName);

	cFile.Init();
	bResult = ReadVcxXMLFile(&cFile, mszFileName.Text());
	if (bResult)
	{
		bResult = ReadProject(cFile.mcMarkup.mpcDoc->GetRootTag());
	}
	else
	{
		gcLogger.Error2(__METHOD__, " Could not read project [", szFileName, "].", NULL);
	}
	cFile.Kill();
	if (!bResult)
	{
		return false;
	}

	szFilters.Init(mszFileName);
	szFilters.Append(".filters");
	if (cFileUtil.Exists(szFilters.Text()))
	{
		cFile.Init();
		bResult = ReadVcxXMLFile(&cFile, szFilters.Text());
		if (bResult)
		{
			bResult = ReadFilters(cFile.mcMarkup.mpcDoc->GetRootTag());
		}
		else
		{
			gcLogger.Error2(__METHOD__, " Could not read filters [", szFilters.Text(), "].", NULL);
		}
		cFile.Kill();
		mbFilters = bResult;
	}
	szFilters.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The <Project> of a .vcxproj, read twice over, its property sheets with
// it.  First for its configurations: those it lists and those of the
// toolchains that a condition anywhere is for.  Then for what each group
// gives them, in the order MSBuild reads them, so a later group replaces
// or adds to an earlier one's.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::ReadProject(CMarkupTag* pcProject)
{
	CArrayChars	aszPlatforms;
	size		i;

	if (!pcProject->Is("Project"))
	{
		return gcLogger.Error2(__METHOD__, " Expected a <Project> in [", mszFileName.Text(), "], not <", pcProject->GetName(), ">.", NULL);
	}

	maszScope.Init();
	mapcSheetFiles.Init();
	mszReadingDirectory = NULL;
	aszPlatforms.Init();
	ReadProjectConfigurations(pcProject);
	ReadGroups(pcProject, mszDirectory.Text(), &aszPlatforms, 0);
	AddToolchainConfigurations(&aszPlatforms);
	ReadGroups(pcProject, mszDirectory.Text(), NULL, 0);
	SetToolchainBases();
	aszPlatforms.Kill();
	for (i = 0; i < mapcSheetFiles.NumElements(); i++)
	{
		mapcSheetFiles.GetPtr(i)->Kill();
		free(mapcSheetFiles.GetPtr(i));
	}
	mapcSheetFiles.Kill();
	maszScope.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The configurations the project lists, <ItemGroup
// Label="ProjectConfigurations">: Visual Studio's.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadProjectConfigurations(CMarkupTag* pcProject)
{
	CMarkupTag*		pcGroup;
	CMarkupTag*		pcTag;
	STagIterator	sGroupIter;
	STagIterator	sIter;
	char*			szInclude;

	for (pcGroup = pcProject->GetTag("ItemGroup", &sGroupIter); pcGroup; pcGroup = pcProject->GetNextTag(&sGroupIter))
	{
		for (pcTag = pcGroup->GetTag("ProjectConfiguration", &sIter); pcTag; pcTag = pcGroup->GetNextTag(&sIter))
		{
			szInclude = pcTag->GetStringAttribute("Include");
			if (szInclude != NULL)
			{
				AddConfiguration(szInclude);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The groups of a project or a property sheet (pcRoot, in szDirectory), and
// those of the sheets it imports where it imports them.  With paszPlatforms
// it only looks for the toolchains' configurations (FindToolchainConditions);
// without, it reads the groups.  A sheet has no items: only the project's
// item groups are read.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadGroups(CMarkupTag* pcRoot, const char* szDirectory, CArrayChars* paszPlatforms, int iDepth)
{
	CMarkupTag*		pcTag;
	CMarkupTag*		pcImport;
	STagIterator	sIter;
	STagIterator	sImportIter;
	bool			bScoped;
	const char*		szOuterDirectory;

	szOuterDirectory = mszReadingDirectory;
	mszReadingDirectory = szDirectory;
	for (pcTag = pcRoot->GetTag(&sIter); pcTag; pcTag = pcRoot->GetNextTag(&sIter))
	{
		if (pcTag->Is("Import"))
		{
			ReadImport(pcTag, szDirectory, paszPlatforms, iDepth);
		}
		else if (pcTag->Is("ImportGroup"))
		{
			bScoped = PushScope(pcTag);
			for (pcImport = pcTag->GetTag("Import", &sImportIter); pcImport; pcImport = pcTag->GetNextTag(&sImportIter))
			{
				ReadImport(pcImport, szDirectory, paszPlatforms, iDepth);
			}
			PopScope(bScoped);
		}
		else if (paszPlatforms != NULL)
		{
			FindToolchainConditions(pcTag, paszPlatforms);
		}
		else if (pcTag->Is("ItemGroup"))
		{
			if (iDepth == 0)
			{
				ReadItemGroup(pcTag);
			}
		}
		else if (pcTag->Is("PropertyGroup"))
		{
			ReadPropertyGroup(pcTag);
		}
		else if (pcTag->Is("ItemDefinitionGroup"))
		{
			ReadItemDefinitionGroup(pcTag);
		}
	}
	mszReadingDirectory = szOuterDirectory;
}


//////////////////////////////////////////////////////////////////////////
//
// An <Import Project="..\Common.props"/>: the sheet's groups, read here.
// Its path is from the file that imports it, or from the project with
// $(ProjectDir) or $(MSBuildProjectDirectory), or from the importing file
// with $(MSBuildThisFileDirectory).  One with any other macro (Microsoft's
// own, $(VCTargetsPath) and $(UserRootDir); $(SolutionDir), which a project
// read by itself hasn't) or that isn't there is skipped.  The import's and
// its group's conditions, if they are on the configuration, are the only
// configurations the sheet's groups are for.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadImport(CMarkupTag* pcImport, const char* szDirectory, CArrayChars* paszPlatforms, int iDepth)
{
	CChars		szFileName;
	CChars		szFullPath;
	CChars		szSheetDirectory;
	CChars		szName;
	CXMLFile*	pcFile;
	const char*	szOuterFile;
	bool		bScoped;

	szFileName.Init();
	if ((iDepth < VCX_MAX_IMPORT_DEPTH) && ResolveImport(pcImport->GetStringAttribute("Project"), szDirectory, &szFileName))
	{
		pcFile = GetSheetFile(szFileName.Text());
		if ((pcFile != NULL) && pcFile->mcMarkup.mpcDoc->GetRootTag()->Is("Project"))
		{
			szFullPath.Init();
			szSheetDirectory.Init();
			szName.Init();
			SplitVcxFileName(szFileName.Text(), &szFullPath, &szSheetDirectory, &szName);
			bScoped = PushScope(pcImport);
			szOuterFile = mszReadingFile;
			mszReadingFile = szFileName.Text();
			ReadGroups(pcFile->mcMarkup.mpcDoc->GetRootTag(), szSheetDirectory.Text(), paszPlatforms, iDepth + 1);
			mszReadingFile = szOuterFile;
			PopScope(bScoped);
			szName.Kill();
			szSheetDirectory.Kill();
			szFullPath.Kill();
		}
	}
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The file an import names, appended to pszDest.  False if it has a macro
// that isn't one of the three read, or there is no such file.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::ResolveImport(const char* szProject, const char* szDirectory, CChars* pszDest)
{
	static const char*	aszProjectMacros[] = { "$(ProjectDir)", "$(MSBuildProjectDirectory)\\", "$(MSBuildProjectDirectory)/", NULL };
	static const char*	szThisMacro = VCX_THIS_FILE_DIRECTORY;
	const char*			szFrom;
	int					i;

	if (szProject == NULL)
	{
		return false;
	}
	szFrom = szDirectory;
	if (strncmp(szProject, szThisMacro, strlen(szThisMacro)) == 0)
	{
		szProject += strlen(szThisMacro);
	}
	else
	{
		for (i = 0; aszProjectMacros[i] != NULL; i++)
		{
			if (strncmp(szProject, aszProjectMacros[i], strlen(aszProjectMacros[i])) == 0)
			{
				szProject += strlen(aszProjectMacros[i]);
				szFrom = mszDirectory.Text();
				break;
			}
		}
	}
	return (strstr(szProject, "$(") == NULL) && ResolveVcxPath(szFrom, szProject, pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// A sheet's file, read the first time it is imported and kept until the
// project has been read (ReadProject reads each twice).  NULL if it can't
// be read.
//
//////////////////////////////////////////////////////////////////////////
CXMLFile* CVcxProject::GetSheetFile(const char* szFileName)
{
	size		uiIndex;
	CXMLFile*	pcFile;

	uiIndex = maszSheets.GetIndex(szFileName);
	if (uiIndex != ARRAY_ELEMENT_NOT_FOUND)
	{
		return mapcSheetFiles.GetPtr(uiIndex);
	}
	pcFile = (CXMLFile*)malloc(sizeof(CXMLFile));
	new(pcFile) CXMLFile();
	pcFile->Init();
	if (!ReadVcxXMLFile(pcFile, szFileName))
	{
		gcLogger.Error2(__METHOD__, " Could not read property sheet [", szFileName, "] of [", mszFileName.Text(), "].", NULL);
		pcFile->Kill();
		free(pcFile);
		return NULL;
	}
	maszSheets.Add(szFileName);
	mapcSheetFiles.Add(pcFile);
	return pcFile;
}


//////////////////////////////////////////////////////////////////////////
//
// A tag's condition becomes one more that every group read inside it must
// meet, if it is on the configuration.  True if it was (PopScope takes it
// off again).
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::PushScope(CMarkupTag* pcTag)
{
	char*			szCondition;
	CChars			szValue;
	EVcxCondition	eCondition;

	szCondition = pcTag->GetStringAttribute("Condition");
	if (szCondition == NULL)
	{
		return false;
	}
	szValue.Init();
	eCondition = ParseVcxCondition(szCondition, &szValue);
	szValue.Kill();
	if (eCondition == VCC_Other)
	{
		return false;
	}
	maszScope.Add(szCondition);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::PopScope(bool bScoped)
{
	if (bScoped)
	{
		maszScope.RemoveTail();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The toolchains' configurations that the conditions of pcTag and of what
// is in it are for: one on a whole configuration (Release|GCC-x64) adds it;
// one on a toolchain's platform alone adds the platform to paszPlatforms,
// for AddToolchainConfigurations.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::FindToolchainConditions(CMarkupTag* pcTag, CArrayChars* paszPlatforms)
{
	CMarkupTag*		pcChild;
	STagIterator	sIter;
	char*			szCondition;
	CChars			szValue;
	EVcxCondition	eCondition;
	const char*		szBar;

	szCondition = pcTag->GetStringAttribute("Condition");
	if (szCondition != NULL)
	{
		szValue.Init();
		eCondition = ParseVcxCondition(szCondition, &szValue);
		szBar = strchr(szValue.Text(), '|');
		if ((eCondition == VCC_Both) && szBar && GetVcxToolchainBasePlatform(szBar + 1))
		{
			AddConfiguration(szValue.Text());
		}
		else if ((eCondition == VCC_Platform) && GetVcxToolchainBasePlatform(szValue.Text()) && !paszPlatforms->Contains(szValue.Text()))
		{
			paszPlatforms->Add(szValue.Text());
		}
		szValue.Kill();
	}
	for (pcChild = pcTag->GetTag(&sIter); pcChild; pcChild = pcTag->GetNextTag(&sIter))
	{
		FindToolchainConditions(pcChild, paszPlatforms);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A toolchain's platform a condition names by itself has a configuration
// for each configuration name the project lists: Debug|GCC-x64 and
// Release|GCC-x64 of a project with Debug and Release.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::AddToolchainConfigurations(CArrayChars* paszPlatforms)
{
	CArrayChars	aszNames;
	CChars		szName;
	size		i;
	size		j;

	aszNames.Init();
	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		if (!masConfigurations.Get(i)->IsToolchain() && !aszNames.Contains(masConfigurations.Get(i)->GetConfiguration()))
		{
			aszNames.Add(masConfigurations.Get(i)->GetConfiguration());
		}
	}
	for (i = 0; i < paszPlatforms->NumElements(); i++)
	{
		for (j = 0; j < aszNames.NumElements(); j++)
		{
			szName.Init(aszNames.GetText(j));
			szName.Append('|');
			szName.Append(paszPlatforms->GetText(i));
			AddConfiguration(szName.Text());
			szName.Kill();
		}
	}
	aszNames.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Each toolchain's configuration gets its base: the listed configuration
// of its name on its base platform (Debug|x64 for Debug|GCC-x64), or the
// first listed of its name if there is none.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::SetToolchainBases(void)
{
	CVcxConfiguration*	pcConfiguration;
	CVcxConfiguration*	pcOther;
	CVcxConfiguration*	pcBase;
	const char*			szBasePlatform;
	size				i;
	size				j;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		pcConfiguration = masConfigurations.Get(i);
		szBasePlatform = GetVcxToolchainBasePlatform(pcConfiguration->GetPlatform());
		if (szBasePlatform == NULL)
		{
			continue;
		}
		pcBase = NULL;
		for (j = 0; j < masConfigurations.NumElements(); j++)
		{
			pcOther = masConfigurations.Get(j);
			if (!pcOther->IsToolchain() && (strcmp(pcOther->GetConfiguration(), pcConfiguration->GetConfiguration()) == 0))
			{
				if (strcmp(pcOther->GetPlatform(), szBasePlatform) == 0)
				{
					pcBase = pcOther;
					break;
				}
				if (pcBase == NULL)
				{
					pcBase = pcOther;
				}
			}
		}
		pcConfiguration->SetBase(pcBase);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Project references and items.  An item is any element with an Include,
// its kind its element's name (ClCompile, ClInclude, None...).
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadItemGroup(CMarkupTag* pcItemGroup)
{
	CMarkupTag*				pcTag;
	STagIterator			sIter;
	char*					szInclude;
	CVcxProjectReference*	pcReference;
	CVcxItem*				pcItem;
	CChars					szProject;

	pcTag = pcItemGroup->GetTag(&sIter);
	while (pcTag != NULL)
	{
		szInclude = pcTag->GetStringAttribute("Include");
		if (szInclude != NULL)
		{
			if (pcTag->Is("ProjectConfiguration"))
			{
				//Read first of all (ReadProjectConfigurations).
			}
			else if (pcTag->Is("ProjectReference"))
			{
				pcReference = masReferences.Add();
				pcReference->Init(szInclude);
				szProject.Init();
				if (GetVcxTagText(pcTag, "Project", &szProject))
				{
					pcReference->SetProject(szProject.Text());
				}
				szProject.Kill();
			}
			else
			{
				pcItem = masItems.Add();
				pcItem->Init(pcTag->GetName(), szInclude);
				ReadItemExclusions(pcTag, pcItem);
			}
		}
		pcTag = pcItemGroup->GetNextTag(&sIter);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// An item's <ExcludedFromBuild Condition="...">true</ExcludedFromBuild>
// elements: each for the configurations its condition is for, or for
// every one with none.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadItemExclusions(CMarkupTag* pcItemTag, CVcxItem* pcItem)
{
	CMarkupTag*		pcTag;
	STagIterator	sIter;
	char*			szCondition;
	CChars			szValue;
	bool			bExcluded;
	size			i;

	for (pcTag = pcItemTag->GetTag("ExcludedFromBuild", &sIter); pcTag; pcTag = pcItemTag->GetNextTag(&sIter))
	{
		szValue.Init();
		pcTag->GetText(&szValue);
		szValue.StripWhitespace(true);
		bExcluded = szValue.EqualsIgnoreCase("true");
		szValue.Kill();
		szCondition = pcTag->GetStringAttribute("Condition");
		if (szCondition == NULL)
		{
			pcItem->SetExcludedFromBuild("", bExcluded);
			continue;
		}
		for (i = 0; i < masConfigurations.NumElements(); i++)
		{
			if (IsConditionFor(szCondition, masConfigurations.Get(i)))
			{
				pcItem->SetExcludedFromBuild(masConfigurations.Get(i)->GetName(), bExcluded);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a build of pcConfiguration compiles the item: a ClCompile that
// isn't excluded from it, by its own ExcludedFromBuild or, if it has none
// for the configuration, by the configuration's item definition
// (<ItemDefinitionGroup><ClCompile><ExcludedFromBuild>, which leaves out
// every source of a project).
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::IsCompiled(CVcxItem* pcItem, CVcxConfiguration* pcConfiguration)
{
	bool	bExcluded;
	char*	szDefault;

	if (pcItem->GetType() != VIT_ClCompile)
	{
		return false;
	}
	if (pcConfiguration == NULL)
	{
		return true;
	}
	if (!pcItem->GetExcludedFromBuild(pcConfiguration->GetName(), &bExcluded))
	{
		szDefault = pcConfiguration->GetProperty("ClCompile.ExcludedFromBuild");
		bExcluded = szDefault && (StringInsensitiveCompare(szDefault, "true") == 0);
	}
	return !bExcluded;
}


//////////////////////////////////////////////////////////////////////////
//
// The globals (the project's GUID and root namespace) and properties.  A
// property's own Condition, or else its group's, says the configuration
// it is for; a property with neither is for them all.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadPropertyGroup(CMarkupTag* pcPropertyGroup)
{
	CMarkupTag*		pcTag;
	STagIterator	sIter;
	char*			szLabel;
	char*			szGroupCondition;
	char*			szCondition;
	CChars			szValue;
	bool			bGlobals;

	szLabel = pcPropertyGroup->GetStringAttribute("Label");
	bGlobals = (szLabel != NULL) && (strcmp(szLabel, "Globals") == 0);
	szGroupCondition = pcPropertyGroup->GetStringAttribute("Condition");
	pcTag = pcPropertyGroup->GetTag(&sIter);
	while (pcTag != NULL)
	{
		szValue.Init();
		pcTag->GetText(&szValue);
		if (bGlobals)
		{
			if (pcTag->Is("ProjectGuid"))
			{
				mszGuid.Set(szValue.Text());
			}
			else if (pcTag->Is("RootNamespace"))
			{
				mszRootNamespace.Set(szValue.Text());
			}
		}
		szCondition = pcTag->GetStringAttribute("Condition");
		SetProperty((szCondition != NULL) ? szCondition : szGroupCondition, pcTag->GetName(), szValue.Text());
		szValue.Kill();
		pcTag = pcPropertyGroup->GetNextTag(&sIter);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The settings of each tool (<ClCompile>, <Link>...) for a configuration,
// as Tool.Setting properties: ClCompile.PreprocessorDefinitions.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadItemDefinitionGroup(CMarkupTag* pcItemDefinitionGroup)
{
	CMarkupTag*		pcTool;
	CMarkupTag*		pcSetting;
	STagIterator	sToolIter;
	STagIterator	sSettingIter;
	char*			szCondition;
	CChars			szName;
	CChars			szValue;

	szCondition = pcItemDefinitionGroup->GetStringAttribute("Condition");
	pcTool = pcItemDefinitionGroup->GetTag(&sToolIter);
	while (pcTool != NULL)
	{
		pcSetting = pcTool->GetTag(&sSettingIter);
		while (pcSetting != NULL)
		{
			szName.Init(pcTool->GetName());
			szName.Append('.');
			szName.Append(pcSetting->GetName());
			szValue.Init();
			pcSetting->GetText(&szValue);
			SetProperty(szCondition, szName.Text(), szValue.Text());
			szValue.Kill();
			szName.Kill();
			pcSetting = pcTool->GetNextTag(&sSettingIter);
		}
		pcTool = pcItemDefinitionGroup->GetNextTag(&sToolIter);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a condition is true of a configuration.  No condition is true of
// every one.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::IsConditionFor(const char* szCondition, CVcxConfiguration* pcConfiguration)
{
	CChars			szValue;
	EVcxCondition	eCondition;
	bool			bResult;

	if (szCondition == NULL)
	{
		return true;
	}
	szValue.Init();
	eCondition = ParseVcxCondition(szCondition, &szValue);
	bResult = ((eCondition == VCC_Both) && szValue.Equals(pcConfiguration->GetName())) ||
			  ((eCondition == VCC_Configuration) && szValue.Equals(pcConfiguration->GetConfiguration())) ||
			  ((eCondition == VCC_NotConfiguration) && !szValue.Equals(pcConfiguration->GetConfiguration())) ||
			  ((eCondition == VCC_Platform) && szValue.Equals(pcConfiguration->GetPlatform()));
	szValue.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Sets a property on the configurations szCondition is for that are also
// those of the imports being read (maszScope).  A value that names itself,
// %(PreprocessorDefinitions) in an item definition's or $(IncludePath) in
// a property's, has what the configuration was given before in its place,
// so a project adds to its sheet's; one that doesn't replaces it.
// $(MSBuildThisFileDirectory) is the directory of the file being read, so
// it is put in now: a sheet names its own folder with it.  Where each was
// set is kept with the configuration (its sources), with the value as the
// file writes it.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::SetProperty(const char* szCondition, const char* szName, const char* szValue)
{
	CVcxConfiguration*	pcConfiguration;
	CChars				szSelf;
	CChars				szNew;
	CChars				szHere;
	CChars				szRead;
	const char*			szDot;
	char*				szOld;
	size				i;
	size				j;
	bool				bInScope;

	szRead.Init(szValue);
	szHere.Init(szValue);
	if (mszReadingDirectory && szHere.Contains(VCX_THIS_FILE_DIRECTORY))
	{
		szNew.Init(mszReadingDirectory);
		szNew.Append(FILE_SEPARATOR[0]);
		szHere.Replace(VCX_THIS_FILE_DIRECTORY, szNew.Text());
		szNew.Kill();
	}
	szValue = szHere.Text();
	szDot = strchr(szName, '.');
	szSelf.Init(szDot ? "%(" : "$(");
	szSelf.Append(szDot ? szDot + 1 : szName);
	szSelf.Append(')');
	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		pcConfiguration = masConfigurations.Get(i);
		bInScope = IsConditionFor(szCondition, pcConfiguration);
		for (j = 0; bInScope && (j < maszScope.NumElements()); j++)
		{
			bInScope = IsConditionFor(maszScope.GetText(j), pcConfiguration);
		}
		if (bInScope)
		{
			pcConfiguration->AddSource(szName, mszReadingFile ? mszReadingFile : mszFileName.Text(), szCondition, szRead.Text());
			szOld = pcConfiguration->GetOwnProperty(szName);
			if ((szOld != NULL) && (strstr(szValue, szSelf.Text()) != NULL))
			{
				szNew.Init(szValue);
				szNew.Replace(szSelf.Text(), szOld);
				pcConfiguration->SetProperty(szName, szNew.Text());
				szNew.Kill();
			}
			else
			{
				pcConfiguration->SetProperty(szName, szValue);
			}
		}
	}
	szRead.Kill();
	szSelf.Kill();
	szHere.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CVcxConfiguration* CVcxProject::AddConfiguration(const char* szName)
{
	CVcxConfiguration*	pcConfiguration;

	pcConfiguration = FindConfiguration(szName);
	if (pcConfiguration == NULL)
	{
		pcConfiguration = masConfigurations.Add();
		pcConfiguration->Init(szName);
	}
	return pcConfiguration;
}


//////////////////////////////////////////////////////////////////////////
//
// The <Project> of a .vcxproj.filters: the filters, and the filter each
// item is in.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::ReadFilters(CMarkupTag* pcProject)
{
	CMarkupTag*		pcTag;
	STagIterator	sIter;

	if (!pcProject->Is("Project"))
	{
		return gcLogger.Error2(__METHOD__, " Expected a <Project> in the filters of [", mszFileName.Text(), "], not <", pcProject->GetName(), ">.", NULL);
	}

	pcTag = pcProject->GetTag(&sIter);
	while (pcTag != NULL)
	{
		if (pcTag->Is("ItemGroup"))
		{
			ReadFilterItemGroup(pcTag);
		}
		pcTag = pcProject->GetNextTag(&sIter);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Filters, and the items of the project with the filter each is in.  An
// item the project doesn't list is ignored, as Visual Studio does.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadFilterItemGroup(CMarkupTag* pcItemGroup)
{
	CMarkupTag*		pcTag;
	STagIterator	sIter;
	char*			szInclude;
	CVcxFilter*		pcFilter;
	CVcxItem*		pcItem;
	CChars			szText;

	pcTag = pcItemGroup->GetTag(&sIter);
	while (pcTag != NULL)
	{
		szInclude = pcTag->GetStringAttribute("Include");
		if (szInclude != NULL)
		{
			szText.Init();
			if (pcTag->Is("Filter"))
			{
				pcFilter = FindFilter(szInclude);
				if (pcFilter == NULL)
				{
					pcFilter = masFilters.Add();
					pcFilter->Init(szInclude);
				}
				if (GetVcxTagText(pcTag, "UniqueIdentifier", &szText))
				{
					pcFilter->SetUniqueIdentifier(szText.Text());
				}
				szText.Clear();
				if (GetVcxTagText(pcTag, "Extensions", &szText))
				{
					pcFilter->SetExtensions(szText.Text());
				}
			}
			else
			{
				pcItem = FindItem(pcTag->GetName(), szInclude);
				if ((pcItem != NULL) && GetVcxTagText(pcTag, "Filter", &szText))
				{
					pcItem->SetFilter(szText.Text());
				}
			}
			szText.Kill();
		}
		pcTag = pcItemGroup->GetNextTag(&sIter);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CVcxConfiguration* CVcxProject::FindConfiguration(const char* szName)
{
	size	i;

	for (i = 0; i < masConfigurations.NumElements(); i++)
	{
		if (strcmp(masConfigurations.Get(i)->GetName(), szName) == 0)
		{
			return masConfigurations.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The item of kind szElement (ClCompile) at szInclude, whatever the case
// of the path.
//
//////////////////////////////////////////////////////////////////////////
CVcxItem* CVcxProject::FindItem(const char* szElement, const char* szInclude)
{
	size	i;

	for (i = 0; i < masItems.NumElements(); i++)
	{
		if (masItems.Get(i)->Is(szElement, szInclude))
		{
			return masItems.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Filter names aren't case sensitive.
//
//////////////////////////////////////////////////////////////////////////
CVcxFilter* CVcxProject::FindFilter(const char* szName)
{
	size	i;

	for (i = 0; i < masFilters.NumElements(); i++)
	{
		if (StringInsensitiveCompare(masFilters.Get(i)->GetName(), szName) == 0)
		{
			return masFilters.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The indices of the items in filter szFilter, in the order the project
// lists them.  "" is the items in no filter.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetItemsInFilter(const char* szFilter, CArrayInt* paiItems)
{
	size	i;

	for (i = 0; i < masItems.NumElements(); i++)
	{
		if (StringInsensitiveCompare(masItems.Get(i)->GetFilter(), szFilter) == 0)
		{
			paiItems->Add((int)i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The indices of the filters directly in filter szParent, in the order
// the filters file lists them.  "" is the filters at the top.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetChildFilters(const char* szParent, CArrayInt* paiFilters)
{
	size		i;
	CChars		szFilterParent;
	CVcxFilter*	pcFilter;
	bool		bTop;

	for (i = 0; i < masFilters.NumElements(); i++)
	{
		pcFilter = masFilters.Get(i);
		szFilterParent.Init();
		bTop = !pcFilter->GetParentName(&szFilterParent);
		if (szParent[0] == '\0')
		{
			if (bTop)
			{
				paiFilters->Add((int)i);
			}
		}
		else if (!bTop && (StringInsensitiveCompare(szFilterParent.Text(), szParent) == 0))
		{
			paiFilters->Add((int)i);
		}
		szFilterParent.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The item's file: the project's directory and the item's path, with
// this platform's separators and the case of the file on disk.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetFullPath(CVcxItem* pcItem, CChars* pszDest)
{
	ResolveVcxPath(mszDirectory.Text(), pcItem->GetInclude(), pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The item whose file is szOldPath (a full path, any case) renamed
// szNewName (a file name, without directories: the file stays where it
// is), in the .vcxproj and its .vcxproj.filters, which are written, then
// the project read again.  Only the Include values that name the item
// change; the rest of each file stays as it was, byte for byte.  False if
// the project has no such item, szNewName isn't a file name or a file
// can't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::RenameItem(const char* szOldPath, const char* szNewName)
{
	CArrayChars	aszIncludes;
	CChars		szPath;
	CChars		szFilters;
	CChars		szFileName;
	size		i;
	bool		bResult;

	if ((szNewName == NULL) || (szNewName[0] == '\0') || strchr(szNewName, '\\') || strchr(szNewName, '/'))
	{
		return false;
	}

	//The Include values, as the project writes them, of the items that are the file.
	aszIncludes.Init();
	for (i = 0; i < masItems.NumElements(); i++)
	{
		szPath.Init();
		GetFullPath(masItems.Get(i), &szPath);
		if (StringInsensitiveCompare(szPath.Text(), szOldPath) == 0)
		{
			aszIncludes.Add(masItems.Get(i)->GetInclude());
		}
		szPath.Kill();
	}
	if (aszIncludes.NumElements() == 0)
	{
		aszIncludes.Kill();
		return false;
	}

	szFileName.Init(mszFileName);
	szFilters.Init(mszFileName);
	szFilters.Append(".filters");
	bResult = RenameIncludes(szFileName.Text(), &aszIncludes, szNewName);
	if (bResult && mbFilters)
	{
		bResult = RenameIncludes(szFilters.Text(), &aszIncludes, szNewName);
	}
	szFilters.Kill();
	aszIncludes.Kill();

	//Read again, so the items have their new names.
	Kill();
	Init();
	if (!Read(szFileName.Text()))
	{
		bResult = false;
	}
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A new item for a file szNewName in the same directory as the file at
// szExistingPath, which is an item: the same kind of item (ClInclude,
// ClCompile), written after it in the .vcxproj and, in its .filters, in
// the same filter.  A file made from another's (a class moved to files of
// its own) goes where that one is.  The project is read again.  False if
// the project has no item for szExistingPath, has one for the new file
// already, or szNewName isn't a file name alone.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::AddItemBeside(const char* szExistingPath, const char* szNewName)
{
	CArrayChars	aszIncludes;
	CChars		szPath;
	CChars		szNewPath;
	CChars		szFilters;
	CChars		szFileName;
	size		uiName;
	size		i;
	bool		bResult;

	if ((szNewName == NULL) || (szNewName[0] == '\0') || strchr(szNewName, '\\') || strchr(szNewName, '/'))
	{
		return false;
	}

	//The new file's path, and the Include values of the items that are the existing file.
	szNewPath.Init(szExistingPath);
	for (uiName = szNewPath.Length(); (uiName > 0) && (szNewPath.Text()[uiName - 1] != '\\') && (szNewPath.Text()[uiName - 1] != '/'); uiName--)
	{
	}
	szNewPath.SetLength(uiName);
	szNewPath.Append(szNewName);
	aszIncludes.Init();
	bResult = true;
	for (i = 0; i < masItems.NumElements(); i++)
	{
		szPath.Init();
		GetFullPath(masItems.Get(i), &szPath);
		if (StringInsensitiveCompare(szPath.Text(), szExistingPath) == 0)
		{
			aszIncludes.Add(masItems.Get(i)->GetInclude());
		}
		else if (StringInsensitiveCompare(szPath.Text(), szNewPath.Text()) == 0)
		{
			bResult = false;
		}
		szPath.Kill();
	}
	szNewPath.Kill();
	if (!bResult || (aszIncludes.NumElements() == 0))
	{
		aszIncludes.Kill();
		return false;
	}

	szFileName.Init(mszFileName);
	szFilters.Init(mszFileName);
	szFilters.Append(".filters");
	bResult = AddIncludesBeside(szFileName.Text(), &aszIncludes, szNewName, false);
	if (bResult && mbFilters)
	{
		bResult = AddIncludesBeside(szFilters.Text(), &aszIncludes, szNewName, true);
	}
	szFilters.Kill();
	aszIncludes.Kill();

	Kill();
	Init();
	if (!Read(szFileName.Text()))
	{
		bResult = false;
	}
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The item whose file is szPath (a full path, any case) taken out of the
// .vcxproj and its .vcxproj.filters, which are written, then the project
// read again: what AddItemBeside added, taken away.  Only the item's own
// elements go, with their lines; the rest of each file stays as it was,
// byte for byte.  False if the project has no such item or a file can't
// be written.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::RemoveItem(const char* szPath)
{
	CArrayChars	aszIncludes;
	CChars		szItemPath;
	CChars		szFilters;
	CChars		szFileName;
	size		i;
	bool		bResult;

	aszIncludes.Init();
	for (i = 0; i < masItems.NumElements(); i++)
	{
		szItemPath.Init();
		GetFullPath(masItems.Get(i), &szItemPath);
		if (StringInsensitiveCompare(szItemPath.Text(), szPath) == 0)
		{
			aszIncludes.Add(masItems.Get(i)->GetInclude());
		}
		szItemPath.Kill();
	}
	if (aszIncludes.NumElements() == 0)
	{
		aszIncludes.Kill();
		return false;
	}

	szFileName.Init(mszFileName);
	szFilters.Init(mszFileName);
	szFilters.Append(".filters");
	bResult = RemoveIncludes(szFileName.Text(), &aszIncludes);
	if (bResult && mbFilters)
	{
		RemoveIncludes(szFilters.Text(), &aszIncludes);
	}
	szFilters.Kill();
	aszIncludes.Kill();

	Kill();
	Init();
	if (!Read(szFileName.Text()))
	{
		bResult = false;
	}
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Each element of szFileName's text with an Include="..." whose value is
// one of paszIncludes (any case) taken out: from its line's start to the
// end of the line its "/>" or its closing tag is on.  The file is
// written.  False if it can't be read or written, or no element has such
// a value.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::RemoveIncludes(const char* szFileName, CArrayChars* paszIncludes)
{
	CTextFile	cFile;
	CChars		szText;
	CChars		szValue;
	CChars		szClose;
	char*		szAttribute;
	char*		szEnd;
	char*		szClosing;
	size		uiStart;
	size		uiEnd;
	size		uiElement;
	size		uiLineStart;
	size		uiElementEnd;
	size		i;
	bool		bFound;
	bool		bRemoved;
	bool		bResult;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	szText.Init();
	szText.AppendSubString(cFile.Text(), (size)cFile.Length());
	cFile.Kill();

	bRemoved = false;
	uiStart = 0;
	for (;;)
	{
		szAttribute = strstr(&szText.Text()[uiStart], "Include=\"");
		if (szAttribute == NULL)
		{
			break;
		}
		uiStart = (size)(szAttribute - szText.Text()) + 9;
		szEnd = strchr(&szText.Text()[uiStart], '"');
		if (szEnd == NULL)
		{
			break;
		}
		uiEnd = (size)(szEnd - szText.Text());

		szValue.Init();
		szValue.AppendSubString(szText.Text(), uiStart, uiEnd);
		bFound = false;
		for (i = 0; !bFound && (i < paszIncludes->NumElements()); i++)
		{
			bFound = StringInsensitiveCompare(szValue.Text(), paszIncludes->GetText(i)) == 0;
		}
		szValue.Kill();
		if (!bFound)
		{
			uiStart = uiEnd + 1;
			continue;
		}

		//The element: from its line's start to the end of the line its "/>" or its closing tag is on.
		for (uiElement = uiStart; (uiElement > 0) && (szText.Text()[uiElement] != '<'); uiElement--)
		{
		}
		for (uiLineStart = uiElement; (uiLineStart > 0) && (szText.Text()[uiLineStart - 1] != '\n'); uiLineStart--)
		{
		}
		for (uiElementEnd = uiEnd; (szText.Text()[uiElementEnd] != '\0') && (szText.Text()[uiElementEnd] != '>'); uiElementEnd++)
		{
		}
		if ((uiElementEnd > 0) && (szText.Text()[uiElementEnd - 1] != '/'))
		{
			szClose.Init("</");
			for (i = uiElement + 1; (szText.Text()[i] != ' ') && (szText.Text()[i] != '>') && (szText.Text()[i] != '\0'); i++)
			{
				szClose.Append(szText.Text()[i]);
			}
			szClose.Append('>');
			szClosing = strstr(&szText.Text()[uiElementEnd], szClose.Text());
			uiElementEnd = szClosing ? (size)(szClosing - szText.Text()) + szClose.Length() - 1 : uiElementEnd;
			szClose.Kill();
		}
		for (; (szText.Text()[uiElementEnd] != '\0') && (szText.Text()[uiElementEnd] != '\n'); uiElementEnd++)
		{
		}
		if (szText.Text()[uiElementEnd] == '\n')
		{
			uiElementEnd++;
		}
		szText.Remove(uiLineStart, uiElementEnd);
		uiStart = uiLineStart;
		bRemoved = true;
	}

	bResult = bRemoved;
	if (bRemoved)
	{
		cFile.Init();
		cFile.mcText.AppendSubString(szText.Text(), (lsize)szText.Length());
		bResult = cFile.Write(szFileName);
		cFile.Kill();
	}
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The kind of item a file is: a ClInclude for a header, a ClCompile for a
// C or C++ source file, None for anything else.
//
//////////////////////////////////////////////////////////////////////////
const char* CVcxProject::GetItemElement(const char* szFileName)
{
	const char*	szDot;

	szDot = strrchr(szFileName, '.');
	if (szDot && ((StringInsensitiveCompare(szDot, ".h") == 0) || (StringInsensitiveCompare(szDot, ".hpp") == 0) || (StringInsensitiveCompare(szDot, ".inl") == 0)))
	{
		return "ClInclude";
	}
	if (szDot && ((StringInsensitiveCompare(szDot, ".cpp") == 0) || (StringInsensitiveCompare(szDot, ".c") == 0) || (StringInsensitiveCompare(szDot, ".cc") == 0)))
	{
		return "ClCompile";
	}
	return "None";
}


//////////////////////////////////////////////////////////////////////////
//
// A new item for the file at szPath, which is in the project's directory
// or under it: its kind (GetItemElement) after the last item of that kind
// (in an ItemGroup of its own if there is none) and, in its .filters, in
// szFilter (NULL or "" for none).  For a file with no other to go beside
// (AddItemBeside): New's.  The project is read again.  False if the
// project has an item for it already, or it isn't under the project's
// directory.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::AddItem(const char* szPath, const char* szFilter)
{
	CChars		szInclude;
	CChars		szItemPath;
	CChars		szFileName;
	CChars		szFilters;
	CChars		szDirectory;
	CChars		szStart;
	const char*	szElement;
	size		uiDirectory;
	size		i;
	bool		bResult;

	//Under the project's directory, either separator.
	uiDirectory = mszDirectory.Length();
	szDirectory.Init(mszDirectory);
	szDirectory.Replace('/', '\\');
	szStart.Init();
	if (strlen(szPath) > uiDirectory)
	{
		szStart.Append(szPath, uiDirectory);
		szStart.Replace('/', '\\');
	}
	bResult = (uiDirectory > 0) && !szStart.Empty() && (StringInsensitiveCompare(szStart.Text(), szDirectory.Text()) == 0);
	szStart.Kill();
	szDirectory.Kill();
	if (!bResult)
	{
		return false;
	}
	szInclude.Init(&szPath[uiDirectory]);
	while (szInclude.StartsWith("\\") || szInclude.StartsWith("/"))
	{
		szInclude.RemoveCharacter(0);
	}
	szInclude.Replace('/', '\\');
	bResult = !szInclude.Empty();
	for (i = 0; bResult && (i < masItems.NumElements()); i++)
	{
		szItemPath.Init();
		GetFullPath(masItems.Get(i), &szItemPath);
		bResult = (StringInsensitiveCompare(szItemPath.Text(), szPath) != 0);
		szItemPath.Kill();
	}
	if (!bResult)
	{
		szInclude.Kill();
		return false;
	}

	szElement = GetItemElement(szInclude.Text());
	szFileName.Init(mszFileName);
	szFilters.Init(mszFileName);
	szFilters.Append(".filters");
	bResult = AddElement(szFileName.Text(), szElement, szInclude.Text(), NULL);
	if (bResult && mbFilters)
	{
		bResult = AddElement(szFilters.Text(), szElement, szInclude.Text(), (szFilter && (szFilter[0] != '\0')) ? szFilter : NULL);
	}
	szFilters.Kill();
	szInclude.Kill();

	Kill();
	Init();
	if (!Read(szFileName.Text()))
	{
		bResult = false;
	}
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// An element <szElement Include="szInclude" /> in szFileName's text (a
// .vcxproj or its .filters), with a <Filter> in it if szFilter isn't
// NULL: after the last element of its kind, or in an ItemGroup of its own
// before the .vcxproj's Microsoft.Cpp.targets import (or the end of the
// project).  The file is written.  False if it can't be read or written.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::AddElement(const char* szFileName, const char* szElement, const char* szInclude, const char* szFilter)
{
	CTextFile	cFile;
	CChars		szText;
	CChars		szOpen;
	CChars		szClose;
	CChars		szLine;
	const char*	szNewLine;
	const char*	szFound;
	const char*	szLast;
	const char*	szClosing;
	size		uiAt;
	bool		bResult;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	szText.Init();
	szText.AppendSubString(cFile.Text(), (size)cFile.Length());
	cFile.Kill();
	szNewLine = strstr(szText.Text(), "\r\n") ? "\r\n" : "\n";

	szLine.Init("    <");
	szLine.Append(szElement)->Append(" Include=\"")->Append(szInclude)->Append('"');
	if (szFilter)
	{
		szLine.Append('>')->Append(szNewLine)->Append("      <Filter>")->Append(szFilter)->Append("</Filter>")->Append(szNewLine);
		szLine.Append("    </")->Append(szElement)->Append('>')->Append(szNewLine);
	}
	else
	{
		szLine.Append(" />")->Append(szNewLine);
	}

	//After the last of its kind: the end of the line its element ends on.
	szOpen.Init("<");
	szOpen.Append(szElement)->Append(" Include=\"");
	szClose.Init("</");
	szClose.Append(szElement)->Append('>');
	szLast = NULL;
	for (szFound = strstr(szText.Text(), szOpen.Text()); szFound; szFound = strstr(szFound + 1, szOpen.Text()))
	{
		szLast = szFound;
	}
	if (szLast)
	{
		for (szFound = szLast; (*szFound != '\0') && (*szFound != '>'); szFound++)
		{
		}
		if ((szFound > szLast) && (szFound[-1] != '/'))
		{
			szClosing = strstr(szFound, szClose.Text());
			szFound = szClosing ? szClosing + szClose.Length() - 1 : szFound;
		}
		for (; (*szFound != '\0') && (*szFound != '\n'); szFound++)
		{
		}
		uiAt = (size)(szFound - szText.Text()) + ((*szFound == '\n') ? 1 : 0);
	}
	else
	{
		//In an ItemGroup of its own, on the line before the targets' import or the project's end.
		szFound = strstr(szText.Text(), "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\"");
		szFound = szFound ? szFound : strstr(szText.Text(), "</Project>");
		uiAt = szFound ? (size)(szFound - szText.Text()) : szText.Length();
		while ((uiAt > 0) && (szText.Text()[uiAt - 1] != '\n'))
		{
			uiAt--;
		}
		szLine.Insert(0, szNewLine);
		szLine.Insert(0, "  <ItemGroup>");
		szLine.Append("  </ItemGroup>")->Append(szNewLine);
	}
	szText.Insert(uiAt, szLine.Text());
	szClose.Kill();
	szOpen.Kill();
	szLine.Kill();

	cFile.Init();
	cFile.mcText.AppendSubString(szText.Text(), (lsize)szText.Length());
	bResult = cFile.Write(szFileName);
	cFile.Kill();
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// After the first element of szFileName's text with an Include="..."
// whose value is one of paszIncludes (any case), a copy of that element
// with szNewName in place of the value's file name: all of it (bWhole: a
// .filters' element, with the Filter in it), or it alone, closed, without
// what is in it (what one file is built with isn't another's).  The file
// is written.  False if it can't be read or written, or no element has
// such a value.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::AddIncludesBeside(const char* szFileName, CArrayChars* paszIncludes, const char* szNewName, bool bWhole)
{
	CTextFile	cFile;
	CChars		szText;
	CChars		szValue;
	CChars		szCopy;
	CChars		szClose;
	char*		szAttribute;
	char*		szEnd;
	char*		szClosing;
	size		uiStart;
	size		uiEnd;
	size		uiName;
	size		uiElement;
	size		uiLineStart;
	size		uiElementEnd;
	size		i;
	bool		bFound;
	bool		bResult;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	szText.Init();
	szText.AppendSubString(cFile.Text(), (size)cFile.Length());
	cFile.Kill();

	bFound = false;
	uiStart = 0;
	while (!bFound)
	{
		szAttribute = strstr(&szText.Text()[uiStart], "Include=\"");
		if (szAttribute == NULL)
		{
			break;
		}
		uiStart = (size)(szAttribute - szText.Text()) + 9;
		szEnd = strchr(&szText.Text()[uiStart], '"');
		if (szEnd == NULL)
		{
			break;
		}
		uiEnd = (size)(szEnd - szText.Text());

		szValue.Init();
		szValue.AppendSubString(szText.Text(), uiStart, uiEnd);
		for (i = 0; !bFound && (i < paszIncludes->NumElements()); i++)
		{
			bFound = StringInsensitiveCompare(szValue.Text(), paszIncludes->GetText(i)) == 0;
		}
		szValue.Kill();
		if (!bFound)
		{
			uiStart = uiEnd + 1;
			continue;
		}

		//The element: from its line's start to the end of the line its "/>" or its closing tag is on.
		for (uiElement = uiStart; (uiElement > 0) && (szText.Text()[uiElement] != '<'); uiElement--)
		{
		}
		for (uiLineStart = uiElement; (uiLineStart > 0) && (szText.Text()[uiLineStart - 1] != '\n'); uiLineStart--)
		{
		}
		for (uiElementEnd = uiEnd; (szText.Text()[uiElementEnd] != '\0') && (szText.Text()[uiElementEnd] != '>'); uiElementEnd++)
		{
		}
		if ((uiElementEnd > 0) && (szText.Text()[uiElementEnd - 1] != '/'))
		{
			szClose.Init("</");
			for (i = uiElement + 1; (szText.Text()[i] != ' ') && (szText.Text()[i] != '>') && (szText.Text()[i] != '\0'); i++)
			{
				szClose.Append(szText.Text()[i]);
			}
			szClose.Append('>');
			szClosing = strstr(&szText.Text()[uiElementEnd], szClose.Text());
			uiElementEnd = szClosing ? (size)(szClosing - szText.Text()) + szClose.Length() - 1 : uiElementEnd;
			szClose.Kill();
		}
		for (; (szText.Text()[uiElementEnd] != '\0') && (szText.Text()[uiElementEnd] != '\n'); uiElementEnd++)
		{
		}
		if (szText.Text()[uiElementEnd] == '\n')
		{
			uiElementEnd++;
		}

		//Its copy, with the new name after the value's last separator.
		for (uiName = uiEnd; (uiName > uiStart) && (szText.Text()[uiName - 1] != '\\') && (szText.Text()[uiName - 1] != '/'); uiName--)
		{
		}
		szCopy.Init();
		szCopy.AppendSubString(szText.Text(), uiLineStart, uiName);
		szCopy.Append(szNewName);
		if (bWhole)
		{
			szCopy.AppendSubString(szText.Text(), uiEnd, uiElementEnd);
		}
		else
		{
			szCopy.Append("\" />");
			szCopy.Append(((uiElementEnd >= 2) && (szText.Text()[uiElementEnd - 2] == '\r')) ? "\r\n" : "\n");
		}
		szText.Insert(uiElementEnd, szCopy.Text());
		szCopy.Kill();
	}

	bResult = bFound;
	if (bFound)
	{
		cFile.Init();
		cFile.mcText.AppendSubString(szText.Text(), (lsize)szText.Length());
		bResult = cFile.Write(szFileName);
		cFile.Kill();
	}
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Each Include="..." in szFileName's text whose value is one of
// paszIncludes (any case) given szNewName in place of its file name, its
// directories kept as written, and the file written if anything changed.
// False if it can't be read or written.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::RenameIncludes(const char* szFileName, CArrayChars* paszIncludes, const char* szNewName)
{
	CTextFile	cFile;
	CChars		szText;
	CChars		szValue;
	char*		szAttribute;
	char*		szEnd;
	size		uiStart;
	size		uiEnd;
	size		uiName;
	size		i;
	bool		bChanged;
	bool		bResult;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	szText.Init();
	szText.AppendSubString(cFile.Text(), (size)cFile.Length());
	cFile.Kill();

	bChanged = false;
	uiStart = 0;
	for (;;)
	{
		szAttribute = strstr(&szText.Text()[uiStart], "Include=\"");
		if (szAttribute == NULL)
		{
			break;
		}
		uiStart = (size)(szAttribute - szText.Text()) + 9;
		szEnd = strchr(&szText.Text()[uiStart], '"');
		if (szEnd == NULL)
		{
			break;
		}
		uiEnd = (size)(szEnd - szText.Text());

		szValue.Init();
		szValue.AppendSubString(szText.Text(), uiStart, uiEnd);
		for (i = 0; i < paszIncludes->NumElements(); i++)
		{
			if (StringInsensitiveCompare(szValue.Text(), paszIncludes->GetText(i)) == 0)
			{
				//The file name is after the value's last separator.
				for (uiName = uiEnd; (uiName > uiStart) && (szText.Text()[uiName - 1] != '\\') && (szText.Text()[uiName - 1] != '/'); uiName--)
				{
				}
				szText.Remove(uiName, uiEnd);
				szText.Insert(uiName, szNewName);
				uiEnd = uiName + strlen(szNewName);
				bChanged = true;
				break;
			}
		}
		szValue.Kill();
		uiStart = uiEnd + 1;
	}

	bResult = true;
	if (bChanged)
	{
		cFile.Init();
		cFile.mcText.AppendSubString(szText.Text(), (lsize)szText.Length());
		bResult = cFile.Write(szFileName);
		cFile.Kill();
	}
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// szValue with its MSBuild macros replaced: $(ProjectDir), $(SolutionDir)
// (both ending in a separator, as MSBuild has them), $(ProjectName),
// $(Configuration), $(Platform) and any property the configuration sets.
// Anything else, like $(IncludePath) (the compiler's own directories) or
// %(PreprocessorDefinitions) (the item's inherited value), is empty.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ExpandMacros(const char* szValue, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CChars* pszDest)
{
	ExpandMacros(szValue, pcConfiguration, szSolutionDirectory, pszDest, 0);
}


//////////////////////////////////////////////////////////////////////////
//
// A property's value can use other macros ($(OutDir) is often
// $(SolutionDir)$(Configuration)\).  Past VCX_MAX_MACRO_DEPTH, as when a
// property uses itself, they are empty.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ExpandMacros(const char* szValue, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CChars* pszDest, int iDepth)
{
	const char*	szPosition;
	const char*	szEnd;
	CChars		szName;

	szPosition = szValue;
	while (*szPosition != '\0')
	{
		if (((szPosition[0] == '$') || (szPosition[0] == '%')) && (szPosition[1] == '('))
		{
			szEnd = strchr(szPosition + 2, ')');
			if (szEnd != NULL)
			{
				if (szPosition[0] == '$')
				{
					szName.Init();
					szName.AppendSubString(szPosition + 2, szEnd);
					GetMacro(szName.Text(), pcConfiguration, szSolutionDirectory, pszDest, iDepth);
					szName.Kill();
				}
				szPosition = szEnd + 1;
				continue;
			}
		}
		pszDest->Append(*szPosition);
		szPosition++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// $(IncludePath) is always empty: in IncludePath it is the compiler's
// own directories, which the IDE supplies.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::GetMacro(const char* szName, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CChars* pszDest, int iDepth)
{
	char*	szProperty;

	if (StringInsensitiveCompare(szName, "ProjectDir") == 0)
	{
		pszDest->Append(mszDirectory);
		pszDest->Append(FILE_SEPARATOR[0]);
	}
	else if (StringInsensitiveCompare(szName, "SolutionDir") == 0)
	{
		pszDest->Append(szSolutionDirectory);
		pszDest->Append(FILE_SEPARATOR[0]);
	}
	else if (StringInsensitiveCompare(szName, "ProjectName") == 0)
	{
		pszDest->Append(mszName);
	}
	else if ((StringInsensitiveCompare(szName, "Configuration") == 0) && (pcConfiguration != NULL))
	{
		pszDest->Append(pcConfiguration->GetConfiguration());
	}
	else if ((StringInsensitiveCompare(szName, "Platform") == 0) && (pcConfiguration != NULL))
	{
		pszDest->Append(pcConfiguration->GetPlatform());
	}
	else if ((StringInsensitiveCompare(szName, "IncludePath") != 0) && (pcConfiguration != NULL))
	{
		szProperty = pcConfiguration->GetProperty(szName);
		if ((szProperty == NULL) || (iDepth >= VCX_MAX_MACRO_DEPTH))
		{
			return false;
		}
		ExpandMacros(szProperty, pcConfiguration, szSolutionDirectory, pszDest, iDepth + 1);
	}
	else
	{
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The directories the compiler looks for includes in, in its order: the
// compiler's AdditionalIncludeDirectories, then the project's IncludePath
// (VC++ Directories).  Full paths with the case they have on disk, each
// once.  The compiler's own directories ($(IncludePath)) aren't included.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetIncludeDirectories(CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszDirectories)
{
	CArrayChars	aszParts;
	CChars		szDirectory;
	size		i;

	aszParts.Init();
	GetPropertyList("ClCompile.AdditionalIncludeDirectories", pcConfiguration, szSolutionDirectory, &aszParts);
	GetPropertyList("IncludePath", pcConfiguration, szSolutionDirectory, &aszParts);
	for (i = 0; i < aszParts.NumElements(); i++)
	{
		szDirectory.Init();
		ResolveVcxPath(mszDirectory.Text(), aszParts.GetText(i), &szDirectory);
		while ((szDirectory.Length() > 1) && szDirectory.EndsWith(FILE_SEPARATOR[0]))
		{
			szDirectory.RemoveFromEnd(1);
		}
		if (!paszDirectories->Contains(&szDirectory))
		{
			paszDirectories->Add(&szDirectory);
		}
		szDirectory.Kill();
	}
	aszParts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The compiler's PreprocessorDefinitions: NAME or NAME=VALUE each.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetPreprocessorDefinitions(CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszDefines)
{
	GetPropertyList("ClCompile.PreprocessorDefinitions", pcConfiguration, szSolutionDirectory, paszDefines);
}


//////////////////////////////////////////////////////////////////////////
//
// A tool's AdditionalOptions ("ClCompile", "Link"): the arguments it is
// given as written, split where a command line is, at the white space
// outside quotes, which are kept.  For g++ they are its own arguments.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetAdditionalOptions(const char* szTool, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszOptions)
{
	CChars		szProperty;
	CChars		szExpanded;
	CChars		szOption;
	char*		szValue;
	const char*	sz;
	bool		bQuoted;

	szProperty.Init(szTool);
	szProperty.Append(".AdditionalOptions");
	szValue = pcConfiguration->GetProperty(szProperty.Text());
	szProperty.Kill();
	if (szValue == NULL)
	{
		return;
	}

	szExpanded.Init();
	ExpandMacros(szValue, pcConfiguration, szSolutionDirectory, &szExpanded);
	szOption.Init();
	bQuoted = false;
	for (sz = szExpanded.Text(); ; sz++)
	{
		if ((*sz == '\0') || (!bQuoted && ((*sz == ' ') || (*sz == '\t') || (*sz == '\r') || (*sz == '\n'))))
		{
			if (!szOption.Empty())
			{
				paszOptions->Add(szOption.Text());
				szOption.Clear();
			}
			if (*sz == '\0')
			{
				break;
			}
		}
		else
		{
			bQuoted = (*sz == '"') ? !bQuoted : bQuoted;
			szOption.Append(*sz);
		}
	}
	szOption.Kill();
	szExpanded.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What the linker is given besides the project's objects: Link's
// AdditionalDependencies.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetAdditionalDependencies(CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszLibraries)
{
	GetPropertyList("Link.AdditionalDependencies", pcConfiguration, szSolutionDirectory, paszLibraries);
}


//////////////////////////////////////////////////////////////////////////
//
// A ';' separated property with its macros expanded, each part trimmed;
// empty parts are left out.  Appended to paszDest.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::GetPropertyList(const char* szProperty, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszDest)
{
	char*		szValue;
	CChars		szExpanded;
	CArrayChars	aszParts;
	CChars*		pszPart;
	size		i;

	szValue = pcConfiguration->GetProperty(szProperty);
	if (szValue == NULL)
	{
		return;
	}

	szExpanded.Init();
	ExpandMacros(szValue, pcConfiguration, szSolutionDirectory, &szExpanded);
	aszParts.Init();
	szExpanded.Split(&aszParts, ';');
	for (i = 0; i < aszParts.NumElements(); i++)
	{
		pszPart = aszParts.Get(i);
		pszPart->StripWhitespace(true);
		if (!pszPart->Empty())
		{
			paszDest->Add(pszPart);
		}
	}
	aszParts.Kill();
	szExpanded.Kill();
}


const char* CVcxProject::GetFileName(void) const { return mszFileName.Text(); }
const char* CVcxProject::GetDirectory(void) const { return mszDirectory.Text(); }
const char* CVcxProject::GetName(void) const { return mszName.Text(); }
const char* CVcxProject::GetGuid(void) const { return mszGuid.Text(); }
const char* CVcxProject::GetRootNamespace(void) const { return mszRootNamespace.Text(); }
bool CVcxProject::HasFilters(void) const { return mbFilters; }
size CVcxProject::NumConfigurations(void) const { return masConfigurations.NumElements(); }
CVcxConfiguration* CVcxProject::GetConfiguration(size uiIndex) { return masConfigurations.Get(uiIndex); }
size CVcxProject::NumItems(void) const { return masItems.NumElements(); }
CVcxItem* CVcxProject::GetItem(size uiIndex) { return masItems.Get(uiIndex); }
size CVcxProject::NumFilters(void) const { return masFilters.NumElements(); }
CVcxFilter* CVcxProject::GetFilter(size uiIndex) { return masFilters.Get(uiIndex); }
size CVcxProject::NumReferences(void) const { return masReferences.NumElements(); }
size CVcxProject::NumSheets(void) const { return maszSheets.NumElements(); }
const char* CVcxProject::GetSheet(size uiIndex) { return maszSheets.GetText(uiIndex); }
CVcxProjectReference* CVcxProject::GetReference(size uiIndex) { return masReferences.Get(uiIndex); }

