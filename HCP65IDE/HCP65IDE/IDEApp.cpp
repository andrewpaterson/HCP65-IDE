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
#include "StandardLib/Objects.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/JewelColours.h"
#include "IDEWindowTick.h"
#include "IDEApp.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::Init(void)
{
	PreInit();

	mbValid = false;
	mbInputs = false;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The window and its content, with szSolution open.  The devices are
// described by the XML in szInputDirectory; pcNativeInput (the platform's
// keyboard and mouse) is added to them.  Run fails if anything here did.
// HCP65_IDE_SNAPSHOT, HCP65_IDE_EXPAND and HCP65_IDE_OPEN are for checking
// it: a snapshot of the window, every folder expanded and a file opened.
// Call it once the app is in the root: the objects made here are only
// held through it, and an object no root reaches is freed as soon as
// the last stack pointer to it goes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Setup(CNativeWindowFactory* pcNativeFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, const char* szSolution)
{
	Ptr<CIDEWindowTick>	pTick;
	int32				iNode;

	pTick = OMalloc<CIDEWindowTick>();
	if (getenv("HCP65_IDE_SNAPSHOT") != NULL)
	{
		pTick->SetSnapshot(getenv("HCP65_IDE_SNAPSHOT"), (getenv("HCP65_IDE_SNAPSHOT_TICKS") != NULL) ? atoi(getenv("HCP65_IDE_SNAPSHOT_TICKS")) : 30);
	}
	mpWindow = OMalloc<CWindow>("HCP65 IDE", pcNativeFactory, pTick, OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);

	mpContent = OMalloc<CIDEContent>();
	mbValid = mpContent->Setup(mpWindow, "Resources/InterVariable.ttf", "Resources/JetBrainsMonoNL-Regular.ttf");
	if (mbValid)
	{
		if (mpContent->OpenSolution(szSolution))
		{
			if (getenv("HCP65_IDE_EXPAND") != NULL)
			{
				mpContent->GetExplorer()->GetTree()->ExpandAll();
			}
			if (getenv("HCP65_IDE_OPEN") != NULL)
			{
				iNode = mpContent->GetExplorer()->FindFileNode(getenv("HCP65_IDE_OPEN"));
				if (iNode != TREE_NO_NODE)
				{
					mpContent->GetExplorer()->GetTree()->SelectNode(iNode);
					mpContent->ExplorerActivated();
				}
			}
		}

		mpInput = OMalloc<CInput>((CNativeInput*)NULL);
		mbValid = mpInput->Read((char*)szInputDirectory);
		mpInput->AddNativeInput(pcNativeInput);
		mpInputSource = OMalloc<CWindowInputSource>(mpInput);
		mcComponentInput.Init(&mpInputSource, &mpWindow);
		mbInputs = true;
		mcComponentInput.AddKeyboards();
		mcComponentInput.AddMice();
		mpWindow->SetComponentInput(&mcComponentInput);

		mpWindow->GetFocus()->SetFocus(mpContent->GetExplorer()->GetTree());
		mpWindow->SetWindowSize(1280, 800);
	}

	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::Class(void)
{
	M_Pointer(mpWindow);
	M_Pointer(mpContent);
	M_Pointer(mpInput);
	M_Pointer(mpInputSource);
	U_Data(CComponentInput, mcComponentInput);
	U_Bool(mbValid);
	U_Bool(mbInputs);
}


//////////////////////////////////////////////////////////////////////////
//
// Only this object's own members: the window, its content and the input
// may already be freed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEApp::Free(void)
{
	if (mbInputs)
	{
		mcComponentInput.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the window until it is closed (if Init worked), then takes the
// component input out of the window.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Run(void)
{
	if (mbValid)
	{
		mpWindow->Show();
	}

	if (mbInputs)
	{
		mpWindow->SetComponentInput(NULL);
	}
	return mbValid;
}

