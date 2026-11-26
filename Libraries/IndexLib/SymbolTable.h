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
#ifndef __SYMBOL_TABLE_H__
#define __SYMBOL_TABLE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"
#include "CppCompilerLib/CPPIndex.h"


//Where documentation links to a symbol (HCP65/Website/DocsPlan.txt D1).
struct SSymbolTableEntry
{
	CChars			szKey;		//The index's key: "CShape", "CShape::Init(int)", "CountSides(CShape*)".
	ECPPSymbolKind	eKind;
	CChars			szFile;		//Relative to the table's root, with '/' between folders.
	int				iLine;		//1 based.
};


typedef CArrayTemplate<SSymbolTableEntry>	CArraySymbolTableEntries;


//Each symbol an index (CCPPIndex) has that a page can name, with the one place a reference to it links to: its
//declaration in a header.  A class's or an enum's is its definition, a function's its declaration (where its
//comment and signature are); a symbol declared only in a .cpp file links there.  Only symbols declared in files
//under the root are kept: not the system headers', nor parameters, labels and the names inside functions.
//Written as text for the website build (Write, Read), sorted by key, one symbol a line:
//	<key> TAB <kind> TAB <file> TAB <line>
class CSymbolTable
{
protected:
	CChars						mszRoot;		//Full path, ending in a separator.
	CArraySymbolTableEntries	masEntries;		//Sorted by key.

public:
	void				Init(const char* szRoot);
	void				Kill(void);

	void				Gather(CCPPIndex* pcIndex);
	size				NumEntries(void) const;
	SSymbolTableEntry*	Get(size uiIndex);
	SSymbolTableEntry*	Find(const char* szKey);
	char*				Print(CChars* pszDest);
	bool				Write(const char* szFileName);
	bool				Read(const char* szFileName);

	static const char*	GetKindName(ECPPSymbolKind eKind);
	static bool			GetKind(const char* szName, ECPPSymbolKind* peKind);

protected:
	bool				IsTabled(CCPPIndexSymbol* pcSymbol);
	bool				GetRelativeFile(const char* szFileName, CChars* pszDest);
	SCPPIndexLocation*	FindPlace(CCPPIndex* pcIndex, CArrayChars* paszFiles, CCPPIndexSymbol* pcSymbol);
	int					ComparePlaces(CArrayChars* paszFiles, ECPPSymbolKind eKind, SCPPIndexLocation* psLeft, SCPPIndexLocation* psRight);
};


#endif // __SYMBOL_TABLE_H__

