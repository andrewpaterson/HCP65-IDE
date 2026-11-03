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
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/StringHelper.h"
#include "VcxPath.h"
#include "VcxProject.h"


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
// The configuration a condition on '$(Configuration)|$(Platform)' is
// for: Debug|Win32 from '$(Configuration)|$(Platform)'=='Debug|Win32'.
// False for any other condition.
//
//////////////////////////////////////////////////////////////////////////
bool GetVcxConditionConfiguration(const char* szCondition, CChars* pszDest)
{
	const char*	szStart;
	const char*	szEnd;

	if (strstr(szCondition, "$(Configuration)") == NULL)
	{
		return false;
	}
	szStart = strstr(szCondition, "=='");
	if (szStart == NULL)
	{
		return false;
	}
	szStart += 3;
	szEnd = strchr(szStart, '\'');
	if (szEnd == NULL)
	{
		return false;
	}
	pszDest->Append(szStart, (int)(szEnd - szStart));
	return true;
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

	mszFileName.Set(szFileName);
	cFileUtil.FullPath(&mszFileName);
	mszDirectory.Clear();
	mszName.Clear();
	cFileUtil.SplitPath(mszFileName.Text(), &mszName, &mszDirectory);
	cFileUtil.RemoveExtension(&mszName);

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
// The <Project> of a .vcxproj.  Its configurations come first (in the
// <ItemGroup Label="ProjectConfigurations">) so the property groups
// after them set properties on them.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxProject::ReadProject(CMarkupTag* pcProject)
{
	CMarkupTag*		pcTag;
	STagIterator	sIter;

	if (!pcProject->Is("Project"))
	{
		return gcLogger.Error2(__METHOD__, " Expected a <Project> in [", mszFileName.Text(), "], not <", pcProject->GetName(), ">.", NULL);
	}

	pcTag = pcProject->GetTag(&sIter);
	while (pcTag != NULL)
	{
		if (pcTag->Is("ItemGroup"))
		{
			ReadItemGroup(pcTag);
		}
		else if (pcTag->Is("PropertyGroup"))
		{
			ReadPropertyGroup(pcTag);
		}
		else if (pcTag->Is("ItemDefinitionGroup"))
		{
			ReadItemDefinitionGroup(pcTag);
		}
		pcTag = pcProject->GetNextTag(&sIter);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Configurations, project references and items.  An item is any element
// with an Include, its kind its element's name (ClCompile, ClInclude,
// None...).
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::ReadItemGroup(CMarkupTag* pcItemGroup)
{
	CMarkupTag*				pcTag;
	STagIterator			sIter;
	char*					szInclude;
	CVcxProjectReference*	pcReference;
	CChars					szProject;

	pcTag = pcItemGroup->GetTag(&sIter);
	while (pcTag != NULL)
	{
		szInclude = pcTag->GetStringAttribute("Include");
		if (szInclude != NULL)
		{
			if (pcTag->Is("ProjectConfiguration"))
			{
				AddConfiguration(szInclude);
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
				masItems.Add()->Init(pcTag->GetName(), szInclude);
			}
		}
		pcTag = pcItemGroup->GetNextTag(&sIter);
	}
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
// Sets a property on the configuration szCondition is for, or on every
// configuration if szCondition is NULL.  A condition that isn't on the
// configuration is ignored.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProject::SetProperty(const char* szCondition, const char* szName, const char* szValue)
{
	CChars				szConfiguration;
	CVcxConfiguration*	pcConfiguration;
	size				i;

	if (szCondition == NULL)
	{
		for (i = 0; i < masConfigurations.NumElements(); i++)
		{
			masConfigurations.Get(i)->SetProperty(szName, szValue);
		}
		return;
	}

	szConfiguration.Init();
	if (GetVcxConditionConfiguration(szCondition, &szConfiguration))
	{
		pcConfiguration = FindConfiguration(szConfiguration.Text());
		if (pcConfiguration != NULL)
		{
			pcConfiguration->SetProperty(szName, szValue);
		}
	}
	szConfiguration.Kill();
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


char* CVcxProject::GetFileName(void) { return mszFileName.Text(); }
char* CVcxProject::GetDirectory(void) { return mszDirectory.Text(); }
char* CVcxProject::GetName(void) { return mszName.Text(); }
char* CVcxProject::GetGuid(void) { return mszGuid.Text(); }
char* CVcxProject::GetRootNamespace(void) { return mszRootNamespace.Text(); }
bool CVcxProject::HasFilters(void) { return mbFilters; }
size CVcxProject::NumConfigurations(void) { return masConfigurations.NumElements(); }
CVcxConfiguration* CVcxProject::GetConfiguration(size uiIndex) { return masConfigurations.Get(uiIndex); }
size CVcxProject::NumItems(void) { return masItems.NumElements(); }
CVcxItem* CVcxProject::GetItem(size uiIndex) { return masItems.Get(uiIndex); }
size CVcxProject::NumFilters(void) { return masFilters.NumElements(); }
CVcxFilter* CVcxProject::GetFilter(size uiIndex) { return masFilters.Get(uiIndex); }
size CVcxProject::NumReferences(void) { return masReferences.NumElements(); }
CVcxProjectReference* CVcxProject::GetReference(size uiIndex) { return masReferences.Get(uiIndex); }

