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
#include "BaseLib/StringHelper.h"
#include "VcxItem.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxItem::Init(const char* szElement, const char* szInclude)
{
	meType = TypeOf(szElement);
	mszElement.Init(szElement);
	mszInclude.Init(szInclude);
	mszFilter.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxItem::Kill(void)
{
	mszFilter.Kill();
	mszInclude.Kill();
	mszElement.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EVcxItemType CVcxItem::TypeOf(const char* szElement)
{
	if (strcmp(szElement, "ClCompile") == 0)
	{
		return VIT_ClCompile;
	}
	else if (strcmp(szElement, "ClInclude") == 0)
	{
		return VIT_ClInclude;
	}
	else if (strcmp(szElement, "None") == 0)
	{
		return VIT_None;
	}
	else if (strcmp(szElement, "Text") == 0)
	{
		return VIT_Text;
	}
	else if (strcmp(szElement, "Image") == 0)
	{
		return VIT_Image;
	}
	else if (strcmp(szElement, "ResourceCompile") == 0)
	{
		return VIT_ResourceCompile;
	}
	else if (strcmp(szElement, "Natvis") == 0)
	{
		return VIT_Natvis;
	}
	else if (strcmp(szElement, "CustomBuild") == 0)
	{
		return VIT_CustomBuild;
	}
	return VIT_Other;
}


//////////////////////////////////////////////////////////////////////////
//
// The last part of the item's path: its file's name.
//
//////////////////////////////////////////////////////////////////////////
char* CVcxItem::GetFileName(void)
{
	char*	szInclude;
	char*	szSlash;
	char*	szBackslash;

	szInclude = mszInclude.Text();
	szSlash = strrchr(szInclude, '/');
	szBackslash = strrchr(szInclude, '\\');
	if ((szBackslash != NULL) && ((szSlash == NULL) || (szBackslash > szSlash)))
	{
		szSlash = szBackslash;
	}
	return (szSlash != NULL) ? szSlash + 1 : szInclude;
}


//////////////////////////////////////////////////////////////////////////
//
// The same element and the same path, whatever the case of the path
// (Windows paths aren't case sensitive).
//
//////////////////////////////////////////////////////////////////////////
bool CVcxItem::Is(const char* szElement, const char* szInclude)
{
	return mszElement.Equals(szElement) && (StringInsensitiveCompare(mszInclude.Text(), szInclude) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxItem::SetFilter(const char* szFilter)
{
	mszFilter.Set(szFilter);
}


EVcxItemType CVcxItem::GetType(void) { return meType; }
char* CVcxItem::GetElement(void) { return mszElement.Text(); }
char* CVcxItem::GetInclude(void) { return mszInclude.Text(); }
char* CVcxItem::GetFilter(void) { return mszFilter.Text(); }

