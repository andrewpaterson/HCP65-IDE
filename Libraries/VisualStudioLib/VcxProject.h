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
#ifndef __VCX_PROJECT_H__
#define __VCX_PROJECT_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/XMLFile.h"
#include "VcxItem.h"
#include "VcxFilter.h"
#include "VcxConfiguration.h"
#include "VcxProjectReference.h"


//A Visual Studio C++ project, read from its .vcxproj and (when it has one) its .vcxproj.filters: its configurations and
//their properties, the files it lists, the filters Solution Explorer shows them in and the projects it references.
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

public:
	void					Init(void);
	void					Kill(void);

	bool					Read(const char* szFileName);

	char*					GetFileName(void);
	char*					GetDirectory(void);
	char*					GetName(void);
	char*					GetGuid(void);
	char*					GetRootNamespace(void);
	bool					HasFilters(void);

	size					NumConfigurations(void);
	CVcxConfiguration*		GetConfiguration(size uiIndex);
	CVcxConfiguration*		FindConfiguration(const char* szName);

	size					NumItems(void);
	CVcxItem*				GetItem(size uiIndex);
	CVcxItem*				FindItem(const char* szElement, const char* szInclude);
	void					GetItemsInFilter(const char* szFilter, CArrayInt* paiItems);
	void					GetFullPath(CVcxItem* pcItem, CChars* pszDest);

	size					NumFilters(void);
	CVcxFilter*				GetFilter(size uiIndex);
	CVcxFilter*				FindFilter(const char* szName);
	void					GetChildFilters(const char* szParent, CArrayInt* paiFilters);

	size					NumReferences(void);
	CVcxProjectReference*	GetReference(size uiIndex);

protected:
	bool					ReadProject(CMarkupTag* pcProject);
	bool					ReadFilters(CMarkupTag* pcProject);
	void					ReadItemGroup(CMarkupTag* pcItemGroup);
	void					ReadPropertyGroup(CMarkupTag* pcPropertyGroup);
	void					ReadItemDefinitionGroup(CMarkupTag* pcItemDefinitionGroup);
	void					ReadFilterItemGroup(CMarkupTag* pcItemGroup);
	void					SetProperty(const char* szCondition, const char* szName, const char* szValue);
	CVcxConfiguration*		AddConfiguration(const char* szName);
};


bool ReadVcxXMLFile(CXMLFile* pcFile, const char* szFileName);
bool GetVcxTagText(CMarkupTag* pcTag, const char* szChild, CChars* pszDest);
bool GetVcxConditionConfiguration(const char* szCondition, CChars* pszDest);


#endif // __VCX_PROJECT_H__

