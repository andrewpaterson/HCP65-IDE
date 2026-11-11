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
#ifndef __G_C_C_BUILD_H__
#define __G_C_C_BUILD_H__
#include "CommandBuild.h"


//Builds the solution with GCC (g++ and ar on the path): each project's sources compiled with its include directories
//and defines into Output\GCC\<configuration>\<project>\, each static library archived (lib<project>.a), then each
//application linked with all the solution's libraries.
class CGCCBuild : public CCommandBuild
{
public:
	static	void	GetExecutablePath(const char* szRoot, CBuildConfiguration* pcConfiguration, const char* szProject, CChars* pszDest);

protected:
	bool	AddCommands(CArrayChars* paszLines) override;
	void	AddCompile(SBuildProject* psProject, const char* szSource, const char* szObject, CArrayChars* paszIncludes, CArrayChars* paszDefines);
	void	GetObjectName(const char* szProjectOutput, const char* szSource, CChars* pszDest);
};


#endif // __G_C_C_BUILD_H__

