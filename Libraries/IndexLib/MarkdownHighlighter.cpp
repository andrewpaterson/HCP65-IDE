/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include <ctype.h>
#include "BaseLib/ArrayInt.h"
#include "MarkdownSpans.h"
#include "MarkdownHighlighter.h"


//C++'s keywords, which a ```cpp block shows as keywords.
static const char*	gaszMarkdownCppKeywords[] =
{
	"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
	"const", "const_cast", "constexpr", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast",
	"else", "enum", "explicit", "export", "extern", "false", "final", "float", "for", "friend", "goto", "if", "inline",
	"int", "long", "mutable", "namespace", "new", "noexcept", "nullptr", "operator", "override", "private", "protected",
	"public", "register", "reinterpret_cast", "return", "short", "signed", "sizeof", "static", "static_assert",
	"static_cast", "struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid",
	"typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while"
};


//////////////////////////////////////////////////////////////////////////
//
// The highlights in line then column order.
//
//////////////////////////////////////////////////////////////////////////
static int CompareMarkdownHighlights(const void* pvLeft, const void* pvRight)
{
	SCPPHighlight*	psLeft;
	SCPPHighlight*	psRight;

	psLeft = (SCPPHighlight*)pvLeft;
	psRight = (SCPPHighlight*)pvRight;
	if (psLeft->iLine != psRight->iLine)
	{
		return (psLeft->iLine < psRight->iLine) ? -1 : 1;
	}
	return psLeft->iColumn - psRight->iColumn;
}


//////////////////////////////////////////////////////////////////////////
//
// The page's highlights added to pacDest, sorted.
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownHighlighter::Highlight(const char* szPage, CArrayPageReferences* pasReferences, CArrayHighlights* pacDest)
{
	CMarkdownSpans		cSpans;
	SMarkdownSpan*		psSpan;
	SPageReference*		psReference;
	const char*			szLine;
	const char*			szEnd;
	CArrayInt			aiHeadings;
	int					iLine;
	int					iLength;
	char				cFence;
	char				cRun;
	int					iFence;
	int					iRun;
	int					iAfter;
	bool				bCpp;
	bool				bCppComment;
	bool				bComment;
	ECPPHighlight		eKind;
	size				i;
	size				j;

	//Line by line: fences, headings and comments.
	aiHeadings.Init();
	cFence = '\0';
	iFence = 0;
	bCpp = false;
	bCppComment = false;
	bComment = false;
	szLine = szPage;
	for (iLine = 0;; iLine++)
	{
		szEnd = strchr(szLine, '\n');
		iLength = szEnd ? (int)(szEnd - szLine) : (int)strlen(szLine);
		iRun = CMarkdownSpans::FindFence(szLine, iLength, &cRun, &iAfter);
		if (iFence > 0)
		{
			if ((iRun > 0) && (cRun == cFence) && (iRun >= iFence) && (strspn(&szLine[iAfter], " \t\r") >= (size)(iLength - iAfter)))
			{
				iFence = 0;
				Add(pacDest, iLine, 0, iLength, CPPH_Directive);
			}
			else if (bCpp)
			{
				HighlightCppLine(iLine, szLine, iLength, &bCppComment, pacDest);
			}
		}
		else if (iRun > 0)
		{
			cFence = cRun;
			iFence = iRun;
			bCpp = IsCppFence(&szLine[iAfter], iLength - iAfter);
			bCppComment = false;
			Add(pacDest, iLine, 0, iLength, CPPH_Directive);
		}
		else if (!bComment && IsHeading(szLine, iLength))
		{
			Add(pacDest, iLine, 0, iLength, CPPH_Keyword);
			aiHeadings.Add(iLine);
		}
		else
		{
			HighlightComments(iLine, szLine, iLength, &bComment, pacDest);
		}

		if (szEnd == NULL)
		{
			break;
		}
		szLine = szEnd + 1;
	}

	//The code spans and references, but on headings (shown as headings).
	cSpans.Init();
	cSpans.Scan(szPage);
	for (i = 0; i < cSpans.NumSpans(); i++)
	{
		psSpan = cSpans.Get(i);
		if (aiHeadings.Find(psSpan->iLine) != ARRAY_ELEMENT_NOT_FOUND)
		{
			continue;
		}
		if (psSpan->eKind != MSK_Reference)
		{
			Add(pacDest, psSpan->iLine, psSpan->iColumn, psSpan->iEnd - psSpan->iColumn, CPPH_String);
			continue;
		}

		//[`Name`]: what it names, or unresolved; a string until the page's references are resolved.
		eKind = CPPH_String;
		for (j = 0; (pasReferences != NULL) && (j < pasReferences->NumElements()); j++)
		{
			psReference = pasReferences->Get(j);
			if ((psReference->iLine == psSpan->iLine) && (psReference->iColumn == psSpan->iColumn - 1) && (psReference->eKind != PRK_Warning))
			{
				eKind = (psReference->eKind == PRK_Error) ? CPPH_Unresolved : GetSymbolHighlight(psReference->iSymbolKind);
				break;
			}
		}
		Add(pacDest, psSpan->iLine, psSpan->iColumn - 1, psSpan->iEnd - psSpan->iColumn + 2, eKind);
	}
	cSpans.Kill();
	aiHeadings.Kill();
	pacDest->QuickSort(CompareMarkdownHighlights);
}


//////////////////////////////////////////////////////////////////////////
//
// A line of a ```cpp block, lexically: comments (a /* */ one can cross
// lines, *pbComment), strings, characters, numbers, keywords and a
// preprocessor line.
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownHighlighter::HighlightCppLine(int iLine, const char* szLine, int iLength, bool* pbComment, CArrayHighlights* pacDest)
{
	int		i;
	int		iEnd;
	char	cQuote;
	bool	bFirst;

	i = 0;
	if (*pbComment)
	{
		for (iEnd = 0; (iEnd + 1 < iLength) && !((szLine[iEnd] == '*') && (szLine[iEnd + 1] == '/')); iEnd++)
		{
		}
		if (iEnd + 1 >= iLength)
		{
			Add(pacDest, iLine, 0, iLength, CPPH_Comment);
			return;
		}
		Add(pacDest, iLine, 0, iEnd + 2, CPPH_Comment);
		*pbComment = false;
		i = iEnd + 2;
	}

	bFirst = true;
	while (i < iLength)
	{
		if ((szLine[i] == ' ') || (szLine[i] == '\t') || (szLine[i] == '\r'))
		{
			i++;
			continue;
		}
		if (bFirst && (szLine[i] == '#'))
		{
			Add(pacDest, iLine, i, iLength - i, CPPH_Directive);
			return;
		}
		bFirst = false;
		if ((szLine[i] == '/') && (i + 1 < iLength) && (szLine[i + 1] == '/'))
		{
			Add(pacDest, iLine, i, iLength - i, CPPH_Comment);
			return;
		}
		if ((szLine[i] == '/') && (i + 1 < iLength) && (szLine[i + 1] == '*'))
		{
			for (iEnd = i + 2; (iEnd + 1 < iLength) && !((szLine[iEnd] == '*') && (szLine[iEnd + 1] == '/')); iEnd++)
			{
			}
			if (iEnd + 1 >= iLength)
			{
				Add(pacDest, iLine, i, iLength - i, CPPH_Comment);
				*pbComment = true;
				return;
			}
			Add(pacDest, iLine, i, iEnd + 2 - i, CPPH_Comment);
			i = iEnd + 2;
			continue;
		}
		if ((szLine[i] == '"') || (szLine[i] == '\''))
		{
			cQuote = szLine[i];
			for (iEnd = i + 1; (iEnd < iLength) && (szLine[iEnd] != cQuote); iEnd++)
			{
				if ((szLine[iEnd] == '\\') && (iEnd + 1 < iLength))
				{
					iEnd++;
				}
			}
			iEnd = (iEnd < iLength) ? iEnd + 1 : iLength;
			Add(pacDest, iLine, i, iEnd - i, (cQuote == '"') ? CPPH_String : CPPH_Character);
			i = iEnd;
			continue;
		}
		if (isdigit((unsigned char)szLine[i]))
		{
			for (iEnd = i; (iEnd < iLength) && (isalnum((unsigned char)szLine[iEnd]) || (szLine[iEnd] == '.') || (szLine[iEnd] == '\'')); iEnd++)
			{
			}
			Add(pacDest, iLine, i, iEnd - i, CPPH_Number);
			i = iEnd;
			continue;
		}
		if (isalpha((unsigned char)szLine[i]) || (szLine[i] == '_'))
		{
			for (iEnd = i; (iEnd < iLength) && (isalnum((unsigned char)szLine[iEnd]) || (szLine[iEnd] == '_')); iEnd++)
			{
			}
			if (IsCppKeyword(&szLine[i], iEnd - i))
			{
				Add(pacDest, iLine, i, iEnd - i, CPPH_Keyword);
			}
			i = iEnd;
			continue;
		}
		i++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A line's HTML comments (one can cross lines, *pbComment).
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownHighlighter::HighlightComments(int iLine, const char* szLine, int iLength, bool* pbComment, CArrayHighlights* pacDest)
{
	int		iStart;
	int		iEnd;

	iStart = 0;
	for (;;)
	{
		if (!*pbComment)
		{
			for (; (iStart + 3 < iLength) && (memcmp(&szLine[iStart], "<!--", 4) != 0); iStart++)
			{
			}
			if (iStart + 3 >= iLength)
			{
				return;
			}
			*pbComment = true;
		}
		for (iEnd = iStart; (iEnd + 2 < iLength) && (memcmp(&szLine[iEnd], "-->", 3) != 0); iEnd++)
		{
		}
		if (iEnd + 2 >= iLength)
		{
			Add(pacDest, iLine, iStart, iLength - iStart, CPPH_Comment);
			return;
		}
		Add(pacDest, iLine, iStart, iEnd + 3 - iStart, CPPH_Comment);
		*pbComment = false;
		iStart = iEnd + 3;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The colour of a reference to a kind of symbol (-1: a file).
//
//////////////////////////////////////////////////////////////////////////
ECPPHighlight CMarkdownHighlighter::GetSymbolHighlight(int iSymbolKind)
{
	switch (iSymbolKind)
	{
	case CPPSK_Class:
	case CPPSK_ClassTemplate:
		return CPPH_Class;
	case CPPSK_Enum:
		return CPPH_Enum;
	case CPPSK_Typedef:
	case CPPSK_AliasTemplate:
		return CPPH_Typedef;
	case CPPSK_Function:
	case CPPSK_FunctionTemplate:
		return CPPH_Function;
	case CPPSK_Field:
		return CPPH_Field;
	case CPPSK_Variable:
	case CPPSK_VariableTemplate:
		return CPPH_Variable;
	case CPPSK_Enumerator:
		return CPPH_Enumerator;
	default:
		return CPPH_String;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A heading: one to six #s at the start of the line, then a space or
// nothing.
//
//////////////////////////////////////////////////////////////////////////
bool CMarkdownHighlighter::IsHeading(const char* szLine, int iLength)
{
	int		i;

	for (i = 0; (i < iLength) && (szLine[i] == '#'); i++)
	{
	}
	return (i >= 1) && (i <= 6) && ((i == iLength) || (szLine[i] == ' ') || (szLine[i] == '\t') || (szLine[i] == '\r'));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a fence's info string (after its backticks) says C or C++:
// cpp, c++, c, h, hpp, or {.cpp ...}.
//
//////////////////////////////////////////////////////////////////////////
bool CMarkdownHighlighter::IsCppFence(const char* szInfo, int iLength)
{
	int		iStart;
	int		iEnd;

	for (iStart = 0; (iStart < iLength) && ((szInfo[iStart] == ' ') || (szInfo[iStart] == '{') || (szInfo[iStart] == '.')); iStart++)
	{
	}
	for (iEnd = iStart; (iEnd < iLength) && (isalnum((unsigned char)szInfo[iEnd]) || (szInfo[iEnd] == '+')); iEnd++)
	{
	}
	return ((iEnd - iStart == 3) && (memcmp(&szInfo[iStart], "cpp", 3) == 0)) || ((iEnd - iStart == 3) && (memcmp(&szInfo[iStart], "c++", 3) == 0)) ||
		   ((iEnd - iStart == 3) && (memcmp(&szInfo[iStart], "hpp", 3) == 0)) || ((iEnd - iStart == 1) && ((szInfo[iStart] == 'c') || (szInfo[iStart] == 'h')));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CMarkdownHighlighter::IsCppKeyword(const char* szWord, int iLength)
{
	size	i;

	for (i = 0; i < sizeof(gaszMarkdownCppKeywords) / sizeof(const char*); i++)
	{
		if (((int)strlen(gaszMarkdownCppKeywords[i]) == iLength) && (memcmp(gaszMarkdownCppKeywords[i], szWord, iLength) == 0))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownHighlighter::Add(CArrayHighlights* pacDest, int iLine, int iColumn, int iLength, ECPPHighlight eKind)
{
	SCPPHighlight*	psHighlight;

	if (iLength <= 0)
	{
		return;
	}
	psHighlight = pacDest->Add();
	psHighlight->iLine = iLine;
	psHighlight->iColumn = iColumn;
	psHighlight->iLength = iLength;
	psHighlight->uiKind = (uint8)eKind;
	psHighlight->uiFlags = 0;
}

