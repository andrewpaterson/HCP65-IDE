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
#ifndef __VCX_CONFIGURATION_H__
#define __VCX_CONFIGURATION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/MapStringString.h"


//A project configuration, "Debug|Win32", and the properties the project sets for it: those of its property groups by
//name ("ConfigurationType", "IncludePath") and those of its item definition groups by tool and name
//("ClCompile.PreprocessorDefinitions", "Link.SubSystem").
class CVcxConfiguration
{
protected:
	CChars				mszName;
	CChars				mszConfiguration;
	CChars				mszPlatform;
	CMapStringString	mcProperties;

public:
	void	Init(const char* szName);
	void	Kill(void);

	char*	GetName(void);
	char*	GetConfiguration(void);
	char*	GetPlatform(void);
	void	SetProperty(const char* szName, const char* szValue);
	char*	GetProperty(const char* szName);
	size	NumProperties(void);
};


typedef CArrayTemplate<CVcxConfiguration>	CArrayVcxConfiguration;


#endif // __VCX_CONFIGURATION_H__

