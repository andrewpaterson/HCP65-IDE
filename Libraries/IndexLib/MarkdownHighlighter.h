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
#ifndef __MARKDOWN_HIGHLIGHTER_H__
#define __MARKDOWN_HIGHLIGHTER_H__
#include "CppCompilerLib/CPPHighlighter.h"
#include "ReferenceResolver.h"


//The colours a documentation page is shown in (HCP65/Website/DocsPlan.txt E5), as the C++ highlighter's kinds so the
//editor's themes colour them: a heading as a keyword; a code span as a string; a reference to code as what it names
//(a class, a function...), or as unresolved when it names nothing; an HTML comment as a comment; a fence's own lines
//as directives, and a ```cpp block's C++ lexically (comments, strings, characters, numbers and keywords; F compiles
//them).  What the references name comes from the page's resolved references (CWorkspaceIndex::GetPageReferences).
class CMarkdownHighlighter
{
public:
	static void				Highlight(const char* szPage, CArrayPageReferences* pasReferences, CArrayHighlights* pacDest);

protected:
	static void				HighlightCppLine(int iLine, const char* szLine, int iLength, bool* pbComment, CArrayHighlights* pacDest);
	static void				HighlightComments(int iLine, const char* szLine, int iLength, bool* pbComment, CArrayHighlights* pacDest);
	static ECPPHighlight	GetSymbolHighlight(int iSymbolKind);
	static bool				IsHeading(const char* szLine, int iLength);
	static bool				IsCppFence(const char* szInfo, int iLength);
	static bool				IsCppKeyword(const char* szWord, int iLength);
	static void				Add(CArrayHighlights* pacDest, int iLine, int iColumn, int iLength, ECPPHighlight eKind);
};


#endif // __MARKDOWN_HIGHLIGHTER_H__

