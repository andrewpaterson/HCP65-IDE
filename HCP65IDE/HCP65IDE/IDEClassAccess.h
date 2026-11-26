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
#ifndef __IDE_CLASS_ACCESS_H__
#define __IDE_CLASS_ACCESS_H__
#include "BaseLib/ArrayTemplate.h"
#include "IDECodeModel.h"


class CIDEContent;


enum EIDEAccess
{
	IA_Public,
	IA_Protected,
	IA_Private,
};


//Where a class's body changes access: from uiFrom of its file's text, what it declares is eAccess.
struct SIDEAccessChange
{
	size		uiFrom;
	EIDEAccess	eAccess;
};


typedef CArrayTemplate<SIDEAccessChange>	CArrayIDEAccessChanges;


//What access a class gives its members, read from its definition's text (the index hasn't it): what it starts as
//(private for a class, public for a struct or a union) and each "public:", "protected:" and "private:" of its own
//body, not of a class inside it.  A member's access is asked by where the class declares it (GetAccess); one that
//isn't declared in the body, as a macro's may not be, is taken to be public.
class CIDEClassAccess
{
protected:
	CChars					mszClass;  //Its key.
	CChars					mszFileName;  //Where it is defined; empty if the index has it defined nowhere.
	size					muiBodyStart;  //After its "{",
	size					muiBodyEnd;  //and its "}".
	CArrayIDEAccessChanges	masChanges;
	CArrayInt				maiLineStarts;  //Where each of the file's lines starts, to its body's end.

public:
	void		Init(void);
	void		Kill(void);

	bool		Read(CIDEContent* pcContent, const char* szClass);
	const char*	GetClass(void);
	EIDEAccess	GetAccess(SIDELocation* psDeclared);
};


typedef CArrayTemplate<CIDEClassAccess>	CArrayIDEClassAccesses;


#endif // __IDE_CLASS_ACCESS_H__

