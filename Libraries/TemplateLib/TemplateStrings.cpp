/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela TemplateLib

Codaphela TemplateLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela TemplateLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela TemplateLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "TemplateStrings.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsTemplateNameChar(char c)
{
	return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}


//////////////////////////////////////////////////////////////////////////
//
// A name's words with an underscore between them, in capitals or not.
//
//////////////////////////////////////////////////////////////////////////
static char* AppendTemplateSnakeCase(const char* szText, bool bUpper, CChars* pszDest)
{
	size	i;
	char	c;
	bool	bIsUpper;
	bool	bPreviousLower;
	bool	bPreviousUpper;
	bool	bNextLower;

	for (i = 0; szText[i] != '\0'; i++)
	{
		c = szText[i];
		bIsUpper = (c >= 'A') && (c <= 'Z');
		bPreviousLower = (i > 0) && (((szText[i - 1] >= 'a') && (szText[i - 1] <= 'z')) || ((szText[i - 1] >= '0') && (szText[i - 1] <= '9')));
		bPreviousUpper = (i > 0) && (szText[i - 1] >= 'A') && (szText[i - 1] <= 'Z');
		bNextLower = (szText[i + 1] >= 'a') && (szText[i + 1] <= 'z');
		if (bIsUpper && (bPreviousLower || (bPreviousUpper && bNextLower)))
		{
			pszDest->Append('_');
		}
		if (!IsTemplateNameChar(c))
		{
			pszDest->Append('_');
		}
		else if (bUpper && (c >= 'a') && (c <= 'z'))
		{
			pszDest->Append((char)(c - 'a' + 'A'));
		}
		else if (!bUpper && bIsUpper)
		{
			pszDest->Append((char)(c - 'A' + 'a'));
		}
		else
		{
			pszDest->Append(c);
		}
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* AppendTemplateUpperSnakeCase(const char* szText, CChars* pszDest)
{
	return AppendTemplateSnakeCase(szText, true, pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* AppendTemplateLowerSnakeCase(const char* szText, CChars* pszDest)
{
	return AppendTemplateSnakeCase(szText, false, pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* AppendTemplateWithoutPrefix(const char* szText, CChars* pszDest)
{
	if (((szText[0] == 'C') || (szText[0] == 'S') || (szText[0] == 'E')) && (szText[1] >= 'A') && (szText[1] <= 'Z'))
	{
		pszDest->Append(&szText[1]);
	}
	else
	{
		pszDest->Append(szText);
	}
	return pszDest->Text();
}
