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
#ifndef __IDE_DUPLICATES_PAGE_H__
#define __IDE_DUPLICATES_PAGE_H__
#include "WindowLib/CheckBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/SettingsDialog.h"
#include "WindowLib/TextField.h"


class CIDEContent;


//Settings' Find Duplicates page: the smallest run of a function's statements Find Duplicates lists (CIDEDuplicates),
//in statements (those inside them counted) and tokens, and whether literals may differ, as the dialog's tick has it
//to begin with.  OK keeps them, and they are written with the IDE's settings.
class CIDEDuplicatesPage : public CSettingsPage
{
CONSTRUCTABLE(CIDEDuplicatesPage);
DESTRUCTABLE(CIDEDuplicatesPage);
protected:
	CIDEContent*		mpcContent;  //Not held.
	Ptr<CDialog>		mpDialog;  //The settings dialog's.
	Ptr<CTextField>		mpStatements;
	Ptr<CTextField>		mpTokens;
	Ptr<CCheckBox>		mpLiterals;
	Ptr<CGridContainer>	mpBody;

public:
	void			Init(CIDEContent* pcContent, Ptr<CDialog> pDialog);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	const char*		GetTitle(void) const override;
	Ptr<CComponent>	GetBody(void) const override;
	Ptr<CComponent>	GetFirstEditor(void) const override;
	void			Keep(void) override;
	void			ApplyEdits(const char* szEdits);

protected:
	Ptr<CLabel>		CreateLabel(const char* szText);
};


#endif // __IDE_DUPLICATES_PAGE_H__

