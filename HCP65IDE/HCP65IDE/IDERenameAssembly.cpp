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
	size	uiLast;

	eKind = eRenameKind;
	szOld.Init(szOldName);
	szNew.Init(szNewName);
	szScope.Init(szScopeKey ? szScopeKey : "");
	szScope.Replace("::", "__");
	szClass.Init(szScope.Text());
	uiLast = szClass.FindFromEnd("__");
	if (uiLast != ARRAY_ELEMENT_NOT_FOUND)
	{
		szClass.RemoveFromStart(uiLast + 2);
	}
	szKey.Init();
	aszOverloads.Init();
	uiUnsure = 0;
}


void SIDEAssemblyRename::Kill(void)
{
	aszOverloads.Kill();
	szKey.Kill();
	szClass.Kill();
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
// If szToken (uiLength long) is a layout constant of szName's: szName,
// one '_' and a member's name ("CFat_muiID", not "CFat__Init").
//
//////////////////////////////////////////////////////////////////////////
bool IsIDELayoutPrefix(const char* szToken, size uiLength, const char* szName, size uiNameLength)
{
	return (uiLength > uiNameLength + 1) && (memcmp(szToken, szName, uiNameLength) == 0) && (szToken[uiNameLength] == '_') && (szToken[uiNameLength + 1] != '_');
}


//////////////////////////////////////////////////////////////////////////
//
// A field's layout constant in szToken (uiLength long): its class, one
// '_' and it ("CFat_muiID").  True if it is one.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDEFieldLabelEdit(const char* szToken, size uiStart, size uiLength, SIDEAssemblyRename* psRename, bool bComment, CArrayIDEAssemblyEdits* pasDest)
{
	size	uiClass;

	uiClass = psRename->szClass.Length();
	if ((uiClass == 0) || (uiLength != uiClass + 1 + psRename->szOld.Length()) || (memcmp(szToken, psRename->szClass.Text(), uiClass) != 0) ||
		(szToken[uiClass] != '_') || (memcmp(&szToken[uiClass + 1], psRename->szOld.Text(), psRename->szOld.Length()) != 0))
	{
		return false;
	}
	AddIDEAssemblyEdit(pasDest, uiStart + uiClass + 1, psRename->szOld.Length(), psRename->szNew.Text(), bComment);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// If szToken has a length-prefixed name at uiAt (a class's "C" or an
// enum's "N" before it: "C12NGear__CGear"), that fits before uiEnd and
// ends where a mangled type can: its length's digits end at *puiName and
// the name runs to *puiEnd.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDELengthName(const char* szToken, size uiAt, size uiEnd, size uiFrom, size* puiName, size* puiEnd)
{
	size	uiCount;
	size	m;

	if ((uiAt <= uiFrom) || (szToken[uiAt] < '1') || (szToken[uiAt] > '9') || ((szToken[uiAt - 1] != 'C') && (szToken[uiAt - 1] != 'N')))
	{
		return false;
	}
	uiCount = 0;
	for (m = uiAt; (m < uiEnd) && (szToken[m] >= '0') && (szToken[m] <= '9'); m++)
	{
		uiCount = (uiCount * 10) + (size)(szToken[m] - '0');
	}
	if ((m + uiCount > uiEnd) || ((m + uiCount < uiEnd) && !strchr("vbchstijxyfdePRKVAFCNMEz_", szToken[m + uiCount])))
	{
		return false;
	}
	*puiName = m;
	*puiEnd = m + uiCount;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// If szToken has template arguments at uiAt ("T_i__", "T_C12NGear__CGear__"
// and "T_JicE__" for a pack), where the part after them starts (past
// their "__"), else uiAt.  Their length-prefixed names can have "__" in
// them.
//
//////////////////////////////////////////////////////////////////////////
static size SkipIDETemplateArguments(const char* szToken, size uiLength, size uiAt)
{
	size	i;
	size	uiName;
	size	uiNameEnd;

	if ((uiAt + 2 > uiLength) || (szToken[uiAt] != 'T') || (szToken[uiAt + 1] != '_'))
	{
		return uiAt;
	}
	for (i = uiAt + 1; i + 1 < uiLength; )
	{
		if (IsIDELengthName(szToken, i, uiLength, uiAt, &uiName, &uiNameEnd))
		{
			i = uiNameEnd;
		}
		else if ((szToken[i] == '_') && (szToken[i + 1] == '_'))
		{
			return i + 2;
		}
		else
		{
			i++;
		}
	}
	return uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// psRename's name where it is a part of szToken from uiFrom to uiEnd: at
// a part's start (uiFrom when bStarts, else after "__") and ending at
// uiEnd or "__".  A length-prefixed name in it ("C12NGear__CGear", a
// parameter or a template argument) is looked in the same way, its length
// changed when its name's is: so are the lengths of the names it is in.
// The edits, in the order of the text, are added to pasDest; the length
// the part's text changes by is returned.
//
//////////////////////////////////////////////////////////////////////////
static int FindIDETypePartEdits(const char* szToken, size uiStart, size uiFrom, size uiEnd, bool bStarts, SIDEAssemblyRename* psRename, bool bComment, CArrayIDEAssemblyEdits* pasDest)
{
	size					uiOld;
	size					i;
	size					uiName;
	size					uiNameEnd;
	int						iChange;
	int						iInner;
	CArrayIDEAssemblyEdits	asInner;
	CChars					szLength;
	SIDEAssemblyEdit*		psEdit;
	size					j;

	uiOld = psRename->szOld.Length();
	iChange = 0;
	i = uiFrom;
	while (i < uiEnd)
	{
		if (IsIDELengthName(szToken, i, uiEnd, uiFrom, &uiName, &uiNameEnd))
		{
			asInner.Init();
			iInner = FindIDETypePartEdits(szToken, uiStart, uiName, uiNameEnd, true, psRename, bComment, &asInner);
			if (asInner.NumElements() > 0)
			{
				if (iInner != 0)
				{
					szLength.Init();
					szLength.Append((int)((int)(uiNameEnd - uiName) + iInner));
					AddIDEAssemblyEdit(pasDest, uiStart + i, uiName - i, szLength.Text(), bComment, true);
					iChange += (int)szLength.Length() - (int)(uiName - i);
					szLength.Kill();
				}
				for (j = 0; j < asInner.NumElements(); j++)
				{
					psEdit = pasDest->Add();
					memcpy(psEdit, asInner.Get(j), sizeof(SIDEAssemblyEdit));
				}
				asInner.Kill();
				iChange += iInner;
				i = uiNameEnd;
				continue;
			}
			asInner.Kill();
		}
		if ((((i == uiFrom) && bStarts) || ((i >= uiFrom + 2) && (szToken[i - 1] == '_') && (szToken[i - 2] == '_'))) && (i + uiOld <= uiEnd) &&
			(memcmp(&szToken[i], psRename->szOld.Text(), uiOld) == 0) && ((i + uiOld == uiEnd) || ((i + uiOld + 2 <= uiEnd) && (szToken[i + uiOld] == '_') && (szToken[i + uiOld + 1] == '_'))))
		{
			AddIDEAssemblyEdit(pasDest, uiStart + i, uiOld, psRename->szNew.Text(), bComment);
			iChange += (int)psRename->szNew.Length() - (int)uiOld;
			i += uiOld;
			continue;
		}
		i++;
	}
	return iChange;
}


//////////////////////////////////////////////////////////////////////////
//
// A type's or namespace's name in a token from uiStart (szToken, uiLength
// long): each part of a label that is the name ("_CFat__Init",
// "_NGear__CGear__Turn", "_CBox__T_i__Fill"), and each length-prefixed
// name with it as a part (parameters, template arguments: "PC7CEngine",
// "C23CBox__T_C12NGear__CGear"), given its new length; a type's
// generated layout constants too ("SIZEOF_CFat", "CFat_muiID").  True if
// it had any.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDETypeLabelEdits(const char* szToken, size uiStart, size uiLength, SIDEAssemblyRename* psRename, bool bComment, CArrayIDEAssemblyEdits* pasDest)
{
	size	uiOld;
	size	uiBefore;

	uiOld = psRename->szOld.Length();
	if ((psRename->eKind == IAR_Type) && (szToken[0] != '_'))
	{
		//A generated layout constant (BaseLib's FatW65C816.inc): "SIZEOF_CFat", and "CFat_muiID" before one of its members.
		if ((uiLength == 7 + uiOld) && (memcmp(szToken, "SIZEOF_", 7) == 0) && (memcmp(&szToken[7], psRename->szOld.Text(), uiOld) == 0))
		{
			AddIDEAssemblyEdit(pasDest, uiStart + 7, uiOld, psRename->szNew.Text(), bComment);
			return true;
		}
		if (IsIDELayoutPrefix(szToken, uiLength, psRename->szOld.Text(), uiOld))
		{
			AddIDEAssemblyEdit(pasDest, uiStart, uiOld, psRename->szNew.Text(), bComment);
			return true;
		}
	}

	//A label's parts start after its "_"; another token (a comment's word) only has length-prefixed names looked in.
	uiBefore = pasDest->NumElements();
	if (szToken[0] == '_')
	{
		FindIDETypePartEdits(szToken, uiStart, 1, uiLength, true, psRename, bComment, pasDest);
	}
	else
	{
		FindIDETypePartEdits(szToken, uiStart, 0, uiLength, false, psRename, bComment, pasDest);
	}
	return pasDest->NumElements() > uiBefore;
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
	if ((uiLength >= 4) && (memcmp(szRest, "__T_", 4) == 0))
	{
		//An instance of a function template: its arguments, then its parameters.
		uiAt = SkipIDETemplateArguments(szRest, uiLength, 2) - 2;
		szRest = &szRest[uiAt];
		uiLength -= uiAt;
		uiAt = 0;
	}
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
		if ((uiName + psRename->szOld.Length() > uiLength) || (memcmp(&szToken[uiName], psRename->szOld.Text(), psRename->szOld.Length()) != 0))
		{
			//A member of a class template's instance: its arguments after the class ("_CBox__T_i__Fill").
			uiName = SkipIDETemplateArguments(szToken, uiLength, uiName);
		}
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
				if ((psRename->eKind == IAR_Type) || (psRename->eKind == IAR_Namespace))
				{
					bLabel = FindIDETypeLabelEdits(&szText[i], i, j - i, psRename, bComment, pasDest);
				}
				else if ((psRename->eKind == IAR_Member) || (psRename->eKind == IAR_Function))
				{
					bLabel = FindIDENamedLabelEdit(&szText[i], i, j - i, psRename, bComment, pasDest);
				}
				else if (psRename->eKind == IAR_Field)
				{
					bLabel = FindIDEFieldLabelEdit(&szText[i], i, j - i, psRename, bComment, pasDest);
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
