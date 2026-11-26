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
#include <string.h>
#include "CodeFormatter.h"


//The punctuators of more than one character, the longest first.
static const char*	gaszCodePunctuators[] = { "<<=", ">>=", "...", "->*", "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=",
											  "/=", "%=", "&=", "|=", "^=", "##" };

//The house's banner over a function.
static const char	gszCodeBanner[] = "//////////////////////////////////////////////////////////////////////////\n//\n//\n"
									  "//////////////////////////////////////////////////////////////////////////\n";

//Words a declaration doesn't start with, a value doesn't end with, labels, and the operators spaced whatever is
//beside them.
static const char	gszCodeNotDeclaration[] = "return delete throw goto case default else new sizeof typedef friend using if while for switch do break continue public "
											  "private protected template namespace class struct enum union extern operator";
static const char	gszCodeNotValue[] = "return case throw sizeof delete else new typedef";
static const char	gszCodeLabels[] = "case default public private protected";
static const char	gszCodeRecords[] = "class struct union";
static const char	gszCodeSpaced[] = "= == != <= >= || += -= *= /= %= &= |= ^= <<= >>=";


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::Init(void)
{
	mszSource = NULL;
	muiLength = 0;
	mbSourceFile = false;
	mszAdded.Init();
	mszLineEnd.Init("\n");
	maszBanner.Init();
	masTokens.Init();
	masEdits.Init();
	mszEdits.Init();
	memset(mauiCounts, 0, sizeof(mauiCounts));
	SetBanner(gszCodeBanner);
}


void CCodeFormatter::Kill(void)
{
	mszEdits.Kill();
	masEdits.Kill();
	masTokens.Kill();
	maszBanner.Kill();
	mszLineEnd.Kill();
	mszAdded.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The comment put over a function that has none: its lines, each a
// comment.  The house's unless one is given.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::SetBanner(const char* szBanner)
{
	const char*	szLine;
	const char*	szEnd;
	CChars		sz;

	maszBanner.ReInit();
	for (szLine = szBanner; *szLine != '\0'; szLine = (*szEnd == '\n') ? szEnd + 1 : szEnd)
	{
		szEnd = szLine;
		while ((*szEnd != '\0') && (*szEnd != '\n'))
		{
			szEnd++;
		}
		sz.Init();
		sz.AppendSubString(szLine, szEnd);
		if (sz.EndsWith('\r'))
		{
			sz.RemoveFromEnd(1);
		}
		maszBanner.Add(sz.Text());
		sz.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szSource laid out in the house style: GetText is the result, and the
// edits are what make the source it.  A source file's functions have
// banners; a header's don't.  szSource must stay until the formatter is
// asked nothing more.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::Format(const char* szSource, bool bSourceFile)
{
	const char*	sz;
	size		uiLineEnds;
	size		uiReturns;

	mszSource = szSource;
	muiLength = strlen(szSource);
	mbSourceFile = bSourceFile;
	mszAdded.Clear();
	masTokens.ReInit();
	memset(mauiCounts, 0, sizeof(mauiCounts));

	uiLineEnds = 0;
	uiReturns = 0;
	for (sz = szSource; *sz != '\0'; sz++)
	{
		if (*sz == '\n')
		{
			uiLineEnds++;
			uiReturns += ((sz > szSource) && (sz[-1] == '\r')) ? 1 : 0;
		}
	}
	mszLineEnd.Set((uiReturns * 2 > uiLineEnds) ? "\r\n" : "\n");

	Tokenise();
	MoveBraces();
	Indent();
	AddSpaces();
	AlignDeclarations();
	AddBanners();
	RemoveTrailing();
	MakeEdits();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsCodeNameStart(char c) { return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || (c == '_') || (c == '$') || ((uint8)c >= 128); }
static bool IsCodeDigit(char c) { return (c >= '0') && (c <= '9'); }
static bool IsCodeNameChar(char c) { return IsCodeNameStart(c) || IsCodeDigit(c); }
static bool IsCodeLineEnd(const char* sz) { return (sz[0] == '\n') || ((sz[0] == '\r') && (sz[1] == '\n')); }
static bool IsCodeWhiteSpace(const char* sz) { return ((sz[0] == ' ') || (sz[0] == '\t') || (sz[0] == '\r') || (sz[0] == '\f') || (sz[0] == '\v')) && !IsCodeLineEnd(sz); }


//////////////////////////////////////////////////////////////////////////
//
// The source's tokens, every character of it in one.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::Tokenise(void)
{
	const char*		sz;
	const char*		szFound;
	SCodeToken*		psLast;
	size			i;
	size			j;
	size			uiPunctuator;
	size			uiDelimiter;
	bool			bLineStart;
	bool			bHex;
	char			c;
	CChars			szClose;

	sz = mszSource;
	bLineStart = true;
	for (i = 0; i < muiLength; i = j)
	{
		c = sz[i];
		if (IsCodeLineEnd(&sz[i]))
		{
			j = i + ((c == '\r') ? 2 : 1);
			AddToken(CTK_LineEnd, i, j);
			bLineStart = true;
			continue;
		}
		if (IsCodeWhiteSpace(&sz[i]))
		{
			for (j = i; (j < muiLength) && IsCodeWhiteSpace(&sz[j]); j++)
			{
			}
			AddToken(CTK_WhiteSpace, i, j);
			continue;
		}

		if ((c == '/') && (sz[i + 1] == '/'))
		{
			for (j = i; (j < muiLength) && !IsCodeLineEnd(&sz[j]); j++)
			{
			}
			AddToken(CTK_Comment, i, j);
		}
		else if ((c == '/') && (sz[i + 1] == '*'))
		{
			szFound = strstr(&sz[i + 2], "*/");
			j = szFound ? (size)(szFound - sz) + 2 : muiLength;
			AddToken(CTK_BlockComment, i, j);
		}
		else if ((c == '#') && bLineStart)
		{
			//A directive carries on over a line that ends with a backslash, and a block comment in it.
			j = i;
			while (j < muiLength)
			{
				if ((sz[j] == '\\') && IsCodeLineEnd(&sz[j + 1]))
				{
					j += (sz[j + 1] == '\n') ? 2 : 3;
				}
				else if ((sz[j] == '/') && (sz[j + 1] == '*'))
				{
					szFound = strstr(&sz[j + 2], "*/");
					j = szFound ? (size)(szFound - sz) + 2 : muiLength;
				}
				else if (IsCodeLineEnd(&sz[j]))
				{
					break;
				}
				else
				{
					j++;
				}
			}
			AddToken(CTK_Directive, i, j);
		}
		else if ((c == '"') || (c == '\''))
		{
			psLast = (masTokens.NumElements() > 0) ? masTokens.Tail() : NULL;
			if ((c == '"') && psLast && (psLast->eKind == CTK_Identifier) && (psLast->uiStart + psLast->uiLength == i) && (sz[i - 1] == 'R') && (strchr(&sz[i], '(') != NULL))
			{
				//A raw string, R"delimiter( ... )delimiter".
				uiDelimiter = (size)(strchr(&sz[i], '(') - &sz[i]) - 1;
				szClose.Init(")");
				szClose.AppendSubString(&sz[i + 1], uiDelimiter);
				szClose.Append('"');
				szFound = strstr(&sz[i + 1 + uiDelimiter], szClose.Text());
				j = szFound ? (size)(szFound - sz) + szClose.Length() : muiLength;
				szClose.Kill();
			}
			else
			{
				j = i + 1;
				while ((j < muiLength) && (sz[j] != c) && (sz[j] != '\n'))
				{
					j += (sz[j] == '\\') ? 2 : 1;
				}
				j = (j + 1 < muiLength) ? j + 1 : muiLength;
			}
			AddToken((c == '"') ? CTK_String : CTK_Character, i, j);
		}
		else if (IsCodeDigit(c) || ((c == '.') && IsCodeDigit(sz[i + 1])))
		{
			//A number has its exponent's sign in it, unless it is in hexadecimal, where an 'e' is a digit.
			bHex = (c == '0') && ((sz[i + 1] == 'x') || (sz[i + 1] == 'X'));
			j = i + 1;
			while ((j < muiLength) && (IsCodeNameChar(sz[j]) || (sz[j] == '.') || (sz[j] == '\'') ||
				   (((sz[j] == '+') || (sz[j] == '-')) && (strchr("eEpP", sz[j - 1]) != NULL) && !bHex)))
			{
				j++;
			}
			AddToken(CTK_Number, i, j);
		}
		else if (IsCodeNameStart(c))
		{
			for (j = i + 1; (j < muiLength) && IsCodeNameChar(sz[j]); j++)
			{
			}
			AddToken(CTK_Identifier, i, j);
		}
		else
		{
			j = i + 1;
			for (uiPunctuator = 0; uiPunctuator < sizeof(gaszCodePunctuators) / sizeof(gaszCodePunctuators[0]); uiPunctuator++)
			{
				if (strncmp(&sz[i], gaszCodePunctuators[uiPunctuator], strlen(gaszCodePunctuators[uiPunctuator])) == 0)
				{
					j = i + strlen(gaszCodePunctuators[uiPunctuator]);
					break;
				}
			}
			AddToken(CTK_Punctuator, i, j);
		}
		bLineStart = false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::AddToken(ECodeTokenKind eKind, size uiStart, size uiEnd)
{
	SCodeToken*	psToken;

	psToken = masTokens.Add();
	memset(psToken, 0, sizeof(SCodeToken));
	psToken->eKind = eKind;
	psToken->uiStart = uiStart;
	psToken->uiLength = uiEnd - uiStart;
	psToken->iHead = CODE_FORMAT_NONE;
	psToken->iMatch = CODE_FORMAT_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// A token the formatter adds, before the one at iIndex.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::InsertToken(int32 iIndex, ECodeTokenKind eKind, const char* szText, size uiLength)
{
	SCodeToken*	psToken;

	psToken = masTokens.InsertAt((size)iIndex);
	memset(psToken, 0, sizeof(SCodeToken));
	psToken->eKind = eKind;
	psToken->bAdded = true;
	psToken->uiStart = mszAdded.Length();
	psToken->uiLength = uiLength;
	psToken->iHead = CODE_FORMAT_NONE;
	psToken->iMatch = CODE_FORMAT_NONE;
	mszAdded.Append(szText, uiLength);
}


void CCodeFormatter::InsertLineEnd(int32 iIndex) { InsertToken(iIndex, CTK_LineEnd, mszLineEnd.Text(), mszLineEnd.Length()); }


//////////////////////////////////////////////////////////////////////////
//
// A token's text, which doesn't end with a zero: it is uiLength long.
//
//////////////////////////////////////////////////////////////////////////
const char* CCodeFormatter::GetTokenText(int32 iToken)
{
	SCodeToken*	psToken;

	psToken = masTokens.Get((size)iToken);
	return psToken->bAdded ? mszAdded.Text() + psToken->uiStart : mszSource + psToken->uiStart;
}


bool CCodeFormatter::Is(int32 iToken, const char* szText)
{
	size	uiLength;

	uiLength = strlen(szText);
	return (iToken >= 0) && (iToken < (int32)masTokens.NumElements()) && (masTokens.Get((size)iToken)->uiLength == uiLength) && (memcmp(GetTokenText(iToken), szText, uiLength) == 0);
}


bool CCodeFormatter::IsPunctuator(int32 iToken, const char* szText) { return Is(iToken, szText) && (masTokens.Get((size)iToken)->eKind == CTK_Punctuator); }
bool CCodeFormatter::IsWord(int32 iToken, const char* szText) { return Is(iToken, szText) && (masTokens.Get((size)iToken)->eKind == CTK_Identifier); }


//////////////////////////////////////////////////////////////////////////
//
// Whether the token is one of szWords, which are separated by spaces.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsOneOf(int32 iToken, const char* szWords)
{
	const char*	szWord;
	const char*	szEnd;
	const char*	szText;
	size		uiLength;

	if ((iToken < 0) || (iToken >= (int32)masTokens.NumElements()))
	{
		return false;
	}
	szText = GetTokenText(iToken);
	uiLength = masTokens.Get((size)iToken)->uiLength;
	for (szWord = szWords; *szWord != '\0'; szWord = (*szEnd == ' ') ? szEnd + 1 : szEnd)
	{
		szEnd = szWord;
		while ((*szEnd != '\0') && (*szEnd != ' '))
		{
			szEnd++;
		}
		if (((size)(szEnd - szWord) == uiLength) && (memcmp(szWord, szText, uiLength) == 0))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A token of the code itself: not white space, a comment or a directive.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsSignificant(int32 iToken)
{
	return (iToken >= 0) && (iToken < (int32)masTokens.NumElements()) && (masTokens.Get((size)iToken)->eKind >= CTK_String);
}


bool CCodeFormatter::HasLineEnd(int32 iToken)
{
	SCodeToken*	psToken;

	psToken = masTokens.Get((size)iToken);
	if (psToken->eKind == CTK_LineEnd)
	{
		return true;
	}
	return ((psToken->eKind == CTK_BlockComment) || (psToken->eKind == CTK_Directive)) && (memchr(GetTokenText(iToken), '\n', psToken->uiLength) != NULL);
}


int32 CCodeFormatter::PreviousSignificant(int32 iToken)
{
	for (iToken--; (iToken >= 0) && !IsSignificant(iToken); iToken--)
	{
	}
	return iToken;
}


int32 CCodeFormatter::NextSignificant(int32 iToken)
{
	for (iToken++; (iToken < (int32)masTokens.NumElements()) && !IsSignificant(iToken); iToken++)
	{
	}
	return iToken;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether no line ends between the two tokens.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsSameLine(int32 iFirst, int32 iSecond)
{
	int32	i;

	for (i = iFirst; i < iSecond; i++)
	{
		if (HasLineEnd(i))
		{
			return false;
		}
	}
	return true;
}


bool CCodeFormatter::IsFirstOnLine(int32 iToken)
{
	for (iToken--; (iToken >= 0) && (masTokens.Get((size)iToken)->eKind == CTK_WhiteSpace); iToken--)
	{
	}
	return (iToken < 0) || (masTokens.Get((size)iToken)->eKind == CTK_LineEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// The first token of the line the token is on, and the line end after it
// (the number of tokens on the last line).
//
//////////////////////////////////////////////////////////////////////////
int32 CCodeFormatter::FindLineStart(int32 iToken)
{
	while ((iToken > 0) && (masTokens.Get((size)iToken - 1)->eKind != CTK_LineEnd))
	{
		iToken--;
	}
	return iToken;
}


int32 CCodeFormatter::FindLineEnd(int32 iToken)
{
	while ((iToken < (int32)masTokens.NumElements()) && (masTokens.Get((size)iToken)->eKind != CTK_LineEnd))
	{
		iToken++;
	}
	return iToken;
}


//////////////////////////////////////////////////////////////////////////
//
// The column after tokens [iFirst, iEnd) written from iColumn, a tab
// going to the next tab stop.
//
//////////////////////////////////////////////////////////////////////////
int32 CCodeFormatter::FindWidth(int32 iFirst, int32 iEnd, int32 iColumn)
{
	const char*	szText;
	size		uiLength;
	size		i;

	for (; iFirst < iEnd; iFirst++)
	{
		szText = GetTokenText(iFirst);
		uiLength = masTokens.Get((size)iFirst)->uiLength;
		for (i = 0; i < uiLength; i++)
		{
			iColumn = (szText[i] == '\t') ? (iColumn / CODE_FORMAT_TAB_WIDTH + 1) * CODE_FORMAT_TAB_WIDTH : iColumn + 1;
		}
	}
	return iColumn;
}


//////////////////////////////////////////////////////////////////////////
//
// The text of tokens [iFirst, iEnd) appended: all of them, or only the
// white space.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::AppendTokens(int32 iFirst, int32 iEnd, bool bWhiteSpaceOnly, CChars* pszDest)
{
	for (; iFirst < iEnd; iFirst++)
	{
		if (!bWhiteSpaceOnly || (masTokens.Get((size)iFirst)->eKind == CTK_WhiteSpace))
		{
			pszDest->Append(GetTokenText(iFirst), masTokens.Get((size)iFirst)->uiLength);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each token's depth in braces and in brackets and whether it starts a
// statement, and each brace's kind, head and match.  A statement starts
// after a ';' that isn't in brackets, a brace, a label's ':' and, in a
// list, a ','.  A brace opens a list after '=', ',', '(' or return, in an
// enum, and straight inside a list; any other opens a block.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::FindStructure(void)
{
	SCodeToken*	psToken;
	CArrayInt	aiBraces;
	CArrayInt	aiRounds;
	int32		iRounds;
	int32		iFirst;
	int32		iPrevious;
	int32		iOpen;
	int32		iNext;
	int32		i;
	bool		bStart;
	bool		bList;

	aiBraces.Init();
	aiRounds.Init();
	iRounds = 0;
	iFirst = CODE_FORMAT_NONE;
	iPrevious = CODE_FORMAT_NONE;
	bStart = true;
	for (i = 0; i < (int32)masTokens.NumElements(); i++)
	{
		psToken = masTokens.Get((size)i);
		psToken->iDepth = (int32)aiBraces.NumElements();
		psToken->iRounds = iRounds;
		psToken->bStarts = bStart;
		psToken->eBrace = CBK_None;
		psToken->iHead = CODE_FORMAT_NONE;
		psToken->iMatch = CODE_FORMAT_NONE;
		if (!IsSignificant(i))
		{
			continue;
		}
		if (bStart)
		{
			iFirst = i;
			bStart = false;
		}
		if (psToken->eKind != CTK_Punctuator)
		{
			iPrevious = i;
			continue;
		}

		if (Is(i, "{"))
		{
			iNext = NextSignificant(iFirst);
			bList = Is(iPrevious, "=") || Is(iPrevious, ",") || Is(iPrevious, "(") || IsWord(iPrevious, "return") || IsWord(iFirst, "enum") || (IsWord(iFirst, "typedef") && IsWord(iNext, "enum"));
			if (!bList && Is(iPrevious, "{") && (aiBraces.NumElements() > 0))
			{
				bList = masTokens.Get((size)aiBraces.GetValue(aiBraces.NumElements() - 1))->eBrace == CBK_List;
			}
			psToken->eBrace = bList ? CBK_List : CBK_Block;
			psToken->iHead = (iFirst != i) ? iFirst : CODE_FORMAT_NONE;
			aiBraces.Add(i);
			aiRounds.Add(iRounds);
			iRounds = 0;
			bStart = true;
		}
		else if (Is(i, "}"))
		{
			if (aiBraces.NumElements() > 0)
			{
				iOpen = aiBraces.GetValue(aiBraces.NumElements() - 1);
				iRounds = aiRounds.GetValue(aiRounds.NumElements() - 1);
				aiBraces.RemoveTail();
				aiRounds.RemoveTail();
				masTokens.Get((size)iOpen)->iMatch = i;
				psToken->iMatch = iOpen;
				psToken->iDepth = (int32)aiBraces.NumElements();
				bStart = (masTokens.Get((size)iOpen)->eBrace == CBK_Block) && (iRounds == 0);
				if (!bStart && (masTokens.Get((size)iOpen)->iHead != CODE_FORMAT_NONE))
				{
					iFirst = masTokens.Get((size)iOpen)->iHead;
				}
			}
			else
			{
				bStart = true;
			}
		}
		else if (Is(i, "(") || Is(i, "["))
		{
			iRounds++;
		}
		else if (Is(i, ")") || Is(i, "]"))
		{
			iRounds = (iRounds > 0) ? iRounds - 1 : 0;
		}
		else if (Is(i, ";") && (iRounds == 0))
		{
			bStart = true;
		}
		else if (Is(i, ",") && (iRounds == 0) && (aiBraces.NumElements() > 0) && (masTokens.Get((size)aiBraces.GetValue(aiBraces.NumElements() - 1))->eBrace == CBK_List))
		{
			bStart = true;
		}
		else if (Is(i, ":") && (iRounds == 0) && (iFirst != CODE_FORMAT_NONE))
		{
			//A label's: a case's, an access label's, or a name's on its own.
			if (IsLabelWord(iFirst) || ((iPrevious == iFirst) && (masTokens.Get((size)iFirst)->eKind == CTK_Identifier)))
			{
				bStart = true;
			}
		}
		iPrevious = i;
	}
	aiRounds.Kill();
	aiBraces.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A block's opening brace that ends a line of code goes on a line of its
// own, and what follows a block's opening brace on its line goes on the
// next.  Not a block that opens and closes on one line.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::MoveBraces(void)
{
	SCodeToken*	psToken;
	CArrayInt	aiLineEnds;
	int32		iAfter;
	int32		iAt;
	int32		i;

	FindStructure();
	aiLineEnds.Init();
	for (i = 0; i < (int32)masTokens.NumElements(); i++)
	{
		psToken = masTokens.Get((size)i);
		if ((psToken->eBrace != CBK_Block) || !Is(i, "{") || (psToken->iMatch == CODE_FORMAT_NONE) || IsSameLine(i, psToken->iMatch))
		{
			continue;
		}
		if (!IsFirstOnLine(i))
		{
			aiLineEnds.Add(i);
			mauiCounts[CFC_Brace]++;
		}
		for (iAfter = i + 1; (iAfter < (int32)masTokens.NumElements()) && (masTokens.Get((size)iAfter)->eKind == CTK_WhiteSpace); iAfter++)
		{
		}
		if ((iAfter < (int32)masTokens.NumElements()) && (masTokens.Get((size)iAfter)->eKind != CTK_LineEnd) && (masTokens.Get((size)iAfter)->eKind != CTK_Comment))
		{
			aiLineEnds.Add(iAfter);
			mauiCounts[CFC_AfterBrace]++;
		}
	}

	//From the last, so the tokens before each are where they were.
	for (i = (int32)aiLineEnds.NumElements() - 1; i >= 0; i--)
	{
		iAt = aiLineEnds.GetValue((size)i);
		if (masTokens.Get((size)iAt - 1)->eKind == CTK_WhiteSpace)
		{
			iAt--;
			masTokens.RemoveAt((size)iAt);
		}
		InsertLineEnd(iAt);
	}
	aiLineEnds.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the token is case, default or an access keyword, and whether
// that is a label: a ':' ends it before its line does.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsLabelWord(int32 iToken)
{
	return (iToken >= 0) && (masTokens.Get((size)iToken)->eKind == CTK_Identifier) && IsOneOf(iToken, gszCodeLabels);
}


bool CCodeFormatter::IsLabel(int32 iToken)
{
	int32	i;

	for (i = iToken + 1; (i < (int32)masTokens.NumElements()) && (masTokens.Get((size)i)->eKind != CTK_LineEnd); i++)
	{
		if (IsPunctuator(i, ":"))
		{
			return true;
		}
		if (Is(i, ";") || Is(i, "{") || Is(i, "="))
		{
			return false;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What a class has at the margin: a friend, and a macro called by a name
// in capitals (CONSTRUCTABLE(CThing);).
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsAtMargin(int32 iToken)
{
	const char*	szText;
	size		uiLength;
	size		i;

	if (IsWord(iToken, "friend"))
	{
		return true;
	}
	if ((masTokens.Get((size)iToken)->eKind != CTK_Identifier) || !Is(iToken + 1, "("))
	{
		return false;
	}
	szText = GetTokenText(iToken);
	uiLength = masTokens.Get((size)iToken)->uiLength;
	for (i = 0; i < uiLength; i++)
	{
		if ((szText[i] >= 'a') && (szText[i] <= 'z'))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether one of [iFirst, iEnd) is class, struct or union.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::HasRecordWord(int32 iFirst, int32 iEnd)
{
	for (; iFirst < iEnd; iFirst++)
	{
		if ((masTokens.Get((size)iFirst)->eKind == CTK_Identifier) && IsOneOf(iFirst, gszCodeRecords))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the token is straight inside a class's, struct's or union's
// braces.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsInRecord(int32 iToken)
{
	SCodeToken*	psToken;
	int32		iDepth;
	int32		i;

	iDepth = masTokens.Get((size)iToken)->iDepth;
	for (i = iToken - 1; i >= 0; i--)
	{
		psToken = masTokens.Get((size)i);
		if ((psToken->eKind == CTK_Punctuator) && (psToken->iDepth == iDepth - 1) && Is(i, "{"))
		{
			return (psToken->iHead != CODE_FORMAT_NONE) && (psToken->eBrace == CBK_Block) && HasRecordWord(psToken->iHead, i);
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The white space a line starts with made szLead.  iLineStart is the
// line's first token, and the line has more than white space on it.
// False if it is that already.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::SetLead(int32 iLineStart, const char* szLead)
{
	size	uiLength;

	uiLength = strlen(szLead);
	if (masTokens.Get((size)iLineStart)->eKind == CTK_WhiteSpace)
	{
		if ((masTokens.Get((size)iLineStart)->uiLength == uiLength) && (memcmp(GetTokenText(iLineStart), szLead, uiLength) == 0))
		{
			return false;
		}
		masTokens.RemoveAt((size)iLineStart);
	}
	else if (uiLength == 0)
	{
		return false;
	}
	if (uiLength > 0)
	{
		InsertToken(iLineStart, CTK_WhiteSpace, szLead, uiLength);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each line that starts a statement, or is a brace's, a comment's between
// statements or a directive's, is indented by its depth in tabs (a
// directive not at all).  A statement's other lines move as its first
// did, and what else is left is left.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::Indent(void)
{
	SCodeToken*	psToken;
	CArrayInt	aiLines;
	CArrayChars	aszLeads;
	CChars		szOld;
	CChars		szLead;
	CChars		szMovedFrom;
	CChars		szMovedTo;
	int32		iStart;
	int32		iEnd;
	int32		iToken;
	int32		iNext;
	int32		iDepth;
	int32		i;
	bool		bLead;
	bool		bLabel;
	bool		bMoved;
	bool		bOnlyComments;

	FindStructure();
	aiLines.Init();
	aszLeads.Init();
	szMovedFrom.Init();
	szMovedTo.Init();
	bMoved = false;
	for (iStart = 0; iStart <= (int32)masTokens.NumElements(); iStart = iEnd + 1)
	{
		iEnd = FindLineEnd(iStart);
		for (iToken = iStart; (iToken < iEnd) && (masTokens.Get((size)iToken)->eKind == CTK_WhiteSpace); iToken++)
		{
		}
		if (iToken == iEnd)
		{
			continue;
		}
		psToken = masTokens.Get((size)iToken);
		szOld.Init();
		AppendTokens(iStart, iToken, true, &szOld);
		szLead.Init();
		bLead = false;
		if (psToken->eKind == CTK_Directive)
		{
			bLead = true;
		}
		else if ((psToken->eKind == CTK_Comment) || (psToken->eKind == CTK_BlockComment))
		{
			if (psToken->bStarts && (psToken->iRounds == 0))
			{
				//A comment over a label is level with it, if it is written so.
				iDepth = psToken->iDepth;
				iNext = NextSignificant(iToken);
				bOnlyComments = true;
				for (i = iToken + 1; i < iNext; i++)
				{
					bOnlyComments = bOnlyComments && (masTokens.Get((size)i)->eKind <= CTK_Comment);
				}
				szLead.Append('\t', (size)((iDepth > 0) ? iDepth - 1 : 0));
				if (!(IsLabelWord(iNext) && (iNext < (int32)masTokens.NumElements()) && masTokens.Get((size)iNext)->bStarts && bOnlyComments && (iDepth > 0) && szOld.Equals(szLead.Text())))
				{
					szLead.Clear();
					szLead.Append('\t', (size)iDepth);
				}
				bLead = true;
			}
		}
		else if (Is(iToken, "{") || Is(iToken, "}"))
		{
			if (Is(iToken, "}") || psToken->bStarts || (psToken->eBrace == CBK_Block))
			{
				szLead.Append('\t', (size)psToken->iDepth);
				bLead = true;
				bMoved = false;
			}
		}
		else if (psToken->bStarts && (psToken->iRounds == 0))
		{
			iDepth = psToken->iDepth;
			bLabel = IsLabelWord(iToken) && (iDepth > 0) && IsLabel(iToken);
			if (bLabel)
			{
				iDepth--;
			}
			szLead.Append('\t', (size)iDepth);
			if (!bLabel && IsInRecord(iToken) && ((IsAtMargin(iToken) && szOld.Empty()) || (szOld.StartsWith(szLead.Text()) && (strspn(szOld.Text(), "\t") == szOld.Length()))))
			{
				//A class's friends and macros at the margin, and its members indented further (their types in a column
				//after "virtual"), stay where they are.
				szLead.Set(szOld);
			}
			bLead = true;
			bMoved = true;
			szMovedFrom.Set(szOld);
			szMovedTo.Set(szLead);
		}
		else if (bMoved && !szMovedFrom.Equals(szMovedTo.Text()) && szOld.StartsWith(szMovedFrom.Text()))
		{
			szLead.Append(szMovedTo);
			szLead.Append(szOld.Text() + szMovedFrom.Length());
			bLead = true;
		}

		if (bLead && !szLead.Equals(szOld.Text()))
		{
			aiLines.Add(iStart);
			aszLeads.Add(szLead.Text());
			mauiCounts[CFC_Indent]++;
		}
		szLead.Kill();
		szOld.Kill();
	}

	for (i = (int32)aiLines.NumElements() - 1; i >= 0; i--)
	{
		SetLead(aiLines.GetValue((size)i), aszLeads.GetText((size)i));
	}
	szMovedTo.Kill();
	szMovedFrom.Kill();
	aszLeads.Kill();
	aiLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether an operator after the token has a value on its left: a number,
// a character, a name that isn't a keyword an expression follows, or a
// closing bracket.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::IsValue(int32 iToken)
{
	SCodeToken*	psToken;

	psToken = masTokens.Get((size)iToken);
	if ((psToken->eKind == CTK_Number) || (psToken->eKind == CTK_Character))
	{
		return true;
	}
	if (psToken->eKind == CTK_Identifier)
	{
		return !IsOneOf(iToken, gszCodeNotValue);
	}
	return (psToken->eKind == CTK_Punctuator) && (Is(iToken, ")") || Is(iToken, "]"));
}


//////////////////////////////////////////////////////////////////////////
//
// A space where the house style has one and the line has none: after a
// comma and a for's ';', after if, for, while, switch and return before a
// bracket, and
// beside the operators that are always binary.  A + or - is spaced only
// where it has a value straight before it and something straight after,
// as a sign has neither; an = after a bracket, const or override (a pure
// function's "=0") is left.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::AddSpaces(void)
{
	SCodeToken*	psToken;
	SCodeToken*	psNext;
	CArrayInt	aiSpaces;
	int32		iPrevious;
	int32		iLast;
	int32		i;
	bool		bTightBefore;
	bool		bTightAfter;
	bool		bSpaced;
	bool		bBinary;
	bool		bSign;

	FindStructure();
	aiSpaces.Init();
	for (i = 0; i + 1 < (int32)masTokens.NumElements(); i++)
	{
		if (!IsSignificant(i))
		{
			continue;
		}
		psToken = masTokens.Get((size)i);
		psNext = masTokens.Get((size)i + 1);
		if (IsPunctuator(i, ",") || (IsPunctuator(i, ";") && (psToken->iRounds > 0)))
		{
			//A ';' in brackets is a for's.
			if (IsSignificant(i + 1) && !IsOneOf(i + 1, ") > ] ;"))
			{
				aiSpaces.Add(i + 1);
				mauiCounts[CFC_Comma]++;
			}
			continue;
		}
		if (psToken->eKind == CTK_Identifier)
		{
			if (IsOneOf(i, "if for while switch return") && IsPunctuator(i + 1, "("))
			{
				aiSpaces.Add(i + 1);
				mauiCounts[CFC_Keyword]++;
			}
			continue;
		}
		if (psToken->eKind != CTK_Punctuator)
		{
			continue;
		}
		bSpaced = IsOneOf(i, gszCodeSpaced);
		bBinary = IsOneOf(i, "/ % | ^");
		bSign = IsOneOf(i, "+ -");
		iPrevious = PreviousSignificant(i);
		if ((!bSpaced && !bBinary && !bSign) || (iPrevious < 0) || (i == 0) || IsWord(iPrevious, "operator"))
		{
			continue;
		}
		bTightBefore = IsSignificant(i - 1);
		bTightAfter = IsSignificant(i + 1);
		if (bSpaced)
		{
			if (Is(i, "=") && (IsOneOf(iPrevious, "[ ) ( , const override final") || ((masTokens.Get((size)iPrevious)->eKind == CTK_Punctuator) && IsOneOf(iPrevious, "= ! < > + - * / & | ^ %"))))
			{
				continue;
			}
			if ((psNext->eKind == CTK_Punctuator) && IsOneOf(i + 1, ") ] , ; >"))
			{
				continue;
			}
		}
		else if (bBinary)
		{
			if (!bTightBefore || !bTightAfter || !IsValue(iPrevious) || ((psNext->eKind == CTK_Punctuator) && !IsOneOf(i + 1, "( - ! ~ * &")))
			{
				continue;
			}
		}
		else if (!bTightBefore || !bTightAfter || !IsValue(iPrevious) || Is(iPrevious, ")") || ((psNext->eKind == CTK_Punctuator) && !Is(i + 1, "(")))
		{
			continue;
		}

		//The space before one operator can be the space after the last.
		iLast = (aiSpaces.NumElements() > 0) ? aiSpaces.GetValue(aiSpaces.NumElements() - 1) : CODE_FORMAT_NONE;
		if (bTightBefore && (iLast != i))
		{
			aiSpaces.Add(i);
		}
		if (bTightAfter)
		{
			aiSpaces.Add(i + 1);
		}
		if (bTightBefore || bTightAfter)
		{
			mauiCounts[CFC_Operator]++;
		}
	}

	for (i = (int32)aiSpaces.NumElements() - 1; i >= 0; i--)
	{
		InsertToken(aiSpaces.GetValue((size)i), CTK_WhiteSpace, " ", 1);
	}
	aiSpaces.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the line [iLineStart, iLineEnd) is a declaration on its own, and
// where its parts are: it starts a statement that ends with the line's
// ';', and is words (with ::, a template's arguments, stars) and then a
// name before a ';', '=', '(', '[', ',' or ':'.  One word and a bracket is
// a call, not a declaration.
//
//////////////////////////////////////////////////////////////////////////
bool CCodeFormatter::FindDeclaration(int32 iLineStart, int32 iLineEnd, SCodeDeclaration* psDest)
{
	SCodeToken*	psToken;
	int32		iFirst;
	int32		iLast;
	int32		iName;
	int32		iStars;
	int32		iTypeEnd;
	int32		iBefore;
	int32		iQualifier;
	int32		iAngles;
	int32		iUnits;
	int32		i;
	bool		bQualified;
	bool		bOperator;

	for (iFirst = iLineStart; (iFirst < iLineEnd) && (masTokens.Get((size)iFirst)->eKind == CTK_WhiteSpace); iFirst++)
	{
	}
	if (iFirst == iLineEnd)
	{
		return false;
	}
	psToken = masTokens.Get((size)iFirst);
	if (!psToken->bStarts || (psToken->iRounds != 0) || (psToken->eKind != CTK_Identifier) || IsOneOf(iFirst, gszCodeNotDeclaration))
	{
		return false;
	}
	for (iLast = iLineEnd - 1; (iLast > iFirst) && ((masTokens.Get((size)iLast)->eKind == CTK_WhiteSpace) || (masTokens.Get((size)iLast)->eKind == CTK_Comment)); iLast--)
	{
	}
	if (!IsPunctuator(iLast, ";"))
	{
		return false;
	}

	iAngles = 0;
	iUnits = 0;
	iName = CODE_FORMAT_NONE;
	bQualified = false;
	bOperator = false;
	for (i = iFirst; i <= iLast; i++)
	{
		psToken = masTokens.Get((size)i);
		if (psToken->eKind == CTK_WhiteSpace)
		{
			continue;
		}
		if (iAngles > 0)
		{
			//A template's arguments are part of the type.
			if (Is(i, "<"))
			{
				iAngles++;
			}
			else if (Is(i, ">"))
			{
				iAngles--;
			}
			else if (Is(i, ">>"))
			{
				iAngles -= 2;
			}
			else if ((psToken->eKind == CTK_Punctuator) && IsOneOf(i, "; { } ( )"))
			{
				return false;
			}
			continue;
		}
		if (psToken->eKind < CTK_Identifier)
		{
			return false;
		}
		if (psToken->eKind == CTK_Identifier)
		{
			if (Is(i, "operator"))
			{
				iName = i;
				bOperator = true;
				break;
			}
			if (!bQualified)
			{
				iUnits++;
				iName = i;
			}
			bQualified = false;
		}
		else if (Is(i, "::"))
		{
			bQualified = true;
		}
		else if (Is(i, "<") && (iName == PreviousSignificant(i)))
		{
			iAngles = 1;
		}
		else if (IsOneOf(i, "; = ( [ , :"))
		{
			break;
		}
		else if (!IsOneOf(i, "* & &&"))
		{
			return false;
		}
	}
	if ((iName == CODE_FORMAT_NONE) || (iUnits < 2) || (!bOperator && (masTokens.Get((size)PreviousSignificant(i))->eKind != CTK_Identifier)))
	{
		return false;
	}

	if (!bOperator)
	{
		//The name is the last word, with any qualifier before it on the line; a pointer to a member's star is in its
		//name (int CThing::*piMember).
		iName = PreviousSignificant(i);
		for (;;)
		{
			iBefore = PreviousSignificant(iName);
			if (Is(iBefore, "*") && Is(PreviousSignificant(iBefore), "::"))
			{
				iBefore = PreviousSignificant(iBefore);
			}
			if (!Is(iBefore, "::") || !IsSameLine(iBefore, iName))
			{
				break;
			}
			iQualifier = PreviousSignificant(iBefore);
			if ((iQualifier < iFirst) || (masTokens.Get((size)iQualifier)->eKind != CTK_Identifier))
			{
				break;
			}
			iName = iQualifier;
		}
	}

	//Stars and ampersands before the name are the type's.
	iStars = iName;
	for (;;)
	{
		for (iBefore = iStars - 1; (iBefore > iFirst) && (masTokens.Get((size)iBefore)->eKind == CTK_WhiteSpace); iBefore--)
		{
		}
		if ((iBefore <= iFirst) || (masTokens.Get((size)iBefore)->eKind != CTK_Punctuator) || !IsOneOf(iBefore, "* & &&"))
		{
			break;
		}
		iStars = iBefore;
	}
	for (iTypeEnd = iStars - 1; (iTypeEnd > iFirst) && (masTokens.Get((size)iTypeEnd)->eKind == CTK_WhiteSpace); iTypeEnd--)
	{
	}
	if ((iTypeEnd < iFirst) || ((masTokens.Get((size)iTypeEnd)->eKind == CTK_Identifier) && IsOneOf(iTypeEnd, gszCodeNotDeclaration) && !IsOneOf(iTypeEnd, gszCodeRecords)))
	{
		return false;
	}
	psDest->iFirst = iFirst;
	psDest->iTypeEnd = iTypeEnd;
	psDest->iStars = iStars;
	psDest->iName = iName;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each run of declaration lines inside braces, one straight after another
// at one depth, has its names put in a column (AlignRun).
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::AlignDeclarations(void)
{
	CArrayCodeDeclarations	asRun;
	SCodeDeclaration		sDeclaration;
	CArrayInt				aiStarts;
	CArrayInt				aiEnds;
	CArrayChars				aszTexts;
	SCodeToken*				psToken;
	int32					iStart;
	int32					iEnd;
	int32					iStar;
	int32					i;
	bool					bDeclaration;

	FindStructure();
	asRun.Init();
	aiStarts.Init();
	aiEnds.Init();
	aszTexts.Init();
	for (iStart = 0; iStart <= (int32)masTokens.NumElements(); iStart = iEnd + 1)
	{
		iEnd = FindLineEnd(iStart);
		bDeclaration = FindDeclaration(iStart, iEnd, &sDeclaration) && (masTokens.Get((size)sDeclaration.iFirst)->iDepth > 0);
		if (bDeclaration && (asRun.NumElements() > 0) && (masTokens.Get((size)asRun.Tail()->iFirst)->iDepth != masTokens.Get((size)sDeclaration.iFirst)->iDepth))
		{
			AlignRun(&asRun, &aiStarts, &aiEnds, &aszTexts);
			asRun.ReInit();
		}
		if (bDeclaration)
		{
			asRun.Add(&sDeclaration);
		}
		else if (asRun.NumElements() > 0)
		{
			AlignRun(&asRun, &aiStarts, &aiEnds, &aszTexts);
			asRun.ReInit();
		}
	}
	if (asRun.NumElements() > 0)
	{
		AlignRun(&asRun, &aiStarts, &aiEnds, &aszTexts);
	}

	//Each change: what is between a type and its name becomes its stars and then the white space, from the last.
	for (i = (int32)aiStarts.NumElements() - 1; i >= 0; i--)
	{
		iStart = aiStarts.GetValue((size)i);
		iEnd = aiEnds.GetValue((size)i);
		iStar = iStart;
		while (iStar < iEnd)
		{
			psToken = masTokens.Get((size)iStar);
			if (psToken->eKind == CTK_WhiteSpace)
			{
				masTokens.RemoveAt((size)iStar);
				iEnd--;
			}
			else
			{
				iStar++;
			}
		}
		InsertToken(iEnd, CTK_WhiteSpace, aszTexts.GetText((size)i), strlen(aszTexts.GetText((size)i)));
	}
	aszTexts.Kill();
	aiEnds.Kill();
	aiStarts.Kill();
	asRun.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A run's names are put in one column: the commonest of the columns that
// its names reached with tabs are in already, or with none the first tab
// stop after its longest type.  A type that reaches the column has one tab
// after it; if most then end up in another column, that is the column.
// The changes are added to paiStarts, paiEnds (the tokens between a type
// and its name) and paszTexts (the white space they become).
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::AlignRun(CArrayCodeDeclarations* pasRun, CArrayInt* paiStarts, CArrayInt* paiEnds, CArrayChars* paszTexts)
{
	SCodeDeclaration*	psDeclaration;
	CArrayInt			aiTypeEnds;		//The column each type ends in, its stars against it.
	CArrayInt			aiColumns;
	CArrayInt			aiCounts;
	CChars				szSeparator;
	CChars				szStars;
	CChars				szOld;
	CChars				szNew;
	int32				iLineStart;
	int32				iTypeEnd;
	int32				iNameColumn;
	int32				iColumn;
	int32				iWant;
	int32				iSettled;
	int32				iBest;
	int32				iIndex;
	size				i;
	size				j;
	bool				bSettled;

	aiTypeEnds.Init();
	aiColumns.Init();
	aiCounts.Init();
	iWant = CODE_FORMAT_NONE;
	iBest = 0;
	for (i = 0; i < pasRun->NumElements(); i++)
	{
		psDeclaration = pasRun->Get(i);
		iLineStart = FindLineStart(psDeclaration->iFirst);
		szStars.Init();
		for (j = (size)psDeclaration->iStars; j < (size)psDeclaration->iName; j++)
		{
			if (masTokens.Get(j)->eKind != CTK_WhiteSpace)
			{
				szStars.Append(GetTokenText((int32)j), masTokens.Get(j)->uiLength);
			}
		}
		iTypeEnd = FindWidth(iLineStart, psDeclaration->iTypeEnd + 1, 0) + (int32)szStars.Length();
		aiTypeEnds.Add(iTypeEnd);
		szStars.Kill();

		//A name reached with tabs votes for the column it is in.
		szSeparator.Init();
		AppendTokens(psDeclaration->iTypeEnd + 1, psDeclaration->iName, true, &szSeparator);
		iNameColumn = FindWidth(iLineStart, psDeclaration->iName, 0);
		if (!szSeparator.Empty() && (strspn(szSeparator.Text(), "\t") == szSeparator.Length()) && (iNameColumn % CODE_FORMAT_TAB_WIDTH == 0))
		{
			iIndex = aiColumns.Find(iNameColumn);
			if (iIndex < 0)
			{
				aiColumns.Add(iNameColumn);
				aiCounts.Add(0);
				iIndex = (int32)aiColumns.NumElements() - 1;
			}
			aiCounts.SetValue((size)iIndex, aiCounts.GetValue((size)iIndex) + 1);
		}
		szSeparator.Kill();
	}
	for (i = 0; i < aiColumns.NumElements(); i++)
	{
		if ((aiCounts.GetValue(i) > iBest) || ((aiCounts.GetValue(i) == iBest) && (aiColumns.GetValue(i) > iWant)))
		{
			iBest = aiCounts.GetValue(i);
			iWant = aiColumns.GetValue(i);
		}
	}
	if ((iWant == CODE_FORMAT_NONE) && (pasRun->NumElements() > 1))
	{
		for (i = 0; i < aiTypeEnds.NumElements(); i++)
		{
			iColumn = (aiTypeEnds.GetValue(i) / CODE_FORMAT_TAB_WIDTH + 1) * CODE_FORMAT_TAB_WIDTH;
			iWant = (iColumn > iWant) ? iColumn : iWant;
		}
	}

	//Lined up there, the commonest column must still be that one, or formatting again would move them again.
	bSettled = (iWant == CODE_FORMAT_NONE);
	while (!bSettled)
	{
		aiColumns.ReInit();
		aiCounts.ReInit();
		iSettled = CODE_FORMAT_NONE;
		iBest = 0;
		for (i = 0; i < aiTypeEnds.NumElements(); i++)
		{
			iColumn = (aiTypeEnds.GetValue(i) < iWant) ? iWant : (aiTypeEnds.GetValue(i) / CODE_FORMAT_TAB_WIDTH + 1) * CODE_FORMAT_TAB_WIDTH;
			iIndex = aiColumns.Find(iColumn);
			if (iIndex < 0)
			{
				aiColumns.Add(iColumn);
				aiCounts.Add(0);
				iIndex = (int32)aiColumns.NumElements() - 1;
			}
			aiCounts.SetValue((size)iIndex, aiCounts.GetValue((size)iIndex) + 1);
		}
		for (i = 0; i < aiColumns.NumElements(); i++)
		{
			if ((aiCounts.GetValue(i) > iBest) || ((aiCounts.GetValue(i) == iBest) && (aiColumns.GetValue(i) > iSettled)))
			{
				iBest = aiCounts.GetValue(i);
				iSettled = aiColumns.GetValue(i);
			}
		}
		bSettled = (iSettled == iWant);
		iWant = iSettled;
	}

	for (i = 0; i < pasRun->NumElements(); i++)
	{
		psDeclaration = pasRun->Get(i);
		szStars.Init();
		for (j = (size)psDeclaration->iStars; j < (size)psDeclaration->iName; j++)
		{
			if (masTokens.Get(j)->eKind != CTK_WhiteSpace)
			{
				szStars.Append(GetTokenText((int32)j), masTokens.Get(j)->uiLength);
			}
		}
		szSeparator.Init();
		if (iWant == CODE_FORMAT_NONE)
		{
			//On its own, written with a space: it keeps the white space before its name, or what was before its stars,
			//or has a space if it had none.
			for (j = (size)psDeclaration->iName; (j > (size)psDeclaration->iTypeEnd + 1) && (masTokens.Get(j - 1)->eKind == CTK_WhiteSpace); j--)
			{
			}
			AppendTokens((int32)j, psDeclaration->iName, true, &szSeparator);
			if (szSeparator.Empty())
			{
				AppendTokens(psDeclaration->iTypeEnd + 1, psDeclaration->iName, true, &szSeparator);
			}
			if (szSeparator.Empty())
			{
				szSeparator.Append(' ');
			}
		}
		else
		{
			szSeparator.Append('\t');
			for (iColumn = (aiTypeEnds.GetValue(i) / CODE_FORMAT_TAB_WIDTH + 1) * CODE_FORMAT_TAB_WIDTH; iColumn < iWant; iColumn += CODE_FORMAT_TAB_WIDTH)
			{
				szSeparator.Append('\t');
			}
		}
		szOld.Init();
		AppendTokens(psDeclaration->iTypeEnd + 1, psDeclaration->iName, false, &szOld);
		szNew.Init(szStars);
		szNew.Append(szSeparator);
		if (!szNew.Equals(szOld.Text()))
		{
			paiStarts->Add(psDeclaration->iTypeEnd + 1);
			paiEnds->Add(psDeclaration->iName);
			paszTexts->Add(szSeparator.Text());
			if (!szOld.StartsWith(szStars.Text()))
			{
				mauiCounts[CFC_Star]++;
			}
			szOld.Clear();
			AppendTokens(psDeclaration->iTypeEnd + 1, psDeclaration->iName, true, &szOld);
			if (!szOld.Equals(szSeparator.Text()))
			{
				mauiCounts[CFC_Column]++;
			}
		}
		szNew.Kill();
		szOld.Kill();
		szSeparator.Kill();
		szStars.Kill();
	}
	aiCounts.Kill();
	aiColumns.Kill();
	aiTypeEnds.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// In a source file, a function defined at the top level over more than
// one line, with no comment on the line above it, has the banner put over
// it, two blank lines above that (unless it starts the file).
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::AddBanners(void)
{
	SCodeToken*	psToken;
	CArrayInt	aiFunctions;
	int32		iHead;
	int32		iPrevious;
	int32		iStart;
	int32		iLine;
	int32		iAbove;
	int32		iBlank;
	int32		iAt;
	int32		i;
	int32		j;
	size		uiLine;
	bool		bBlank;
	bool		bRecord;

	if (!mbSourceFile)
	{
		return;
	}
	FindStructure();
	aiFunctions.Init();
	for (i = 0; i < (int32)masTokens.NumElements(); i++)
	{
		psToken = masTokens.Get((size)i);
		if ((psToken->eBrace != CBK_Block) || !Is(i, "{") || (psToken->iDepth != 0) || (psToken->iMatch == CODE_FORMAT_NONE) || IsSameLine(i, psToken->iMatch))
		{
			continue;
		}
		iHead = psToken->iHead;
		iPrevious = PreviousSignificant(i);
		if ((iHead == CODE_FORMAT_NONE) || (iPrevious < 0) || IsOneOf(iHead, "class struct union enum namespace extern typedef"))
		{
			continue;
		}
		if (!Is(iPrevious, ")") && !((masTokens.Get((size)iPrevious)->eKind == CTK_Identifier) && IsOneOf(iPrevious, "const override final noexcept")))
		{
			continue;
		}
		//A record defined with a body isn't a function: "static struct SName { ... } gsName;".
		bRecord = HasRecordWord(iHead, i);
		for (j = iHead; bRecord && (j < i); j++)
		{
			bRecord = !Is(j, "(");
		}
		if (!bRecord)
		{
			aiFunctions.Add(iHead);
		}
	}

	for (i = (int32)aiFunctions.NumElements() - 1; i >= 0; i--)
	{
		iHead = aiFunctions.GetValue((size)i);
		iStart = FindLineStart(iHead);
		for (j = iStart; (j < iHead) && (masTokens.Get((size)j)->eKind == CTK_WhiteSpace); j++)
		{
		}
		if (j != iHead)
		{
			continue;
		}
		if (iStart > 0)
		{
			//A comment alone on the line above is over the function: a banner, or what it has for one.
			iAbove = FindLineStart(iStart - 1);
			for (j = iAbove; (j < iStart - 1) && (masTokens.Get((size)j)->eKind == CTK_WhiteSpace); j++)
			{
			}
			if ((j < iStart - 1) && (masTokens.Get((size)j)->eKind == CTK_Comment))
			{
				continue;
			}
		}

		//How many blank lines are above it, and whether anything is above those.
		iBlank = 0;
		iLine = iStart;
		while (iLine > 0)
		{
			iAbove = FindLineStart(iLine - 1);
			bBlank = true;
			for (j = iAbove; j < iLine - 1; j++)
			{
				bBlank = bBlank && (masTokens.Get((size)j)->eKind == CTK_WhiteSpace);
			}
			if (!bBlank)
			{
				break;
			}
			iBlank++;
			iLine = iAbove;
		}

		iAt = iStart;
		for (; (iLine > 0) && (iBlank < 2); iBlank++)
		{
			InsertLineEnd(iAt);
			iAt++;
		}
		for (uiLine = 0; uiLine < maszBanner.NumElements(); uiLine++)
		{
			InsertToken(iAt, CTK_Comment, maszBanner.GetText(uiLine), strlen(maszBanner.GetText(uiLine)));
			InsertLineEnd(iAt + 1);
			iAt += 2;
		}
		mauiCounts[CFC_Banner]++;
	}
	aiFunctions.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// White space at a line's end goes, a comment's too.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::RemoveTrailing(void)
{
	SCodeToken*	psToken;
	const char*	szText;
	int32		i;
	bool		bEnd;

	for (i = (int32)masTokens.NumElements() - 1; i >= 0; i--)
	{
		bEnd = (i + 1 == (int32)masTokens.NumElements()) || (masTokens.Get((size)i + 1)->eKind == CTK_LineEnd);
		psToken = masTokens.Get((size)i);
		if (bEnd && (psToken->eKind == CTK_WhiteSpace))
		{
			masTokens.RemoveAt((size)i);
			mauiCounts[CFC_Trailing]++;
		}
		else if (bEnd && (psToken->eKind == CTK_Comment))
		{
			szText = GetTokenText(i);
			if ((szText[psToken->uiLength - 1] == ' ') || (szText[psToken->uiLength - 1] == '\t'))
			{
				while ((psToken->uiLength > 0) && ((szText[psToken->uiLength - 1] == ' ') || (szText[psToken->uiLength - 1] == '\t')))
				{
					psToken->uiLength--;
				}
				mauiCounts[CFC_Trailing]++;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The edits that make the source the formatted text.  The source's tokens
// that aren't white space or line ends are all still there, in order: what
// is between two of them in the source becomes what is between them now,
// where that is different, and only the part of it that is.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::MakeEdits(void)
{
	SCodeToken*	psToken;
	SCodeEdit*	psEdit;
	CChars		szGap;
	size		uiSourceEnd;	//Where the last of the source's tokens kept ends in the source.
	size		uiNext;
	size		uiOld;
	size		uiNew;
	size		uiSame;
	size		uiTail;
	size		i;

	masEdits.ReInit();
	mszEdits.Clear();
	szGap.Init();
	uiSourceEnd = 0;
	for (i = 0; i <= masTokens.NumElements(); i++)
	{
		psToken = (i < masTokens.NumElements()) ? masTokens.Get(i) : NULL;
		if (psToken && (psToken->bAdded || (psToken->eKind == CTK_WhiteSpace) || (psToken->eKind == CTK_LineEnd)))
		{
			szGap.Append(GetTokenText((int32)i), psToken->uiLength);
			continue;
		}
		uiNext = psToken ? psToken->uiStart : muiLength;
		if ((szGap.Length() != uiNext - uiSourceEnd) || (memcmp(szGap.Text(), mszSource + uiSourceEnd, szGap.Length()) != 0))
		{
			//Only what differs: what the two start with and end with alike is left out of the edit.
			uiOld = uiNext - uiSourceEnd;
			uiNew = szGap.Length();
			for (uiSame = 0; (uiSame < uiOld) && (uiSame < uiNew) && (mszSource[uiSourceEnd + uiSame] == szGap.Text()[uiSame]); uiSame++)
			{
			}
			for (uiTail = 0; (uiTail < uiOld - uiSame) && (uiTail < uiNew - uiSame) && (mszSource[uiNext - 1 - uiTail] == szGap.Text()[uiNew - 1 - uiTail]); uiTail++)
			{
			}
			psEdit = masEdits.Add();
			psEdit->uiStart = uiSourceEnd + uiSame;
			psEdit->uiEnd = uiNext - uiTail;
			psEdit->uiText = mszEdits.Length();
			psEdit->uiLength = uiNew - uiSame - uiTail;
			mszEdits.Append(szGap.Text() + uiSame, psEdit->uiLength);
		}
		szGap.Clear();
		if (psToken)
		{
			//A comment that lost its trailing white space is shorter than it is in the source.
			uiSourceEnd = psToken->uiStart + psToken->uiLength;
		}
	}
	szGap.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The formatted text, appended.
//
//////////////////////////////////////////////////////////////////////////
void CCodeFormatter::GetText(CChars* pszDest)
{
	size	i;

	for (i = 0; i < masTokens.NumElements(); i++)
	{
		pszDest->Append(GetTokenText((int32)i), masTokens.Get(i)->uiLength);
	}
}


bool CCodeFormatter::IsChanged(void) { return masEdits.NumElements() > 0; }
size CCodeFormatter::NumEdits(void) { return masEdits.NumElements(); }
SCodeEdit* CCodeFormatter::GetEdit(size uiIndex) { return masEdits.Get(uiIndex); }
const char* CCodeFormatter::GetEditText(SCodeEdit* psEdit) { return mszEdits.Text() + psEdit->uiText; }
size CCodeFormatter::GetCount(ECodeFormatChange eChange) const { return mauiCounts[eChange]; }


size CCodeFormatter::NumChanges(void) const
{
	size	uiChanges;
	size	i;

	uiChanges = 0;
	for (i = 0; i < CFC_NumChanges; i++)
	{
		uiChanges += mauiCounts[i];
	}
	return uiChanges;
}

