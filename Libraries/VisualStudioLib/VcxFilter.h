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
#ifndef __VCX_FILTER_H__
#define __VCX_FILTER_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"


//A folder in Solution Explorer, from a .vcxproj.filters file: <Filter Include="Source\Map">.  Its name is its path in
//the project, the parts separated by backslashes.
class CVcxFilter
{
protected:
	CChars	mszName;
	CChars	mszUniqueIdentifier;
	CChars	mszExtensions;

public:
	void	Init(const char* szName);
	void	Kill(void);

	char*	GetName(void);
	char*	GetLeafName(void);
	size	GetDepth(void);
	bool	GetParentName(CChars* pszDest);
	char*	GetUniqueIdentifier(void);
	char*	GetExtensions(void);
	void	SetUniqueIdentifier(const char* szUniqueIdentifier);
	void	SetExtensions(const char* szExtensions);
};


typedef CArrayTemplate<CVcxFilter>	CArrayVcxFilter;


#endif // __VCX_FILTER_H__

