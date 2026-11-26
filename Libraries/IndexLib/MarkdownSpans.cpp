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
#include "MarkdownSpans.h"


//////////////////////////////////////////////////////////////////////////
//
// White space as Python's \s has it.
//
//////////////////////////////////////////////////////////////////////////
static bool IsMarkdownSpace(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f') || (c == '\v');
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szText[iStart, iEnd) is only white space.
//
//////////////////////////////////////////////////////////////////////////
static bool IsMarkdownBlank(const char* szText, int iStart, int iEnd)
{
	int		i;

	for (i = iStart; i < iEnd; i++)
	{
		if (!IsMarkdownSpace(szText[i]))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where szFind is in szText[iStart, iLength), -1 if it isn't.
//
//////////////////////////////////////////////////////////////////////////
static int FindMarkdownText(const char* szText, int iLength, int iStart, const char* szFind)
{
	int		iFind;
	int		i;

	iFind = (int)strlen(szFind);
	for (i = iStart; i + iFind <= iLength; i++)
	{
		if (memcmp(&szText[i], szFind, iFind) == 0)
		{
			return i;
		}
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownSpans::Init(void)
{
	masSpans.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownSpans::Kill(void)
{
	size	i;

	for (i = 0; i < masSpans.NumElements(); i++)
	{
		masSpans.Get(i)->szText.Kill();
	}
	masSpans.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The page's code spans, line by line.  A fence opens with three or more
// backticks or tildes at the start of a line (after white space) and
// closes with as many or more of the same and nothing after them.  An
// HTML comment can cross lines; what it covers is skipped as spaces, so
// columns don't move.
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownSpans::Scan(const char* szText)
{
	const char*	szLine;
	const char*	szEnd;
	int			iLength;
	int			iLine;
	char		cFence;
	int			iFence;
	char		cRun;
	int			iRun;
	int			iAfter;
	bool		bComment;
	bool		bInComment;
	CChars		szVisible;
	int			iStart;
	int			iClose;

	cFence = '\0';
	iFence = 0;
	bComment = false;
	szVisible.Init();
	szLine = szText;
	for (iLine = 0;; iLine++)
	{
		szEnd = strchr(szLine, '\n');
		iLength = szEnd ? (int)(szEnd - szLine) : (int)strlen(szLine);

		iRun = FindFence(szLine, iLength, &cRun, &iAfter);
		if (iFence > 0)
		{
			if ((iRun > 0) && (cRun == cFence) && (iRun >= iFence) && IsMarkdownBlank(szLine, iAfter, iLength))
			{
				iFence = 0;
			}
		}
		else if (iRun > 0)
		{
			cFence = cRun;
			iFence = iRun;
		}
		else
		{
			szVisible.Clear();
			szVisible.AppendSubString(szLine, iLength);
			bInComment = false;
			if (bComment)
			{
				iClose = FindMarkdownText(szVisible.Text(), iLength, 0, "-->");
				bInComment = iClose < 0;
				if (!bInComment)
				{
					memset(szVisible.Text(), ' ', iClose + 3);
					bComment = false;
				}
			}
			if (!bInComment)
			{
				iStart = 0;
				while ((iStart = FindMarkdownText(szVisible.Text(), iLength, iStart, "<!--")) >= 0)
				{
					iClose = FindMarkdownText(szVisible.Text(), iLength, iStart + 4, "-->");
					if (iClose < 0)
					{
						memset(&szVisible.Text()[iStart], ' ', iLength - iStart);
						bComment = true;
						break;
					}
					memset(&szVisible.Text()[iStart], ' ', iClose + 3 - iStart);
				}
				ScanLine(iLine, szLine, szVisible.Text(), iLength);
			}
		}

		if (szEnd == NULL)
		{
			break;
		}
		szLine = szEnd + 1;
	}
	szVisible.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The spans of one line: found in szVisible (the line with its comments
// blanked out), their brackets read from szLine.
//
//////////////////////////////////////////////////////////////////////////
void CMarkdownSpans::ScanLine(int iLine, const char* szLine, const char* szVisible, int iLength)
{
	int				i;
	int				iRun;
	int				j;
	int				k;
	int				iClose;
	int				iFirst;
	int				iLast;
	SMarkdownSpan*	psSpan;

	i = 0;
	while (i < iLength)
	{
		if (szVisible[i] != '`')
		{
			i++;
			continue;
		}

		for (iRun = 0; (i + iRun < iLength) && (szVisible[i + iRun] == '`'); iRun++)
		{
		}
		iClose = -1;
		j = i + iRun;
		while (j < iLength)
		{
			if (szVisible[j] != '`')
			{
				j++;
				continue;
			}
			for (k = j; (k < iLength) && (szVisible[k] == '`'); k++)
			{
			}
			if (k - j == iRun)
			{
				iClose = j;
				break;
			}
			j = k;
		}
		if (iClose < 0)
		{
			i += iRun;
			continue;
		}

		for (iFirst = i + iRun; (iFirst < iClose) && IsMarkdownSpace(szVisible[iFirst]); iFirst++)
		{
		}
		for (iLast = iClose; (iLast > iFirst) && IsMarkdownSpace(szVisible[iLast - 1]); iLast--)
		{
		}
		psSpan = masSpans.Add();
		psSpan->iLine = iLine;
		psSpan->iColumn = i;
		psSpan->iEnd = iClose + iRun;
		psSpan->iText = iFirst;
		psSpan->eKind = GetKind(szLine, iLength, psSpan->iColumn, psSpan->iEnd);
		psSpan->szText.Init();
		psSpan->szText.AppendSubString(&szVisible[iFirst], iLast - iFirst);
		i = psSpan->iEnd;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The length of the fence that starts the line (three or more backticks
// or tildes after white space), 0 if none does; *piAfter is where it ends.
//
//////////////////////////////////////////////////////////////////////////
int CMarkdownSpans::FindFence(const char* szLine, int iLength, char* pcFence, int* piAfter)
{
	int		i;
	int		iStart;

	for (i = 0; (i < iLength) && IsMarkdownSpace(szLine[i]); i++)
	{
	}
	if ((i >= iLength) || ((szLine[i] != '`') && (szLine[i] != '~')))
	{
		return 0;
	}
	*pcFence = szLine[i];
	for (iStart = i; (i < iLength) && (szLine[i] == *pcFence); i++)
	{
	}
	*piAfter = i;
	return (i - iStart >= 3) ? (i - iStart) : 0;
}


//////////////////////////////////////////////////////////////////////////
//
// [`Name`] is a reference; [`Name`](url), [`Name`][ref] and an image's
// ![`Name`](url) are links, as is [`Name`]: url at the start of a line (a
// definition; elsewhere the colon is text); \[`Name`] is escaped.
//
//////////////////////////////////////////////////////////////////////////
EMarkdownSpanKind CMarkdownSpans::GetKind(const char* szLine, int iLength, int iStart, int iEnd)
{
	char	cAfter;

	if ((iStart == 0) || (szLine[iStart - 1] != '[') || (iEnd >= iLength) || (szLine[iEnd] != ']'))
	{
		return MSK_Plain;
	}
	cAfter = (iEnd + 1 < iLength) ? szLine[iEnd + 1] : '\0';
	if ((cAfter == '(') || (cAfter == '[') || ((iStart >= 2) && (szLine[iStart - 2] == '!')))
	{
		return MSK_Link;
	}
	if ((cAfter == ':') && IsMarkdownBlank(szLine, 0, iStart - 1) && (iStart - 1 <= 3))
	{
		return MSK_Link;
	}
	if ((iStart >= 2) && (szLine[iStart - 2] == '\\'))
	{
		return MSK_Escaped;
	}
	return MSK_Reference;
}


size CMarkdownSpans::NumSpans(void) const { return masSpans.NumElements(); }
SMarkdownSpan* CMarkdownSpans::Get(size uiIndex) { return masSpans.Get(uiIndex); }

