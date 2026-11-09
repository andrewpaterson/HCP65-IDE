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
#ifndef __IDE_SETTINGS_DIALOG_H__
#define __IDE_SETTINGS_DIALOG_H__
#include "StandardLib/Array.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/ListContainer.h"


//A page of File, Settings: what it's called in the list on the left, what it shows, and what it does when OK closes the
//dialog.  It works on copies until then, so Cancel changes nothing.
class CIDESettingsPage : public CObject
{
CONSTRUCTABLE(CIDESettingsPage);
DESTRUCTABLE(CIDESettingsPage);
public:
	void					Init(void);
	void					Class(void) override;
	void					Free(void) override;
	bool					Save(CObjectWriter* pcFile) override;
	bool					Load(CObjectReader* pcFile) override;

	virtual const char*		GetTitle(void);
	virtual Ptr<CComponent>	GetBody(void);
	virtual Ptr<CComponent>	GetFirstEditor(void);
	virtual void			Keep(void);
};


//File, Settings, as IntelliJ IDEA's: its pages on the left and the one selected on the right.
class CIDESettingsDialog : public CObject
{
CONSTRUCTABLE(CIDESettingsDialog);
DESTRUCTABLE(CIDESettingsDialog);
protected:
	Ptr<CDialog>				mpDialog;
	Ptr<CListContainer>			mpPageList;
	Ptr<CGridContainer>			mpPageHolder;
	CArray<CIDESettingsPage>	maPages;

public:
	void			Init(Ptr<CWindow> pOwner, SDialogLook* psLook);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			AddPage(Ptr<CIDESettingsPage> pPage);
	bool			Show(void);
	Ptr<CDialog>	GetDialog(void);

protected:
	Ptr<CComponent>	CreateBody(void);
	void			PageSelected(void);
};


#endif // __IDE_SETTINGS_DIALOG_H__

