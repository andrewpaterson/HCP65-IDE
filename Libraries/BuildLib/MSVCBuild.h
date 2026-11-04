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
#ifndef __M_S_V_C_BUILD_H__
#define __M_S_V_C_BUILD_H__
#include "CommandBuild.h"


//Builds the solution (or project) as Visual Studio does: MSBuild in the configuration's solution configuration, its
//projects' outputs where they say.  MSBuild is the newest Visual Studio's (found with vswhere), or the one on the path.
//Only on Windows.
class CMSVCBuild : public CCommandBuild
{
public:
	static bool	FindMSBuild(CChars* pszDest);

protected:
	bool		AddCommands(CArrayChars* paszLines) override;
};


#endif // __M_S_V_C_BUILD_H__

