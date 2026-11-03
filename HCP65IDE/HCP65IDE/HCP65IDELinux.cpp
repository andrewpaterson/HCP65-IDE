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
#include "X11Lib/X11WindowFactory.h"
#include "X11Lib/X11Input.h"
#include "IDEWindowTick.h"
#include "IDEContent.h"


#define IDE_DEFAULT_PROJECT		"../../Libraries/VisualStudioLib/VisualStudioLib.vcxproj"


//////////////////////////////////////////////////////////////////////////
//
// The IDE's window on Linux, showing the .vcxproj given (or VisualStudioLib's).  Run from the HCP65IDE directory the
// resources and WinInputLib's XML are found relative to it.
//
// HCP65_IDE_SNAPSHOT=file.png (and HCP65_IDE_SNAPSHOT_TICKS) saves the window and closes it.  HCP65_IDE_EXPAND opens
// every folder in Solution Explorer and HCP65_IDE_OPEN=name opens the file of that name, as a double click on it does.
//
//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	gcLogger.Init();
	TypesInit();
	TypeConverterInit();
	MemoryInit();
	DataIOInit();
	ObjectsInit();

	bool bResult;

	{
		CX11WindowFactory		cNativeFactory;
		Ptr<CWindow>			pWindow;
		Ptr<CIDEWindowTick>		pTick;
		CIDEContent				cContent;
		CInput					cInput;
		CX11Input				cX11Input;
		Ptr<CWindowInputSource>	pInputSource;
		CComponentInput			cComponentInput;
		const char*				szProject;
		int32					iNode;

		bResult = cNativeFactory.Init(&gcMemoryAllocator, NULL);
		if (bResult)
		{
			//Validating every object each time one changes is too slow for the text's glyphs.  They are validated once at the end.
			gcObjects.DisableValidation();

			//The window is in the root so that changing pointers into its components doesn't make the objects work out again which
			//of them are still on the stack.  Its tick is allocated, not on the stack, because stack objects can't be under the root.
			pTick = OMalloc<CIDEWindowTick>();
			if (getenv("HCP65_IDE_SNAPSHOT") != NULL)
			{
				pTick->SetSnapshot(getenv("HCP65_IDE_SNAPSHOT"), (getenv("HCP65_IDE_SNAPSHOT_TICKS") != NULL) ? atoi(getenv("HCP65_IDE_SNAPSHOT_TICKS")) : 30);
			}
			pWindow = OMalloc<CWindow>("HCP65 IDE", &cNativeFactory, pTick, OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);
			ORoot()->Add(pWindow);

			if (cContent.Init(pWindow, "Resources/InterVariable.ttf", "Resources/JetBrainsMonoNL-Regular.ttf"))
			{
				szProject = (argc > 1) ? argv[1] : IDE_DEFAULT_PROJECT;
				if (cContent.OpenProject(szProject))
				{
					if (getenv("HCP65_IDE_EXPAND") != NULL)
					{
						cContent.GetExplorer()->GetTree()->ExpandAll();
					}
					if (getenv("HCP65_IDE_OPEN") != NULL)
					{
						iNode = cContent.GetExplorer()->FindFileNode(getenv("HCP65_IDE_OPEN"));
						if (iNode != TREE_NO_NODE)
						{
							cContent.GetExplorer()->GetTree()->SelectNode(iNode);
							cContent.ExplorerActivated();
						}
					}
				}

				//The devices are described by the XML so they are added after it is read.
				cInput.Init(NULL);
				cInput.Read("../../../commonlibraries/Libraries/WinInputLib/xml");
				cX11Input.Init();
				cInput.AddNativeInput(&cX11Input);
				cNativeFactory.SetInput(&cX11Input);
				pInputSource = OMalloc<CWindowInputSource>(&cInput);
				cComponentInput.Init(&pInputSource, &pWindow);
				cComponentInput.AddKeyboards();
				cComponentInput.AddMice();
				pWindow->SetComponentInput(&cComponentInput);

				pWindow->GetFocus()->SetFocus(cContent.GetExplorer()->GetTree());
				pWindow->SetWindowSize(1280, 800);
				pWindow->Show();

				pWindow->SetComponentInput(NULL);
				cNativeFactory.SetInput(NULL);
				cComponentInput.Kill();
				pInputSource = NULL;
				cInput.Kill();
				cX11Input.Kill();
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
		}

		cNativeFactory.Kill();
	}

	ObjectsFlush();
	ObjectsKill();
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();

	gcLogger.Kill();

	if (bResult)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

