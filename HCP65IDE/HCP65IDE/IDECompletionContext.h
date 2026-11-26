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
#ifndef __IDE_COMPLETION_CONTEXT_H__
#define __IDE_COMPLETION_CONTEXT_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"


//What code completion would complete at a place in a text.
enum EIDECompletionKind
{
	ICK_None,		//Nothing: in a comment, a string or a number, or after a "." that nothing is before.
	ICK_Name,		//A name on its own: whatever is in scope.
	ICK_Member,		//After "." or "->": a member of what the expression before it is.
	ICK_Scope,		//After "::": what the scope before it declares; with nothing before it, what is outside any.
	ICK_Include,	//In an #include's name: a file or a folder of the include directories.
};


//Where the caret is, for completion (FindIDECompletionContext), from the text alone: the name being written (from
//uiPrefixStart to the caret; uiWordEnd is where the name the caret is in ends, which Tab replaces to), and what is
//before it: for a member or a scope, the expression or the scope's name before the operator (uiChainStart to
//uiChainEnd, the same where there is none) and whether the operator is "->"; for an #include, where its name starts
//(after its quote or bracket) and whether it is a system one.
struct SIDECompletionContext
{
	EIDECompletionKind	eKind;
	size				uiPrefixStart;
	size				uiCaret;
	size				uiWordEnd;
	size				uiChainStart;
	size				uiChainEnd;
	bool				bArrow;
	size				uiIncludeStart;
	bool				bSystemInclude;
};


//A part of the expression before a "." or "->" (ParseIDEChain), "pcGauge->GetLimits()[2]" having two: a name (uiStart
//to uiEnd) that may be called and may be indexed, how it is joined to the part before it ('\0' the first, '.', '>'
//for "->", ':' for "::"), or a group in brackets, "((CGauge*)pvThing)", which is the first.
struct SIDEChainLink
{
	size	uiStart;
	size	uiEnd;
	char	cJoin;
	bool	bCall;
	int32	iIndexes;
	bool	bGroup;
};


typedef CArrayTemplate<SIDEChainLink>	CArrayIDEChainLinks;


bool	FindIDECompletionContext(const char* szText, size uiLength, size uiCaret, SIDECompletionContext* psDest);
bool	FindIDEChainStart(const char* szText, size uiEnd, size* puiStart);
bool	ParseIDEChain(const char* szText, size uiStart, size uiEnd, CArrayIDEChainLinks* pasDest);
bool	IsIDECompletionKeyword(const char* szText, size uiLength);
size	NumIDECompletionKeywords(void);
const char*	GetIDECompletionKeyword(size uiIndex);


#endif // __IDE_COMPLETION_CONTEXT_H__

