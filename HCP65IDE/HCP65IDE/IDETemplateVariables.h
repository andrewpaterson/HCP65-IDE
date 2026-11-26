/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_TEMPLATE_VARIABLES_H__
#define __IDE_TEMPLATE_VARIABLES_H__
#include "BaseLib/Chars.h"
#include "TemplateLib/TemplateValues.h"


//A project's licence, as its properties have it (<Project>.hcp65project beside the .vcxproj): what the File Header
//template writes.
#define IDE_LICENCE_LGPL		"LGPL 3"
#define IDE_LICENCE_AGPL		"AGPL 3"
#define IDE_LICENCE_NONE		"None"
#define IDE_LICENCE_DEFAULT		IDE_LICENCE_LGPL


class CIDEContent;
class CVcxProject;


//The predefined variables of a new file's templates (IntelliJ's and CLion's, CodeTemplates.txt item 2), so every
//refactoring and New fill them the same way: szFileName is the new file, szProjectOf a file of the project it goes in
//(it is added to that one's), and szClassName the class it is made for, or NULL.
void	SetIDETemplateFileVariables(CIDEContent* pcContent, const char* szFileName, const char* szProjectOf, const char* szClassName, CTemplateValues* pcDest);
void	SetIDETemplateFileVariables(CIDEContent* pcContent, const char* szFileName, CVcxProject* pcProject, const char* szClassName, CTemplateValues* pcDest);
void	SetIDETemplateDateVariables(CTemplateValues* pcDest);
char*	ReadIDEProjectLicence(const char* szProjectFile, CChars* pszDest);
char*	ReadIDEProjectDocsSite(const char* szProjectFile, CChars* pszDest);
bool	WriteIDEProjectProperties(const char* szProjectFile, const char* szLicence, const char* szDocsSite);
char*	AppendIDEProjectPropertiesFile(const char* szProjectFile, CChars* pszDest);


#endif // __IDE_TEMPLATE_VARIABLES_H__
