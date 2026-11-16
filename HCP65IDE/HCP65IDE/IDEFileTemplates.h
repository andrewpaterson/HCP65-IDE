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


//The Files templates the refactorings that make files write them from.
#define IDE_TEMPLATE_CPP_HEADER				"C++ Header"
#define IDE_TEMPLATE_CPP_SOURCE				"C++ Source"


//The Code templates the generators write from.
#define IDE_TEMPLATE_DEFINITION_BANNER		"Definition Banner"
#define IDE_TEMPLATE_NEW_FUNCTION_BODY		"New Function Body"
#define IDE_TEMPLATE_GETTER_BODY			"Getter Body"
#define IDE_TEMPLATE_SETTER_BODY			"Setter Body"
#define IDE_TEMPLATE_INTERFACE_COMMENT		"Template Interface Comment"


//Where a template's text is from, as IntelliJ's schemes: the IDE's own, the user's for every workspace (Default), or
//the workspace's (kept with its code).  A later one is used over an earlier one of the same tab and name.
enum EIDETemplateScheme
{
	ITS_BuiltIn,
	ITS_Default,
	ITS_Workspace,
};


//A scheme's templates are kept in <directory>\fileTemplates: each one's text in <Tab>\<Name>.ft (a Files template's
//<Name>.<extension>.ft) and all of them in Templates.xml, with what isn't in their text.
#define IDE_TEMPLATES_DIRECTORY				"fileTemplates"
#define IDE_TEMPLATES_INDEX					"Templates.xml"


struct SIDEFileTemplate
{
	EIDETemplateTab		eTab;
	EIDETemplateScheme	eScheme;
	CChars				szName;
	CChars				szExtension;	//A Files template's file's: "h".
	CChars				szFileName;		//A Files template's file's name, with variables: "${NAME}".
	CChars				szParent;		//The template a child is made with (C++ Class's C++ Source), or empty.
	CChars				szText;
	CTemplateText		cParsed;		//szText, parsed when it is first applied.
	bool				bParsed;

	void	Init(EIDETemplateTab eTab, EIDETemplateScheme eScheme, const char* szName, const char* szExtension, const char* szFileName, const char* szParent, const char* szText);
	void	Kill(void);
	void	SetText(const char* szText);
};


typedef CArrayTemplate<SIDEFileTemplate>	CArrayIDEFileTemplates;


//Every file and code template by its tab and name: the built-in ones (written as the IDE wrote its files and code
//before there were templates), and the Default and Workspace schemes' changes to them and additions, read from their
//directories (Read) and written back (Write) by Settings' File and Code Templates.  Each scheme's is an entry of its
//own, so taking a scheme's away (Remove) gives the one under it back, as IntelliJ's Reset does.  A template's
//#parse("File Header") finds the Includes tab's of that name.
class CIDEFileTemplates : public CTemplateIncludes
{
protected:
	CArrayIDEFileTemplates	masTemplates;
	CChars					mszError;					//Why the last Apply, Read or Write failed.
	CChars					maszDirectories[3];			//Each scheme's directory (ITS_), empty for none.

public:
	void				Init(void);
	void				Kill(void);
	void				Copy(CIDEFileTemplates* pcSource);

	bool				Read(const char* szDefaultDirectory, const char* szWorkspaceDirectory);
	bool				Write(EIDETemplateScheme eScheme);
	char*				GetDirectory(EIDETemplateScheme eScheme);

	size				NumTemplates(void);
	SIDEFileTemplate*	GetTemplate(size uiIndex);
	SIDEFileTemplate*	Get(EIDETemplateTab eTab, const char* szName, EIDETemplateScheme eMost = ITS_Workspace);
	SIDEFileTemplate*	Find(EIDETemplateTab eTab, const char* szName, EIDETemplateScheme eScheme);
	SIDEFileTemplate*	Set(EIDETemplateScheme eScheme, EIDETemplateTab eTab, const char* szName, const char* szExtension, const char* szFileName, const char* szParent, const char* szText);
	bool				Remove(EIDETemplateScheme eScheme, EIDETemplateTab eTab, const char* szName);
	bool				IsBuiltIn(EIDETemplateTab eTab, const char* szName);

	bool				Apply(EIDETemplateTab eTab, const char* szName, CTemplateValues* pcValues, CChars* pszDest);
	bool				ApplyCode(const char* szName, CChars* pszDest, ...);
	char*				GetError(void);
	bool				GetInclude(const char* szName, CChars* pszDest) override;

	static const char*	GetTabName(EIDETemplateTab eTab);

protected:
	void				AddBuiltIns(void);
	void				Clear(void);
	bool				ReadScheme(EIDETemplateScheme eScheme, const char* szDirectory);
	void				AppendTemplateFile(const char* szDirectory, SIDEFileTemplate* psTemplate, CChars* pszDest);
	bool				FindTab(const char* szName, EIDETemplateTab* peDest);
};


#endif // __IDE_FILE_TEMPLATES_H__
