/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_MAIN_H__
#define __IDE_MAIN_H__
#include "BaseLib/ArrayChars.h"
#include "InputLib/NativeInput.h"
#include "WindowLib/NativeWindowFactory.h"


//What every platform's main does around the window factory and input it makes (X11, Windows' GDI, or the reference
//screen the checks run on): the libraries started, the IDE run, and everything stopped again.
void	StartIDE(void);
bool	RunIDE(CNativeWindowFactory* pcFactory, CNativeInput* pcInput, CArrayChars* paszSolutions);
int		StopIDE(bool bResult);


#endif // __IDE_MAIN_H__

