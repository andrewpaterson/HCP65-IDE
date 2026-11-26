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
#ifndef __IDE_OUTPUT_MENU_H__
#define __IDE_OUTPUT_MENU_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/CommandSet.h"


class CIDEContent;


//The context menu of a tab's text that isn't typed in (the Build, Indexing and Run tabs' output, the Claude tab's
//conversation), in place of the window's text menu, whose Cut and Paste it can't do: Copy and Select All (the
//application's, which act on the text with the keys), Copy All (the whole text, whatever is selected), then what its
//owner adds: commands of the menu's own (AddOwnCommand: they have no keys, any given are only shown) or the
//application's (AddCommand), such as Jump to Source, Clear All and Stop.
class CIDEOutputMenu : public CObject
{
CONSTRUCTABLE(CIDEOutputMenu);
DESTRUCTABLE(CIDEOutputMenu);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds what holds the menu.
	Ptr<CTextArea>		mpText;
	Ptr<CCommandSet>	mpCommands;
	Ptr<CMenuContainer>	mpMenu;

public:
	void				Init(CIDEContent* pcContent, Ptr<CTextArea> pText);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	Ptr<CMenuContainer>	GetMenu(void) const;
	void				AddCommand(Ptr<CCommand> pCommand);
	template<class O, class M>
	Ptr<CCommand>		AddOwnCommand(const char* szName, const char* szCaption, const char* szKeys, O* pcObject, void(M::*fRun)(void));
	void				AddSeparator(void);
	bool				CopyAll(void);

protected:
	void				CopyAllClicked(void);
	bool				HasText(void);
};


//////////////////////////////////////////////////////////////////////////
//
// An item for a command of the menu's own that runs pcObject's fRun,
// after those there are.  The command, for an enabled check.
//
//////////////////////////////////////////////////////////////////////////
template<class O, class M>
Ptr<CCommand> CIDEOutputMenu::AddOwnCommand(const char* szName, const char* szCaption, const char* szKeys, O* pcObject, void(M::*fRun)(void))
{
	Ptr<CCommand>	pCommand;

	pCommand = mpCommands->Add(szName, szCaption, szKeys, Ptr<CImageCel>(), pcObject, fRun);
	mpMenu->AddItem(pCommand);
	return pCommand;
}


#endif // __IDE_OUTPUT_MENU_H__

