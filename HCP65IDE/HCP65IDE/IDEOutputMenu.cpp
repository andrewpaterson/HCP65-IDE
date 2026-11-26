/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEOutputMenu.h"


//////////////////////////////////////////////////////////////////////////
//
// pText's context menu, with Copy, Copy All and Select All.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutputMenu::Init(CIDEContent* pcContent, Ptr<CTextArea> pText)
{
	Ptr<CCommand>	pCommand;

	PreInit();
	mpcContent = pcContent;
	mpText = pText;
	mpCommands = OMalloc<CCommandSet>();
	mpMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpMenu->AddItem(mpcContent->GetCommands()->Find("Edit.Copy"));
	pCommand = mpCommands->Add("Output.CopyAll", "Copy All", "", Ptr<CImageCel>(), this, &CIDEOutputMenu::CopyAllClicked);
	pCommand->SetEnabledCheck(this, &CIDEOutputMenu::HasText);
	mpMenu->AddItem(pCommand);
	mpMenu->AddItem(mpcContent->GetCommands()->Find("Edit.SelectAll"));
	mpText->SetContextMenu(mpMenu);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutputMenu::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpText);
	M_Pointer(mpCommands);
	M_Pointer(mpMenu);
}


void CIDEOutputMenu::Free(void) {}
bool CIDEOutputMenu::Save(CObjectWriter* pcFile) { return false; }
bool CIDEOutputMenu::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// An item for a command, after those there are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutputMenu::AddCommand(Ptr<CCommand> pCommand) { mpMenu->AddItem(pCommand); }
void CIDEOutputMenu::AddSeparator(void) { mpMenu->AddSeparator(); }
Ptr<CCommandSet> CIDEOutputMenu::GetCommands(void) const { return mpCommands; }
Ptr<CMenuContainer> CIDEOutputMenu::GetMenu(void) const { return mpMenu; }


//////////////////////////////////////////////////////////////////////////
//
// Copy All: the whole text goes to the clipboard; what is selected stays
// selected.  False if there is no text.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEOutputMenu::CopyAll(void)
{
	return HasText() && mpcContent->GetWindow()->GetClipboard()->SetText(mpText->GetText());
}


void CIDEOutputMenu::CopyAllClicked(void) { CopyAll(); }
bool CIDEOutputMenu::HasText(void) { return mpText->GetText()[0] != '\0'; }

