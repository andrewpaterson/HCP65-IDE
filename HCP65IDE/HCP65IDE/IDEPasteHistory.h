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
#ifndef __IDE_PASTE_HISTORY_H__
#define __IDE_PASTE_HISTORY_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/PasteHistoryDialog.h"


class CIDEContent;


//Paste from History (Edit, Paste from History..., Control+Shift+V), as IntelliJ IDEA's: what the clipboard's history
//asks which of the texts copied before to paste.  It shows Choose Content to Paste (CPasteHistoryDialog, made when
//it is first wanted, its preview in the code font) over the window that asks: the IDE's, or a dialog's whose field
//the keys were pressed in.  Every text in the IDE cuts and copies into the one history, and pastes from it.
//
//The dialog is watched by the script (CIDEScript::WatchDialog), so a check can answer it.
class CIDEPasteHistory : public CObject, public CClipboardChooser
{
CONSTRUCTABLE(CIDEPasteHistory);
DESTRUCTABLE(CIDEPasteHistory);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	Ptr<CPasteHistoryDialog>	mpDialog;  //NULL until it is first shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	ChooseContent(Ptr<CWindow> pOwner, Ptr<CClipboardHistory> pHistory, CChars* pszDest) override;
	Ptr<CPasteHistoryDialog>	GetDialog(void);
	Ptr<CClipboardHistory>		GetHistory(void);
};


#endif // __IDE_PASTE_HISTORY_H__

