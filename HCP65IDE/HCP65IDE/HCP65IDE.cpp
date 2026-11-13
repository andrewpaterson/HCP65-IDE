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
#include <windows.h>
#include "BaseLib/LeakDetector.h"
#include "BaseLib/GlobalMemory.h"
#include "WinGdiLib/WinGDIWindowFactory.h"
#include "WinInputLib/WinInput.h"
#include "IDEMain.h"


//////////////////////////////////////////////////////////////////////////
//
// The command line's files: separated by spaces, a path with spaces in it
// in quotes.
//
//////////////////////////////////////////////////////////////////////////
void SplitIDECommandLine(const char* szCommandLine, CArrayChars* paszDest)
{
	const char*	sz;
	CChars		szFile;
	bool		bQuoted;

	sz = szCommandLine;
	for (;;)
	{
		while ((*sz == ' ') || (*sz == '\t'))
		{
			sz++;
		}
		if (*sz == '\0')
		{
			return;
		}
		szFile.Init();
		bQuoted = false;
		while ((*sz != '\0') && (bQuoted || ((*sz != ' ') && (*sz != '\t'))))
		{
			if (*sz == '"')
			{
				bQuoted = !bQuoted;
			}
			else
			{
				szFile.Append(*sz);
			}
			sz++;
		}
		paszDest->Add(szFile.Text());
		szFile.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE's window on Windows, showing the .sln, .vcxproj or
// .hcp65workspace on the command line (or nothing).  More than
// one solution are opened together, the first the startup solution.
//
//////////////////////////////////////////////////////////////////////////
int PASCAL WinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	bool		bResult;
	CArrayChars	aszSolutions;
	int			iResult;

	BreakOnAllocation(0);
	StartIDE();

	aszSolutions.Init();
	SplitIDECommandLine((lpCmdLine != NULL) ? lpCmdLine : "", &aszSolutions);

	{
		CWinGDIWindowFactory	cNativeFactory;
		CWinInput				cWinInput;

		cNativeFactory.Init(&gcMemoryAllocator,
			hInstance,
			hPrevInstance,
			nCmdShow,
			"HCP65IDE");

		//The app's input reads the devices from the XML, then adds these.
		cWinInput.Init(NULL, false);
		cNativeFactory.SetRawInput(WinRawInputCallback, cWinInput.GetRaw());

		bResult = RunIDE(&cNativeFactory, &cWinInput, &aszSolutions);

		cNativeFactory.SetRawInput(NULL, NULL);
		cWinInput.Kill();
		cNativeFactory.Kill();
	}

	aszSolutions.Kill();
	iResult = StopIDE(bResult);
	LeakDetection();
	return iResult;
}

