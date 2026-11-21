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
#ifndef __IDE_SYMBOL_INFO_H__
#define __IDE_SYMBOL_INFO_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"
#include "IDECodeModel.h"


class CIDEContent;


#define IDE_DECLARATION_MAXIMUM	240		//The most of a declaration's text shown.


//What a symbol is, in words (NavigationPlan.txt's S6): its declaration as it is written, on one line, where its name
//is in that, where it is declared ("CShape, Shape.h:15": its scope if it has one, its file and its line from 1), and
//for a function each parameter as it is written.
struct SIDESymbolInfo
{
	CChars			szKey;
	uint8			uiKind;			//ECPPSymbolKind.
	bool			bFunction;
	CChars			szDeclaration;
	size			uiName;			//Where its name is in szDeclaration,
	size			uiNameLength;	//0 if it isn't there (the declaration was cut short before it).
	CChars			szWhere;
	SIDELocation	sPlace;			//The declaration read: a function's declaration, anything else's definition, or the other.
	CArrayChars		aszParameters;

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDESymbolInfo>	CArrayIDESymbolInfos;


//They read the text, not a parse of it: a symbol's declaration is the statement its name is in (from the end of the
//statement or block before it to the body or semicolon after it), a parameter's what is between the commas round it.
//That is quick enough to do as the pointer moves, and needs nothing of the file but its text.
bool	ReadIDEDeclaration(const char* szText, size uiLength, size uiName, size uiNameLength, uint8 uiKind, CChars* pszDest, size* puiNameInDest, CArrayChars* paszParameters);
bool	ReadIDESymbolInfo(CIDEContent* pcContent, const char* szKey, SIDESymbolInfo* psDest);
void	AppendIDEDeclaredType(SIDESymbolInfo* psInfo, CChars* pszDest);
bool	FindIDECall(const char* szText, size uiCaret, size* puiNameStart, size* puiNameEnd, size* puiOpen, size* puiArgument);
bool	FindIDENameAt(const char* szText, size uiPosition, size* puiStart, size* puiEnd);
void	KillIDESymbolInfos(CArrayIDESymbolInfos* pasInfos);


#endif // __IDE_SYMBOL_INFO_H__

