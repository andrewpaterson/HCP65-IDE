/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_FILE_TEMPLATES_H__
#define __IDE_FILE_TEMPLATES_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "TemplateLib/TemplateText.h"
#include "TemplateLib/TemplateValues.h"


//The tabs of Settings' File and Code Templates, as IntelliJ's: a template for each kind of new file, the pieces they
//#parse, and what the IDE's generators write.
enum EIDETemplateTab
{
	ITT_Files,
	ITT_Includes,
	ITT_Code,
};


//The Code templates the generators write from.
#define IDE_TEMPLATE_DEFINITION_BANNER		"Definition Banner"
#define IDE_TEMPLATE_NEW_FUNCTION_BODY		"New Function Body"
#define IDE_TEMPLATE_GETTER_BODY			"Getter Body"
#define IDE_TEMPLATE_SETTER_BODY			"Setter Body"
#define IDE_TEMPLATE_INTERFACE_COMMENT		"Template Interface Comment"


struct SIDEFileTemplate
{
	EIDETemplateTab	eTab;
	CChars			szName;
	CChars			szExtension;	//A Files template's file's: "h".
	CChars			szFileName;		//A Files template's file's name, with variables: "${NAME}".
	CChars			szParent;		//The template a child is made with (C++ Class's C++ Source), or empty.
	CChars			szText;
	CTemplateText	cParsed;		//szText, parsed when it is first applied.
	bool			bParsed;

	void	Init(EIDETemplateTab eTab, const char* szName, const char* szExtension, const char* szFileName, const char* szParent, const char* szText);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEFileTemplate>	CArrayIDEFileTemplates;


//Every file and code template by its tab and name: the built-in ones (written as the IDE wrote its files and code
//before there were templates) and, later, the IDE's and the workspace's changes to them (CodeTemplates.txt).  A
//template's #parse("File Header") finds the Includes tab's of that name.
class CIDEFileTemplates : public CTemplateIncludes
{
protected:
	CArrayIDEFileTemplates	masTemplates;
	CChars					mszError;		//Why the last Apply failed.

public:
	void				Init(void);
	void				Kill(void);

	size				NumTemplates(void);
	SIDEFileTemplate*	GetTemplate(size uiIndex);
	SIDEFileTemplate*	Get(EIDETemplateTab eTab, const char* szName);

	bool				Apply(EIDETemplateTab eTab, const char* szName, CTemplateValues* pcValues, CChars* pszDest);
	bool				ApplyCode(const char* szName, CChars* pszDest, ...);
	char*				GetError(void);
	bool				GetInclude(const char* szName, CChars* pszDest) override;

protected:
	void				AddBuiltIns(void);
};


#endif // __IDE_FILE_TEMPLATES_H__
