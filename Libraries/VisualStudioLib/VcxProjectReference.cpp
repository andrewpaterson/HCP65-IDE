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
#include "VcxProjectReference.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxProjectReference::Init(const char* szInclude)
{
	mszInclude.Init(szInclude);
	mszProject.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxProjectReference::Kill(void)
{
	mszProject.Kill();
	mszInclude.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The referenced project's GUID.
//
//////////////////////////////////////////////////////////////////////////
void CVcxProjectReference::SetProject(const char* szProject)
{
	mszProject.Set(szProject);
}


char* CVcxProjectReference::GetInclude(void) { return mszInclude.Text(); }
char* CVcxProjectReference::GetProject(void) { return mszProject.Text(); }

