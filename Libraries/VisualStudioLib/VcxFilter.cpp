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
#include "VcxFilter.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxFilter::Init(const char* szName)
{
	mszName.Init(szName);
	mszUniqueIdentifier.Init();
	mszExtensions.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxFilter::Kill(void)
{
	mszExtensions.Kill();
	mszUniqueIdentifier.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The last part of the name: Map for Source\Map.
//
//////////////////////////////////////////////////////////////////////////
char* CVcxFilter::GetLeafName(void)
{
	char*	szBackslash;

	szBackslash = strrchr(mszName.Text(), '\\');
	return (szBackslash != NULL) ? szBackslash + 1 : mszName.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// 0 for a filter at the top of the project, 1 for one in it and so on.
//
//////////////////////////////////////////////////////////////////////////
size CVcxFilter::GetDepth(void)
{
	size	uiDepth;
	char*	sz;

	uiDepth = 0;
	for (sz = mszName.Text(); *sz != '\0'; sz++)
	{
		if (*sz == '\\')
		{
			uiDepth++;
		}
	}
	return uiDepth;
}


//////////////////////////////////////////////////////////////////////////
//
// The name of the filter this one is in.  False (with pszDest unchanged)
// for a filter at the top of the project.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxFilter::GetParentName(CChars* pszDest)
{
	char*	szBackslash;

	szBackslash = strrchr(mszName.Text(), '\\');
	if (szBackslash == NULL)
	{
		return false;
	}
	pszDest->Append(mszName.Text(), (int)(szBackslash - mszName.Text()));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxFilter::SetUniqueIdentifier(const char* szUniqueIdentifier)
{
	mszUniqueIdentifier.Set(szUniqueIdentifier);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxFilter::SetExtensions(const char* szExtensions)
{
	mszExtensions.Set(szExtensions);
}


char* CVcxFilter::GetName(void) { return mszName.Text(); }
char* CVcxFilter::GetUniqueIdentifier(void) { return mszUniqueIdentifier.Text(); }
char* CVcxFilter::GetExtensions(void) { return mszExtensions.Text(); }

