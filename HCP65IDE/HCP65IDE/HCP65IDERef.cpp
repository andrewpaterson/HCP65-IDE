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
#include <stdlib.h>
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/StringHelper.h"
#include "WinRefLib/WinRefWindowFactory.h"
#include "WinRefLib/WinRefInput.h"
#include "IDEMain.h"


//The reference screen: a common developer's.  The main window's size comes from the layout; this is for one without.
#define IDE_REF_SCREEN_WIDTH	2560
#define IDE_REF_SCREEN_HEIGHT	1440
#define IDE_REF_WINDOW_WIDTH	800
#define IDE_REF_WINDOW_HEIGHT	600


//////////////////////////////////////////////////////////////////////////
//
// The reference screen's options, from the environment:
//
// HCP65_IDE_VERIFY_PRESENT checks every present: what a window shows
// after it must be what it drew ("A present of" is logged if not).
// HCP65_IDE_PRESENT_HASH=file hashes every present to the file, a line
// each, of what was presented, or of all the screen with
// HCP65_IDE_PRESENT_HASH_AREA=screen.
// HCP65_IDE_FRAMES=directory saves the windows' frames there as PNGs,
// every one or every HCP65_IDE_FRAME_INTERVAL'th.
// HCP65_IDE_CONTINUOUS ticks and paints as fast as it can rather than
// sleeping until something is due.
//
//////////////////////////////////////////////////////////////////////////
static bool SetUpReferenceScreen(CWinRefWindowFactory* pcFactory)
{
	CInt32Rect		cScreen;
	const char*		szHashArea;
	EWinRefHashArea	eHashArea;

	cScreen.Init(0, 0, IDE_REF_SCREEN_WIDTH, IDE_REF_SCREEN_HEIGHT);
	pcFactory->SetScreenArea(&cScreen);
	pcFactory->SetWaitForEvents(getenv("HCP65_IDE_CONTINUOUS") == NULL);
	pcFactory->SetCheckPresents(getenv("HCP65_IDE_VERIFY_PRESENT") != NULL);
	if (getenv("HCP65_IDE_FRAME_INTERVAL") != NULL)
	{
		pcFactory->SetFrameInterval((uint32)atoi(getenv("HCP65_IDE_FRAME_INTERVAL")));
	}
	if (getenv("HCP65_IDE_PRESENT_HASH") != NULL)
	{
		szHashArea = getenv("HCP65_IDE_PRESENT_HASH_AREA");
		eHashArea = ((szHashArea != NULL) && (StringCompare(szHashArea, "screen") == 0)) ? WRHA_Screen : WRHA_Presented;
		return pcFactory->SetHashPresents(getenv("HCP65_IDE_PRESENT_HASH"), eHashArea);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE on the reference screen, drawn into memory with nothing on any
// display, for the checks (Checks/IDEChecks.py) to script.  It takes the
// same arguments and HCP65_IDE_* variables as on screen, and is run from
// the HCP65IDE directory the same way.  Nothing outside it can press a
// key or move the pointer.
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
		CWinRefWindowFactory	cNativeFactory;
		CWinRefInput			cRefInput;

		cNativeFactory.Init(&gcMemoryAllocator, IDE_REF_WINDOW_WIDTH, IDE_REF_WINDOW_HEIGHT, getenv("HCP65_IDE_FRAMES"));
		bResult = SetUpReferenceScreen(&cNativeFactory);
		if (bResult)
		{
			cRefInput.Init();
			bResult = RunIDE(&cNativeFactory, &cRefInput, &aszSolutions);
			cRefInput.Kill();
		}
		cNativeFactory.Kill();
	}

	aszSolutions.Kill();
	return StopIDE(bResult);
}

