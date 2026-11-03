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
#ifndef __VCX_PROJECT_REFERENCE_H__
#define __VCX_PROJECT_REFERENCE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"


//A project this one uses: <ProjectReference Include="..\BaseLib\BaseLib.vcxproj"> and the GUID in its <Project>.
class CVcxProjectReference
{
protected:
	CChars	mszInclude;
	CChars	mszProject;

public:
	void	Init(const char* szInclude);
	void	Kill(void);

	char*	GetInclude(void);
	char*	GetProject(void);
	void	SetProject(const char* szProject);
};


typedef CArrayTemplate<CVcxProjectReference>	CArrayVcxProjectReference;


#endif // __VCX_PROJECT_REFERENCE_H__

