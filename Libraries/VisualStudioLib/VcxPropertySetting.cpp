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
#include "VcxPropertySetting.h"


#define VCX_PROPERTY_GROUP		"<PropertyGroup"
#define VCX_PROPERTY_GROUP_END	"</PropertyGroup>"
#define VCX_PROPERTY_INDENT		"  "


//A property group in a file's text, and a property in it.
struct SVcxPropertyPlace
{
	bool	bGroup;
	size	uiGroup;		//The '<' of the group's opening tag,
	size	uiGroupTagEnd;	//the character after its '>',
	size	uiGroupEnd;		//and the '<' of its closing tag.
	bool	bProperty;
	size	uiProperty;		//The '<' of <Name ...>,
	size	uiValue;		//its value,
	size	uiValueEnd;		//and the '<' of </Name>.
};


static void EscapeVcxPropertyValue(const char* szValue, CChars* pszDest)
{
	pszDest->Set(szValue);
	pszDest->Replace("&", "&amp;");
	pszDest->Replace("<", "&lt;");
	pszDest->Replace(">", "&gt;");
}


static void UnescapeVcxPropertyValue(CChars* pszValue)
{
	pszValue->Replace("&lt;", "<");
	pszValue->Replace("&gt;", ">");
	pszValue->Replace("&quot;", "\"");
	pszValue->Replace("&apos;", "'");
	pszValue->Replace("&amp;", "&");
}


//////////////////////////////////////////////////////////////////////////
//
// The value of the attribute szAttribute in the opening tag from uiTag to
// uiTagEnd, its entities as their characters; empty if it has none.
//
//////////////////////////////////////////////////////////////////////////
static void GetVcxAttribute(const char* szText, size uiTag, size uiTagEnd, const char* szAttribute, CChars* pszDest)
{
	CChars		szFind;
	const char*	szFound;
	const char*	szEnd;

	pszDest->Set("");
	szFind.Init(" ");
	szFind.Append(szAttribute)->Append("=\"");
	szFound = strstr(&szText[uiTag], szFind.Text());
	if (szFound && ((size)(szFound - szText) < uiTagEnd))
	{
		szFound += szFind.Length();
		szEnd = strchr(szFound, '"');
		if (szEnd && ((size)(szEnd - szText) < uiTagEnd))
		{
			pszDest->AppendSubString(szFound, szEnd);
			UnescapeVcxPropertyValue(pszDest);
		}
	}
	szFind.Kill();
}


static size StartOfVcxPropertyLine(const char* szText, size uiAt)
{
	while ((uiAt > 0) && (szText[uiAt - 1] != '\n'))
	{
		uiAt--;
	}
	return uiAt;
}


static size AfterVcxPropertyLine(const char* szText, size uiAt)
{
	while ((szText[uiAt] != '\0') && (szText[uiAt] != '\n'))
	{
		uiAt++;
	}
	return (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
}


static void GetVcxPropertyIndent(const char* szText, size uiAt, CChars* pszDest)
{
	size	uiStart;
	size	uiEnd;

	uiStart = StartOfVcxPropertyLine(szText, uiAt);
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
// The property group whose opening tag is at or after uiFrom: false if
// there is none.  One written <PropertyGroup ... /> has nothing in it.
//
//////////////////////////////////////////////////////////////////////////
static bool FindVcxPropertyGroup(const char* szText, size uiFrom, SVcxPropertyPlace* psPlace)
{
	const char*	szGroup;
	const char*	szTagEnd;
	const char*	szEnd;
	char		cAfter;

	psPlace->bGroup = false;
	szGroup = strstr(&szText[uiFrom], VCX_PROPERTY_GROUP);
	while (szGroup)
	{
		cAfter = szGroup[strlen(VCX_PROPERTY_GROUP)];
		szTagEnd = strchr(szGroup, '>');
		if (!szTagEnd)
		{
			return false;
		}
		if ((cAfter == ' ') || (cAfter == '>') || (cAfter == '/'))
		{
			psPlace->uiGroup = (size)(szGroup - szText);
			psPlace->uiGroupTagEnd = (size)(szTagEnd + 1 - szText);
			if (szTagEnd[-1] == '/')
			{
				psPlace->uiGroupEnd = psPlace->uiGroupTagEnd;
			}
			else
			{
				szEnd = strstr(szTagEnd, VCX_PROPERTY_GROUP_END);
				if (!szEnd)
				{
					return false;
				}
				psPlace->uiGroupEnd = (size)(szEnd - szText);
			}
			psPlace->bGroup = true;
			return true;
		}
		szGroup = strstr(szGroup + 1, VCX_PROPERTY_GROUP);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a group has an element in it.
//
//////////////////////////////////////////////////////////////////////////
static bool HasVcxPropertyElements(const char* szText, SVcxPropertyPlace* psGroup)
{
	const char*	szElement;

	szElement = strchr(&szText[psGroup->uiGroupTagEnd], '<');
	return szElement && ((size)(szElement - szText) < psGroup->uiGroupEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// Where the file says the property for szCondition: the last element
// szName whose own Condition is szCondition, or that has none in a group
// whose Condition is.  bProperty is false if nothing says it.
//
//////////////////////////////////////////////////////////////////////////
static void FindVcxProperty(const char* szText, const char* szCondition, const char* szName, SVcxPropertyPlace* psPlace)
{
	SVcxPropertyPlace	sGroup;
	CChars				szOpen;
	CChars				szClose;
	CChars				szGroupCondition;
	CChars				szOwnCondition;
	const char*			szElement;
	const char*			szTagEnd;
	const char*			szValueEnd;
	size				uiFrom;
	char				cAfter;

	psPlace->bGroup = false;
	psPlace->bProperty = false;
	szOpen.Init("<");
	szOpen.Append(szName);
	szClose.Init("</");
	szClose.Append(szName)->Append('>');
	szGroupCondition.Init();
	szOwnCondition.Init();
	uiFrom = 0;
	while (FindVcxPropertyGroup(szText, uiFrom, &sGroup))
	{
		GetVcxAttribute(szText, sGroup.uiGroup, sGroup.uiGroupTagEnd, "Condition", &szGroupCondition);
		szElement = strstr(&szText[sGroup.uiGroupTagEnd], szOpen.Text());
		while (szElement && ((size)(szElement - szText) < sGroup.uiGroupEnd))
		{
			cAfter = szElement[szOpen.Length()];
			szTagEnd = strchr(szElement, '>');
			szValueEnd = szTagEnd ? strstr(szTagEnd, szClose.Text()) : NULL;
			if (((cAfter == ' ') || (cAfter == '>')) && szValueEnd && ((size)(szValueEnd - szText) < sGroup.uiGroupEnd))
			{
				GetVcxAttribute(szText, (size)(szElement - szText), (size)(szTagEnd + 1 - szText), "Condition", &szOwnCondition);
				if (szOwnCondition.Empty() ? szGroupCondition.Equals(szCondition) : szOwnCondition.Equals(szCondition))
				{
					*psPlace = sGroup;
					psPlace->bProperty = true;
					psPlace->uiProperty = (size)(szElement - szText);
					psPlace->uiValue = (size)(szTagEnd + 1 - szText);
					psPlace->uiValueEnd = (size)(szValueEnd - szText);
				}
			}
			szElement = strstr(szElement + 1, szOpen.Text());
		}
		uiFrom = sGroup.uiGroupTagEnd;
	}
	szOwnCondition.Kill();
	szGroupCondition.Kill();
	szClose.Kill();
	szOpen.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The property's line taken out, and its group's lines too if that leaves
// it empty and it has no Label.
//
//////////////////////////////////////////////////////////////////////////
static void RemoveVcxProperty(CChars* pszText, SVcxPropertyPlace* psPlace, const char* szName)
{
	SVcxPropertyPlace	sGroup;
	CChars				szLabel;
	size				uiStart;
	size				uiEnd;

	uiStart = StartOfVcxPropertyLine(pszText->Text(), psPlace->uiProperty);
	uiEnd = AfterVcxPropertyLine(pszText->Text(), psPlace->uiValueEnd);
	pszText->Remove(uiStart, uiEnd);

	if (FindVcxPropertyGroup(pszText->Text(), psPlace->uiGroup, &sGroup) && (sGroup.uiGroup == psPlace->uiGroup) && !HasVcxPropertyElements(pszText->Text(), &sGroup))
	{
		szLabel.Init();
		GetVcxAttribute(pszText->Text(), sGroup.uiGroup, sGroup.uiGroupTagEnd, "Label", &szLabel);
		if (szLabel.Empty())
		{
			pszText->Remove(StartOfVcxPropertyLine(pszText->Text(), sGroup.uiGroup), AfterVcxPropertyLine(pszText->Text(), sGroup.uiGroupEnd));
		}
		szLabel.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The property added where the header says one that isn't there goes.
// False if it is a configuration's own and the file has no Configuration
// group for szCondition, or the file isn't a project's.
//
//////////////////////////////////////////////////////////////////////////
static bool AddVcxProperty(CChars* pszText, const char* szCondition, const char* szName, const char* szValue, bool bConfiguration, const char* szNewLine)
{
	SVcxPropertyPlace	sGroup;
	SVcxPropertyPlace	sFound;
	CChars				szAttribute;
	CChars				szLabel;
	CChars				szEscaped;
	CChars				szCondEscaped;
	CChars				szIndent;
	CChars				szLine;
	const char*			szAt;
	size				uiFrom;
	size				uiAt;
	bool				bOwnCondition;

	sFound.bGroup = false;
	szAttribute.Init();
	szLabel.Init();
	uiFrom = 0;
	while (FindVcxPropertyGroup(pszText->Text(), uiFrom, &sGroup))
	{
		GetVcxAttribute(pszText->Text(), sGroup.uiGroup, sGroup.uiGroupTagEnd, "Condition", &szAttribute);
		GetVcxAttribute(pszText->Text(), sGroup.uiGroup, sGroup.uiGroupTagEnd, "Label", &szLabel);
		if (sGroup.uiGroupEnd != sGroup.uiGroupTagEnd)
		{
			if (bConfiguration ? (szLabel.Equals("Configuration") && szAttribute.Equals(szCondition)) : (szLabel.Empty() && szAttribute.Empty()))
			{
				sFound = sGroup;
			}
		}
		uiFrom = sGroup.uiGroupTagEnd;
	}
	szLabel.Kill();
	szAttribute.Kill();
	if (!sFound.bGroup && bConfiguration)
	{
		return false;
	}

	szEscaped.Init();
	EscapeVcxPropertyValue(szValue, &szEscaped);
	szCondEscaped.Init();
	EscapeVcxPropertyValue(szCondition, &szCondEscaped);
	szCondEscaped.Replace("\"", "&quot;");
	bOwnCondition = !bConfiguration && (szCondition[0] != '\0');
	szIndent.Init();
	szLine.Init();
	if (sFound.bGroup)
	{
		//Last in the group: before its closing tag's line, a level in from it.
		GetVcxPropertyIndent(pszText->Text(), sFound.uiGroup, &szIndent);
		uiAt = StartOfVcxPropertyLine(pszText->Text(), sFound.uiGroupEnd);
		szLine.Append(szIndent)->Append(VCX_PROPERTY_INDENT)->Append("<")->Append(szName);
	}
	else
	{
		szAt = strstr(pszText->Text(), "<ItemDefinitionGroup");
		if (!szAt)
		{
			szAt = strstr(pszText->Text(), "<ItemGroup>");
		}
		if (!szAt)
		{
			szAt = strstr(pszText->Text(), "</Project>");
		}
		if (!szAt)
		{
			szLine.Kill();
			szIndent.Kill();
			szCondEscaped.Kill();
			szEscaped.Kill();
			return false;
		}
		uiAt = StartOfVcxPropertyLine(pszText->Text(), (size)(szAt - pszText->Text()));
		szIndent.Set((strncmp(szAt, "</Project>", 10) == 0) ? VCX_PROPERTY_INDENT : "");
		if (szIndent.Empty())
		{
			GetVcxPropertyIndent(pszText->Text(), (size)(szAt - pszText->Text()), &szIndent);
		}
		szLine.Append(szIndent)->Append(VCX_PROPERTY_GROUP)->Append(">")->Append(szNewLine);
		szLine.Append(szIndent)->Append(VCX_PROPERTY_INDENT)->Append("<")->Append(szName);
	}
	if (bOwnCondition)
	{
		szLine.Append(" Condition=\"")->Append(szCondEscaped)->Append("\"");
	}
	szLine.Append(">")->Append(szEscaped)->Append("</")->Append(szName)->Append(">")->Append(szNewLine);
	if (!sFound.bGroup)
	{
		szLine.Append(szIndent)->Append(VCX_PROPERTY_GROUP_END)->Append(szNewLine);
	}
	pszText->Insert(uiAt, szLine.Text());
	szLine.Kill();
	szIndent.Kill();
	szCondEscaped.Kill();
	szEscaped.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool SetVcxProperty(const char* szFileName, const char* szCondition, const char* szName, const char* szValue, bool bConfiguration)
{
	SVcxPropertyPlace	sPlace;
	CTextFile			cFile;
	CChars				szText;
	CChars				szEscaped;
	CChars				szOld;
	bool				bResult;
	bool				bWrite;

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
	bWrite = true;
	FindVcxProperty(szText.Text(), szCondition, szName, &sPlace);
	szEscaped.Init();
	szOld.Init();
	if (sPlace.bProperty)
	{
		szOld.AppendSubString(szText.Text(), sPlace.uiValue, sPlace.uiValueEnd);
	}
	if (szValue == NULL)
	{
		bWrite = sPlace.bProperty;
		if (sPlace.bProperty)
		{
			RemoveVcxProperty(&szText, &sPlace, szName);
		}
	}
	else
	{
		EscapeVcxPropertyValue(szValue, &szEscaped);
		if (sPlace.bProperty && szOld.Equals(szEscaped))
		{
			bWrite = false;
		}
		else if (sPlace.bProperty)
		{
			szText.Remove(sPlace.uiValue, sPlace.uiValueEnd);
			szText.Insert(sPlace.uiValue, szEscaped.Text());
		}
		else
		{
			bResult = AddVcxProperty(&szText, szCondition, szName, szValue, bConfiguration, szText.Contains("\r\n") ? "\r\n" : "\n");
		}
	}
	szOld.Kill();
	szEscaped.Kill();

	if (bResult && bWrite)
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
bool GetVcxProperty(const char* szFileName, const char* szCondition, const char* szName, CChars* pszValue)
{
	SVcxPropertyPlace	sPlace;
	CTextFile			cFile;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	FindVcxProperty(cFile.Text(), szCondition ? szCondition : "", szName, &sPlace);
	if (sPlace.bProperty)
	{
		pszValue->Set("");
		pszValue->AppendSubString(cFile.Text(), sPlace.uiValue, sPlace.uiValueEnd);
		UnescapeVcxPropertyValue(pszValue);
	}
	cFile.Kill();
	return sPlace.bProperty;
}

