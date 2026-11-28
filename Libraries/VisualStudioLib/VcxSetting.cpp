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
#include "VcxSetting.h"


#define VCX_SETTING_GROUP		"<ItemDefinitionGroup"
#define VCX_SETTING_GROUP_END	"</ItemDefinitionGroup>"
#define VCX_SETTING_INDENT		"  "


//Where a setting is in a file's text, as far as it is there: each part's start and the end of what is inside it.
struct SVcxSettingPlace
{
	bool	bGroup;
	size	uiGroup;		//The '<' of the group's opening tag...
	size	uiGroupEnd;		//...and of its closing tag.
	bool	bTool;
	size	uiTool;
	size	uiToolEnd;
	bool	bSetting;
	size	uiSetting;		//The '<' of <Name>...
	size	uiValue;		//...its value...
	size	uiValueEnd;		//...and the '<' of </Name>.
};


//////////////////////////////////////////////////////////////////////////
//
// Text as an element's: & < and > as their entities.
//
//////////////////////////////////////////////////////////////////////////
static void EscapeVcxValue(const char* szValue, CChars* pszDest)
{
	pszDest->Set(szValue);
	pszDest->Replace("&", "&amp;");
	pszDest->Replace("<", "&lt;");
	pszDest->Replace(">", "&gt;");
}


//////////////////////////////////////////////////////////////////////////
//
// An element's text as it was written: its entities as their characters.
//
//////////////////////////////////////////////////////////////////////////
static void UnescapeVcxValue(CChars* pszValue)
{
	pszValue->Replace("&lt;", "<");
	pszValue->Replace("&gt;", ">");
	pszValue->Replace("&quot;", "\"");
	pszValue->Replace("&apos;", "'");
	pszValue->Replace("&amp;", "&");
}


//////////////////////////////////////////////////////////////////////////
//
// The first of szFind in szText from uiFrom and before uiBefore; MAX_SIZE
// if there is none.
//
//////////////////////////////////////////////////////////////////////////
static size FindVcxText(const char* szText, const char* szFind, size uiFrom, size uiBefore)
{
	const char*	szFound;

	szFound = strstr(&szText[uiFrom], szFind);
	return (szFound && ((size)(szFound - szText) < uiBefore)) ? (size)(szFound - szText) : MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line uiAt is in.
//
//////////////////////////////////////////////////////////////////////////
static size StartOfVcxLine(const char* szText, size uiAt)
{
	while ((uiAt > 0) && (szText[uiAt - 1] != '\n'))
	{
		uiAt--;
	}
	return uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line after the one uiAt is in.
//
//////////////////////////////////////////////////////////////////////////
static size AfterVcxLine(const char* szText, size uiAt)
{
	while ((szText[uiAt] != '\0') && (szText[uiAt] != '\n'))
	{
		uiAt++;
	}
	return (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// The indent of the line uiAt is in: its spaces and tabs.
//
//////////////////////////////////////////////////////////////////////////
static void GetVcxIndent(const char* szText, size uiAt, CChars* pszDest)
{
	size	uiStart;
	size	uiEnd;

	uiStart = StartOfVcxLine(szText, uiAt);
	uiEnd = uiStart;
	while ((szText[uiEnd] == ' ') || (szText[uiEnd] == '\t'))
	{
		uiEnd++;
	}
	pszDest->Set("");
	pszDest->AppendSubString(szText, uiStart, uiEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the group whose opening tag is at uiTag has szCondition for its
// Condition (none, if szCondition is empty).
//
//////////////////////////////////////////////////////////////////////////
static bool IsVcxGroup(const char* szText, size uiTag, const char* szCondition)
{
	CChars		szValue;
	size		uiEnd;
	size		uiAttribute;
	size		uiClose;
	bool		bResult;

	uiEnd = FindVcxText(szText, ">", uiTag, MAX_SIZE);
	if (uiEnd == MAX_SIZE)
	{
		return false;
	}
	uiAttribute = FindVcxText(szText, "Condition=\"", uiTag, uiEnd);
	if (uiAttribute == MAX_SIZE)
	{
		return szCondition[0] == '\0';
	}
	uiAttribute += 11;
	uiClose = FindVcxText(szText, "\"", uiAttribute, uiEnd);
	if (uiClose == MAX_SIZE)
	{
		return false;
	}
	szValue.Init();
	szValue.AppendSubString(szText, uiAttribute, uiClose);
	UnescapeVcxValue(&szValue);
	bResult = szValue.Equals(szCondition);
	szValue.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the setting is in one group (psPlace's): its tool in that and
// itself in that, as far as each is there.
//
//////////////////////////////////////////////////////////////////////////
static void FindVcxSettingInGroup(const char* szText, const char* szTool, const char* szName, SVcxSettingPlace* psPlace)
{
	CChars	szOpen;
	CChars	szClose;

	szOpen.Init("<");
	szOpen.Append(szTool)->Append('>');
	szClose.Init("</");
	szClose.Append(szTool)->Append('>');
	psPlace->bSetting = false;
	psPlace->uiTool = FindVcxText(szText, szOpen.Text(), psPlace->uiGroup, psPlace->uiGroupEnd);
	psPlace->uiToolEnd = (psPlace->uiTool != MAX_SIZE) ? FindVcxText(szText, szClose.Text(), psPlace->uiTool, psPlace->uiGroupEnd) : MAX_SIZE;
	psPlace->bTool = psPlace->uiToolEnd != MAX_SIZE;
	if (psPlace->bTool)
	{
		szOpen.Set("<");
		szOpen.Append(szName)->Append('>');
		szClose.Set("</");
		szClose.Append(szName)->Append('>');
		psPlace->uiSetting = FindVcxText(szText, szOpen.Text(), psPlace->uiTool, psPlace->uiToolEnd);
		psPlace->uiValue = psPlace->uiSetting + szOpen.Length();
		psPlace->uiValueEnd = (psPlace->uiSetting != MAX_SIZE) ? FindVcxText(szText, szClose.Text(), psPlace->uiSetting, psPlace->uiToolEnd) : MAX_SIZE;
		psPlace->bSetting = psPlace->uiValueEnd != MAX_SIZE;
	}
	szClose.Kill();
	szOpen.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where the setting is in the text: its group, its tool in that and
// itself in that, as far as each is there.  A file can have several
// groups with one condition (a project's own beside one Visual Studio
// wrote): the group is the last that has the setting, which is the one
// read last; else the first that has its tool; else the first.
//
//////////////////////////////////////////////////////////////////////////
static void FindVcxSetting(const char* szText, const char* szCondition, const char* szTool, const char* szName, SVcxSettingPlace* psPlace)
{
	SVcxSettingPlace	sGroup;
	size				uiAt;
	size				uiEnd;

	memset(psPlace, 0, sizeof(SVcxSettingPlace));
	for (uiAt = FindVcxText(szText, VCX_SETTING_GROUP, 0, MAX_SIZE); uiAt != MAX_SIZE; uiAt = FindVcxText(szText, VCX_SETTING_GROUP, uiAt + 1, MAX_SIZE))
	{
		uiEnd = FindVcxText(szText, VCX_SETTING_GROUP_END, uiAt, MAX_SIZE);
		if ((uiEnd != MAX_SIZE) && IsVcxGroup(szText, uiAt, szCondition))
		{
			memset(&sGroup, 0, sizeof(SVcxSettingPlace));
			sGroup.bGroup = true;
			sGroup.uiGroup = uiAt;
			sGroup.uiGroupEnd = uiEnd;
			FindVcxSettingInGroup(szText, szTool, szName, &sGroup);
			if (!psPlace->bGroup || sGroup.bSetting || (sGroup.bTool && !psPlace->bTool))
			{
				*psPlace = sGroup;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether there is only white space from uiFrom to uiTo.
//
//////////////////////////////////////////////////////////////////////////
static bool IsVcxBlank(const char* szText, size uiFrom, size uiTo)
{
	for (; uiFrom < uiTo; uiFrom++)
	{
		if (!strchr(" \t\r\n", szText[uiFrom]))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The setting's lines taken out of the text, and its tool's and its
// group's if that leaves them with nothing in them.
//
//////////////////////////////////////////////////////////////////////////
static void RemoveVcxSetting(CChars* pszText, const char* szCondition, const char* szTool, const char* szName)
{
	SVcxSettingPlace	sPlace;
	const char*			szText;

	szText = pszText->Text();
	FindVcxSetting(szText, szCondition, szTool, szName, &sPlace);
	pszText->Remove(StartOfVcxLine(szText, sPlace.uiSetting), AfterVcxLine(szText, sPlace.uiValueEnd));

	szText = pszText->Text();
	FindVcxSetting(szText, szCondition, szTool, szName, &sPlace);
	if (sPlace.bTool && IsVcxBlank(szText, FindVcxText(szText, ">", sPlace.uiTool, MAX_SIZE) + 1, sPlace.uiToolEnd))
	{
		pszText->Remove(StartOfVcxLine(szText, sPlace.uiTool), AfterVcxLine(szText, sPlace.uiToolEnd));
		szText = pszText->Text();
		FindVcxSetting(szText, szCondition, szTool, szName, &sPlace);
		if (sPlace.bGroup && IsVcxBlank(szText, FindVcxText(szText, ">", sPlace.uiGroup, MAX_SIZE) + 1, sPlace.uiGroupEnd))
		{
			pszText->Remove(StartOfVcxLine(szText, sPlace.uiGroup), AfterVcxLine(szText, sPlace.uiGroupEnd));
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The setting written into the text, with whatever of its tool and its
// group isn't there (see the header).  False if there is nowhere for a
// group to go: the text isn't a project's.
//
//////////////////////////////////////////////////////////////////////////
static bool AddVcxSetting(CChars* pszText, const char* szCondition, const char* szTool, const char* szName, const char* szValue, const char* szNewLine)
{
	SVcxSettingPlace	sPlace;
	const char*			szText;
	CChars				szIndent;
	CChars				szLines;
	CChars				szEscaped;
	size				uiAt;
	size				uiLast;

	szText = pszText->Text();
	FindVcxSetting(szText, szCondition, szTool, szName, &sPlace);
	szIndent.Init();
	szLines.Init();
	if (sPlace.bTool)
	{
		uiAt = StartOfVcxLine(szText, sPlace.uiToolEnd);
		GetVcxIndent(szText, sPlace.uiToolEnd, &szIndent);
		szIndent.Append(VCX_SETTING_INDENT);
	}
	else if (sPlace.bGroup)
	{
		uiAt = StartOfVcxLine(szText, sPlace.uiGroupEnd);
		GetVcxIndent(szText, sPlace.uiGroupEnd, &szIndent);
		szIndent.Append(VCX_SETTING_INDENT);
	}
	else
	{
		//After the last group there is; else before the first group of items that aren't the configurations, or the end.
		uiLast = MAX_SIZE;
		for (uiAt = FindVcxText(szText, VCX_SETTING_GROUP_END, 0, MAX_SIZE); uiAt != MAX_SIZE; uiAt = FindVcxText(szText, VCX_SETTING_GROUP_END, uiAt + 1, MAX_SIZE))
		{
			uiLast = uiAt;
		}
		if (uiLast != MAX_SIZE)
		{
			uiAt = AfterVcxLine(szText, uiLast);
		}
		else
		{
			uiAt = FindVcxText(szText, "<ItemGroup>", 0, MAX_SIZE);
			if (uiAt == MAX_SIZE)
			{
				uiAt = FindVcxText(szText, "</Project>", 0, MAX_SIZE);
			}
			if (uiAt == MAX_SIZE)
			{
				szLines.Kill();
				szIndent.Kill();
				return false;
			}
			uiAt = StartOfVcxLine(szText, uiAt);
		}
		szIndent.Set(VCX_SETTING_INDENT);
	}

	szEscaped.Init();
	if (!sPlace.bGroup)
	{
		EscapeVcxValue(szCondition, &szEscaped);
		szLines.Append(szIndent)->Append(VCX_SETTING_GROUP);
		if (szCondition[0] != '\0')
		{
			szLines.Append(" Condition=\"")->Append(szEscaped)->Append('"');
		}
		szLines.Append('>')->Append(szNewLine);
		szIndent.Append(VCX_SETTING_INDENT);
	}
	if (!sPlace.bTool)
	{
		szLines.Append(szIndent)->Append('<')->Append(szTool)->Append('>')->Append(szNewLine);
		szIndent.Append(VCX_SETTING_INDENT);
	}
	EscapeVcxValue(szValue, &szEscaped);
	szLines.Append(szIndent)->Append('<')->Append(szName)->Append('>')->Append(szEscaped)->Append("</")->Append(szName)->Append('>')->Append(szNewLine);
	if (!sPlace.bTool)
	{
		szIndent.RemoveFromEnd(strlen(VCX_SETTING_INDENT));
		szLines.Append(szIndent)->Append("</")->Append(szTool)->Append('>')->Append(szNewLine);
	}
	if (!sPlace.bGroup)
	{
		szIndent.RemoveFromEnd(strlen(VCX_SETTING_INDENT));
		szLines.Append(szIndent)->Append(VCX_SETTING_GROUP_END)->Append(szNewLine);
	}
	pszText->Insert(uiAt, szLines.Text());
	szEscaped.Kill();
	szLines.Kill();
	szIndent.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool SetVcxSetting(const char* szFileName, const char* szCondition, const char* szTool, const char* szName, const char* szValue)
{
	SVcxSettingPlace	sPlace;
	CTextFile			cFile;
	CChars				szText;
	CChars				szEscaped;
	CChars				szOld;
	bool				bResult;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	szText.Init();
	szText.AppendSubString(cFile.Text(), (size)cFile.Length());
	cFile.Kill();
	if (szCondition == NULL)
	{
		szCondition = "";
	}

	bResult = true;
	FindVcxSetting(szText.Text(), szCondition, szTool, szName, &sPlace);
	szEscaped.Init();
	szOld.Init();
	if (sPlace.bSetting)
	{
		szOld.AppendSubString(szText.Text(), sPlace.uiValue, sPlace.uiValueEnd);
	}
	if (szValue == NULL)
	{
		if (!sPlace.bSetting)
		{
			szOld.Kill();
			szEscaped.Kill();
			szText.Kill();
			return true;
		}
		RemoveVcxSetting(&szText, szCondition, szTool, szName);
	}
	else
	{
		EscapeVcxValue(szValue, &szEscaped);
		if (sPlace.bSetting && szOld.Equals(szEscaped))
		{
			szOld.Kill();
			szEscaped.Kill();
			szText.Kill();
			return true;
		}
		if (sPlace.bSetting)
		{
			szText.Remove(sPlace.uiValue, sPlace.uiValueEnd);
			szText.Insert(sPlace.uiValue, szEscaped.Text());
		}
		else
		{
			bResult = AddVcxSetting(&szText, szCondition, szTool, szName, szValue, szText.Contains("\r\n") ? "\r\n" : "\n");
		}
	}
	szOld.Kill();
	szEscaped.Kill();

	if (bResult)
	{
		cFile.Init(szText.Text());
		bResult = cFile.Write(szFileName);
		cFile.Kill();
	}
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool GetVcxSetting(const char* szFileName, const char* szCondition, const char* szTool, const char* szName, CChars* pszValue)
{
	SVcxSettingPlace	sPlace;
	CTextFile			cFile;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	FindVcxSetting(cFile.Text(), szCondition ? szCondition : "", szTool, szName, &sPlace);
	if (sPlace.bSetting)
	{
		pszValue->Set("");
		pszValue->AppendSubString(cFile.Text(), sPlace.uiValue, sPlace.uiValueEnd);
		UnescapeVcxValue(pszValue);
	}
	cFile.Kill();
	return sPlace.bSetting;
}
