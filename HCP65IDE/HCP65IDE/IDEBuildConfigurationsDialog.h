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
#ifndef __IDE_BUILD_CONFIGURATIONS_DIALOG_H__
#define __IDE_BUILD_CONFIGURATIONS_DIALOG_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BuildLib/BuildConfigurations.h"
#include "VisualStudioLib/VcxSolution.h"
#include "WindowLib/Button.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/ListContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TextField.h"


#define IDE_BUILD_DIALOG_OPEN_FILE	"Open File"


//Edit Build Configurations: a solution's build configurations on the left, and the one selected in a form on the right.
//What it has inside it (its image, executable or module, and its loads) is said but not edited: Open File closes the
//dialog and opens the file for that.  Add copies the one selected, with all that is inside it.  It works on edits
//(CBuildConfigurationEdit), so Cancel changes nothing, and OK isn't allowed while they couldn't be read back.
class CIDEBuildConfigurationsDialog : public CObject
{
CONSTRUCTABLE(CIDEBuildConfigurationsDialog);
DESTRUCTABLE(CIDEBuildConfigurationsDialog);
protected:
	Ptr<CDialog>					mpDialog;
	Ptr<CListContainer>				mpList;
	Ptr<CScrollPane>				mpScrollPane;
	Ptr<CButton>					mpAdd;
	Ptr<CButton>					mpRemove;
	Ptr<CTextField>					mpName;
	Ptr<CComboBox>					mpToolchain;
	Ptr<CComboBox>					mpSolution;
	Ptr<CTextField>					mpBreakpointSpace;
	Ptr<CCheckBox>					mpNoFloats;
	Ptr<CCheckBox>					mpMathTables;
	Ptr<CCheckBox>					mpPreferSize;
	Ptr<CTextField>					mpCard;
	Ptr<CTextField>					mpCardPath;
	Ptr<CLabel>						mpContents;
	Ptr<CLabel>						mpError;
	CBuildConfigurations*			mpcConfigurations;  //Not held.
	CVcxSolution*					mpcSolution;  //Not held.
	CArrayBuildConfigurationEdits	masEdits;
	int32							miSelected;  //-1 if there are none.
	bool							mbUpdating;  //The widgets are being set, not changed by the user.

public:
	void				Init(Ptr<CWindow> pOwner, SDialogLook* psLook, const char* szSolution, CVcxSolution* pcSolution, CBuildConfigurations* pcConfigurations, const char* szSelected);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	bool				Show(void);
	void				ApplyEdits(const char* szEdits);
	bool				Approve(void);

	Ptr<CDialog>		GetDialog(void);
	CArrayBuildConfigurationEdits*	GetEdits(void);

protected:
	Ptr<CComponent>		CreateBody(void);
	void				AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor);
	Ptr<CTextField>		CreateField(void);
	void				ShowList(void);
	void				ShowSelected(void);
	void				ShowContents(CBuildConfigurationEdit* pcEdit);
	void				SelectItem(Ptr<CComboBox> pBox, const char* szText);
	void				Keep(void);
	void				AddClicked(CButton* pcButton);
	void				RemoveClicked(CButton* pcButton);
	void				AddConfiguration(void);
	void				RemoveConfiguration(void);
	void				SelectionChanged(void);
	void				NameChanged(void);
	const char*			FindProblem(CBuildConfigurationEdit* pcEdit, size uiEdit);
};


#endif // __IDE_BUILD_CONFIGURATIONS_DIALOG_H__

