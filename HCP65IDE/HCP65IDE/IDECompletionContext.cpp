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
#include "IDERename.h"
#include "IDECompletionContext.h"


//What can be written wherever a name can: the language's own words.
static const char*	gaszIDECompletionKeywords[] =
{
	"alignas", "alignof", "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do",
	"double", "dynamic_cast", "else", "enum", "explicit", "extern", "false", "final", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace",
	"new", "noexcept", "nullptr", "operator", "override", "private", "protected", "public", "register", "reinterpret_cast", "return", "short", "signed", "sizeof", "static",
	"static_assert", "static_cast", "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typename", "union", "unsigned", "using", "virtual", "void",
	"volatile", "while"
};


size NumIDECompletionKeywords(void) { return sizeof(gaszIDECompletionKeywords) / sizeof(const char*); }
const char* GetIDECompletionKeyword(size uiIndex) { return gaszIDECompletionKeywords[uiIndex]; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool IsIDECompletionKeyword(const char* szText, size uiLength)
{
	size	i;

	for (i = 0; i < NumIDECompletionKeywords(); i++)
	{
		if ((strlen(gaszIDECompletionKeywords[i]) == uiLength) && (strncmp(gaszIDECompletionKeywords[i], szText, uiLength) == 0))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What the text from its line's start to uiCaret leaves the caret in: 0
// code, '/' a comment that runs to the line's end, '"' or '\'' a string
// or a character not yet closed.  A comment begun on a line before isn't
// known.
//
//////////////////////////////////////////////////////////////////////////
static char FindIDELineState(const char* szText, size uiLineStart, size uiCaret)
{
	size	i;
	char	cQuote;

	cQuote = 0;
	for (i = uiLineStart; i < uiCaret; i++)
	{
		if (cQuote != 0)
		{
			if ((szText[i] == '\\') && (i + 1 < uiCaret))
			{
				i++;
			}
			else if (szText[i] == cQuote)
			{
				cQuote = 0;
			}
		}
		else if ((szText[i] == '"') || (szText[i] == '\''))
		{
			//A digit separator (1'000) starts no character.
			if (!((szText[i] == '\'') && (i > uiLineStart) && isdigit((unsigned char)szText[i - 1])))
			{
				cQuote = szText[i];
			}
		}
		else if ((szText[i] == '/') && (i + 1 < uiCaret) && ((szText[i + 1] == '/') || (szText[i + 1] == '*')))
		{
			return '/';
		}
	}
	return cQuote;
}


//////////////////////////////////////////////////////////////////////////
//
// An #include's line, from uiLineStart: where its name starts (after its
// quote or angle bracket) if the caret is in the name, before it is
// closed.  *pbInclude is whether the line is an #include's at all, as far
// as the caret: nothing but its name is completed on one.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDEIncludeName(const char* szText, size uiLineStart, size uiCaret, size* puiName, bool* pbSystem, bool* pbInclude)
{
	size	i;
	char	cClose;

	*pbInclude = false;
	for (i = uiLineStart; (i < uiCaret) && ((szText[i] == ' ') || (szText[i] == '\t')); i++)
	{
	}
	if ((i >= uiCaret) || (szText[i] != '#'))
	{
		return false;
	}
	for (i++; (i < uiCaret) && ((szText[i] == ' ') || (szText[i] == '\t')); i++)
	{
	}
	if ((i + 7 > uiCaret) || (strncmp(&szText[i], "include", 7) != 0))
	{
		return false;
	}
	*pbInclude = true;
	for (i += 7; (i < uiCaret) && ((szText[i] == ' ') || (szText[i] == '\t')); i++)
	{
	}
	if ((i >= uiCaret) || ((szText[i] != '"') && (szText[i] != '<')))
	{
		return false;
	}
	*pbSystem = szText[i] == '<';
	cClose = *pbSystem ? '>' : '"';
	*puiName = i + 1;
	for (i++; i < uiCaret; i++)
	{
		if (szText[i] == cClose)
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the brackets that close just before uiClose (a ')' or a ']' is
// at uiClose - 1) open: counted back, a string's own left alone.  False
// if they don't.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDEBracketOpen(const char* szText, size uiClose, size* puiOpen)
{
	size	i;
	int		iDepth;
	char	cQuote;

	iDepth = 0;
	cQuote = 0;
	for (i = uiClose; i > 0; i--)
	{
		if (cQuote != 0)
		{
			if ((szText[i - 1] == cQuote) && ((i < 2) || (szText[i - 2] != '\\')))
			{
				cQuote = 0;
			}
		}
		else if ((szText[i - 1] == '"') || (szText[i - 1] == '\''))
		{
			cQuote = szText[i - 1];
		}
		else if ((szText[i - 1] == ')') || (szText[i - 1] == ']'))
		{
			iDepth++;
		}
		else if ((szText[i - 1] == '(') || (szText[i - 1] == '['))
		{
			iDepth--;
			if (iDepth == 0)
			{
				*puiOpen = i - 1;
				return true;
			}
		}
		else if ((szText[i - 1] == ';') || (szText[i - 1] == '{') || (szText[i - 1] == '}'))
		{
			return false;
		}
	}
	return false;
}


static size SkipIDESpacesBack(const char* szText, size ui)
{
	while ((ui > 0) && ((szText[ui - 1] == ' ') || (szText[ui - 1] == '\t') || (szText[ui - 1] == '\r') || (szText[ui - 1] == '\n')))
	{
		ui--;
	}
	return ui;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the expression that ends at uiEnd starts, as far back as it is
// names, calls, indexings and groups in brackets joined by ".", "->" and
// "::": "pcGauge->GetLimits()[2]", "((CGauge*)pvThing)", "NS::CGauge".
// False if nothing of the kind ends there.
//
//////////////////////////////////////////////////////////////////////////
bool FindIDEChainStart(const char* szText, size uiEnd, size* puiStart)
{
	size	i;
	size	uiOpen;
	size	uiName;
	bool	bPart;

	i = SkipIDESpacesBack(szText, uiEnd);
	for (;;)
	{
		//A part: its indexings and its call's arguments, then its name; or a group, which only the first can be.
		bPart = false;
		while ((i > 0) && (szText[i - 1] == ']'))
		{
			if (!FindIDEBracketOpen(szText, i, &uiOpen))
			{
				return false;
			}
			i = SkipIDESpacesBack(szText, uiOpen);
		}
		if ((i > 0) && (szText[i - 1] == ')'))
		{
			if (!FindIDEBracketOpen(szText, i, &uiOpen))
			{
				return false;
			}
			i = uiOpen;
			bPart = true;
			uiName = SkipIDESpacesBack(szText, i);
			if ((uiName == 0) || !IsIDENameChar(szText[uiName - 1]))
			{
				//A group: nothing joins on before it.
				*puiStart = i;
				return true;
			}
			i = uiName;
		}
		uiName = i;
		while ((i > 0) && IsIDENameChar(szText[i - 1]))
		{
			i--;
		}
		if (i < uiName)
		{
			if (isdigit((unsigned char)szText[i]))
			{
				return false;
			}
			bPart = true;
		}
		if (!bPart)
		{
			return false;
		}

		//What it is joined to, if anything.
		uiName = SkipIDESpacesBack(szText, i);
		if ((uiName > 0) && (szText[uiName - 1] == '.'))
		{
			i = SkipIDESpacesBack(szText, uiName - 1);
		}
		else if ((uiName > 1) && (((szText[uiName - 2] == '-') && (szText[uiName - 1] == '>')) || ((szText[uiName - 2] == ':') && (szText[uiName - 1] == ':'))))
		{
			i = SkipIDESpacesBack(szText, uiName - 2);
			if ((szText[uiName - 1] == ':') && ((i == 0) || !IsIDENameChar(szText[i - 1])))
			{
				//"::name": what is outside any scope.
				*puiStart = uiName - 2;
				return true;
			}
		}
		else
		{
			*puiStart = i;
			return true;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The parts of the expression FindIDEChainStart found, in order.  False
// if it isn't made of them after all (a call of what a template's
// arguments made, an operator between two names).
//
//////////////////////////////////////////////////////////////////////////
bool ParseIDEChain(const char* szText, size uiStart, size uiEnd, CArrayIDEChainLinks* pasDest)
{
	SIDEChainLink*	psLink;
	size			i;
	size			j;
	int				iDepth;
	char			cJoin;
	char			cClose;

	i = uiStart;
	cJoin = '\0';
	for (;;)
	{
		while ((i < uiEnd) && isspace((unsigned char)szText[i]))
		{
			i++;
		}
		if ((cJoin == '\0') && (i + 1 < uiEnd) && (szText[i] == ':') && (szText[i + 1] == ':'))
		{
			//"::name": the first part is the scope outside any, which has no name.
			psLink = pasDest->Add();
			memset(psLink, 0, sizeof(SIDEChainLink));
			psLink->uiStart = i;
			psLink->uiEnd = i;
			cJoin = ':';
			i += 2;
			continue;
		}
		if (i >= uiEnd)
		{
			return false;
		}
		psLink = pasDest->Add();
		memset(psLink, 0, sizeof(SIDEChainLink));
		psLink->cJoin = cJoin;
		psLink->uiStart = i;
		if (szText[i] == '(')
		{
			if (cJoin != '\0')
			{
				return false;
			}
			psLink->bGroup = true;
		}
		else
		{
			while ((i < uiEnd) && IsIDENameChar(szText[i]))
			{
				i++;
			}
			if (i == psLink->uiStart)
			{
				return false;
			}
		}
		psLink->uiEnd = i;

		//Its call's arguments, or its group, and its indexings: to the bracket that closes each.
		for (;;)
		{
			for (j = i; (j < uiEnd) && isspace((unsigned char)szText[j]); j++)
			{
			}
			if ((j >= uiEnd) || ((szText[j] != '(') && (szText[j] != '[')))
			{
				break;
			}
			if ((szText[j] == '(') && (psLink->bCall || (psLink->iIndexes > 0)) && !psLink->bGroup)
			{
				return false;
			}
			cClose = (szText[j] == '(') ? ')' : ']';
			iDepth = 0;
			for (i = j; i < uiEnd; i++)
			{
				if ((szText[i] == '(') || (szText[i] == '['))
				{
					iDepth++;
				}
				else if ((szText[i] == ')') || (szText[i] == ']'))
				{
					iDepth--;
					if (iDepth == 0)
					{
						break;
					}
				}
			}
			if ((i >= uiEnd) || (szText[i] != cClose))
			{
				return false;
			}
			i++;
			if (cClose == ']')
			{
				psLink->iIndexes++;
			}
			else if (psLink->bGroup && (psLink->uiEnd == psLink->uiStart))
			{
				psLink->uiEnd = i;
			}
			else
			{
				psLink->bCall = true;
			}
		}

		while ((i < uiEnd) && isspace((unsigned char)szText[i]))
		{
			i++;
		}
		if (i >= uiEnd)
		{
			return true;
		}
		if (szText[i] == '.')
		{
			cJoin = '.';
			i++;
		}
		else if ((i + 1 < uiEnd) && (szText[i] == '-') && (szText[i + 1] == '>'))
		{
			cJoin = '>';
			i += 2;
		}
		else if ((i + 1 < uiEnd) && (szText[i] == ':') && (szText[i + 1] == ':'))
		{
			cJoin = ':';
			i += 2;
		}
		else
		{
			return false;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What completion would complete with the caret at uiCaret of szText.
// False (ICK_None) where nothing would be: in a comment or a string, in
// a number, after a "." or "->" that no expression is before, or on an
// #include's line outside its name.
//
//////////////////////////////////////////////////////////////////////////
bool FindIDECompletionContext(const char* szText, size uiLength, size uiCaret, SIDECompletionContext* psDest)
{
	size	uiLineStart;
	size	uiBefore;
	char	cState;
	bool	bInclude;

	memset(psDest, 0, sizeof(SIDECompletionContext));
	psDest->eKind = ICK_None;
	if (uiCaret > uiLength)
	{
		return false;
	}
	psDest->uiCaret = uiCaret;
	for (uiLineStart = uiCaret; (uiLineStart > 0) && (szText[uiLineStart - 1] != '\n'); uiLineStart--)
	{
	}

	if (FindIDEIncludeName(szText, uiLineStart, uiCaret, &psDest->uiIncludeStart, &psDest->bSystemInclude, &bInclude))
	{
		//The part of the path being written: after its last slash.
		for (psDest->uiPrefixStart = uiCaret; (psDest->uiPrefixStart > psDest->uiIncludeStart) && (szText[psDest->uiPrefixStart - 1] != '/') && (szText[psDest->uiPrefixStart - 1] != '\\'); psDest->uiPrefixStart--)
		{
		}
		for (psDest->uiWordEnd = uiCaret; (psDest->uiWordEnd < uiLength) && (IsIDENameChar(szText[psDest->uiWordEnd]) || (szText[psDest->uiWordEnd] == '.')); psDest->uiWordEnd++)
		{
		}
		psDest->eKind = ICK_Include;
		return true;
	}

	cState = FindIDELineState(szText, uiLineStart, uiCaret);
	if (bInclude || (cState != 0))
	{
		return false;
	}
	for (psDest->uiPrefixStart = uiCaret; (psDest->uiPrefixStart > uiLineStart) && IsIDENameChar(szText[psDest->uiPrefixStart - 1]); psDest->uiPrefixStart--)
	{
	}
	for (psDest->uiWordEnd = uiCaret; (psDest->uiWordEnd < uiLength) && IsIDENameChar(szText[psDest->uiWordEnd]); psDest->uiWordEnd++)
	{
	}
	if ((psDest->uiPrefixStart < uiCaret) && isdigit((unsigned char)szText[psDest->uiPrefixStart]))
	{
		return false;
	}

	uiBefore = SkipIDESpacesBack(szText, psDest->uiPrefixStart);
	psDest->uiChainStart = uiBefore;
	psDest->uiChainEnd = uiBefore;
	if ((uiBefore > 0) && (szText[uiBefore - 1] == '.'))
	{
		psDest->uiChainEnd = uiBefore - 1;
		psDest->eKind = ICK_Member;
	}
	else if ((uiBefore > 1) && (szText[uiBefore - 2] == '-') && (szText[uiBefore - 1] == '>'))
	{
		psDest->uiChainEnd = uiBefore - 2;
		psDest->bArrow = true;
		psDest->eKind = ICK_Member;
	}
	else if ((uiBefore > 1) && (szText[uiBefore - 2] == ':') && (szText[uiBefore - 1] == ':'))
	{
		psDest->uiChainEnd = uiBefore - 2;
		psDest->eKind = ICK_Scope;
	}
	else
	{
		psDest->eKind = ICK_Name;
		return true;
	}

	if (!FindIDEChainStart(szText, psDest->uiChainEnd, &psDest->uiChainStart))
	{
		psDest->uiChainStart = psDest->uiChainEnd;
		if (psDest->eKind == ICK_Member)
		{
			psDest->eKind = ICK_None;
			return false;
		}
	}
	return true;
}
