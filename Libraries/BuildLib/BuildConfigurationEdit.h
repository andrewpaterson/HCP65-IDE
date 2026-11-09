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
#ifndef __BUILD_CONFIGURATION_EDIT_H__
#define __BUILD_CONFIGURATION_EDIT_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "BuildConfiguration.h"


//What a build configuration's file is to say about one configuration, for CBuildConfigurations::ApplyEdits: the
//attributes that can be changed without looking at what is inside it.  It is a configuration kept (muiSource says which),
//or a new one (mbNew), which is a copy of muiSource if that is set, with all that is inside it.
class CBuildConfigurationEdit
{
public:
	size			muiSource;		//Its index in the configurations, MAX_SIZE for a new one that isn't a copy.
	bool			mbNew;
	bool			mbNeedsHCP65;	//It has an image, executable, module or <Load>, which only an HCP65 toolchain can.
	CChars			mszName;
	EBuildToolchain	meToolchain;
	CChars			mszSolution;
	CChars			mszBreakpointSpace;
	bool			mbNoFloats;
	bool			mbMathTables;
	CChars			mszCard;
	CChars			mszCardPath;

	void	Init(const char* szName, EBuildToolchain eToolchain, const char* szSolution);
	void	Init(CBuildConfiguration* pcConfiguration, size uiSource);
	void	Init(CBuildConfigurationEdit* pcCopied, const char* szName);
	void	Kill(void);
};


typedef CArrayTemplate<CBuildConfigurationEdit>	CArrayBuildConfigurationEdits;


#endif // __BUILD_CONFIGURATION_EDIT_H__

