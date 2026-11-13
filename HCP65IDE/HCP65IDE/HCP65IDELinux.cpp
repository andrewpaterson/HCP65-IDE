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
#include "BaseLib/GlobalMemory.h"
#include "X11Lib/X11WindowFactory.h"
#include "X11Lib/X11Input.h"
#include "IDEMain.h"


//////////////////////////////////////////////////////////////////////////
//
// The IDE's window on Linux, on the X display, showing the solutions or
// workspace given (or nothing).
//
// HCP65_IDE_SNAPSHOT=file.png (and HCP65_IDE_SNAPSHOT_TICKS) saves the
// window and closes it.  HCP65_IDE_EXPAND opens every folder in Solution
// Explorer and HCP65_IDE_OPEN=name opens the file of that name, as a
// double click on it does.
//
//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	bool		bResult;
	CArrayChars	aszSolutions;
	int			i;

	StartIDE();

	aszSolutions.Init();
	for (i = 1; i < argc; i++)
	{
		aszSolutions.Add(argv[i]);
	}

	{
		CX11WindowFactory	cNativeFactory;
		CX11Input			cX11Input;

		bResult = cNativeFactory.Init(&gcMemoryAllocator, NULL);
		if (bResult)
		{
			//The app's input reads the devices from the XML, then adds these.
			cX11Input.Init();
			cNativeFactory.SetInput(&cX11Input);

			bResult = RunIDE(&cNativeFactory, &cX11Input, &aszSolutions);

			cNativeFactory.SetInput(NULL);
			cX11Input.Kill();
		}
		cNativeFactory.Kill();
	}

	aszSolutions.Kill();
	return StopIDE(bResult);
}

