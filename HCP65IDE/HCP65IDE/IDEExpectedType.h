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
#ifndef __IDE_EXPECTED_TYPE_H__
#define __IDE_EXPECTED_TYPE_H__
#include "IDELightTyper.h"


//A type that would fit where something is being written: what the light typer makes of it (empty for one the index
//has no name for, an int), and the type as it is written, less its spaces and its const ("CGauge*", "int"), which is
//what such a one is compared by.
struct SIDEExpectedType
{
	SIDELightType	sType;
	CChars			szText;
};


typedef CArrayTemplate<SIDEExpectedType>	CArrayIDEExpectedTypes;


//How well what a symbol is fits an expected type.
enum EIDETypeFit
{
	ITF_None,
	ITF_Converts,	//A pointer to a class derived from the one expected.
	ITF_Same,
};


//The types that would fit where an expression is being written (smart completion, NextIDEWork.txt's item 5), from
//what is before it in the text, typed by the light typer:
//
//  - an argument of a call: the parameter it is given to, of each function the call could be of;
//  - the right of "=", "==" and "!=": what the left is (a variable being declared, its declared type);
//  - after "return": what the function returns;
//  - after "case": what the switch it is in switches on.
//
//And whether a symbol fits one of them (GetFit): what it is declared as is the same type, or a pointer to a class
//derived from the one expected; an enum's enumerators fit the enum.
class CIDEExpectedTypes
{
protected:
	CIDELightTyper*			mpcTyper;  //Not held.
	CIDEContent*			mpcContent;
	CArrayIDEExpectedTypes	masTypes;

public:
	void		Init(CIDEContent* pcContent, CIDELightTyper* pcTyper);
	void		Kill(void);

	bool		Find(const char* szText, size uiLength, size uiExpression);
	size		NumTypes(void) const;
	EIDETypeFit	GetFit(SIDEScopeSymbol* psSymbol);
	void		Print(CChars* pszDest);

protected:
	void		Clear(void);
	void		Add(const char* szType, const char* szDeclaredIn);
	void		AddType(SIDELightType* psType, const char* szType);
	bool		FindArgument(const char* szText, size uiExpression);
	bool		FindAssigned(const char* szText, size uiOperator);
	bool		FindSwitched(const char* szText, size uiCase);
};


void NormaliseIDEType(const char* szType, CChars* pszDest);


#endif // __IDE_EXPECTED_TYPE_H__

