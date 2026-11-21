/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <ctype.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDELocationPreview.h"
#include "IDESymbolInfo.h"


#define IDE_DECLARATION_PARENS	32		//The brackets a name can be inside, each inside the last, and its declaration still be read.
#define IDE_CALL_LINES			60		//The lines before the caret read to find the call it is in.
#define IDE_CALL_BRACKETS		64

#define IDE_SKIPPED_NOTHING		0
#define IDE_SKIPPED_COMMENT		1
#define IDE_SKIPPED_LITERAL		2


//The brackets a name is inside: where the part it is in starts (after the bracket, or the last comma or semicolon
//since), and how many angle brackets are open, in which a comma isn't a parameter's.
struct SIDEParenLevel
{
	size	uiSeparator;
	int32	iAngle;
};


//An open bracket before the caret: which, where, and the commas after it.
struct SIDECallBracket
{
	char	cBracket;
	size	uiOpen;
	size	uiCommas;
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESymbolInfo::Init(void)
{
	szKey.Init();
	uiKind = 0;
	bFunction = false;
	szDeclaration.Init();
	uiName = 0;
	uiNameLength = 0;
	szWhere.Init();
	memset(&sPlace, 0, sizeof(SIDELocation));
	sPlace.szFileName.Init();
	sPlace.iContainer = -1;
	aszParameters.Init();
}


void SIDESymbolInfo::Kill(void)
{
	aszParameters.Kill();
	sPlace.szFileName.Kill();
	szWhere.Kill();
	szDeclaration.Kill();
	szKey.Kill();
}


void KillIDESymbolInfos(CArrayIDESymbolInfos* pasInfos)
{
	size	i;

	for (i = 0; i < pasInfos->NumElements(); i++)
	{
		pasInfos->Get(i)->Kill();
	}
	pasInfos->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A comment, a string or a character at *pui is stepped over: which it
// was, IDE_SKIPPED_NOTHING if none starts there.
//
//////////////////////////////////////////////////////////////////////////
static int SkipIDENonCode(const char* szText, size uiLength, size* pui)
{
	size	i;
	char	cQuote;

	i = *pui;
	if ((szText[i] == '/') && (i + 1 < uiLength) && (szText[i + 1] == '/'))
	{
		while ((i < uiLength) && (szText[i] != '\n'))
		{
			i++;
		}
		*pui = i;
		return IDE_SKIPPED_COMMENT;
	}
	if ((szText[i] == '/') && (i + 1 < uiLength) && (szText[i + 1] == '*'))
	{
		i += 2;
		while ((i + 1 < uiLength) && !((szText[i] == '*') && (szText[i + 1] == '/')))
		{
			i++;
		}
		*pui = (i + 2 < uiLength) ? i + 2 : uiLength;
		return IDE_SKIPPED_COMMENT;
	}
	if ((szText[i] == '"') || (szText[i] == '\''))
	{
		//A digit separator (1'000) isn't a character's start.
		if ((szText[i] == '\'') && (i > 0) && isdigit((unsigned char)szText[i - 1]) && (i + 1 < uiLength) && isdigit((unsigned char)szText[i + 1]))
		{
			return IDE_SKIPPED_NOTHING;
		}
		cQuote = szText[i];
		i++;
		while ((i < uiLength) && (szText[i] != cQuote) && (szText[i] != '\n'))
		{
			i += ((szText[i] == '\\') && (i + 1 < uiLength)) ? 2 : 1;
		}
		*pui = (i < uiLength) ? i + 1 : uiLength;
		return IDE_SKIPPED_LITERAL;
	}
	return IDE_SKIPPED_NOTHING;
}


//////////////////////////////////////////////////////////////////////////
//
// szText from uiStart to uiEnd appended on one line: its comments left
// out and each run of spaces, tabs and line ends one space, none at its
// start or end.  If puiMark is given, *puiMark (a place in the text) is
// made that place in pszDest.
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDEOneLine(const char* szText, size uiStart, size uiEnd, CChars* pszDest, size* puiMark)
{
	size	i;
	size	uiMark;
	size	uiFirst;
	bool	bSpace;
	bool	bMarked;

	uiMark = puiMark ? *puiMark : 0;
	uiFirst = pszDest->Length();
	bSpace = false;
	bMarked = (puiMark == NULL);
	for (i = uiStart; i < uiEnd; )
	{
		if (!bMarked && (i >= uiMark))
		{
			if (bSpace && (pszDest->Length() > uiFirst))
			{
				pszDest->Append(' ');
			}
			bSpace = false;
			*puiMark = pszDest->Length();
			bMarked = true;
		}
		if ((szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r') || (szText[i] == '\n'))
		{
			bSpace = true;
			i++;
			continue;
		}
		if ((szText[i] == '/') && (i + 1 < uiEnd) && ((szText[i + 1] == '/') || (szText[i + 1] == '*')))
		{
			SkipIDENonCode(szText, uiEnd, &i);
			bSpace = true;
			continue;
		}
		if (bSpace && (pszDest->Length() > uiFirst))
		{
			pszDest->Append(' ');
		}
		bSpace = false;
		pszDest->Append(szText[i]);
		i++;
	}
	if (!bMarked)
	{
		*puiMark = pszDest->Length();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The statement that starts at uiBoundary begins with a word a colon ends
// (public, private, protected, case, default): what follows the colon is
// a statement of its own.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDELabelStart(const char* szText, size uiBoundary, size uiColon)
{
	static const char*	aszWords[] = { "public", "private", "protected", "case", "default" };
	size				i;
	size				uiLength;

	while ((uiBoundary < uiColon) && isspace((unsigned char)szText[uiBoundary]))
	{
		uiBoundary++;
	}
	for (i = 0; i < 5; i++)
	{
		uiLength = strlen(aszWords[i]);
		if ((uiBoundary + uiLength <= uiColon) && (memcmp(&szText[uiBoundary], aszWords[i], uiLength) == 0) && !IsIDENameChar(szText[uiBoundary + uiLength]))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The parameters written from the bracket at uiOpen to the one that
// closes it, each on one line; none for "()" and "(void)".
//
//////////////////////////////////////////////////////////////////////////
static void ReadIDEParameters(const char* szText, size uiLength, size uiOpen, CArrayChars* paszDest)
{
	CChars	szParameter;
	size	i;
	size	uiStart;
	int32	iDepth;
	int32	iAngle;
	bool	bEnd;

	iDepth = 0;
	iAngle = 0;
	uiStart = uiOpen + 1;
	for (i = uiOpen + 1; i < uiLength; )
	{
		if (SkipIDENonCode(szText, uiLength, &i) != IDE_SKIPPED_NOTHING)
		{
			continue;
		}
		bEnd = false;
		switch (szText[i])
		{
		case '(': case '[': case '{':
			iDepth++;
			break;
		case ')': case ']': case '}':
			bEnd = (iDepth == 0);
			iDepth--;
			break;
		case '<':
			iAngle++;
			break;
		case '>':
			if ((iAngle > 0) && (szText[i - 1] != '-'))
			{
				iAngle--;
			}
			break;
		case ';':
			bEnd = true;
			break;
		}
		if (bEnd || ((szText[i] == ',') && (iDepth == 0) && (iAngle == 0)))
		{
			szParameter.Init();
			AppendIDEOneLine(szText, uiStart, i, &szParameter, NULL);
			if (!szParameter.Empty() && !(bEnd && (paszDest->NumElements() == 0) && szParameter.Equals("void")))
			{
				paszDest->Add(szParameter.Text());
			}
			szParameter.Kill();
			uiStart = i + 1;
			if (bEnd)
			{
				return;
			}
		}
		i++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The declaration the name at uiName (uiNameLength long) of szText is in,
// appended to pszDest on one line, with where the name is in it and, for
// a function (paszParameters given), its parameters.
//
// It is read from the text alone.  The text before the name is stepped
// through (comments, strings and preprocessor lines over) for where the
// name's statement starts: after the last ; { or } outside brackets, or
// the colon of public: or a case.  A name inside brackets is a parameter,
// or a variable a for declares: its declaration starts after the bracket
// or the last comma, and ends at the next.  An enumerator's is between
// its commas.  Anything else's ends at its body's { or its ;, or at the
// colon before a constructor's initialisers.  A macro's is its #define's
// first line.  False if the name isn't in the text.
//
//////////////////////////////////////////////////////////////////////////
bool ReadIDEDeclaration(const char* szText, size uiLength, size uiName, size uiNameLength, uint8 uiKind, CChars* pszDest, size* puiNameInDest, CArrayChars* paszParameters)
{
	SIDEParenLevel	asParens[IDE_DECLARATION_PARENS];
	SIDEParenLevel*	psParen;
	size			uiDepth;
	size			uiBoundary;
	size			uiComma;
	size			uiStart;
	size			uiEnd;
	size			uiFirst;
	size			uiMark;
	size			i;
	size			j;
	int32			iParen;
	int32			iAngle;
	bool			bLineStart;
	bool			bParameter;
	bool			bEnumerator;
	bool			bSeenParen;
	bool			bEnd;
	int				iSkipped;
	char			c;

	if ((uiName >= uiLength) || (uiName + uiNameLength > uiLength))
	{
		return false;
	}

	uiDepth = 0;
	uiBoundary = 0;
	uiComma = 0;
	bLineStart = true;
	for (i = 0; i < uiName; )
	{
		c = szText[i];
		if (c == '\n')
		{
			bLineStart = true;
			i++;
			continue;
		}
		if ((c == ' ') || (c == '\t') || (c == '\r'))
		{
			i++;
			continue;
		}
		iSkipped = SkipIDENonCode(szText, uiLength, &i);
		if (iSkipped != IDE_SKIPPED_NOTHING)
		{
			bLineStart = bLineStart && (iSkipped == IDE_SKIPPED_COMMENT);
			continue;
		}
		if ((c == '#') && bLineStart)
		{
			//A preprocessor line, with the lines a backslash continues it on.  The name in it is a macro: its first line.
			for (j = i; (j < uiLength) && !((szText[j] == '\n') && !((j > 0) && ((szText[j - 1] == '\\') || ((szText[j - 1] == '\r') && (j > 1) && (szText[j - 2] == '\\'))))); j++)
			{
			}
			if (uiName < j)
			{
				for (uiEnd = i; (uiEnd < j) && (szText[uiEnd] != '\n'); uiEnd++)
				{
				}
				while ((uiEnd > uiName + uiNameLength) && ((szText[uiEnd - 1] == '\\') || (szText[uiEnd - 1] == '\r')))
				{
					uiEnd--;
				}
				uiFirst = pszDest->Length();
				uiMark = uiName;
				AppendIDEOneLine(szText, i, uiEnd, pszDest, &uiMark);
				*puiNameInDest = uiMark - uiFirst;
				return true;
			}
			i = j;
			if (uiDepth == 0)
			{
				uiBoundary = i;
				uiComma = i;
			}
			continue;
		}
		bLineStart = false;
		psParen = ((uiDepth > 0) && (uiDepth <= IDE_DECLARATION_PARENS)) ? &asParens[uiDepth - 1] : NULL;
		switch (c)
		{
		case ';':
			if (uiDepth == 0)
			{
				uiBoundary = i + 1;
				uiComma = i + 1;
			}
			else if (psParen)
			{
				psParen->uiSeparator = i + 1;
			}
			break;
		case '{':
		case '}':
			if (uiDepth == 0)
			{
				uiBoundary = i + 1;
				uiComma = i + 1;
			}
			break;
		case '(':
			uiDepth++;
			if (uiDepth <= IDE_DECLARATION_PARENS)
			{
				asParens[uiDepth - 1].uiSeparator = i + 1;
				asParens[uiDepth - 1].iAngle = 0;
			}
			break;
		case ')':
			if (uiDepth > 0)
			{
				uiDepth--;
			}
			break;
		case ',':
			if (uiDepth == 0)
			{
				uiComma = i + 1;
			}
			else if (psParen && (psParen->iAngle == 0))
			{
				psParen->uiSeparator = i + 1;
			}
			break;
		case '<':
			if (psParen)
			{
				psParen->iAngle++;
			}
			break;
		case '>':
			if (psParen && (psParen->iAngle > 0) && (szText[i - 1] != '-'))
			{
				psParen->iAngle--;
			}
			break;
		case ':':
			if ((i + 1 < uiLength) && (szText[i + 1] == ':'))
			{
				i++;
			}
			else if ((uiDepth == 0) && IsIDELabelStart(szText, uiBoundary, i))
			{
				uiBoundary = i + 1;
				uiComma = i + 1;
			}
			break;
		}
		i++;
	}

	bParameter = uiDepth > 0;
	bEnumerator = !bParameter && (uiKind == CPPSK_Enumerator);
	uiStart = uiBoundary;
	if (bParameter && (uiDepth <= IDE_DECLARATION_PARENS))
	{
		uiStart = asParens[uiDepth - 1].uiSeparator;
	}
	else if (bEnumerator && (uiComma > uiStart))
	{
		uiStart = uiComma;
	}

	//Its end.
	iParen = 0;
	iAngle = 0;
	bSeenParen = false;
	bEnd = false;
	for (i = uiName + uiNameLength; (i < uiLength) && !bEnd; )
	{
		if (SkipIDENonCode(szText, uiLength, &i) != IDE_SKIPPED_NOTHING)
		{
			continue;
		}
		c = szText[i];
		if (bParameter)
		{
			if ((c == '(') || (c == '[') || (c == '{'))
			{
				iParen++;
			}
			else if ((c == ')') || (c == ']') || (c == '}'))
			{
				bEnd = (iParen == 0);
				iParen--;
			}
			else if (c == '<')
			{
				iAngle++;
			}
			else if ((c == '>') && (iAngle > 0) && (szText[i - 1] != '-'))
			{
				iAngle--;
			}
			else if (((c == ',') || (c == ';')) && (iParen == 0) && (iAngle == 0))
			{
				bEnd = true;
			}
		}
		else if (bEnumerator)
		{
			if (c == '(')
			{
				iParen++;
			}
			else if (c == ')')
			{
				iParen--;
			}
			else if (((c == ',') || (c == '}')) && (iParen <= 0))
			{
				bEnd = true;
			}
		}
		else
		{
			if (c == '(')
			{
				iParen++;
				bSeenParen = true;
			}
			else if ((c == ')') && (iParen > 0))
			{
				iParen--;
			}
			else if (((c == '{') || (c == ';')) && (iParen == 0))
			{
				bEnd = true;
			}
			else if (c == ':')
			{
				if ((i + 1 < uiLength) && (szText[i + 1] == ':'))
				{
					i++;
				}
				else if ((iParen == 0) && bSeenParen)
				{
					bEnd = true;
				}
			}
		}
		if (!bEnd)
		{
			i++;
		}
	}
	uiEnd = i;

	uiFirst = pszDest->Length();
	uiMark = uiName;
	AppendIDEOneLine(szText, uiStart, uiEnd, pszDest, &uiMark);
	*puiNameInDest = uiMark - uiFirst;

	//A variable given a list: "int ai[] =" is "int ai[]".
	while ((pszDest->Length() > uiFirst + *puiNameInDest + uiNameLength) && ((pszDest->GetChar(pszDest->Length() - 1) == '=') || (pszDest->GetChar(pszDest->Length() - 1) == ' ')))
	{
		pszDest->RemoveLastCharacter();
	}
	if (pszDest->Length() - uiFirst > IDE_DECLARATION_MAXIMUM)
	{
		pszDest->SetLength(uiFirst + IDE_DECLARATION_MAXIMUM);
		pszDest->Append(" ...");
	}

	//A function's parameters: in the bracket after its name (and, a template's, its arguments).
	if (paszParameters && !bParameter && !bEnumerator)
	{
		i = uiName + uiNameLength;
		while ((i < uiEnd) && isspace((unsigned char)szText[i]))
		{
			i++;
		}
		if ((i < uiEnd) && (szText[i] == '<'))
		{
			for (iAngle = 0; i < uiEnd; i++)
			{
				if (szText[i] == '<')
				{
					iAngle++;
				}
				else if ((szText[i] == '>') && (--iAngle == 0))
				{
					i++;
					break;
				}
			}
			while ((i < uiEnd) && isspace((unsigned char)szText[i]))
			{
				i++;
			}
		}
		if ((i < uiEnd) && (szText[i] == '('))
		{
			ReadIDEParameters(szText, uiEnd, i, paszParameters);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol szKey's declaration read from where the index says it is
// declared: a function's from its declaration (a class's member as its
// class writes it), anything else's from its definition; the other if it
// hasn't that.  False if the index hasn't it declared anywhere, or its
// file can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool ReadIDESymbolInfo(CIDEContent* pcContent, const char* szKey, SIDESymbolInfo* psDest)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SIDELocation*		psBest;
	CTextFile			cFile;
	CChars				szScope;
	const char*			szText;
	const char*			szName;
	size				uiLength;
	size				uiName;
	size				i;
	uint8				uiKind;
	uint8				uiWanted;
	bool				bResult;

	asLocations.Init();
	uiKind = 0;
	if (!pcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind))
	{
		KillIDELocations(&asLocations);
		return false;
	}
	psDest->bFunction = (uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate);
	uiWanted = psDest->bFunction ? (uint8)CPPIR_Declaration : (uint8)CPPIR_Definition;
	psBest = NULL;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (((psLocation->uiRole == CPPIR_Declaration) || (psLocation->uiRole == CPPIR_Definition)) && ((psBest == NULL) || ((psBest->uiRole != uiWanted) && (psLocation->uiRole == uiWanted))))
		{
			psBest = psLocation;
		}
	}
	bResult = false;
	if (psBest)
	{
		cFile.Init();
		if (pcContent->GetFindInFiles()->GetFileText(psBest->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			uiName = FindIDETextPosition(szText, uiLength, psBest->iLine, psBest->iColumn);
			psDest->uiNameLength = (psBest->iLength > 0) ? (size)psBest->iLength : 0;
			bResult = ReadIDEDeclaration(szText, uiLength, uiName, psDest->uiNameLength, uiKind, &psDest->szDeclaration, &psDest->uiName, psDest->bFunction ? &psDest->aszParameters : NULL);
			if (bResult && (psDest->uiName + psDest->uiNameLength > psDest->szDeclaration.Length()))
			{
				psDest->uiNameLength = 0;
			}
		}
		cFile.Kill();
	}
	if (bResult)
	{
		psDest->szKey.Set(szKey);
		psDest->uiKind = uiKind;
		psDest->sPlace.szFileName.Set(psBest->szFileName);
		psDest->sPlace.iLine = psBest->iLine;
		psDest->sPlace.iColumn = psBest->iColumn;
		psDest->sPlace.iLength = psBest->iLength;
		psDest->sPlace.uiRole = psBest->uiRole;

		//"CShape, Shape.h:15": the scope of what a class or a namespace has.
		szScope.Init();
		if ((uiKind != CPPSK_Variable) && (uiKind != CPPSK_Parameter) && (uiKind != CPPSK_Label) && (uiKind != CPPSK_TypeParameter) && (uiKind != CPPSK_ValueParameter))
		{
			GetIDEScopedName(szKey, &szScope);
			szName = strrchr(szScope.Text(), ':');
			if (szName && (szName > szScope.Text()) && (szName[-1] == ':'))
			{
				psDest->szWhere.Append(szScope.Text(), (size)(szName - szScope.Text()) - 1);
				psDest->szWhere.Append(", ");
			}
		}
		szScope.Kill();
		psDest->szWhere.Append(GetIDEFileNamePart(psBest->szFileName.Text()));
		psDest->szWhere.Append(':');
		psDest->szWhere.Append((int32)(psBest->iLine + 1));
	}
	KillIDELocations(&asLocations);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The type a symbol is declared with, appended: a variable's, a field's
// or a parameter's is its declaration without its name and what it starts
// as ("CShape*", "int[4]"); a function's is what it returns; anything
// else's (a class, a typedef) is its declaration.  Words that say where
// it is kept, not what it is (static, extern, virtual ...), are left out.
//
//////////////////////////////////////////////////////////////////////////
void AppendIDEDeclaredType(SIDESymbolInfo* psInfo, CChars* pszDest)
{
	static const char*	aszWords[] = { "static", "extern", "virtual", "inline", "explicit", "friend", "mutable", "register", "constexpr" };
	CChars				szType;
	const char*			szDeclaration;
	size				uiEnd;
	size				uiStart;
	size				uiLength;
	size				i;
	bool				bFound;
	bool				bValue;

	bValue = (psInfo->uiKind == CPPSK_Variable) || (psInfo->uiKind == CPPSK_Field) || (psInfo->uiKind == CPPSK_Parameter) || (psInfo->uiKind == CPPSK_ValueParameter);
	if ((!bValue && !psInfo->bFunction) || (psInfo->uiNameLength == 0))
	{
		pszDest->Append(psInfo->szDeclaration);
		return;
	}

	//Before its name, less the scope it is defined with ("int CCircle::").
	szDeclaration = psInfo->szDeclaration.Text();
	uiEnd = psInfo->uiName;
	bFound = true;
	while (bFound)
	{
		while ((uiEnd > 0) && (szDeclaration[uiEnd - 1] == ' '))
		{
			uiEnd--;
		}
		bFound = (uiEnd >= 2) && (szDeclaration[uiEnd - 1] == ':') && (szDeclaration[uiEnd - 2] == ':');
		if (bFound)
		{
			uiEnd -= 2;
			while ((uiEnd > 0) && IsIDENameChar(szDeclaration[uiEnd - 1]))
			{
				uiEnd--;
			}
		}
	}
	if ((uiEnd > 0) && (szDeclaration[uiEnd - 1] == '~'))
	{
		uiEnd--;
	}

	uiStart = 0;
	bFound = true;
	while (bFound)
	{
		bFound = false;
		for (i = 0; i < 9; i++)
		{
			uiLength = strlen(aszWords[i]);
			if ((uiStart + uiLength < uiEnd) && (memcmp(&szDeclaration[uiStart], aszWords[i], uiLength) == 0) && (szDeclaration[uiStart + uiLength] == ' '))
			{
				uiStart += uiLength + 1;
				bFound = true;
			}
		}
	}
	szType.Init();
	if (uiEnd > uiStart)
	{
		szType.Append(&szDeclaration[uiStart], uiEnd - uiStart);
	}

	//An array's bounds are after its name.
	if (bValue)
	{
		i = psInfo->uiName + psInfo->uiNameLength;
		while (szDeclaration[i] == '[')
		{
			uiStart = i;
			while ((szDeclaration[i] != '\0') && (szDeclaration[i] != ']'))
			{
				i++;
			}
			if (szDeclaration[i] == ']')
			{
				i++;
			}
			szType.Append(&szDeclaration[uiStart], i - uiStart);
		}
	}
	szType.StripWhitespace();
	pszDest->Append(szType);
	szType.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The name the character at uiPosition is in, or just after: where it
// starts and ends.  False if it is in none, or in a number.
//
//////////////////////////////////////////////////////////////////////////
bool FindIDENameAt(const char* szText, size uiPosition, size* puiStart, size* puiEnd)
{
	size	uiStart;
	size	uiEnd;

	uiStart = uiPosition;
	uiEnd = uiPosition;
	while (IsIDENameChar(szText[uiEnd]))
	{
		uiEnd++;
	}
	while ((uiStart > 0) && IsIDENameChar(szText[uiStart - 1]))
	{
		uiStart--;
	}
	if ((uiEnd == uiStart) || isdigit((unsigned char)szText[uiStart]))
	{
		return false;
	}
	*puiStart = uiStart;
	*puiEnd = uiEnd;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The call whose arguments the caret (uiCaret, in szText) is among: the
// innermost bracket open before it that follows a name which isn't a
// statement's (if, while ...).  Where the name is, where the bracket is
// and which argument the caret is in, from 0.  The lines before the caret
// are read from a line's start, IDE_CALL_LINES of them at most.  False if
// it is in none: outside every bracket, or in a block inside one.
//
//////////////////////////////////////////////////////////////////////////
bool FindIDECall(const char* szText, size uiCaret, size* puiNameStart, size* puiNameEnd, size* puiOpen, size* puiArgument)
{
	static const char*	aszWords[] = { "if", "while", "for", "switch", "return", "sizeof", "catch", "alignof", "decltype", "else", "do" };
	SIDECallBracket		asBrackets[IDE_CALL_BRACKETS];
	SIDECallBracket*	psBracket;
	size				uiBrackets;
	size				uiStart;
	size				uiLines;
	size				uiEnd;
	size				uiName;
	size				uiLength;
	size				i;
	size				j;
	int32				iAngle;
	bool				bWord;
	char				c;

	uiStart = uiCaret;
	uiLines = 0;
	while ((uiStart > 0) && (uiLines < IDE_CALL_LINES))
	{
		uiStart--;
		if (szText[uiStart] == '\n')
		{
			uiLines++;
		}
	}
	if ((uiStart > 0) || (szText[uiStart] == '\n'))
	{
		uiStart++;
	}

	uiBrackets = 0;
	for (i = uiStart; i < uiCaret; )
	{
		j = i;
		if (SkipIDENonCode(szText, uiCaret, &j) != IDE_SKIPPED_NOTHING)
		{
			//The caret inside a comment or a string is in no call's arguments... but a string's is: only a comment ends it.
			if ((j >= uiCaret) && (szText[i] == '/') && (szText[uiCaret - 1] != '\n') && (szText[i + 1] == '/'))
			{
				return false;
			}
			i = j;
			continue;
		}
		c = szText[i];
		if ((c == '(') || (c == '{') || (c == '['))
		{
			if (uiBrackets < IDE_CALL_BRACKETS)
			{
				asBrackets[uiBrackets].cBracket = c;
				asBrackets[uiBrackets].uiOpen = i;
				asBrackets[uiBrackets].uiCommas = 0;
			}
			uiBrackets++;
		}
		else if ((c == ')') || (c == '}') || (c == ']'))
		{
			if (uiBrackets > 0)
			{
				uiBrackets--;
			}
		}
		else if ((c == ',') && (uiBrackets > 0) && (uiBrackets <= IDE_CALL_BRACKETS))
		{
			asBrackets[uiBrackets - 1].uiCommas++;
		}
		i++;
	}
	if (uiBrackets > IDE_CALL_BRACKETS)
	{
		return false;
	}

	for (i = uiBrackets; i > 0; i--)
	{
		psBracket = &asBrackets[i - 1];
		if (psBracket->cBracket == '{')
		{
			return false;
		}
		if (psBracket->cBracket != '(')
		{
			continue;
		}

		//The name before it, over spaces and a template's arguments.
		uiEnd = psBracket->uiOpen;
		while ((uiEnd > uiStart) && isspace((unsigned char)szText[uiEnd - 1]))
		{
			uiEnd--;
		}
		if ((uiEnd > uiStart) && (szText[uiEnd - 1] == '>'))
		{
			for (iAngle = 0; uiEnd > uiStart; uiEnd--)
			{
				if (szText[uiEnd - 1] == '>')
				{
					iAngle++;
				}
				else if ((szText[uiEnd - 1] == '<') && (--iAngle == 0))
				{
					uiEnd--;
					break;
				}
			}
		}
		uiName = uiEnd;
		while ((uiName > uiStart) && IsIDENameChar(szText[uiName - 1]))
		{
			uiName--;
		}
		if ((uiName == uiEnd) || isdigit((unsigned char)szText[uiName]))
		{
			continue;
		}
		bWord = false;
		for (j = 0; j < 11; j++)
		{
			uiLength = strlen(aszWords[j]);
			bWord = bWord || ((uiEnd - uiName == uiLength) && (memcmp(&szText[uiName], aszWords[j], uiLength) == 0));
		}
		if (bWord)
		{
			continue;
		}
		*puiNameStart = uiName;
		*puiNameEnd = uiEnd;
		*puiOpen = psBracket->uiOpen;
		*puiArgument = psBracket->uiCommas;
		return true;
	}
	return false;
}

