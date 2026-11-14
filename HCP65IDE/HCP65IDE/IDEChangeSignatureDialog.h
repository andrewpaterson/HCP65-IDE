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
#ifndef __IDE_CHANGE_SIGNATURE_DIALOG_H__
#define __IDE_CHANGE_SIGNATURE_DIALOG_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Button.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/ListContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TextField.h"
#include "IDEChangeSignature.h"


//Change Signature's dialog, as IntelliJ's: the function's name and what it returns, its parameters in a list with
//Add, Remove, Up and Down under it, the selected parameter's type, name, default value and (a new one's) value in
//the calls there already are in a form beside it, and under them the signature as it would be declared, as it is
//changed.  OK isn't allowed while what is written can't be the signature (CIDEChangeSignature::FindProblem says
//why).  It works on rows of its own, so Cancel changes nothing; GetChange gives what was asked for, with only what
//differs from the function as it is.
class CIDEChangeSignatureDialog : public CObject
{
CONSTRUCTABLE(CIDEChangeSignatureDialog);
DESTRUCTABLE(CIDEChangeSignatureDialog);
protected:
	Ptr<CDialog>			mpDialog;
	Ptr<CTextField>			mpFunction;
	Ptr<CTextField>			mpReturn;
	Ptr<CListContainer>		mpList;
	Ptr<CScrollPane>		mpScrollPane;
	Ptr<CButton>			mpAdd;
	Ptr<CButton>			mpRemove;
	Ptr<CButton>			mpUp;
	Ptr<CButton>			mpDown;
	Ptr<CTextField>			mpType;
	Ptr<CTextField>			mpName;
	Ptr<CTextField>			mpDefault;
	Ptr<CTextField>			mpCallValue;
	Ptr<CLabel>				mpPreview;
	Ptr<CLabel>				mpError;
	CIDEChangeSignature*	mpcOwner;  //Not held.
	SIDESignature*			mpsSignature;  //Not held.
	CArrayIDENewParameters	masRows;  //Each parameter as the dialog has it, every part written out.
	int32					miSelected;  //-1 if there are none.
	bool					mbUpdating;  //The widgets are being set, not changed by the user.

public:
	void				Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CIDEChangeSignature* pcOwner, SIDESignature* psSignature);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	bool				Show(void);
	void				ApplyEdits(const char* szEdits);
	bool				Approve(void);
	void				GetChange(SIDESignatureChange* psDest);
	Ptr<CDialog>		GetDialog(void);

protected:
	Ptr<CComponent>		CreateBody(void);
	void				AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor);
	Ptr<CTextField>		CreateField(void);
	Ptr<CButton>		CreateButton(const char* szCaption);
	Ptr<CComponent>		CreateButtonGap(void);
	void				StopListening(Ptr<CTextField> pField);
	void				StopListening(Ptr<CButton> pButton);
	void				ShowList(void);
	void				ShowSelected(void);
	void				ShowPreview(void);
	void				AppendRow(SIDENewParameter* psRow, CChars* pszDest);
	void				Keep(void);
	void				AddParameter(void);
	void				RemoveParameter(void);
	void				MoveParameter(int32 iBy);
	void				AddClicked(CButton* pcButton);
	void				RemoveClicked(CButton* pcButton);
	void				UpClicked(CButton* pcButton);
	void				DownClicked(CButton* pcButton);
	void				SelectionChanged(void);
	void				FieldChanged(void);
};


#endif // __IDE_CHANGE_SIGNATURE_DIALOG_H__

