/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __BUILD_CONFIGURATIONS_H__
#define __BUILD_CONFIGURATIONS_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildConfiguration.h"


//The file a solution's build configurations were once kept in, beside it.  It is no longer read: a build says so if
//one is there (ConfigurationsPlan.txt question 36).
#define BUILD_CONFIGURATIONS_OLD_EXTENSION	".hcp65build"


typedef CArrayTemplate<CBuildConfiguration>	CArrayBuildConfigurations;


//A solution's build configurations: its solution configurations (AddDefaults).  One whose platform is a toolchain's
//(Release|GCC-x64, Debug|HCP65C816) is that toolchain's and the rest are MSVC's; a solution that lists no toolchain's
//is given each toolchain's for each of its configurations' names (Debug, Release).  How one compiles and links is in
//the projects it builds (CBuildConfiguration::ReadProject), and how it is debugged in a run configuration.
class CBuildConfigurations
{
protected:
	CArrayBuildConfigurations	masConfigurations;

public:
	void					Init(void);
	void					Kill(void);

	void					AddDefaults(CVcxSolution* pcSolution);
	CBuildConfiguration*	Add(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration);

	size					NumConfigurations(void) const;
	CBuildConfiguration*	GetConfiguration(size uiIndex);
	CBuildConfiguration*	FindConfiguration(const char* szName);
	CBuildConfiguration*	FindConfiguration(const char* szConfiguration, const char* szPlatform);
	void					GetConfigurationNames(CArrayChars* paszDest);
	void					GetPlatformNames(CArrayChars* paszDest);


protected:
	void					Clear(void);
	void					AddDefault(CVcxSolution* pcSolution, const char* szConfiguration, EBuildToolchain eToolchain, const char* szPlatform);
	void					GetConfigurationName(const char* szSolutionConfiguration, CChars* pszDest);
	void					GetNames(CArrayChars* paszDest, bool bPlatforms);
};


#endif // __BUILD_CONFIGURATIONS_H__

