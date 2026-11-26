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
#ifndef __REFERENCE_RESOLVER_H__
#define __REFERENCE_RESOLVER_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "SymbolTable.h"


//A name a reference can use for a symbol: its key's name, and an enumerator's without its enum.
struct SReferenceName
{
	CChars				szName;
	SSymbolTableEntry*	psEntry;
};


typedef CArrayTemplate<SReferenceName>	CArrayReferenceNames;


enum EPageReferenceKind
{
	PRK_Link,		//A reference that names one symbol or file.
	PRK_Error,		//A reference that names nothing, or more than one file.
	PRK_Warning,	//A plain code span that names a symbol declared in a header: it could be a reference.
};


//What a page's code span is found to be (ResolvePage).
struct SPageReference
{
	EPageReferenceKind	eKind;
	int					iLine;		//0 based.
	int					iColumn;	//0 based, in bytes: a reference's '[', a plain span's first backtick.
	int					iEnd;		//After a reference's ']': where its link goes.
	int					iText;		//Where szText starts.
	CChars				szText;		//What its backticks hold, without the white space around it.
	SSymbolTableEntry*	psSymbol;	//What a link names, NULL for a file (or once the table it is in has gone).
	CChars				szFile;		//A link's file, relative to the table's root.
	int					iTargetLine;	//1 based, 0 for a file.
	CChars				szMessage;	//An error's or a warning's.
};


typedef CArrayTemplate<SPageReference>	CArrayPageReferences;


//What a reference to code in a page names (HCP65/Website/DocsPlan.txt D, E1), for the website's build (hcp65index
//-resolve) and the IDE alike:
//  - A symbol by its qualified name: [`CTextArea`], [`CTextArea::ReplaceRanges`].  An overloaded function without
//    its parameters is its first declaration (types first, then by file and line); with them,
//    [`CShape::Init(int)`], the one that has them (spaces and where const is written don't matter, nor a trailing
//    const).  An enumerator can be named without its enum, as C++ allows an unscoped one.
//  - A file by the end of its path among the files given: [`BaseLib/Chars.h`].
//A plain code span that names a symbol whose place is in a header, by a name of three letters or more, could be a
//reference: a warning.  Not one in another language (`#!asm lda`, `#!text abs`), nor a file.
class CReferenceResolver
{
protected:
	CSymbolTable*			mpcTable;
	CArrayChars*			mpaszFiles;		//Relative to the table's root, '/' between folders.
	CArrayReferenceNames	masNames;		//Sorted by name, then the order a name's symbols are chosen in.

public:
	void				Init(CSymbolTable* pcTable, CArrayChars* paszFiles);
	void				Kill(void);

	SSymbolTableEntry*	ResolveSymbol(const char* szText);
	size				ResolveFiles(const char* szText, CArrayChars* paszDest);
	void				ResolvePage(const char* szPage, CArrayPageReferences* pasDest);
	void				ResolveReference(const char* szText, SPageReference* psDest);

	static bool			IsFileReference(const char* szText);
	static bool			CouldBeReference(const char* szText);
	static bool			IsInHeader(SSymbolTableEntry* psEntry);
	static void			SplitName(const char* szKey, CChars* pszName, CChars* pszRest);
	static bool			SplitParameters(const char* szText, CArrayChars* paszParameters, CChars* pszAfter);
	static void			NormaliseType(const char* szText, size uiLength, CChars* pszDest);

	static bool			ReadFiles(const char* szFileName, CArrayChars* paszDest);
	static void			KillPageReferences(CArrayPageReferences* pasReferences);
	static char*		PrintPageReferences(const char* szPage, CArrayPageReferences* pasReferences, CChars* pszDest);

protected:
	void				AddName(const char* szName, SSymbolTableEntry* psEntry);
	size				FindFirstName(const char* szName);
};


#endif // __REFERENCE_RESOLVER_H__

