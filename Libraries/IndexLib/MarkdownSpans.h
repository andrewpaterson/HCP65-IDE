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
#ifndef __MARKDOWN_SPANS_H__
#define __MARKDOWN_SPANS_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"


//What the brackets around a code span make it.
enum EMarkdownSpanKind
{
	MSK_Plain,			//`Name`
	MSK_Reference,		//[`Name`]: a reference to code (HCP65/Website/DocsPlan.txt D).
	MSK_Link,			//A link's or an image's text, [`Name`](url) or [`Name`][ref], or a definition [`Name`]: url.
	MSK_Escaped,		//\[`Name`]: written so as not to be a reference.
};


struct SMarkdownSpan
{
	int					iLine;		//0 based.
	int					iColumn;	//0 based, in bytes: its first backtick.
	int					iEnd;		//After its last backtick.
	EMarkdownSpanKind	eKind;
	CChars				szText;		//Between the backticks, without the white space around it.
};


typedef CArrayTemplate<SMarkdownSpan>	CArrayMarkdownSpans;


//A Markdown page's code spans, outside fenced code blocks (``` and ~~~) and HTML comments, each with what its
//brackets make it.  A span is a run of backticks, what follows, and the next run of as many; a run with no such
//closing run is text.  Spans don't cross lines.
class CMarkdownSpans
{
protected:
	CArrayMarkdownSpans		masSpans;

public:
	void					Init(void);
	void					Kill(void);

	void					Scan(const char* szText);
	size					NumSpans(void) const;
	SMarkdownSpan*			Get(size uiIndex);

protected:
	void					ScanLine(int iLine, const char* szLine, const char* szVisible, int iLength);
	static int				FindFence(const char* szLine, int iLength, char* pcFence, int* piAfter);
	static EMarkdownSpanKind	GetKind(const char* szLine, int iLength, int iStart, int iEnd);
};


#endif // __MARKDOWN_SPANS_H__

