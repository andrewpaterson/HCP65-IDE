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
#ifndef __IDE_SETTINGS_H__
#define __IDE_SETTINGS_H__
#include "BaseLib/Chars.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/CommandSet.h"
#include "IDESettingsDialog.h"
#include "IDEKeymapPage.h"


class CIDEContent;


//File, Settings, and the keys a user has changed, which are read from a file of theirs as the IDE starts and written to
//it when OK closes the dialog.  They apply straight away.
class CIDESettings : public CObject
{
CONSTRUCTABLE(CIDESettings);
DESTRUCTABLE(CIDESettings);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	CChars						mszKeymapFile;  //Empty if the keys aren't kept.
	Ptr<CIDESettingsDialog>		mpDialog;  //While it's shown.
	Ptr<CIDEKeymapPage>			mpKeymapPage;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	ReadKeymap(const char* szFile);
	char*	GetKeymapFile(void);

protected:
	void	RequestDialog(void);
	void	ShowDialog(void);
};


#endif // __IDE_SETTINGS_H__

