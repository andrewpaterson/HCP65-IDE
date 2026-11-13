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
	szKey.Init();
	aszOverloads.Init();
	uiUnsure = 0;
}


void SIDEAssemblyRename::Kill(void)
{
	aszOverloads.Kill();
	szKey.Kill();
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


//A scalar type a key names, and the sizes and signs the compiler can give it on the 65C64 and the W65C816.
struct SIDEKeyScalar
{
	const char*	szName;
	const char*	szCodes;	//The mangled codes it can be (c h s t i j x y b f d e v).
};


static const SIDEKeyScalar gasIDEKeyScalars[] =
{
	{ "void", "v" },					{ "bool", "b" },
	{ "char", "ch" },					{ "signed char", "c" },				{ "unsigned char", "h" },
	{ "short", "s" },					{ "short int", "s" },				{ "signed short", "s" },
	{ "unsigned short", "t" },			{ "unsigned short int", "t" },
	{ "int", "si" },					{ "signed", "si" },					{ "signed int", "si" },
	{ "unsigned", "tj" },				{ "unsigned int", "tj" },
	{ "long", "ix" },					{ "long int", "ix" },				{ "unsigned long", "jy" },
	{ "unsigned long int", "jy" },		{ "long long", "x" },				{ "unsigned long long", "y" },
	{ "int8", "c" },					{ "uint8", "h" },					{ "int16", "s" },
	{ "uint16", "t" },					{ "int32", "i" },					{ "uint32", "j" },
	{ "int64", "x" },					{ "uint64", "y" },					{ "size", "tjy" },
	{ "lsize", "jy" },					{ "wchar_t", "stij" },				{ "char16_t", "t" },
	{ "char32_t", "j" },				{ "float", "f" },					{ "double", "df" },
	{ "long double", "ed" },
	{ NULL, NULL },
};


//////////////////////////////////////////////////////////////////////////
//
// szText from uiStart to uiEnd without spaces either side, into pszDest.
//
//////////////////////////////////////////////////////////////////////////
static void TrimIDEKeyPart(const char* szText, size uiStart, size uiEnd, CChars* pszDest)
{
	while ((uiStart < uiEnd) && (szText[uiStart] == ' '))
	{
		uiStart++;
	}
	while ((uiEnd > uiStart) && (szText[uiEnd - 1] == ' '))
	{
		uiEnd--;
	}
	pszDest->Init();
	pszDest->AppendSubString(szText, uiStart, uiEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// A function's key split into its parameters' types ("char const*"),
// whether it takes "..." and whether it is a const member.  False if it
// has no parameter list.
//
//////////////////////////////////////////////////////////////////////////
static bool SplitIDEKeyParameters(const char* szKey, CArrayChars* paszDest, bool* pbVariadic, bool* pbConst)
{
	const char*	szOpen;
	size		uiStart;
	size		uiPart;
	size		i;
	int			iDepth;
	CChars		szPart;

	szOpen = strchr(szKey, '(');
	if (szOpen == NULL)
	{
		return false;
	}
	*pbVariadic = false;
	iDepth = 0;
	uiStart = (size)(szOpen - szKey) + 1;
	uiPart = uiStart;
	for (i = uiStart; szKey[i] != '\0'; i++)
	{
		if ((szKey[i] == '(') || (szKey[i] == '[') || (szKey[i] == '<'))
		{
			iDepth++;
		}
		else if ((szKey[i] == ']') || (szKey[i] == '>'))
		{
			iDepth--;
		}
		else if (szKey[i] == ')')
		{
			if (iDepth == 0)
			{
				break;
			}
			iDepth--;
		}
		else if ((szKey[i] == ',') && (iDepth == 0))
		{
			TrimIDEKeyPart(szKey, uiPart, i, &szPart);
			paszDest->Add(szPart.Text());
			szPart.Kill();
			uiPart = i + 1;
		}
	}
	TrimIDEKeyPart(szKey, uiPart, i, &szPart);
	if (!szPart.Empty())
	{
		paszDest->Add(szPart.Text());
	}
	szPart.Kill();
	if ((paszDest->NumElements() > 0) && (strcmp(paszDest->GetText(paszDest->NumElements() - 1), "...") == 0))
	{
		paszDest->RemoveTail();
		*pbVariadic = true;
	}
	*pbConst = (szKey[i] == ')') && (strstr(&szKey[i], " const") != NULL);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The mangled type at *puiAt passed over (an array's, a function's or a
// member pointer's, which keys aren't compared with).
//
//////////////////////////////////////////////////////////////////////////
static bool SkipIDEMangledType(const char* szMangled, size uiLength, size* puiAt)
{
	size	uiCount;
	char	c;

	while ((*puiAt < uiLength) && ((szMangled[*puiAt] == 'V') || (szMangled[*puiAt] == 'K')))
	{
		(*puiAt)++;
	}
	if (*puiAt >= uiLength)
	{
		return false;
	}
	c = szMangled[(*puiAt)++];
	if (strchr("vbchstijxyfde", c))
	{
		return true;
	}
	if ((c == 'P') || (c == 'R'))
	{
		return SkipIDEMangledType(szMangled, uiLength, puiAt);
	}
	if ((c == 'C') || (c == 'N'))
	{
		for (uiCount = 0; (*puiAt < uiLength) && (szMangled[*puiAt] >= '0') && (szMangled[*puiAt] <= '9'); (*puiAt)++)
		{
			uiCount = (uiCount * 10) + (size)(szMangled[*puiAt] - '0');
		}
		*puiAt += uiCount;
		return *puiAt <= uiLength;
	}
	if (c == 'A')
	{
		while ((*puiAt < uiLength) && (szMangled[*puiAt] != '_'))
		{
			(*puiAt)++;
		}
		(*puiAt)++;
		return SkipIDEMangledType(szMangled, uiLength, puiAt);
	}
	if (c == 'M')
	{
		return SkipIDEMangledType(szMangled, uiLength, puiAt) && SkipIDEMangledType(szMangled, uiLength, puiAt);
	}
	if (c == 'F')
	{
		while ((*puiAt < uiLength) && (szMangled[*puiAt] != 'E'))
		{
			if (szMangled[*puiAt] == 'z')
			{
				(*puiAt)++;
			}
			else if (!SkipIDEMangledType(szMangled, uiLength, puiAt))
			{
				return false;
			}
		}
		(*puiAt)++;
		return *puiAt <= uiLength;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the mangled type at *puiAt (passed over) can be the key's type
// szKeyType ("char const*", "uint32", "NS::CFat&").  A scalar a key names
// by a typedef this doesn't know (or a function pointer, an array) fits
// any: only what is known tells overloads apart.
//
//////////////////////////////////////////////////////////////////////////
static bool MatchIDEMangledType(const char* szMangled, size uiLength, size* puiAt, const char* szKeyType)
{
	CChars		szKey;
	CChars		szInner;
	CChars		szName;
	bool		bKeyConst;
	bool		bKeyVolatile;
	bool		bConst;
	bool		bVolatile;
	bool		bResult;
	size		uiCount;
	size		i;
	char		c;

	szKey.Init(szKeyType);
	szKey.StripWhitespace();
	bKeyConst = false;
	bKeyVolatile = false;
	for (;;)
	{
		if (szKey.EndsWith(" const"))
		{
			bKeyConst = true;
			szKey.SetLength(szKey.Length() - 6);
		}
		else if (szKey.EndsWith(" volatile"))
		{
			bKeyVolatile = true;
			szKey.SetLength(szKey.Length() - 9);
		}
		else
		{
			break;
		}
	}
	if ((szKey.Contains('(') || szKey.Contains('[')))
	{
		szKey.Kill();
		return SkipIDEMangledType(szMangled, uiLength, puiAt);
	}

	bVolatile = (*puiAt < uiLength) && (szMangled[*puiAt] == 'V');
	if (bVolatile)
	{
		(*puiAt)++;
	}
	bConst = (*puiAt < uiLength) && (szMangled[*puiAt] == 'K');
	if (bConst)
	{
		(*puiAt)++;
	}
	if ((*puiAt >= uiLength) || (bConst != bKeyConst) || (bVolatile != bKeyVolatile))
	{
		szKey.Kill();
		return false;
	}

	c = szMangled[(*puiAt)++];
	bResult = false;
	if ((c == 'P') || (c == 'R'))
	{
		if (szKey.EndsWith((c == 'P') ? "*" : "&"))
		{
			szInner.Init(szKey);
			szInner.SetLength(szKey.Length() - 1);
			if ((c == 'R') && szInner.EndsWith("&"))
			{
				szInner.SetLength(szInner.Length() - 1);
			}
			bResult = MatchIDEMangledType(szMangled, uiLength, puiAt, szInner.Text());
			szInner.Kill();
		}
	}
	else if ((c == 'C') || (c == 'N'))
	{
		for (uiCount = 0; (*puiAt < uiLength) && (szMangled[*puiAt] >= '0') && (szMangled[*puiAt] <= '9'); (*puiAt)++)
		{
			uiCount = (uiCount * 10) + (size)(szMangled[*puiAt] - '0');
		}
		if (*puiAt + uiCount <= uiLength)
		{
			szName.Init();
			szName.AppendSubString(&szMangled[*puiAt], uiCount);
			szKey.Replace("::", "__");
			bResult = szName.Equals(szKey) || (szName.EndsWith(szKey.Text()) && (szName.Length() > szKey.Length() + 1) && szName.SubStringEquals(szName.Length() - szKey.Length() - 2, "__"));
			szName.Kill();
		}
		*puiAt += uiCount;
	}
	else if (strchr("vbchstijxyfde", c))
	{
		bResult = !szKey.Contains('*') && !szKey.Contains('&');
		for (i = 0; bResult && gasIDEKeyScalars[i].szName; i++)
		{
			if (szKey.Equals(gasIDEKeyScalars[i].szName))
			{
				bResult = (strchr(gasIDEKeyScalars[i].szCodes, c) != NULL);
				break;
			}
		}
	}
	else
	{
		(*puiAt)--;
		bResult = SkipIDEMangledType(szMangled, uiLength, puiAt);
	}
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a label's parameter part (after the name: "", "__K",
// "__F<types>", "__K__F<types>") can be the key's function.
//
//////////////////////////////////////////////////////////////////////////
static bool FitsIDEOverload(const char* szRest, size uiLength, const char* szKey)
{
	CArrayChars	aszParameters;
	bool		bVariadic;
	bool		bConst;
	bool		bLabelConst;
	bool		bFits;
	size		uiAt;
	size		i;

	aszParameters.Init();
	if (!SplitIDEKeyParameters(szKey, &aszParameters, &bVariadic, &bConst))
	{
		aszParameters.Kill();
		return false;
	}
	uiAt = 0;
	bLabelConst = (uiLength >= 3) && (memcmp(szRest, "__K", 3) == 0);
	if (bLabelConst)
	{
		uiAt = 3;
	}
	bFits = (bLabelConst == bConst);
	if (bFits && (uiAt == uiLength))
	{
		bFits = (aszParameters.NumElements() == 0) && !bVariadic;
	}
	else if (bFits)
	{
		bFits = (uiLength >= uiAt + 3) && (memcmp(&szRest[uiAt], "__F", 3) == 0);
		uiAt += 3;
		for (i = 0; bFits && (i < aszParameters.NumElements()); i++)
		{
			bFits = MatchIDEMangledType(szRest, uiLength, &uiAt, aszParameters.GetText(i));
		}
		if (bFits && bVariadic)
		{
			bFits = (uiAt < uiLength) && (szRest[uiAt] == 'z');
			uiAt++;
		}
		bFits = bFits && (uiAt == uiLength);
	}
	aszParameters.Kill();
	return bFits;
}


//////////////////////////////////////////////////////////////////////////
//
// A member's or function's own label: "_" and its scopes ("Scope__", none
// for a function outside every scope) then its name, ending there or
// before "__" (its parameters).  An overloaded function's is changed only
// when its parameters fit it and no other overload (FitsIDEOverload); one
// that fits it and another too is counted (uiUnsure) and left.  True if
// the token is one.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDENamedLabelEdit(const char* szToken, size uiStart, size uiLength, SIDEAssemblyRename* psRename, bool bComment, CArrayIDEAssemblyEdits* pasDest)
{
	size	uiName;
	size	uiOld;
	size	uiEnd;
	size	uiFits;
	size	i;
	bool	bOwn;

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
	//An overload's label is its own when it fits it and no other overload; one that fits it and others is left.
	if (psRename->aszOverloads.NumElements() > 0)
	{
		bOwn = FitsIDEOverload(&szToken[uiEnd], uiLength - uiEnd, psRename->szKey.Text());
		uiFits = 0;
		for (i = 0; i < psRename->aszOverloads.NumElements(); i++)
		{
			if (FitsIDEOverload(&szToken[uiEnd], uiLength - uiEnd, psRename->aszOverloads.GetText(i)))
			{
				uiFits++;
			}
		}
		if (!bOwn)
		{
			return true;
		}
		if (uiFits != 1)
		{
			psRename->uiUnsure++;
			return true;
		}
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
