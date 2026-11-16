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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
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
	{ ITT_Files, "C++ Header", "h", "${NAME}", "",
		"## A new header.  A refactoring gives ${INCLUDES} (its #includes, each line ended) and ${BODY} (the class, and what\n"
		"## it defines outside the class); New gives neither.\n"
		"#parse(\"File Header\")\n"
		"#parse(\"Include Guard Open\")\n"
		"$!{INCLUDES}\n"
		"\n"
		"$!{BODY}\n"
		"\n"
		"#parse(\"Include Guard Close\")\n" },
	{ ITT_Files, "C++ Source", "cpp", "${NAME}", "",
		"## A new source file: its header's #include, then a refactoring's ${INCLUDES} and ${BODY} (its definitions, each\n"
		"## after two blank lines).\n"
		"#parse(\"File Header\")\n"
		"#include \"${HEADER_FILENAME}\"\n"
		"$!{INCLUDES}$!{BODY}" },
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
void SIDEFileTemplate::Init(EIDETemplateTab eTab, EIDETemplateScheme eScheme, const char* szName, const char* szExtension, const char* szFileName, const char* szParent, const char* szText)
{
	this->eTab = eTab;
	this->eScheme = eScheme;
	this->szName.Init(szName);
	this->szExtension.Init(szExtension ? szExtension : "");
	this->szFileName.Init(szFileName ? szFileName : "");
	this->szParent.Init(szParent ? szParent : "");
	this->szText.Init(szText ? szText : "");
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
// New text, parsed again when it is next applied.
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileTemplate::SetText(const char* szText)
{
	this->szText.Set(szText);
	cParsed.Kill();
	cParsed.Init();
	bParsed = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::Init(void)
{
	size	i;

	masTemplates.Init();
	mszError.Init();
	mszAuthor.Init();
	for (i = 0; i < 3; i++)
	{
		maszDirectories[i].Init();
	}
	AddBuiltIns();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::Kill(void)
{
	size	i;

	Clear();
	masTemplates.Kill();
	for (i = 0; i < 3; i++)
	{
		maszDirectories[i].Kill();
	}
	mszAuthor.Kill();
	mszError.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::Clear(void)
{
	size	i;

	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		masTemplates.Get(i)->Kill();
	}
	masTemplates.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// These templates made the same as pcSource's (Settings' copy, which is
// changed until OK).
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::Copy(CIDEFileTemplates* pcSource)
{
	SIDEFileTemplate*	psSource;
	size				i;

	Clear();
	for (i = 0; i < pcSource->masTemplates.NumElements(); i++)
	{
		psSource = pcSource->masTemplates.Get(i);
		masTemplates.Add()->Init(psSource->eTab, psSource->eScheme, psSource->szName.Text(), psSource->szExtension.Text(), psSource->szFileName.Text(), psSource->szParent.Text(), psSource->szText.Text());
	}
	for (i = 0; i < 3; i++)
	{
		maszDirectories[i].Set(pcSource->maszDirectories[i]);
	}
	mszAuthor.Set(pcSource->mszAuthor);
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
		masTemplates.Add()->Init(psBuiltIn->eTab, ITS_BuiltIn, psBuiltIn->szName, psBuiltIn->szExtension, psBuiltIn->szFileName, psBuiltIn->szParent, psBuiltIn->szText);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The tab's name, as its directory and Templates.xml have it.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEFileTemplates::GetTabName(EIDETemplateTab eTab)
{
	switch (eTab)
	{
	case ITT_Files:
		return "Files";
	case ITT_Includes:
		return "Includes";
	default:
		return "Code";
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::FindTab(const char* szName, EIDETemplateTab* peDest)
{
	EIDETemplateTab	aeTabs[] = { ITT_Files, ITT_Includes, ITT_Code };
	size			i;

	for (i = 0; i < 3; i++)
	{
		if (strcmp(GetTabName(aeTabs[i]), szName) == 0)
		{
			*peDest = aeTabs[i];
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The built-in templates, then the Default scheme's and the Workspace
// scheme's (each read from its directory's fileTemplates; an empty
// directory is no scheme).  False, with why (GetError), if a scheme's
// files can't be read; the others are still read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::Read(const char* szDefaultDirectory, const char* szWorkspaceDirectory)
{
	bool	bResult;

	mszError.Clear();
	Clear();
	AddBuiltIns();
	maszDirectories[ITS_BuiltIn].Clear();
	maszDirectories[ITS_Default].Set(szDefaultDirectory ? szDefaultDirectory : "");
	maszDirectories[ITS_Workspace].Set(szWorkspaceDirectory ? szWorkspaceDirectory : "");
	bResult = ReadScheme(ITS_Default, maszDirectories[ITS_Default].Text());
	bResult = ReadScheme(ITS_Workspace, maszDirectories[ITS_Workspace].Text()) && bResult;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CIDEFileTemplates::GetDirectory(EIDETemplateScheme eScheme) { return maszDirectories[eScheme].Text(); }
char* CIDEFileTemplates::GetAuthor(void) { return mszAuthor.Text(); }
void CIDEFileTemplates::SetAuthor(const char* szAuthor) { mszAuthor.Set(szAuthor ? szAuthor : ""); }


//////////////////////////////////////////////////////////////////////////
//
// What the user's settings keep of the templates: the author
// (<FileTemplates Author="..."/>).  The templates are their schemes'.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szAuthor;

	pcTag = pcSettings->GetTag("FileTemplates");
	szAuthor = pcTag ? pcTag->GetStringAttribute("Author") : NULL;
	mszAuthor.Set(szAuthor ? szAuthor : "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin("FileTemplates");
	pcWriter->Attribute("Author", mszAuthor.Text());
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// A template's file in a scheme's directory: <directory>\fileTemplates\
// <Tab>\<Name>.ft, or <Name>.<extension>.ft for a Files template.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplates::AppendTemplateFile(const char* szDirectory, SIDEFileTemplate* psTemplate, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;

	pszDest->Append(szDirectory);
	cFileUtil.AppendToPath(pszDest, IDE_TEMPLATES_DIRECTORY);
	cFileUtil.AppendToPath(pszDest, GetTabName(psTemplate->eTab));
	szName.Init(psTemplate->szName);
	if ((psTemplate->eTab == ITT_Files) && !psTemplate->szExtension.Empty())
	{
		szName.Append('.')->Append(psTemplate->szExtension);
	}
	szName.Append(".ft");
	cFileUtil.AppendToPath(pszDest, szName.Text());
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A scheme's templates from its directory's Templates.xml and their .ft
// files.  True if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::ReadScheme(EIDETemplateScheme eScheme, const char* szDirectory)
{
	CXMLFile			cFile;
	CFileUtil			cFileUtil;
	CTextFile			cText;
	CChars				szIndex;
	CChars				szFolder;
	CChars				szPath;
	CMarkupTag*			pcRoot;
	CMarkupTag*			pcTag;
	STagIterator		sIterator;
	SIDEFileTemplate	sTemplate;
	EIDETemplateTab		eTab;
	char*				szName;
	bool				bResult;

	if (!szDirectory || (szDirectory[0] == '\0'))
	{
		return true;
	}
	szFolder.Init(szDirectory);
	cFileUtil.AppendToPath(&szFolder, IDE_TEMPLATES_DIRECTORY);
	szIndex.Init(szFolder);
	cFileUtil.AppendToPath(&szIndex, IDE_TEMPLATES_INDEX);
	if (!cFileUtil.Exists(szIndex.Text()))
	{
		szIndex.Kill();
		szFolder.Kill();
		return true;
	}

	cFile.Init();
	bResult = cFile.Read(IDE_TEMPLATES_INDEX, szFolder.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = (pcRoot != NULL);
	for (pcTag = pcRoot ? pcRoot->GetTag(&sIterator) : NULL; pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
	{
		szName = pcTag->GetAttribute("Name");
		if (!szName || !pcTag->GetAttribute("Tab") || !FindTab(pcTag->GetAttribute("Tab"), &eTab))
		{
			bResult = false;
			continue;
		}
		sTemplate.Init(eTab, eScheme, szName, pcTag->GetAttribute("Extension"), pcTag->GetAttribute("FileName"), pcTag->GetAttribute("Parent"), "");
		szPath.Init();
		AppendTemplateFile(szDirectory, &sTemplate, &szPath);
		cText.Init();
		if (cText.Read(szPath.Text()))
		{
			cText.PassifyNewlines();
			Set(eScheme, eTab, szName, sTemplate.szExtension.Text(), sTemplate.szFileName.Text(), sTemplate.szParent.Text(), cText.Text());
		}
		else
		{
			bResult = false;
		}
		cText.Kill();
		szPath.Kill();
		sTemplate.Kill();
	}
	cFile.Kill();
	if (!bResult)
	{
		mszError.Append(mszError.Empty() ? "" : " ")->Append("The templates in ")->Append(szFolder)->Append(" couldn't all be read.");
	}
	szIndex.Kill();
	szFolder.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A scheme's templates written to its directory: Templates.xml lists
// them, each one's text is its .ft file, and a .ft file of the scheme's
// that is no longer one of them (it was reset) is deleted.  False, with
// why, if they can't all be written or the scheme has no directory.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::Write(EIDETemplateScheme eScheme)
{
	CXMLWriter			cWriter;
	CFileUtil			cFileUtil;
	CTextFile			cText;
	CArrayChars			aszWritten;
	CArrayChars			aszFound;
	CChars				szFolder;
	CChars				szPath;
	SIDEFileTemplate*	psTemplate;
	EIDETemplateTab		aeTabs[] = { ITT_Files, ITT_Includes, ITT_Code };
	char*				szDirectory;
	size				i;
	size				j;
	bool				bResult;
	bool				bAny;

	mszError.Clear();
	szDirectory = maszDirectories[eScheme].Text();
	if ((eScheme == ITS_BuiltIn) || (szDirectory[0] == '\0'))
	{
		mszError.Set("There is nowhere to keep the templates.");
		return false;
	}
	szFolder.Init(szDirectory);
	cFileUtil.AppendToPath(&szFolder, IDE_TEMPLATES_DIRECTORY);
	bAny = false;
	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		bAny = bAny || (masTemplates.Get(i)->eScheme == eScheme);
	}
	if (!bAny && !cFileUtil.Exists(szFolder.Text()))
	{
		szFolder.Kill();
		return true;
	}

	bResult = true;
	aszWritten.Init();
	cWriter.Init();
	cWriter.Begin("FileTemplates");
	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		psTemplate = masTemplates.Get(i);
		if (psTemplate->eScheme != eScheme)
		{
			continue;
		}
		cWriter.Begin("Template");
		cWriter.Attribute("Tab", GetTabName(psTemplate->eTab));
		cWriter.Attribute("Name", psTemplate->szName.Text());
		if (psTemplate->eTab == ITT_Files)
		{
			cWriter.Attribute("Extension", psTemplate->szExtension.Text());
			cWriter.Attribute("FileName", psTemplate->szFileName.Text());
			if (!psTemplate->szParent.Empty())
			{
				cWriter.Attribute("Parent", psTemplate->szParent.Text());
			}
		}
		cWriter.End();
		szPath.Init();
		AppendTemplateFile(szDirectory, psTemplate, &szPath);
		cFileUtil.TouchDir(szPath.Text(), true);
		cText.Init(psTemplate->szText.Text());
		bResult = cText.Write(szPath.Text()) && bResult;
		cText.Kill();
		aszWritten.Add(szPath.Text());
		szPath.Kill();
	}
	cWriter.End();
	szPath.Init(szFolder);
	cFileUtil.AppendToPath(&szPath, IDE_TEMPLATES_INDEX);
	cFileUtil.TouchDir(szPath.Text(), true);
	bResult = cWriter.Write(szPath.Text()) && bResult;
	szPath.Kill();
	cWriter.Kill();

	//The scheme's files that are no longer its templates'.
	for (i = 0; i < 3; i++)
	{
		szPath.Init(szFolder);
		cFileUtil.AppendToPath(&szPath, GetTabName(aeTabs[i]));
		aszFound.Init();
		if (cFileUtil.IsDirectory(szPath.Text()))
		{
			cFileUtil.FindFilesWithExtension(szPath.Text(), "ft", &aszFound);
		}
		for (j = 0; j < aszFound.NumElements(); j++)
		{
			if (!aszWritten.Contains(aszFound.GetText(j)))
			{
				cFileUtil.Delete(aszFound.GetText(j));
			}
		}
		aszFound.Kill();
		szPath.Kill();
	}
	aszWritten.Kill();
	if (!bResult)
	{
		mszError.Append("The templates couldn't all be written to ")->Append(szFolder)->Append('.');
	}
	szFolder.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CIDEFileTemplates::NumTemplates(void) { return masTemplates.NumElements(); }
SIDEFileTemplate* CIDEFileTemplates::GetTemplate(size uiIndex) { return masTemplates.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The template of a tab by its name, as the schemes up to eMost have it
// (the latest scheme's), or NULL.
//
//////////////////////////////////////////////////////////////////////////
SIDEFileTemplate* CIDEFileTemplates::Get(EIDETemplateTab eTab, const char* szName, EIDETemplateScheme eMost)
{
	SIDEFileTemplate*	psTemplate;
	SIDEFileTemplate*	psFound;
	size				i;

	psFound = NULL;
	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		psTemplate = masTemplates.Get(i);
		if ((psTemplate->eTab == eTab) && (psTemplate->eScheme <= eMost) && psTemplate->szName.Equals(szName) && (!psFound || (psTemplate->eScheme > psFound->eScheme)))
		{
			psFound = psTemplate;
		}
	}
	return psFound;
}


//////////////////////////////////////////////////////////////////////////
//
// A scheme's own entry for a template, or NULL.
//
//////////////////////////////////////////////////////////////////////////
SIDEFileTemplate* CIDEFileTemplates::Find(EIDETemplateTab eTab, const char* szName, EIDETemplateScheme eScheme)
{
	SIDEFileTemplate*	psTemplate;
	size				i;

	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		psTemplate = masTemplates.Get(i);
		if ((psTemplate->eTab == eTab) && (psTemplate->eScheme == eScheme) && psTemplate->szName.Equals(szName))
		{
			return psTemplate;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A scheme's template changed, or added to it.
//
//////////////////////////////////////////////////////////////////////////
SIDEFileTemplate* CIDEFileTemplates::Set(EIDETemplateScheme eScheme, EIDETemplateTab eTab, const char* szName, const char* szExtension, const char* szFileName, const char* szParent, const char* szText)
{
	SIDEFileTemplate*	psTemplate;

	psTemplate = Find(eTab, szName, eScheme);
	if (!psTemplate)
	{
		psTemplate = masTemplates.Add();
		psTemplate->Init(eTab, eScheme, szName, szExtension, szFileName, szParent, szText);
		return psTemplate;
	}
	psTemplate->szExtension.Set(szExtension ? szExtension : "");
	psTemplate->szFileName.Set(szFileName ? szFileName : "");
	psTemplate->szParent.Set(szParent ? szParent : "");
	psTemplate->SetText(szText ? szText : "");
	return psTemplate;
}


//////////////////////////////////////////////////////////////////////////
//
// A scheme's entry for a template taken away: the scheme under it gives
// the template (IntelliJ's Reset), or, if none has it, it is gone.  A
// built-in one can't be.  False if the scheme has none of it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::Remove(EIDETemplateScheme eScheme, EIDETemplateTab eTab, const char* szName)
{
	SIDEFileTemplate*	psTemplate;
	size				i;

	if (eScheme == ITS_BuiltIn)
	{
		return false;
	}
	for (i = 0; i < masTemplates.NumElements(); i++)
	{
		psTemplate = masTemplates.Get(i);
		if ((psTemplate->eTab == eTab) && (psTemplate->eScheme == eScheme) && psTemplate->szName.Equals(szName))
		{
			psTemplate->Kill();
			masTemplates.RemoveAt(i);
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplates::IsBuiltIn(EIDETemplateTab eTab, const char* szName) { return Find(eTab, szName, ITS_BuiltIn) != NULL; }


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
