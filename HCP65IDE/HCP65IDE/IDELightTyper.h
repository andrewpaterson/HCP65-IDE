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
#ifndef __IDE_LIGHT_TYPER_H__
#define __IDE_LIGHT_TYPER_H__
#include "StandardLib/Pointer.h"
#include "IDECodeModel.h"
#include "IDECompletionContext.h"


class CIDEContent;
class CIDEEditorTab;


#define IDE_LIGHT_TYPE_DEPTH	8	//The typedefs followed from one to the next before a type is given up on.


//What an expression is, as far as the light typer can say: the class, enum or namespace it is (its key in the
//index), how many levels of pointer or array to it, and, for a class template written with arguments
//("Ptr<CGauge>"), its first as it is written, which is what "->" on one that isn't a pointer is taken to reach.
struct SIDELightType
{
	CChars	szKey;
	uint8	uiKind;		//ECPPSymbolKind.
	int32	iPointers;
	CChars	szArgument;

	void	Init(void);
	void	Kill(void);
	void	Clear(void);
	bool	Equals(SIDELightType* psOther);
};


//Works out what a name or a short expression is from the index and the text, with no parse, so that it can be asked
//on a key (NextIDEWork.txt's item 5: completion's light path).  It knows where it is asked from (SetPlace: the
//function and the class the caret is in, by the file's outline as it was last worked out, and so the scopes a name
//on its own is looked for in: the class, its bases, the namespaces round it, then outside any), and from there:
//
//  - what is in scope (FindVisible: the function's parameters and locals, then the scopes' own, then the enumerators
//    of the enums among them) and what a class, an enum or a namespace has (FindMembers: a class's bases' too),
//    all of it or only what has a name, or a name with a word starting with a character;
//  - what a symbol is (TypeOfSymbol: a variable's, a field's or a parameter's declared type, what a function
//    returns, an enumerator's enum), from its declaration as it is written (ReadIDESymbolInfo);
//  - what a type written somewhere means (TypeOfText: "const CGauge*", "NS::CGauge&", "Ptr<CGauge>"), typedefs and
//    aliases followed;
//  - what the expression before a "." or "->" is (TypeOfChain: names, members, calls by what they return, indexings,
//    "this", a cast in brackets, "NS::CGauge"), what its last name could be (FindChainSymbols: the functions a call
//    could be of), and what the function the place is in returns (TypeOfFunction).
//
//Nothing is kept from one question to the next but the place: each is a pass of the index for the names it needs.
//
//It answers only what it is sure of, and false otherwise: an overloaded function whose overloads return different
//things, an operator a class defines, a template's parameter, an expression with an operator in it.  A checked
//parse can answer those.  Using directives aren't followed, and a block's own scope isn't known: a local variable is
//in scope from the start of its function.
class CIDELightTyper
{
protected:
	CIDEContent*			mpcContent;  //Not held.
	CArrayChars				maszScopes;  //Where a name on its own is looked for, the innermost first, "" last.
	CChars					mszClass;  //The class the caret is in a member of; empty for none.
	CChars					mszFunctionFile;  //The function the caret is in: its name's place, as the index has it.
	int32					miFunctionLine;
	int32					miFunctionColumn;
	bool					mbFunction;

public:
	void			Init(CIDEContent* pcContent);
	void			Kill(void);

	void			SetPlace(Ptr<CIDEEditorTab> pTab, size uiPosition);
	CArrayChars*	GetScopes(void);
	const char*		GetClass(void);

	size			FindVisible(CArrayIDEScopeSymbols* pasDest, const char* szName = NULL, size uiNameLength = 0, char cWordStart = 0, size uiMaximum = 0);
	size			FindMembers(const char* szKey, CArrayIDEScopeSymbols* pasDest, const char* szName = NULL, size uiNameLength = 0, char cWordStart = 0, size uiMaximum = 0);
	bool			TypeOfSymbol(SIDEScopeSymbol* psSymbol, SIDELightType* psDest);
	bool			TypeOfText(const char* szType, const char* szDeclaredIn, SIDELightType* psDest, int iDepth = 0);
	bool			TypeOfChain(const char* szText, size uiStart, size uiEnd, SIDELightType* psDest);
	bool			FindChainSymbols(const char* szText, size uiStart, size uiEnd, CArrayIDEScopeSymbols* pasDest);
	bool			TypeOfFunction(SIDELightType* psDest, CChars* pszText);
	bool			TypeOfName(const char* szName, size uiLength, bool bCall, SIDELightType* psDest);
	bool			Dereference(SIDELightType* psType, bool bArrow);
	bool			ReadDeclaredType(const char* szKey, CChars* pszDest);

protected:
	void			Forget(void);
	bool			TypeOfNamed(CArrayIDEScopeSymbols* pasSymbols, const char* szName, size uiLength, bool bCall, SIDELightType* psDest);
	bool			TypeOfGroup(const char* szText, size uiStart, size uiEnd, SIDELightType* psDest);
	bool			TypeOfLinks(const char* szText, CArrayIDEChainLinks* pasLinks, size uiNumLinks, SIDELightType* psDest);
	void			AddScopesOf(const char* szScope, CArrayChars* paszDest);
};


#endif // __IDE_LIGHT_TYPER_H__

