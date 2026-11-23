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
#ifndef __IDE_EDITOR_PAGE_H__
#define __IDE_EDITOR_PAGE_H__
#include "WindowLib/CheckBox.h"
#include "WindowLib/Label.h"
#include "WindowLib/TextField.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/SettingsDialog.h"


class CIDEContent;


//Settings' Editor page: whether an editor shows its breadcrumbs (what the caret is in, under the code) and its sticky
//lines (the first lines of what the top of the view is in, kept over the code), and whether held keys repeat, after how
//long and how fast.  OK keeps them (CIDEStructure has the editors', CIDESettings the keys'), and they are written with
//the IDE's settings.
class CIDEEditorPage : public CSettingsPage
{
CONSTRUCTABLE(CIDEEditorPage);
DESTRUCTABLE(CIDEEditorPage);
protected:
	CIDEContent*		mpcContent;  //Not held.
	Ptr<CCheckBox>		mpBreadcrumbs;
	Ptr<CCheckBox>		mpStickyLines;
	Ptr<CCheckBox>		mpKeyRepeat;
	Ptr<CTextField>		mpRepeatDelay;
	Ptr<CTextField>		mpRepeatRate;
	Ptr<CDialog>		mpDialog;
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
	Ptr<CTextField>	CreateNumberField(uint32 uiNumber);
	static uint32	ReadNumber(const char* szText, uint32 uiMinimum, uint32 uiMaximum, uint32 uiWas);
};


#endif // __IDE_EDITOR_PAGE_H__

