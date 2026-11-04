/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __BUILD_CONFIGURATION_H__
#define __BUILD_CONFIGURATION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"


//What a configuration is built with: the HCP65 compiler for one of its processors, or a compiler for this computer.
enum EBuildToolchain
{
	BT_W65C816,
	BT_65C64,
	BT_MSVC,
	BT_GCC,

	BT_NumToolchains
};


//A way the solution is built: a name to choose it by, its toolchain, and the solution configuration ("Debug|Win32")
//whose projects' files, include directories and defines it builds with.  MSVC builds that configuration as Visual
//Studio does.
class CBuildConfiguration
{
protected:
	CChars			mszName;
	EBuildToolchain	meToolchain;
	CChars			mszSolutionConfiguration;

public:
	void			Init(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration);
	void			Init(void);
	void			Kill(void);

	void			SaveXML(CXMLWriter* pcWriter);
	bool			LoadXML(CMarkupTag* pcTag);

	char*			GetName(void);
	EBuildToolchain	GetToolchain(void);
	char*			GetSolutionConfiguration(void);
	bool			IsHCP65(void);

	static const char*	GetToolchainName(EBuildToolchain eToolchain);
	static bool			FindToolchain(const char* szName, EBuildToolchain* peToolchain);
};


#endif // __BUILD_CONFIGURATION_H__

