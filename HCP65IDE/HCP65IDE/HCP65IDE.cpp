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
#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "BaseLib/Socket.h"
#include "StandardLib/Objects.h"
#include "WinGdiLib/WinGDIWindowFactory.h"
#include "WinInputLib/WinInput.h"
#include "IDEApp.h"


#define IDE_DEFAULT_PROJECT		"../../Libraries/VisualStudioLib/VisualStudioLib.vcxproj"


//////////////////////////////////////////////////////////////////////////
//
// The IDE's window on Windows, showing the .vcxproj on the command line
// (or VisualStudioLib's).
//
//////////////////////////////////////////////////////////////////////////
int PASCAL WinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	_CrtSetBreakAlloc(0);
	gcLogger.Init();
	TypesInit();
	TypeConverterInit();
	MemoryInit();
	DataIOInit();
	ObjectsInit();
	SocketsInit();  //The debugger's connection to the emulator.

	bool bResult;

	bResult = true;

	{
		CWinGDIWindowFactory	cNativeFactory;
		CWinInput				cWinInput;
		Ptr<CIDEApp>			pApp;

		cNativeFactory.Init(&gcMemoryAllocator,
			hInstance,
			hPrevInstance,
			nCmdShow,
			"HCP65IDE");

		//Validating every object each time one changes is too slow for the text's glyphs.  They are validated once at the end.
		gcObjects.DisableValidation();

		//The app's input reads the devices from the XML, then adds these.
		cWinInput.Init(NULL, false);
		cNativeFactory.SetRawInput(WinRawInputCallback, cWinInput.GetRaw());

		//The app (its window, content and input) is held by the root while it runs.
		pApp = OMalloc<CIDEApp>();
		ORoot()->Add(pApp);
		pApp->Setup(&cNativeFactory, &cWinInput, "../../../CommonLibraries/Libraries/WinInputLib/xml", ((lpCmdLine != NULL) && (lpCmdLine[0] != '\0')) ? lpCmdLine : IDE_DEFAULT_PROJECT);
		bResult = pApp->Run();
		ORoot()->Remove(pApp);
		pApp = NULL;

		cNativeFactory.SetRawInput(NULL, NULL);
		cWinInput.Kill();

		gcObjects.EnableValidation();
		gcObjects.ValidateObjectsConsistency();

		cNativeFactory.Kill();
	}

	ObjectsFlush();
	SocketsKill();
	ObjectsKill();
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();

	gcLogger.Kill();

	_CrtDumpMemoryLeaks();

	if (bResult)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

