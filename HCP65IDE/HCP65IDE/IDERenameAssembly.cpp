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
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "IDERenameAssembly.h"


//////////////////////////////////////////////////////////////////////////
//
// szScopeKey is the symbol's scopes as the index has them ("NS::CFat"),
// which labels write with "__" between them ("NS__CFat").
//
//////////////////////////////////////////////////////////////////////////
void SIDEAssemblyRename::Init(EIDEAssemblyRename eRenameKind, const char* szOldName, const char* szNewName, const char* szScopeKey)
{
	eKind = eRenameKind;
	szOld.Init(szOldName);
	szNew.Init(szNewName);
	szScope.Init(szScopeKey ? szScopeKey : "");
	szScope.Replace("::", "__");
}


void SIDEAssemblyRename::Kill(void)
{
	szScope.Kill();
	szNew.Kill();
	szOld.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDEAssemblyEdits(CArrayIDEAssemblyEdits* pasEdits)
{
	size	i;

	for (i = 0; i < pasEdits->NumElements(); i++)
	{
		pasEdits->Get(i)->szReplacement.Kill();
	}
	pasEdits->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file Rename looks in for labels: hand written assembly and its
// includes.
//
//////////////////////////////////////////////////////////////////////////
bool IsIDEAssemblyFile(const char* szFileName)
{
	const char*	szDot;

	szDot = strrchr(szFileName, '.');
	return szDot && ((StringInsensitiveCompare(szDot, ".asm") == 0) || (StringInsensitiveCompare(szDot, ".inc") == 0));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDELabelChar(char c)
{
	return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}


static void AddIDEAssemblyEdit(CArrayIDEAssemblyEdits* pasDest, size uiStart, size uiLength, const char* szReplacement, bool bComment, bool bLength = false)
{
	SIDEAssemblyEdit*	psEdit;

	psEdit = pasDest->Add();
	psEdit->uiStart = uiStart;
	psEdit->uiLength = uiLength;
	psEdit->szReplacement.Init(szReplacement);
	psEdit->bComment = bComment;
	psEdit->bLength = bLength;
}


//////////////////////////////////////////////////////////////////////////
//
// A type's name in a label token from uiStart (szToken, uiLength long):
// each name in a parameter list, its length before it
// ("17CFatInfoSectorCache", or "8NS__CFat" in a namespace), given the new
// name and length; and each part of the label ("_CFat__Init",
// "_NS__CFat__0") that is the name.  True if it had any.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDETypeLabelEdits(const char* szToken, size uiStart, size uiLength, SIDEAssemblyRename* psRename, bool bComment, CArrayIDEAssemblyEdits* pasDest)
{
	size	uiOld;
	size	k;
	size	m;
	size	uiCount;
	size	uiBlob;
	size	uiCovered;
	size	uiPart;
	size	uiPartEnd;
	bool	bFound;
	CChars	szLength;

	uiOld = psRename->szOld.Length();
	bFound = false;
	uiCovered = 0;

	//Names in parameter lists: a length, then that many characters ending with the name.
	for (k = 1; k < uiLength; k++)
	{
		if ((szToken[k] < '0') || (szToken[k] > '9') || ((szToken[k - 1] >= '0') && (szToken[k - 1] <= '9')))
		{
			continue;
		}
		uiCount = 0;
		for (m = k; (m < uiLength) && (szToken[m] >= '0') && (szToken[m] <= '9'); m++)
		{
			uiCount = (uiCount * 10) + (size)(szToken[m] - '0');
		}
		uiBlob = m;
		if ((uiCount < uiOld) || (uiBlob + uiCount > uiLength) || (memcmp(&szToken[uiBlob + uiCount - uiOld], psRename->szOld.Text(), uiOld) != 0))
		{
			continue;
		}
		if ((uiCount != uiOld) && ((uiCount < uiOld + 2) || (memcmp(&szToken[uiBlob + uiCount - uiOld - 2], "__", 2) != 0)))
		{
			continue;
		}
		szLength.Init();
		szLength.Append((int)(uiCount - uiOld + psRename->szNew.Length()));
		AddIDEAssemblyEdit(pasDest, uiStart + k, m - k, szLength.Text(), bComment, true);
		szLength.Kill();
		AddIDEAssemblyEdit(pasDest, uiStart + uiBlob + uiCount - uiOld, uiOld, psRename->szNew.Text(), bComment);
		uiCovered = uiBlob + uiCount;
		k = uiCovered - 1;
		bFound = true;
	}

	//Parts of the label itself, between "_" or "__" and "__" or its end.
	if (szToken[0] == '_')
	{
		for (uiPart = 1; uiPart < uiLength; uiPart = uiPartEnd + 2)
		{
			for (uiPartEnd = uiPart; (uiPartEnd + 1 < uiLength) && !((szToken[uiPartEnd] == '_') && (szToken[uiPartEnd + 1] == '_')); uiPartEnd++)
			{
			}
			if (uiPartEnd + 1 >= uiLength)
			{
				uiPartEnd = uiLength;
			}
			if ((uiPart >= uiCovered) && (uiPartEnd - uiPart == uiOld) && (memcmp(&szToken[uiPart], psRename->szOld.Text(), uiOld) == 0))
			{
				AddIDEAssemblyEdit(pasDest, uiStart + uiPart, uiOld, psRename->szNew.Text(), bComment);
				bFound = true;
			}
			if (uiPartEnd >= uiLength)
			{
				break;
			}
		}
	}
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// A member's or function's own label: "_" and its scopes ("Scope__", none
// for a function outside every scope) then its name, ending there or
// before "__" (its parameters).  True if the token is one.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDENamedLabelEdit(const char* szToken, size uiStart, size uiLength, SIDEAssemblyRename* psRename, bool bComment, CArrayIDEAssemblyEdits* pasDest)
{
	size	uiName;
	size	uiOld;
	size	uiEnd;

	if (szToken[0] != '_')
	{
		return false;
	}
	uiName = 1;
	if (!psRename->szScope.Empty())
	{
		if ((uiLength < 1 + psRename->szScope.Length() + 2) || (memcmp(&szToken[1], psRename->szScope.Text(), psRename->szScope.Length()) != 0) ||
			(memcmp(&szToken[1 + psRename->szScope.Length()], "__", 2) != 0))
		{
			return false;
		}
		uiName = 1 + psRename->szScope.Length() + 2;
	}
	uiOld = psRename->szOld.Length();
	uiEnd = uiName + uiOld;
	if ((uiEnd > uiLength) || (memcmp(&szToken[uiName], psRename->szOld.Text(), uiOld) != 0) ||
		((uiEnd < uiLength) && ((uiEnd + 1 >= uiLength) || (szToken[uiEnd] != '_') || (szToken[uiEnd + 1] != '_'))))
	{
		return false;
	}
	AddIDEAssemblyEdit(pasDest, uiStart + uiName, uiOld, psRename->szNew.Text(), bComment);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where hand written assembly names psRename's symbol: in its labels, as
// the compiler mangles it (see EIDEAssemblyRename), and with bComments in
// its comments (from ';' to the line's end) too, as a word and in labels
// the comments quote.  A string ("...") is left as it is.  The edits are
// in the order of the text.
//
//////////////////////////////////////////////////////////////////////////
void FindIDEAssemblyEdits(const char* szText, size uiLength, SIDEAssemblyRename* psRename, bool bComments, CArrayIDEAssemblyEdits* pasDest)
{
	size	i;
	size	j;
	bool	bComment;
	bool	bLabel;
	char	c;

	bComment = false;
	i = 0;
	while (i < uiLength)
	{
		c = szText[i];
		if (c == '\n')
		{
			bComment = false;
			i++;
		}
		else if (c == ';')
		{
			bComment = true;
			i++;
		}
		else if ((c == '"') && !bComment)
		{
			for (i++; (i < uiLength) && (szText[i] != '"') && (szText[i] != '\n'); i++)
			{
			}
			if ((i < uiLength) && (szText[i] == '"'))
			{
				i++;
			}
		}
		else if (IsIDELabelChar(c))
		{
			for (j = i; (j < uiLength) && IsIDELabelChar(szText[j]); j++)
			{
			}
			if (!bComment || bComments)
			{
				bLabel = false;
				if (psRename->eKind == IAR_Type)
				{
					bLabel = FindIDETypeLabelEdits(&szText[i], i, j - i, psRename, bComment, pasDest);
				}
				else if ((psRename->eKind == IAR_Member) || (psRename->eKind == IAR_Function))
				{
					bLabel = FindIDENamedLabelEdit(&szText[i], i, j - i, psRename, bComment, pasDest);
				}
				if (!bLabel && bComment && (j - i == psRename->szOld.Length()) && (memcmp(&szText[i], psRename->szOld.Text(), j - i) == 0))
				{
					AddIDEAssemblyEdit(pasDest, i, j - i, psRename->szNew.Text(), true);
				}
			}
			i = j;
		}
		else
		{
			i++;
		}
	}
}
