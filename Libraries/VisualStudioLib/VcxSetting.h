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
#ifndef __VCX_SETTING_H__
#define __VCX_SETTING_H__
#include "BaseLib/Chars.h"


//A tool's setting written in a project (.vcxproj) or a property sheet (.props), in place: the file is its text with
//only the setting changed, the rest byte for byte as it was (its line endings, its byte order mark, its indents), so
//git shows the one change and Visual Studio reloads it as any edit made outside it (ConfigurationsPlan.txt C9-1).
//
//SetVcxSetting sets <szTool><szName> in the ItemDefinitionGroup whose Condition is szCondition as the file writes it
//("'$(Configuration)|$(Platform)'=='Release|HCP65C816'"; NULL or "" is the group with none):
//
//  <ItemDefinitionGroup Condition="'$(Platform)'=='HCP65C816'">
//    <HCP65Compile>
//      <NoFloats>true</NoFloats>
//    </HCP65Compile>
//  </ItemDefinitionGroup>
//
//What isn't there is added: the setting last in its tool, the tool last in its group, the group after the file's last
//ItemDefinitionGroup (before its first item group or its end if it has none).  A NULL szValue removes the setting, so
//the file takes it from wherever it did before (a sheet it imports), and with it a tool or a group left empty.  False
//if the file can't be read or written, or isn't a project's; true with nothing written if it says so already.
bool SetVcxSetting(const char* szFileName, const char* szCondition, const char* szTool, const char* szName, const char* szValue);

//Whether the file itself sets it (not a sheet it imports), and to what.
bool GetVcxSetting(const char* szFileName, const char* szCondition, const char* szTool, const char* szName, CChars* pszValue);


#endif // __VCX_SETTING_H__

