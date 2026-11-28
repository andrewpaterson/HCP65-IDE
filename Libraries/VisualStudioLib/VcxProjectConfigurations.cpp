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
#include <string.h>
#include "BaseLib/TextFile.h"
#include "VcxProjectConfigurations.h"


#define VCX_CONFIGURATION_ENTRY		"<ProjectConfiguration Include=\""
#define VCX_CONFIGURATION_BOTH		"'$(Configuration)|$(Platform)'=='"
#define VCX_CONFIGURATION_NAME		"'$(Configuration)'=='"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool ReadVcxText(const char* szFileName, CChars* pszText)
{
	CTextFile	cFile;
	bool		bResult;

	cFile.Init();
	bResult = cFile.Read(szFileName);
	if (bResult)
	{
		pszText->AppendSubString(cFile.Text(), (size)cFile.Length());
	}
	cFile.Kill();
	return bResult;
}


static bool WriteVcxText(const char* szFileName, CChars* pszText)
{
	CTextFile	cFile;
	bool		bResult;

	cFile.Init(pszText->Text());
	bResult = cFile.Write(szFileName);
	cFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The first place at or after uiFrom where the text says an element is
// the configuration's: its entry's Include, or a Condition on it.  MAX_SIZE
// if there is none.
//
//////////////////////////////////////////////////////////////////////////
static size FindVcxConfigurationMark(const char* szText, size uiFrom, const char* szName)
{
	CChars		aszMarks[3];
	const char*	szFound;
	size		uiFirst;
	int			i;

	aszMarks[0].Init(VCX_CONFIGURATION_ENTRY);
	aszMarks[0].Append(szName)->Append('|');
	aszMarks[1].Init(VCX_CONFIGURATION_BOTH);
	aszMarks[1].Append(szName)->Append('|');
	aszMarks[2].Init(VCX_CONFIGURATION_NAME);
	aszMarks[2].Append(szName)->Append('\'');
	uiFirst = MAX_SIZE;
	for (i = 0; i < 3; i++)
	{
		szFound = strstr(&szText[uiFrom], aszMarks[i].Text());
		if (szFound && ((size)(szFound - szText) < uiFirst))
		{
			uiFirst = (size)(szFound - szText);
		}
		aszMarks[i].Kill();
	}
	return uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The lines of the element whose opening tag uiMark is in: from the start
// of that tag's line to the start of the line after its closing tag (or
// after the tag itself, if it closes itself).  False if the text isn't
// as a project's is there.
//
//////////////////////////////////////////////////////////////////////////
static bool FindVcxElementLines(const char* szText, size uiMark, size* puiStart, size* puiEnd)
{
	CChars		szClose;
	const char*	szFound;
	size		uiTag;
	size		uiTagEnd;
	size		uiName;
	size		uiEnd;

	uiTag = uiMark;
	while ((uiTag > 0) && (szText[uiTag] != '<'))
	{
		uiTag--;
	}
	szFound = strchr(&szText[uiMark], '>');
	if ((szText[uiTag] != '<') || !szFound)
	{
		return false;
	}
	uiTagEnd = (size)(szFound - szText);
	if (szText[uiTagEnd - 1] == '/')
	{
		uiEnd = uiTagEnd;
	}
	else
	{
		uiName = uiTag + 1;
		while (szText[uiName] && !strchr(" \t\r\n>", szText[uiName]))
		{
			uiName++;
		}
		szClose.Init("</");
		szClose.AppendSubString(szText, uiTag + 1, uiName);
		szClose.Append('>');
		szFound = strstr(&szText[uiTagEnd], szClose.Text());
		uiEnd = szFound ? (size)(szFound - szText) + szClose.Length() - 1 : MAX_SIZE;
		szClose.Kill();
		if (uiEnd == MAX_SIZE)
		{
			return false;
		}
	}
	while ((uiTag > 0) && (szText[uiTag - 1] != '\n'))
	{
		uiTag--;
	}
	while (szText[uiEnd] && (szText[uiEnd] != '\n'))
	{
		uiEnd++;
	}
	*puiStart = uiTag;
	*puiEnd = szText[uiEnd] ? uiEnd + 1 : uiEnd;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A copy of an element of szCopied's as szNew's: every mark of the one
// as the other's, and its entry's <Configuration>.
//
//////////////////////////////////////////////////////////////////////////
static void RenameVcxConfigurationLines(CChars* pszLines, const char* szNew, const char* szCopied)
{
	static const char*	aszStarts[] = { VCX_CONFIGURATION_ENTRY, VCX_CONFIGURATION_BOTH, VCX_CONFIGURATION_NAME, "<Configuration>", NULL };
	static const char*	aszEnds[] = { "|", "|", "'", "<", NULL };
	CChars				szOld;
	CChars				szReplacement;
	int					i;

	for (i = 0; aszStarts[i] != NULL; i++)
	{
		szOld.Init(aszStarts[i]);
		szOld.Append(szCopied)->Append(aszEnds[i]);
		szReplacement.Init(aszStarts[i]);
		szReplacement.Append(szNew)->Append(aszEnds[i]);
		pszLines->Replace(szOld.Text(), szReplacement.Text());
		szReplacement.Kill();
		szOld.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool AddVcxProjectConfiguration(const char* szFileName, const char* szNew, const char* szCopied)
{
	CChars	szText;
	CChars	szLines;
	size	uiAt;
	size	uiStart;
	size	uiEnd;
	bool	bResult;

	szText.Init();
	if (!ReadVcxText(szFileName, &szText) || (FindVcxConfigurationMark(szText.Text(), 0, szCopied) == MAX_SIZE) || (FindVcxConfigurationMark(szText.Text(), 0, szNew) != MAX_SIZE))
	{
		szText.Kill();
		return false;
	}

	bResult = true;
	szLines.Init();
	for (uiAt = FindVcxConfigurationMark(szText.Text(), 0, szCopied); bResult && (uiAt != MAX_SIZE); uiAt = FindVcxConfigurationMark(szText.Text(), uiAt, szCopied))
	{
		bResult = FindVcxElementLines(szText.Text(), uiAt, &uiStart, &uiEnd);
		if (bResult)
		{
			szLines.Set("");
			szLines.AppendSubString(szText.Text(), uiStart, uiEnd);
			if (!szLines.EndsWith('\n'))
			{
				szLines.Insert(0, szText.Contains("\r\n") ? "\r\n" : "\n");
			}
			RenameVcxConfigurationLines(&szLines, szNew, szCopied);
			szText.Insert(uiEnd, szLines.Text());
			uiAt = uiEnd + szLines.Length();
		}
	}
	szLines.Kill();
	bResult = bResult && WriteVcxText(szFileName, &szText);
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool RemoveVcxProjectConfiguration(const char* szFileName, const char* szName)
{
	CChars	szText;
	size	uiAt;
	size	uiStart;
	size	uiEnd;
	bool	bResult;

	szText.Init();
	if (!ReadVcxText(szFileName, &szText) || (FindVcxConfigurationMark(szText.Text(), 0, szName) == MAX_SIZE))
	{
		szText.Kill();
		return false;
	}
	bResult = true;
	for (uiAt = FindVcxConfigurationMark(szText.Text(), 0, szName); bResult && (uiAt != MAX_SIZE); uiAt = FindVcxConfigurationMark(szText.Text(), uiAt, szName))
	{
		bResult = FindVcxElementLines(szText.Text(), uiAt, &uiStart, &uiEnd);
		if (bResult)
		{
			szText.Remove(uiStart, uiEnd);
			uiAt = uiStart;
		}
	}
	bResult = bResult && WriteVcxText(szFileName, &szText);
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool GetVcxProjectConfigurationNames(const char* szFileName, CArrayChars* paszDest)
{
	CChars		szText;
	CChars		szName;
	const char*	szAt;
	const char*	szBar;

	szText.Init();
	if (!ReadVcxText(szFileName, &szText))
	{
		szText.Kill();
		return false;
	}
	for (szAt = strstr(szText.Text(), VCX_CONFIGURATION_ENTRY); szAt; szAt = strstr(szAt, VCX_CONFIGURATION_ENTRY))
	{
		szAt += strlen(VCX_CONFIGURATION_ENTRY);
		szBar = strchr(szAt, '|');
		if (szBar)
		{
			szName.Init();
			szName.AppendSubString(szAt, szBar);
			paszDest->AddIfUnique(szName.Text());
			szName.Kill();
		}
	}
	szText.Kill();
	return true;
}
