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
#include <stdarg.h>
#include "IDEFileTemplates.h"


//The built-in templates: tab, name, extension, file name, parent, text.
struct SIDEBuiltInTemplate
{
	EIDETemplateTab	eTab;
	const char*		szName;
	const char*		szExtension;
	const char*		szFileName;
	const char*		szParent;
	const char*		szText;
};


static const SIDEBuiltInTemplate gasIDEBuiltInTemplates[] =
{
	{ ITT_Includes, "File Header", "", "", "",
		"## The licence comment a new file starts with: the project's licence (its properties), or none.\n"
		"#if ($LICENCE == \"LGPL 3\" || $LICENCE == \"AGPL 3\")\n"
		"#if ($LICENCE == \"LGPL 3\")\n"
		"#set ($LICENCE_NAME = \"GNU Lesser General Public License\")\n"
		"#else\n"
		"#set ($LICENCE_NAME = \"GNU Affero General Public License\")\n"
		"#end\n"
		"/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **\n"
		"\n"
		"Copyright (c) ${YEAR} ${AUTHOR}\n"
		"\n"
		"This file is part of The Codaphela Project: Codaphela ${PROJECT_NAME}\n"
		"\n"
		"Codaphela ${PROJECT_NAME} is free software: you can redistribute it and/or modify\n"
		"it under the terms of the ${LICENCE_NAME} as published by\n"
		"the Free Software Foundation, either version 3 of the License, or\n"
		"(at your option) any later version.\n"
		"\n"
		"Codaphela ${PROJECT_NAME} is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"${LICENCE_NAME} for more details.\n"
		"\n"
		"You should have received a copy of the ${LICENCE_NAME}\n"
		"along with Codaphela ${PROJECT_NAME}.  If not, see <http://www.gnu.org/licenses/>.\n"
		"\n"
		"** ------------------------------------------------------------------------ **/\n"
		"#end\n" },
	{ ITT_Includes, "Include Guard Open", "", "", "",
		"#ifndef ${INCLUDE_GUARD}\n"
		"#define ${INCLUDE_GUARD}\n" },
	{ ITT_Includes, "Include Guard Close", "", "", "",
		"#endif // ${INCLUDE_GUARD}\n" },
	{ ITT_Code, "Definition Banner", "", "", "",
		"//////////////////////////////////////////////////////////////////////////\n"
		"//\n"
		"//\n"
		"//////////////////////////////////////////////////////////////////////////\n" },
	{ ITT_Code, "New Function Body", "", "", "",
		"## What goes between a new function's braces: Generate Definition's (nothing), Extract Method's (${BODY}, the\n"
		"## statements taken out, each line ended).  ${FUNCTION_NAME}, ${CLASS_NAME} and ${RETURN_TYPE} can be used too.\n"
		"$!{BODY}" },
	{ ITT_Code, "Getter Body", "", "", "",
		"## Encapsulate Field's getter: the one statement between its braces.  ${FIELD} is the field, ${IS_OBJECT} true where\n"
		"## the getter gives a pointer to it (a field that is an object).\n"
		"return #if($IS_OBJECT)&#end${FIELD};" },
	{ ITT_Code, "Setter Body", "", "", "",
		"## Encapsulate Field's setter: the one statement between its braces.\n"
		"${FIELD} = ${PARAMETER};" },
	{ ITT_Code, "Template Interface Comment", "", "", "",
		"## Write Template's Interface: what a template needs of a parameter, over the template.  ${PARAMETER} is the\n"
		"## parameter, ${MEMBERS} the members it must have (each one's declaration), ${INDENT} the template's indent.\n"
		"${INDENT}//${PARAMETER} must have:\n"
		"#foreach ($MEMBER in $MEMBERS)\n"
		"${INDENT}//\t${MEMBER};\n"
		"#end\n" },
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileTemplate::Init(EIDETemplateTab eTab, const char* szName, const char* szExtension, const char* szFileName, const char* szParent, const char* szText)
{
	this->eTab = eTab;
	this->szName.Init(szName);
	this->szExtension.Init(szExtension);
	this->szFileName.Init(szFileName);
	this->szParent.Init(szParent);
	this->szText.Init(szText);
	cParsed.Init();
	bParsed = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileTemplate::Kill(void)
{
	cParsed.Kill();
	szText.Kill();
	szParent.Kill();
	szFileName.Kill();
	szExtension.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::Init(void)
{
	masTemplates.Init();
	mszError.Init();
	AddBuiltIns();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::Kill(void)
{
	size	i;

	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		masTemplates.Get(i)->Kill();
	}
	masTemplates.Kill();
	mszError.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::AddBuiltIns(void)
{
	const SIDEBuiltInTemplate*	psBuiltIn;
	size						i;

	for (i = 0; i < sizeof(gasIDEBuiltInTemplates) / sizeof(SIDEBuiltInTemplate); i++)
	{
		psBuiltIn = &gasIDEBuiltInTemplates[i];
		masTemplates.Add()->Init(psBuiltIn->eTab, psBuiltIn->szName, psBuiltIn->szExtension, psBuiltIn->szFileName, psBuiltIn->szParent, psBuiltIn->szText);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CIDEFileTemplates::NumTemplates(void) { return masTemplates.NumElements(); }
SIDEFileTemplate* CIDEFileTemplates::GetTemplate(size uiIndex) { return masTemplates.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The template of a tab by its name, or NULL.
//
//////////////////////////////////////////////////////////////////////////
SIDEFileTemplate* CIDEFileTemplates::Get(EIDETemplateTab eTab, const char* szName)
{
	SIDEFileTemplate*	psTemplate;
	size				i;

	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		psTemplate = masTemplates.Get(i);
		if ((psTemplate->eTab == eTab) && psTemplate->szName.Equals(szName))
		{
			return psTemplate;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A template applied with values, appended to pszDest.  False, with why
// (GetError: "Definition Banner: Line 2, column 1: #if has no #end"), if
// there is no template of that name or it has an error; pszDest is then
// as it was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::Apply(EIDETemplateTab eTab, const char* szName, CTemplateValues* pcValues, CChars* pszDest)
{
	SIDEFileTemplate*	psTemplate;
	CChars				szOutput;
	bool				bResult;

	mszError.Clear();
	psTemplate = Get(eTab, szName);
	if (!psTemplate)
	{
		mszError.Append("There is no template called ")->Append(szName);
		return false;
	}
	if (!psTemplate->bParsed)
	{
		psTemplate->bParsed = true;
		psTemplate->cParsed.Parse(psTemplate->szText.Text());
	}
	szOutput.Init();
	bResult = psTemplate->cParsed.Evaluate(pcValues, this, &szOutput);
	if (bResult)
	{
		pszDest->Append(szOutput);
	}
	else
	{
		mszError.Append("The ")->Append(szName)->Append(" template has an error: ")->Append(psTemplate->cParsed.GetError());
	}
	szOutput.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A Code template applied with the values given after pszDest as pairs
// of name and text, ended by a NULL: ApplyCode("Setter Body", &sz,
// "FIELD", "miSize", "PARAMETER", "iSize", NULL).  A value that is "" is
// false to an #if.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::ApplyCode(const char* szName, CChars* pszDest, ...)
{
	CTemplateValues	cValues;
	va_list			vaArguments;
	const char*		szVariable;
	const char*		szValue;
	bool			bResult;

	cValues.Init();
	va_start(vaArguments, pszDest);
	for (szVariable = va_arg(vaArguments, const char*); szVariable; szVariable = va_arg(vaArguments, const char*))
	{
		szValue = va_arg(vaArguments, const char*);
		cValues.Set(szVariable, szValue ? szValue : "");
	}
	va_end(vaArguments);
	bResult = Apply(ITT_Code, szName, &cValues, pszDest);
	cValues.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CIDEFileTemplates::GetError(void) { return mszError.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// #parse("File Header"): the Includes tab's template of that name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::GetInclude(const char* szName, CChars* pszDest)
{
	SIDEFileTemplate*	psTemplate;

	psTemplate = Get(ITT_Includes, szName);
	if (!psTemplate)
	{
		return false;
	}
	pszDest->Append(psTemplate->szText);
	return true;
}
