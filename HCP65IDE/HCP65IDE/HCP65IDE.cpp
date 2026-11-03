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
#include "StandardLib/Objects.h"
#include "InputLib/Input.h"
#include "WindowLib/Window.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/ComponentInput.h"
#include "WinGdiLib/WinGDIWindowFactory.h"
#include "WinInputLib/WinInput.h"
#include "IDEWindowTick.h"
#include "IDEContent.h"


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

	bool bResult;

	bResult = true;

	{
		CWinGDIWindowFactory	cNativeFactory;
		Ptr<CWindow>			pWindow;
		Ptr<CIDEWindowTick>		pTick;
		CIDEContent				cContent;
		CInput					cInput;
		CWinInput				cWinInput;
		Ptr<CWindowInputSource>	pInputSource;
		CComponentInput			cComponentInput;

		cNativeFactory.Init(&gcMemoryAllocator,
			hInstance,
			hPrevInstance,
			nCmdShow,
			"HCP65IDE");

		//Validating every object each time one changes is too slow for the text's glyphs.  They are validated once at the end.
		gcObjects.DisableValidation();

		//The window is in the root so that changing pointers into its components doesn't make the objects work out again which
		//of them are still on the stack.  Its tick is allocated, not on the stack, because stack objects can't be under the root.
		pTick = OMalloc<CIDEWindowTick>();
		pWindow = OMalloc<CWindow>("HCP65 IDE", &cNativeFactory, pTick, OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);
		ORoot()->Add(pWindow);

		if (cContent.Init(pWindow, "Resources/InterVariable.ttf", "Resources/JetBrainsMonoNL-Regular.ttf"))
		{
			cContent.OpenProject(((lpCmdLine != NULL) && (lpCmdLine[0] != '\0')) ? lpCmdLine : IDE_DEFAULT_PROJECT);

			//The devices are described by the XML so they are added after it is read.
			cInput.Init(NULL);
			cInput.Read("../../../CommonLibraries/Libraries/WinInputLib/xml");
			cWinInput.Init(NULL, false);
			cInput.AddNativeInput(&cWinInput);
			cNativeFactory.SetRawInput(WinRawInputCallback, cWinInput.GetRaw());

			pInputSource = OMalloc<CWindowInputSource>(&cInput);
			cComponentInput.Init(&pInputSource, &pWindow);
			cComponentInput.AddKeyboards();
			cComponentInput.AddMice();
			pWindow->SetComponentInput(&cComponentInput);

			pWindow->GetFocus()->SetFocus(cContent.GetExplorer()->GetTree());
			pWindow->SetWindowSize(1280, 800);
			pWindow->Show();

			pWindow->SetComponentInput(NULL);
			cNativeFactory.SetRawInput(NULL, NULL);
			cComponentInput.Kill();
			pInputSource = NULL;
			cInput.Kill();
			cWinInput.Kill();
		}
		else
		{
			bResult = false;
		}

		cContent.Kill();
		ORoot()->Remove(pWindow);
		pWindow = NULL;
		pTick = NULL;
		ORoot()->RemoveAll();

		gcObjects.EnableValidation();
		gcObjects.ValidateObjectsConsistency();

		cNativeFactory.Kill();
	}

	ObjectsFlush();
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

