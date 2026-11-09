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
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDESettings.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszKeymapFile.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::Class(void)
{
	U_Pointer(mpcContent);
	U_String(mszKeymapFile);
	M_Pointer(mpDialog);
	M_Pointer(mpKeymapPage);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::Free(void)
{
	mszKeymapFile.Kill();
}


char* CIDESettings::GetKeymapFile(void) { return mszKeymapFile.Text(); }
bool CIDESettings::Save(CObjectWriter* pcFile) { return false; }
bool CIDESettings::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("File.Settings", "Settings...", "Control+Alt+S", Ptr<CImageCel>(), this, &CIDESettings::RequestDialog);
}


//////////////////////////////////////////////////////////////////////////
//
// The user's keys, from szFile, before the commands are bound.  It being
// missing only means none have been changed; one that can't be read is
// said, and the commands keep their own keys.  "" keeps none.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ReadKeymap(const char* szFile)
{
	CFileUtil	cFileUtil;

	mszKeymapFile.Set(szFile);
	if (!mszKeymapFile.Empty() && cFileUtil.Exists(szFile))
	{
		if (!mpcContent->GetCommands()->ReadKeys(szFile))
		{
			gcLogger.Warning2(__METHOD__, " Keymap [", szFile, "] couldn't be read: the default keys are used.", NULL);
			mpcContent->ShowStatus("The keymap couldn't be read: the default keys are used.");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A dialog can't be shown from the keys or a menu item's input action.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::RequestDialog(void)
{
	mpcContent->GetWindow()->Post(this, &CIDESettings::ShowDialog);
}


//////////////////////////////////////////////////////////////////////////
//
// What OK keeps applies now, and is written for next time.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ShowDialog(void)
{
	Ptr<CWindow>	pWindow;
	bool			bOK;

	pWindow = mpcContent->GetWindow();
	mpDialog = OMalloc<CIDESettingsDialog>(pWindow, mpcContent->GetDialogLook());
	mpKeymapPage = OMalloc<CIDEKeymapPage>(mpDialog->GetDialog(), mpcContent->GetCommands(), pWindow->GetComponentInput(), mpcContent->GetScript());
	mpDialog->AddPage(mpKeymapPage);
	if (getenv("HCP65_IDE_KEYMAP_EDITS") != NULL)
	{
		mpKeymapPage->ApplyEdits(getenv("HCP65_IDE_KEYMAP_EDITS"));
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	bOK = mpDialog->Show();
	mpcContent->GetScript()->StopWatchingDialog();
	mpKeymapPage = NULL;
	mpDialog = NULL;

	if (bOK)
	{
		mpcContent->GetCommands()->BindKeys(pWindow->GetComponentInput());
		if (mszKeymapFile.Empty())
		{
			mpcContent->ShowStatus("Changed the keymap.");
		}
		else if (mpcContent->GetCommands()->WriteKeys(mszKeymapFile.Text()))
		{
			mpcContent->ShowStatus("Saved the keymap.");
		}
		else
		{
			mpcContent->ShowStatus("Changed the keymap, but it couldn't be saved for next time.");
		}
	}
}

