/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __VCX_ITEM_H__
#define __VCX_ITEM_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"


//The kinds of item a Visual Studio C++ project lists, by the element they are in.
enum EVcxItemType
{
	VIT_ClCompile,
	VIT_ClInclude,
	VIT_None,
	VIT_Text,
	VIT_Image,
	VIT_ResourceCompile,
	VIT_Natvis,
	VIT_CustomBuild,
	VIT_Other,
};


//A file in a project: <ClCompile Include="Source\Main.cpp" />.  Its filter is the folder Solution Explorer shows it in,
//from the .vcxproj.filters file (empty if it is in none).
class CVcxItem
{
protected:
	EVcxItemType	meType;
	CChars			mszElement;
	CChars			mszInclude;
	CChars			mszFilter;

public:
	void				Init(const char* szElement, const char* szInclude);
	void				Kill(void);

	EVcxItemType		GetType(void);
	char*				GetElement(void);
	char*				GetInclude(void);
	char*				GetFilter(void);
	void				SetFilter(const char* szFilter);
	char*				GetFileName(void);
	bool				Is(const char* szElement, const char* szInclude);

	static EVcxItemType	TypeOf(const char* szElement);
};


typedef CArrayTemplate<CVcxItem>	CArrayVcxItem;


#endif // __VCX_ITEM_H__

