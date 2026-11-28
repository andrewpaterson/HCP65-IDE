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
#ifndef __VCX_PROJECT_CONFIGURATIONS_H__
#define __VCX_PROJECT_CONFIGURATIONS_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"


//A project's configurations edited in its .vcxproj, in place: the file is its text with only their lines added or
//taken away, the rest byte for byte as it was (ConfigurationsPlan.txt C9-6, question 35).  A configuration is a name
//("Release") on each platform the project has it on; what is its own is every element whose Condition is on it,
//"'$(Configuration)|$(Platform)'=='Release|x64'" or "'$(Configuration)'=='Release'": its entries among the
//<ProjectConfigurations>, its groups, and single properties and settings (a file's ExcludedFromBuild).  The
//platforms are Visual Studio's and the toolchains' alike (Release|GCC-x64, Release|HCP65C816).
//
//AddVcxProjectConfiguration gives the project szNew ("Fast") on every platform it has szCopied on: a copy of each
//element that is szCopied's, after it, so the new configuration is built as the copied one until something of it
//is changed.  False if the file can't be read or written, hasn't szCopied or has szNew already.
bool AddVcxProjectConfiguration(const char* szFileName, const char* szNew, const char* szCopied);

//Takes away every element that is the configuration's.  False if the file hasn't it.
bool RemoveVcxProjectConfiguration(const char* szFileName, const char* szName);

//The names of the configurations the file lists among its <ProjectConfigurations>, each once, in its order.
bool GetVcxProjectConfigurationNames(const char* szFileName, CArrayChars* paszDest);


#endif // __VCX_PROJECT_CONFIGURATIONS_H__

