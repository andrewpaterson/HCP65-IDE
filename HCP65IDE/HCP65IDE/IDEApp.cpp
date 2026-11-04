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
#include <stdio.h>
#include "BaseLib/ArrayChars.h"
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
// HCP65_IDE_SNAPSHOT, HCP65_IDE_CONFIGURATION, HCP65_IDE_EXPAND,
// HCP65_IDE_OPEN, HCP65_IDE_CLOSE, HCP65_IDE_SCROLL,
// HCP65_IDE_WAIT_INDEX, HCP65_IDE_CARET=line:column (1 based), HCP65_IDE_TYPE,
// HCP65_IDE_REFERENCES and HCP65_IDE_DEFINITION are for checking it: a
// snapshot of the window, every folder expanded, files opened (a comma
// separated list, each in a tab), the
// editor scrolled down that many pixels, the indexing finished, the caret
// put somewhere, HCP65_IDE_TYPE's text typed there, then Find All
// References and Go to Definition.
// Call it once the app is in the root: the objects made here are only
// held through it, and an object no root reaches is freed as soon as
// the last stack pointer to it goes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEApp::Setup(CNativeWindowFactory* pcNativeFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, const char* szSolution)
{
	Ptr<CIDEWindowTick>	pTick;
	int32				iNode;
	int					iLine;
	int					iColumn;
	const char*			szTyped;
	CArrayChars			aszOpen;
	CChars				szPath;
	size				i;

	pTick = OMalloc<CIDEWindowTick>();
	if (getenv("HCP65_IDE_SNAPSHOT") != NULL)
	{
		pTick->SetSnapshot(getenv("HCP65_IDE_SNAPSHOT"), (getenv("HCP65_IDE_SNAPSHOT_TICKS") != NULL) ? atoi(getenv("HCP65_IDE_SNAPSHOT_TICKS")) : 30);
	}
	mpWindow = OMalloc<CWindow>("HCP65 IDE", pcNativeFactory, pTick, OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);

	mpContent = OMalloc<CIDEContent>();
	pTick->SetContent(mpContent);
	mbValid = mpContent->Setup(mpWindow, "Resources/InterVariable.ttf", "Resources/JetBrainsMonoNL-Regular.ttf");
	if (mbValid)
	{
		//Sized first: what the checks below scroll to depends on the size.
		mpWindow->SetWindowSize(1280, 800);
		if (mpContent->OpenSolution(szSolution))
		{
			if (getenv("HCP65_IDE_CONFIGURATION") != NULL)
			{
				mpContent->SetConfiguration(getenv("HCP65_IDE_CONFIGURATION"));
			}
			if (getenv("HCP65_IDE_WAIT_INDEX") != NULL)
			{
				mpContent->GetCodeModel()->WaitForIndexing();
			}
			if (getenv("HCP65_IDE_EXPAND") != NULL)
			{
				mpContent->GetExplorer()->GetTree()->ExpandAll();
			}
			if (getenv("HCP65_IDE_OPEN") != NULL)
			{
				//Each of a comma separated list, in its own tab; the last stays selected.
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_OPEN"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					iNode = mpContent->GetExplorer()->FindFileNode(aszOpen.GetText(i));
					if (iNode != TREE_NO_NODE)
					{
						mpContent->GetExplorer()->GetTree()->SelectNode(iNode);
						mpContent->ExplorerActivated();
					}
				}
				aszOpen.Kill();
			}
			if (getenv("HCP65_IDE_CLOSE") != NULL)
			{
				//Each of a comma separated list of open files' names is closed, as its tab's cross does.
				aszOpen.Init();
				aszOpen.Split(getenv("HCP65_IDE_CLOSE"), ',');
				for (i = 0; i < aszOpen.NumElements(); i++)
				{
					iNode = mpContent->GetExplorer()->FindFileNode(aszOpen.GetText(i));
					szPath.Init();
					if ((iNode != TREE_NO_NODE) && mpContent->GetExplorer()->GetNodeFilePath(iNode, &szPath))
					{
						mpContent->CloseFile(szPath.Text());
					}
					szPath.Kill();
				}
				aszOpen.Kill();
			}
			if ((getenv("HCP65_IDE_CARET") != NULL) && (sscanf(getenv("HCP65_IDE_CARET"), "%d:%d", &iLine, &iColumn) == 2))
			{
				mpContent->SetCaretPlace(iLine - 1, iColumn - 1);
			}
			if ((getenv("HCP65_IDE_TYPE") != NULL) && mpContent->GetEditor().IsNotNull())
			{
				mpWindow->GetFocus()->SetFocus(mpContent->GetEditor());
				for (szTyped = getenv("HCP65_IDE_TYPE"); *szTyped != '\0'; szTyped++)
				{
					mpWindow->InputPrintable(*szTyped, true);
				}
			}
			if (getenv("HCP65_IDE_REFERENCES") != NULL)
			{
				mpContent->FindReferences();
			}
			if (getenv("HCP65_IDE_DEFINITION") != NULL)
			{
				mpContent->GoToDefinition();
			}
			if ((getenv("HCP65_IDE_SCROLL") != NULL) && mpContent->GetEditorPane().IsNotNull())
			{
				mpContent->GetEditorPane()->SetScrollPosition(0, atoi(getenv("HCP65_IDE_SCROLL")));
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

