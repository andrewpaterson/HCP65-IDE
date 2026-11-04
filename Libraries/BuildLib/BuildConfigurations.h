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
#ifndef __BUILD_CONFIGURATIONS_H__
#define __BUILD_CONFIGURATIONS_H__
#include "BaseLib/ArrayTemplate.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildConfiguration.h"


typedef CArrayTemplate<CBuildConfiguration>	CArrayBuildConfigurations;


//A solution's build configurations, kept in a file beside it (GetFileName) so its Visual Studio projects are left as
//they are.  Without the file each solution configuration is built with MSVC, and each of its configurations (Debug,
//Release) with the W65C816, the 65C64 and GCC too.
class CBuildConfigurations
{
protected:
	CArrayBuildConfigurations	masConfigurations;

public:
	void					Init(void);
	void					Kill(void);

	void					AddDefaults(CVcxSolution* pcSolution);
	CBuildConfiguration*	Add(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration);
	bool					Read(const char* szFileName);
	bool					Write(const char* szFileName);

	size					NumConfigurations(void);
	CBuildConfiguration*	GetConfiguration(size uiIndex);
	CBuildConfiguration*	FindConfiguration(const char* szName);

	static void				GetFileName(const char* szSolutionFile, CChars* pszDest);

protected:
	void					Clear(void);
	void					AddDefault(CVcxSolution* pcSolution, const char* szConfiguration, EBuildToolchain eToolchain, const char* szPlatform);
	void					GetConfigurationName(const char* szSolutionConfiguration, CChars* pszDest);
};


#endif // __BUILD_CONFIGURATIONS_H__

