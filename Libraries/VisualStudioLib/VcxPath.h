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
#ifndef __VCX_PATH_H__
#define __VCX_PATH_H__
#include "BaseLib/Chars.h"


//Paths in Visual Studio files are relative, use '\' and aren't case sensitive: "..\..\CommonLibraries\BaseLib".
bool ResolveVcxPath(const char* szDirectory, const char* szRelative, CChars* pszDest);
bool FindVcxPathCaseInsensitive(CChars* pszPath);
void SplitVcxFileName(const char* szFileName, CChars* pszFullPath, CChars* pszDirectory, CChars* pszName);


#endif // __VCX_PATH_H__

