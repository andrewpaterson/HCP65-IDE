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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/XMLFile.h"
#include "GitLib/GitCommand.h"
#include "TemplateLib/TemplateStrings.h"
#include "IDEContent.h"
#include "IDETemplateVariables.h"


//The properties file's root and its licence.
#define IDE_PROJECT_PROPERTIES_TAG		"HCP65Project"
#define IDE_PROJECT_LICENCE_ATTRIBUTE	"Licence"


//////////////////////////////////////////////////////////////////////////
//
// A project's properties file: its .vcxproj's path with .hcp65project in
// place of .vcxproj.
//
//////////////////////////////////////////////////////////////////////////
char* AppendIDEProjectPropertiesFile(const char* szProjectFile, CChars* pszDest)
{
	const char*	szDot;
	const char*	szName;

	szName = GetIDEFileNamePart(szProjectFile);
	szDot = strrchr(szName, '.');
	if (szDot)
	{
		pszDest->AppendSubString(szProjectFile, szDot);
	}
	else
	{
		pszDest->Append(szProjectFile);
	}
	return pszDest->Append(".hcp65project")->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The licence a project's files are written with: its properties file's
// (<HCP65Project Licence="None"/>), or LGPL 3 if it has none.
//
//////////////////////////////////////////////////////////////////////////
char* ReadIDEProjectLicence(const char* szProjectFile, CChars* pszDest)
{
	CXMLFile	cFile;
	CFileUtil	cFileUtil;
	CChars		szPath;
	CChars		szName;
	CChars		szDirectory;
	CMarkupTag*	pcRoot;
	char*		szLicence;

	szPath.Init();
	szName.Init();
	szDirectory.Init();
	szLicence = NULL;
	AppendIDEProjectPropertiesFile(szProjectFile, &szPath);
	cFileUtil.SplitPath(szPath.Text(), &szName, &szDirectory);
	cFile.Init();
	if (cFileUtil.Exists(szPath.Text()) && cFile.Read(szName.Text(), szDirectory.Empty() ? "." : szDirectory.Text()) && cFile.mcMarkup.mpcDoc)
	{
		pcRoot = cFile.mcMarkup.mpcDoc->GetRootTag();
		if (pcRoot && (strcmp(pcRoot->GetName(), IDE_PROJECT_PROPERTIES_TAG) == 0))
		{
			szLicence = pcRoot->GetAttribute(IDE_PROJECT_LICENCE_ATTRIBUTE);
		}
	}
	pszDest->Append((szLicence && (szLicence[0] != '\0')) ? szLicence : IDE_LICENCE_DEFAULT);
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	szPath.Kill();
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// A whole number as two digits or more ("07").
//
//////////////////////////////////////////////////////////////////////////
static void SetIDETemplateNumber(CTemplateValues* pcDest, const char* szName, int iValue, bool bTwoDigits)
{
	CChars	sz;

	sz.Init();
	if (bTwoDigits && (iValue < 10))
	{
		sz.Append('0');
	}
	sz.Append(iValue);
	pcDest->Set(szName, sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// ${YEAR}, ${MONTH}, ${DAY}, ${HOUR}, ${MINUTE}, their names, ${DATE}
// (2026-11-16) and ${TIME} (14:05), now.
//
//////////////////////////////////////////////////////////////////////////
void SetIDETemplateDateVariables(CTemplateValues* pcDest)
{
	static const char*	aszMonths[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
	static const char*	aszDays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
	time_t				tNow;
	struct tm*			psNow;
	CChars				sz;

	tNow = time(NULL);
	psNow = localtime(&tNow);
	if (!psNow)
	{
		return;
	}
	SetIDETemplateNumber(pcDest, "YEAR", psNow->tm_year + 1900, false);
	SetIDETemplateNumber(pcDest, "MONTH", psNow->tm_mon + 1, true);
	SetIDETemplateNumber(pcDest, "DAY", psNow->tm_mday, true);
	SetIDETemplateNumber(pcDest, "HOUR", psNow->tm_hour, true);
	SetIDETemplateNumber(pcDest, "MINUTE", psNow->tm_min, true);
	pcDest->Set("MONTH_NAME_FULL", aszMonths[psNow->tm_mon]);
	pcDest->Set("DAY_NAME_FULL", aszDays[psNow->tm_wday]);
	sz.Init();
	sz.Append(aszMonths[psNow->tm_mon], 3);
	pcDest->Set("MONTH_NAME_SHORT", sz.Text());
	sz.Clear();
	sz.Append(aszDays[psNow->tm_wday], 3);
	pcDest->Set("DAY_NAME_SHORT", sz.Text());
	sz.Clear();
	sz.Append(psNow->tm_year + 1900)->Append('-')->Append((psNow->tm_mon + 1 < 10) ? "0" : "")->Append(psNow->tm_mon + 1)->Append('-')->Append((psNow->tm_mday < 10) ? "0" : "")->Append(psNow->tm_mday);
	pcDest->Set("DATE", sz.Text());
	sz.Clear();
	sz.Append((psNow->tm_hour < 10) ? "0" : "")->Append(psNow->tm_hour)->Append(':')->Append((psNow->tm_min < 10) ? "0" : "")->Append(psNow->tm_min);
	pcDest->Set("TIME", sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// ${AUTHOR}: git's user.name where the file is (the Settings field comes
// first when there is one, CodeTemplates.txt T4), or ${USER}.
//
//////////////////////////////////////////////////////////////////////////
static void SetIDETemplateAuthor(const char* szDirectory, const char* szUser, CTemplateValues* pcDest)
{
	CGitCommand	cGit;
	CArrayChars	aszArguments;
	CChars		szName;

	szName.Init();
	cGit.Init();
	aszArguments.Init();
	aszArguments.Add("config");
	aszArguments.Add("user.name");
	if (cGit.Run(szDirectory, &aszArguments) && cGit.Succeeded())
	{
		szName.Set(cGit.GetOutput()->Text());
		szName.StripWhitespace(true);
	}
	pcDest->Set("AUTHOR", szName.Empty() ? szUser : szName.Text());
	aszArguments.Kill();
	cGit.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The variables a new file's template is given: the file's ${NAME} (no
// extension), ${FILE_NAME}, ${HEADER_FILENAME}, ${INCLUDE_GUARD}
// (__KEG_H__) and ${DIRECTORY}; the class's ${CLASS_NAME} and
// ${CLASS_STEM} (CKeg, Keg); the project's ${PROJECT_NAME} and ${LICENCE};
// ${USER}, ${AUTHOR}, the date's, ${PRODUCT_NAME}, and
// ${NAMESPACES_OPEN} and ${NAMESPACES_CLOSE} (empty until namespaces are
// done).
//
//////////////////////////////////////////////////////////////////////////
void SetIDETemplateFileVariables(CIDEContent* pcContent, const char* szFileName, const char* szProjectOf, const char* szClassName, CTemplateValues* pcDest)
{
	CBuildWorkspace*	pcWorkspace;
	CVcxProject*		pcProject;
	CChars				szStem;
	CChars				szDirectory;
	CChars				sz;
	CChars				szLicence;
	const char*			szName;
	const char*			szDot;
	const char*			szUser;
	size				uiProject;

	szName = GetIDEFileNamePart(szFileName);
	szDot = strrchr(szName, '.');
	szStem.Init();
	if (szDot)
	{
		szStem.AppendSubString(szName, szDot);
	}
	else
	{
		szStem.Append(szName);
	}
	pcDest->Set("NAME", szStem.Text());
	pcDest->Set("FILE_NAME", szName);
	sz.Init(szStem);
	sz.Append(".h");
	pcDest->Set("HEADER_FILENAME", sz.Text());
	sz.Set("__");
	AppendTemplateUpperSnakeCase(szStem.Text(), &sz);
	sz.Append("_H__");
	pcDest->Set("INCLUDE_GUARD", sz.Text());
	szDirectory.Init();
	szDirectory.AppendSubString(szFileName, szName);
	pcDest->Set("DIRECTORY", szDirectory.Text());
	if (szClassName)
	{
		pcDest->Set("CLASS_NAME", szClassName);
		sz.Clear();
		AppendTemplateWithoutPrefix(szClassName, &sz);
		pcDest->Set("CLASS_STEM", sz.Text());
	}

	//The project's.
	szLicence.Init(IDE_LICENCE_DEFAULT);
	pcProject = NULL;
	pcWorkspace = pcContent->GetWorkspace();
	uiProject = pcContent->GetCodeModel()->FindProjectOfFile(szProjectOf ? szProjectOf : szFileName);
	if ((uiProject != MAX_SIZE) && (uiProject < pcWorkspace->NumProjects()))
	{
		pcProject = pcWorkspace->GetSolutionProject(uiProject)->GetProject();
	}
	if (pcProject)
	{
		pcDest->Set("PROJECT_NAME", pcProject->GetName());
		szLicence.Clear();
		ReadIDEProjectLicence(pcProject->GetFileName(), &szLicence);
	}
	pcDest->Set("LICENCE", szLicence.Text());

	szUser = getenv("USERNAME");
	szUser = szUser ? szUser : getenv("USER");
	szUser = szUser ? szUser : "";
	pcDest->Set("USER", szUser);
	if (pcContent->GetFileTemplates()->GetAuthor()[0] != '\0')
	{
		pcDest->Set("AUTHOR", pcContent->GetFileTemplates()->GetAuthor());
	}
	else if (!szLicence.Equals(IDE_LICENCE_NONE))
	{
		SetIDETemplateAuthor(szDirectory.Empty() ? "." : szDirectory.Text(), szUser, pcDest);
	}
	SetIDETemplateDateVariables(pcDest);
	pcDest->Set("PRODUCT_NAME", "HCP65 IDE");
	pcDest->Set("NAMESPACES_OPEN", "");
	pcDest->Set("NAMESPACES_CLOSE", "");
	szLicence.Kill();
	szDirectory.Kill();
	sz.Kill();
	szStem.Kill();
}
