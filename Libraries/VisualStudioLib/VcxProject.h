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
#ifndef __VCX_PROJECT_H__
#define __VCX_PROJECT_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/XMLFile.h"
#include "VcxItem.h"
#include "VcxFilter.h"
#include "VcxConfiguration.h"
#include "VcxProjectReference.h"


//The directory of the file it is written in, with its last separator: a property sheet's own.
#define VCX_THIS_FILE_DIRECTORY	"$(MSBuildThisFileDirectory)"


//How deep one property sheet may import another.
#define VCX_MAX_IMPORT_DEPTH	8


//What a Condition is on (ParseVcxCondition).
enum EVcxCondition
{
	VCC_Configuration,
	VCC_Platform,
	VCC_Both,
	VCC_NotConfiguration,
	VCC_Other,
};


//A Visual Studio C++ project, read from its .vcxproj and (when it has one) its .vcxproj.filters: its configurations and
//their properties, the files it lists, the filters Solution Explorer shows them in and the projects it references.
//The property sheets it imports are read where it imports them, as MSBuild reads them.  Its configurations are those
//it lists and, for each toolchain a group of it or of a sheet is for, that toolchain's (CVcxConfiguration).
class CVcxProject
{
protected:
	CChars						mszFileName;		//The .vcxproj's full path.
	CChars						mszDirectory;		//The directory it is in; the items' paths are from here.
	CChars						mszName;			//Its file's name without .vcxproj, as Solution Explorer shows it.
	CChars						mszGuid;
	CChars						mszRootNamespace;
	bool						mbFilters;			//It has a .vcxproj.filters and it was read.
	CArrayVcxConfiguration		masConfigurations;
	CArrayVcxItem				masItems;
	CArrayVcxFilter				masFilters;
	CArrayVcxProjectReference	masReferences;
	CArrayChars					maszSheets;			//The property sheets it imports, each's full path once.
	CArrayTemplatePtr<CXMLFile>	mapcSheetFiles;		//Their files, while the project is read.
	CArrayChars					maszScope;			//The conditions of the imports being read.
	const char*					mszReadingDirectory;	//The directory of the file being read.
	const char*					mszReadingFile;			//A property sheet being read; NULL while it is the project.

public:
	void					Init(void);
	void					Kill(void);

	bool					Read(const char* szFileName);

	const char*				GetFileName(void) const;
	const char*				GetDirectory(void) const;
	const char*				GetName(void) const;
	const char*				GetGuid(void) const;
	const char*				GetRootNamespace(void) const;
	bool					HasFilters(void) const;

	size					NumConfigurations(void) const;
	CVcxConfiguration*		GetConfiguration(size uiIndex);
	CVcxConfiguration*		FindConfiguration(const char* szName);

	size					NumItems(void) const;
	CVcxItem*				GetItem(size uiIndex);
	CVcxItem*				FindItem(const char* szElement, const char* szInclude);
	void					GetItemsInFilter(const char* szFilter, CArrayInt* paiItems);
	void					GetFullPath(CVcxItem* pcItem, CChars* pszDest);
	bool					IsCompiled(CVcxItem* pcItem, CVcxConfiguration* pcConfiguration);
	bool					RenameItem(const char* szOldPath, const char* szNewName);
	bool					AddItemBeside(const char* szExistingPath, const char* szNewName);
	bool					AddItem(const char* szPath, const char* szFilter);
	static const char*		GetItemElement(const char* szFileName);
	bool					RemoveItem(const char* szPath);

	size					NumFilters(void) const;
	CVcxFilter*				GetFilter(size uiIndex);
	CVcxFilter*				FindFilter(const char* szName);
	void					GetChildFilters(const char* szParent, CArrayInt* paiFilters);

	size					NumReferences(void) const;
	CVcxProjectReference*	GetReference(size uiIndex);

	size					NumSheets(void) const;
	const char*				GetSheet(size uiIndex);

	void					ExpandMacros(const char* szValue, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CChars* pszDest);
	void					GetIncludeDirectories(CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszDirectories);
	void					GetPreprocessorDefinitions(CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszDefines);
	void					GetAdditionalOptions(const char* szTool, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszOptions);
	void					GetAdditionalDependencies(CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszLibraries);

protected:
	bool					ReadProject(CMarkupTag* pcProject);
	bool					ReadFilters(CMarkupTag* pcProject);
	bool					RenameIncludes(const char* szFileName, CArrayChars* paszIncludes, const char* szNewName);
	bool					AddIncludesBeside(const char* szFileName, CArrayChars* paszIncludes, const char* szNewName, bool bWhole);
	bool					AddElement(const char* szFileName, const char* szElement, const char* szInclude, const char* szFilter);
	bool					RemoveIncludes(const char* szFileName, CArrayChars* paszIncludes);
	void					ReadProjectConfigurations(CMarkupTag* pcProject);
	void					ReadGroups(CMarkupTag* pcRoot, const char* szDirectory, CArrayChars* paszPlatforms, int iDepth);
	void					ReadImport(CMarkupTag* pcImport, const char* szDirectory, CArrayChars* paszPlatforms, int iDepth);
	bool					ResolveImport(const char* szProject, const char* szDirectory, CChars* pszDest);
	CXMLFile*				GetSheetFile(const char* szFileName);
	bool					PushScope(CMarkupTag* pcTag);
	void					PopScope(bool bScoped);
	void					FindToolchainConditions(CMarkupTag* pcTag, CArrayChars* paszPlatforms);
	void					AddToolchainConfigurations(CArrayChars* paszPlatforms);
	void					SetToolchainBases(void);
	bool					IsConditionFor(const char* szCondition, CVcxConfiguration* pcConfiguration);
	void					ReadItemGroup(CMarkupTag* pcItemGroup);
	void					ReadItemExclusions(CMarkupTag* pcItemTag, CVcxItem* pcItem);
	void					ReadPropertyGroup(CMarkupTag* pcPropertyGroup);
	void					ReadItemDefinitionGroup(CMarkupTag* pcItemDefinitionGroup);
	void					ReadFilterItemGroup(CMarkupTag* pcItemGroup);
	void					SetProperty(const char* szCondition, const char* szName, const char* szValue);
	CVcxConfiguration*		AddConfiguration(const char* szName);
	void					GetPropertyList(const char* szProperty, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CArrayChars* paszDest);
	void					ExpandMacros(const char* szValue, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CChars* pszDest, int iDepth);
	bool					GetMacro(const char* szName, CVcxConfiguration* pcConfiguration, const char* szSolutionDirectory, CChars* pszDest, int iDepth);
};


bool ReadVcxXMLFile(CXMLFile* pcFile, const char* szFileName);
bool GetVcxTagText(CMarkupTag* pcTag, const char* szChild, CChars* pszDest);
EVcxCondition ParseVcxCondition(const char* szCondition, CChars* pszValue);
bool FindVcxConditionProperty(const char* szCondition, const char* szEquals, const char* szProperty);


#endif // __VCX_PROJECT_H__

