/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __VCX_PROPERTY_SETTING_H__
#define __VCX_PROPERTY_SETTING_H__
#include "BaseLib/Chars.h"


//A property set in a project's or a property sheet's file in place, as SetVcxSetting sets a tool's setting: only the
//lines of the property change (ConfigurationsPlan.txt C9, stage 3).
//
//A property is an element of a PropertyGroup.  The configurations it is for are said by its own Condition, as Visual
//Studio writes a project's directories,
//
//  <PropertyGroup>
//    <IncludePath Condition="'$(Configuration)|$(Platform)'=='Release|x64'">..\Libraries;$(IncludePath)</IncludePath>
//  </PropertyGroup>
//
//or by its group's, as it writes a configuration's type:
//
//  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
//    <ConfigurationType>Application</ConfigurationType>
//  </PropertyGroup>
//
//SetVcxProperty sets the property szName where the file says it for szCondition (as the file writes it; NULL or ""
//is for every configuration) in either way, the last if it says it more than once.  One it doesn't say is added:
//with bConfiguration in the group labelled Configuration whose Condition is szCondition, which must be there, as a
//configuration's own properties are read before the sheets (its type, its toolset); otherwise with a Condition of
//its own in the file's last group that has neither a Condition nor a Label, or in a new such group before the first
//ItemDefinitionGroup (or the first group of items, or the file's end).  A NULL szValue removes it, and with it a
//group left empty that has no Label.  False if the file can't be read or written, isn't a project's, or has no
//Configuration group to add to; true with nothing written if it says so already.
bool SetVcxProperty(const char* szFileName, const char* szCondition, const char* szName, const char* szValue, bool bConfiguration);

//Whether the file itself sets it (not a sheet it imports), and to what.
bool GetVcxProperty(const char* szFileName, const char* szCondition, const char* szName, CChars* pszValue);


#endif // __VCX_PROPERTY_SETTING_H__

