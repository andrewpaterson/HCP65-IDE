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
#ifndef __IDE_KEYMAP_PAGE_H__
#define __IDE_KEYMAP_PAGE_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/Button.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/ComponentInput.h"
#include "WindowLib/KeyCaptureField.h"
#include "WindowLib/Label.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TextField.h"
#include "IDEScript.h"
#include "IDESettingsDialog.h"


class CIDEKeymapPage;


//Add Shortcut: the keys pressed, and what giving them to the command would mean.  OK isn't allowed for keys that can't
//be given to it.
class CIDEKeyCaptureDialog : public CObject
{
CONSTRUCTABLE(CIDEKeyCaptureDialog);
DESTRUCTABLE(CIDEKeyCaptureDialog);
protected:
	Ptr<CDialog>			mpDialog;
	Ptr<CKeyCaptureField>	mpField;
	Ptr<CLabel>				mpMessage;
	CIDEKeymapPage*			mpcPage;  //Not held: it holds this dialog.
	size					muiRow;

public:
	void			Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CIDEKeymapPage* pcPage, size uiRow);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	bool			Show(void);
	char*			GetKeys(void);
	void			SetKeys(const char* szKeys);
	Ptr<CDialog>	GetDialog(void);
	bool			Approve(void);

protected:
	void			KeysCaptured(void);
};


//A command and the keys it will have if OK closes the dialog.
struct SIDEKeymapRow
{
	CCommand*	pcCommand;
	CArrayChars	aszKeys;
};


typedef CArrayTemplate<SIDEKeymapRow>	CArrayIDEKeymapRows;


//Settings' Keymap, as IntelliJ IDEA's: every command in the groups a user knows them by, with a filter, and the keys
//each has.  A key belongs to one command, so giving it to another moves it.  Refused are keys that type a character, and
//for a command that works everywhere keys a dialog or text field needs, as they don't have the keys of an editor's.
class CIDEKeymapPage : public CIDESettingsPage
{
CONSTRUCTABLE(CIDEKeymapPage);
DESTRUCTABLE(CIDEKeymapPage);
protected:
	Ptr<CDialog>				mpDialog;
	Ptr<CCommandSet>			mpCommands;
	CComponentInput*			mpcInput;  //Not held: the application holds it.
	Ptr<CIDEScript>				mpScript;  //Watches Add Shortcut as it does the dialog the page is on.
	CChars						mszScriptedKeys;  //What a check has Add Shortcut capture, ApplyEdits's capture:<keys>.
	Ptr<CComponent>				mpBody;
	Ptr<CTextField>				mpFilter;
	Ptr<CListContainer>			mpList;
	Ptr<CScrollPane>			mpScrollPane;
	Ptr<CButton>				mpAdd;
	Ptr<CButton>				mpRemove;
	Ptr<CButton>				mpReset;
	Ptr<CButton>				mpResetAll;
	Ptr<CLabel>					mpStatus;
	Ptr<CIDEKeyCaptureDialog>	mpCapture;  //Add Shortcut, while it's shown.
	CArrayIDEKeymapRows			masRows;
	CArrayInt					maiListRows;  //The row in masRows of each row of the list, -1 for a group's heading.
	bool						mbUpdating;  //The list is being remade, not selected in by the user.

public:
	void			Init(Ptr<CDialog> pDialog, Ptr<CCommandSet> pCommands, CComponentInput* pcInput, Ptr<CIDEScript> pScript);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	const char*		GetTitle(void) override;
	Ptr<CComponent>	GetBody(void) override;
	Ptr<CComponent>	GetFirstEditor(void) override;
	void			Keep(void) override;

	void			ApplyEdits(const char* szEdits);
	bool			DescribeKeys(size uiRow, const char* szKeys, CChars* pszMessage);

protected:
	Ptr<CComponent>	CreateBody(void);
	Ptr<CButton>	CreateButton(Ptr<CGridContainer> pButtons, int32 iColumn, const char* szCaption);
	void			AddRows(void);
	void			ShowList(void);
	void			ShowKeys(size uiRow, CChars* pszDest);
	bool			IsShown(size uiRow);
	const char*		GetGroupCaption(CCommand* pcCommand);
	int32			GetSelectedRow(void);
	void			SelectRow(size uiRow);
	bool			AddKeys(size uiRow, const char* szKeys);
	void			RemoveKeys(size uiRow);
	void			ResetKeys(size uiRow);
	void			ResetAllKeys(void);
	void			ShowCapture(void);
	void			FilterChanged(void);
	void			RowActivated(void);
	void			AddClicked(CButton* pcButton);
	void			RemoveClicked(CButton* pcButton);
	void			ResetClicked(CButton* pcButton);
	void			ResetAllClicked(CButton* pcButton);
};


#endif // __IDE_KEYMAP_PAGE_H__

