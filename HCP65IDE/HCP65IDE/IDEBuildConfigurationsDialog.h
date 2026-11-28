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
#ifndef __IDE_BUILD_CONFIGURATIONS_DIALOG_H__
#define __IDE_BUILD_CONFIGURATIONS_DIALOG_H__
#include "BaseLib/ArrayInt.h"
#include "StandardLib/Array.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BuildLib/BuildOptionEdits.h"
#include "VisualStudioLib/VcxSolution.h"
#include "WindowLib/Button.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/ListContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TextField.h"


#define IDE_BUILD_DIALOG_CLEAN_UP	"Clean Up"


//One of the dialog's build configurations: its name as edited, the solution configuration it was read from (its
//own, or for a new one the configuration it copies), and whether Add made it.
class CIDEBuildDialogEntry
{
public:
	CChars	mszName;
	CChars	mszSource;
	bool	mbNew;

	void	Init(const char* szName, const char* szSource, bool bNew);
	void	Kill(void);
};


typedef CArrayTemplate<CIDEBuildDialogEntry>	CArrayIDEBuildDialogEntries;


//Edit Build Configurations (ConfigurationsPlan.txt C9): a solution's build configurations, which are its solution
//configurations, on the left, and the one selected on the right in one page of two sections (question 30).  The
//first has the whole program's options, which are the startup project's: what it is compiled with and what it is
//linked into.  The second has the options of one project's own code, the project chosen from a drop-down.  Beside
//each option is where its value comes from, a drop-down (questions 33 and 34): the project's own for the one
//configuration or for every configuration of the platform, a property sheet the project imports with how many
//projects share it, the program, or the default.  Changing a value writes it where the drop-down says; choosing
//another place moves it.  The options are the HCP65 compiler's, so only an HCP65 configuration has them.
//
//Add copies the configuration selected under a new name on the same toolchain, Remove takes one away and the name
//renames it: all three change the solution's rows alone (question 35).  A project is given a configuration of its own
//only when one of its options is made to differ for a configuration it is built as another's for, and the note under
//the options says so before OK does it.  Clean Up closes the dialog as OK does and then offers the projects'
//configurations that no solution builds (CIDEBuilding::ShowCleanUpDialog).
//
//It works on edits (CBuildOptionEdits), so Cancel changes nothing, and OK isn't allowed while what it has can't be
//written.  Write writes them.
class CIDEBuildConfigurationsDialog : public CObject
{
CONSTRUCTABLE(CIDEBuildConfigurationsDialog);
DESTRUCTABLE(CIDEBuildConfigurationsDialog);
protected:
	Ptr<CDialog>					mpDialog;
	Ptr<CListContainer>				mpList;
	Ptr<CScrollPane>				mpScrollPane;
	Ptr<CScrollPane>				mpFormPane;
	Ptr<CButton>					mpAdd;
	Ptr<CButton>					mpRemove;
	Ptr<CButton>					mpCleanUp;
	Ptr<CTextField>					mpName;
	Ptr<CLabel>						mpToolchain;
	Ptr<CLabel>						mpProgram;
	Ptr<CComboBox>					mpProject;
	CArray<CCheckBox>				maChecks;	//The options that are on or off...
	CArray<CTextField>				maFields;	//...and those that are text,
	CArrayInt						maiEditors;	//each option's index in its one of those two,
	CArray<CComboBox>				maPlaces;	//and where each option's value is from, by the option's index.
	Ptr<CLabel>						mpNote;
	Ptr<CLabel>						mpError;
	CVcxSolution*					mpcSolution;  //Not held.
	CBuildOptionEdits				mcOptions;	//A configuration for each entry, at its index.
	CArrayIDEBuildDialogEntries		masEntries;
	int32							miSelected;  //-1 if there are none.
	size							muiProject;  //The project whose own options show; MAX_SIZE if there is none.
	bool							mbUpdating;  //The widgets are being set, not changed by the user.

public:
	void				Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CVcxSolution* pcSolution, const char* szStartupProject, const char* szSelected);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	bool				Show(void);
	void				ApplyEdits(const char* szEdits);
	bool				Approve(void);
	bool				Write(CChars* pszNote);

	Ptr<CDialog>		GetDialog(void) const;
	size				NumEntries(void) const;
	CIDEBuildDialogEntry*	GetEntry(size uiIndex);
	bool				IsEdited(void);

protected:
	Ptr<CComponent>		CreateBody(void);
	void				AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor, Ptr<CComponent> pPlace);
	void				AddOptionRows(Ptr<CGridContainer> pForm, int32* piRow, bool bProgram);
	Ptr<CTextField>		CreateField(void);
	void				ShowList(void);
	void				ShowSelected(void);
	void				ShowOptions(void);
	void				GetPlatform(const char* szName, CChars* pszDest);
	size				GetOptionProject(size uiOption);
	void				SetOptionValue(size uiOption, const char* szValue);
	void				SetOptionPlace(size uiOption, EBuildOptionPlace ePlace);
	void				SelectProject(const char* szName);
	void				SetName(const char* szName);
	void				AddClicked(CButton* pcButton);
	void				RemoveClicked(CButton* pcButton);
	void				CleanUpClicked(CButton* pcButton);
	void				AddConfiguration(void);
	void				RemoveConfiguration(void);
	void				SelectionChanged(void);
	void				NameChanged(void);
	void				ProjectChanged(void);
	void				CheckChanged(CCheckBox* pcCheckBox);
	void				FieldChanged(CTextField* pcField);
	void				PlaceChanged(CComboBox* pcComboBox);
	void				PrintOptions(void);
	const char*			FindProblem(size uiEntry);
};


#endif // __IDE_BUILD_CONFIGURATIONS_DIALOG_H__

