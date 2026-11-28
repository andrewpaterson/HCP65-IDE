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
#ifndef __IDE_RUN_CONFIGURATIONS_DIALOG_H__
#define __IDE_RUN_CONFIGURATIONS_DIALOG_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BuildLib/BuildWorkspace.h"
#include "WindowLib/Button.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/ListContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TextField.h"


//Edit Configurations, as IntelliJ IDEA's: the run configurations on the left, added to and removed from with the
//buttons under them, and the one selected in a form on the right.  It works on copies, so Cancel changes nothing.
//OK isn't allowed while a name is empty or used twice.
//
//How an HCP65 program is debugged is in the form too (ConfigurationsPlan.txt C8-3, question 29): the file loaded first
//and its address, and its card, chosen by name from the card files found near the solutions, with what the card file
//says under it.  A run configuration with more than one file loaded first shows the first and says so, and keeps them
//all: they are edited in its file.  Stored in says where it is kept: the user's workspace (private), or beside one of
//the solutions (shared, a file of its own: CRunConfiguration::GetShared); its paths are written relative to there.
//Open Card File closes the dialog and opens the selected one's card file.
#define IDE_RUN_DIALOG_OPEN_CARD		"Open Card File"
#define IDE_RUN_DIALOG_NO_CARD			"(none)"
#define IDE_RUN_DIALOG_PRIVATE			"My workspace (private)"
#define IDE_RUN_DIALOG_SEVERAL_LOADS	"More than one file is loaded first: the first is shown, and they are edited in its .hcp65run file."
class CIDERunConfigurationsDialog : public CObject
{
CONSTRUCTABLE(CIDERunConfigurationsDialog);
DESTRUCTABLE(CIDERunConfigurationsDialog);
protected:
	Ptr<CDialog>			mpDialog;
	Ptr<CListContainer>		mpList;
	Ptr<CScrollPane>		mpScrollPane;
	Ptr<CButton>			mpAdd;
	Ptr<CButton>			mpRemove;
	Ptr<CTextField>			mpName;
	Ptr<CComboBox>			mpBuildConfiguration;
	Ptr<CTextField>			mpTool;
	Ptr<CTextField>			mpDirectory;
	Ptr<CTextField>			mpArguments;
	Ptr<CTextField>			mpLoadFile;
	Ptr<CTextField>			mpLoadAddress;
	Ptr<CComboBox>			mpCard;
	Ptr<CGridContainer>		mpCardPrograms;
	Ptr<CComboBox>			mpStored;
	Ptr<CLabel>				mpError;
	CArrayRunConfigurations	masEdited;
	CArrayChars				maszBuildConfigurations;
	CArrayChars				maszCards;				//The card files in mpCard, full paths, after "(none)".
	CArrayChars				maszSolutions;			//The solutions' folders in mpStored, after the workspace.
	CChars					mszPrivateDirectory;	//What a private one's paths are relative to: the workspace file's folder.
	int32					miSelected;  //-1 if there are none.
	bool					mbUpdating;  //The widgets are being set, not changed by the user.

public:
	void				Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CBuildWorkspace* pcWorkspace, const char* szSelected);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	bool				Show(void);
	void				ApplyEdits(const char* szEdits);
	void				Keep(void);
	bool				Approve(void);

	Ptr<CDialog>		GetDialog(void) const;
	CArrayRunConfigurations*	GetEdited(void);
	char*				GetSelectedName(void);
	void				GetSelectedCard(CChars* pszDest);

protected:
	Ptr<CComponent>		CreateBody(void);
	void				AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor);
	Ptr<CTextField>		CreateField(void);
	void				ShowList(void);
	void				ShowSelected(void);
	void				SelectBuildConfiguration(const char* szConfiguration);
	void				AddClicked(CButton* pcButton);
	void				RemoveClicked(CButton* pcButton);
	void				AddConfiguration(void);
	void				RemoveConfiguration(void);
	void				SelectionChanged(void);
	void				NameChanged(void);
	void				CardChanged(void);
	void				FindCards(CBuildWorkspace* pcWorkspace);
	void				AddCard(const char* szFile);
	void				AddCardsIn(const char* szDirectory);
	void				ShowCard(void);
	void				GetDirectory(CRunConfiguration* pcRun, CChars* pszDest);
	void				MakeRelative(const char* szDirectory, const char* szFull, CChars* pszDest);
	void				Store(CRunConfiguration* pcRun, const char* szShared);
	void				KillEdited(void);
};


#endif // __IDE_RUN_CONFIGURATIONS_DIALOG_H__

