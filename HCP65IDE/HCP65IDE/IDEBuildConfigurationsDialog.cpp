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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/PrimitiveTypes.h"
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDEBuildConfigurationsDialog.h"


#define IDE_BUILD_DIALOG_WIDTH			1080
#define IDE_BUILD_DIALOG_HEIGHT			640
#define IDE_BUILD_DIALOG_LIST_WIDTH		240
#define IDE_BUILD_DIALOG_PLACE_WIDTH	370
#define IDE_BUILD_DIALOG_GAP			12


static const char*	gaszIDEBuildPlaces[BOP_NumPlaces] = { "configuration", "platform", "project", "sheet", "program", "default" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildDialogEntry::Init(const char* szName, const char* szSource, bool bNew)
{
	mszName.Init(szName);
	mszSource.Init(szSource);
	mbNew = bNew;
}


void CIDEBuildDialogEntry::Kill(void)
{
	mszSource.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An entry for each of the solution's configurations, szSelected selected.
// The solution isn't held.  szStartupProject is the project whose options
// are the whole program's, NULL or empty if none is chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CVcxSolution* pcSolution, const char* szStartupProject, const char* szSelected)
{
	Ptr<CComponent>	pBody;
	CChars			szTitle;
	size			i;

	PreInit();

	mpcSolution = pcSolution;
	mcOptions.Init(pcSolution, szStartupProject);
	masEntries.Init();
	maiEditors.Init();
	maChecks.Init();
	maFields.Init();
	maPlaces.Init();
	miSelected = -1;
	muiProject = MAX_SIZE;
	mbUpdating = false;
	for (i = 0; i < pcSolution->NumConfigurations(); i++)
	{
		masEntries.Add()->Init(pcSolution->GetConfiguration(i), pcSolution->GetConfiguration(i), false);
		mcOptions.Add(pcSolution->GetConfiguration(i), pcSolution->GetConfiguration(i));
		if ((miSelected == -1) || (szSelected && (StringCompare(pcSolution->GetConfiguration(i), szSelected) == 0)))
		{
			miSelected = (int32)i;
		}
	}

	szTitle.Init("Build Configurations: ");
	szTitle.Append(pcSolution->GetName());
	mpDialog = OMalloc<CDialog>(pOwner, psLook, szTitle.Text());
	szTitle.Kill();
	mpDialog->SetResizable(IDE_BUILD_DIALOG_WIDTH / 2, IDE_BUILD_DIALOG_HEIGHT / 2);
	//The body is made first: it makes the component that has the keys when the dialog opens.
	pBody = CreateBody();
	mpDialog->SetBody(pBody, mpName);
	mpDialog->SetApprover(this, &CIDEBuildConfigurationsDialog::Approve);

	mpList->GetSelectionEvent()->Add(this, &CIDEBuildConfigurationsDialog::SelectionChanged);
	mpName->GetChangeEvent()->Add(this, &CIDEBuildConfigurationsDialog::NameChanged);
	mpProject->GetSelectionEvent()->Add(this, &CIDEBuildConfigurationsDialog::ProjectChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDEBuildConfigurationsDialog::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDEBuildConfigurationsDialog::RemoveClicked);
	mpCleanUp->GetClickEvent()->Add(this, &CIDEBuildConfigurationsDialog::CleanUpClicked);
	for (i = 0; i < maChecks.NumElements(); i++)
	{
		maChecks.Get(i)->GetChangeEvent()->Add(this, &CIDEBuildConfigurationsDialog::CheckChanged);
	}
	for (i = 0; i < maFields.NumElements(); i++)
	{
		maFields.Get(i)->GetChangeEvent()->Add(this, &CIDEBuildConfigurationsDialog::FieldChanged);
	}
	for (i = 0; i < maPlaces.NumElements(); i++)
	{
		maPlaces.Get(i)->GetSelectionEvent()->Add(this, &CIDEBuildConfigurationsDialog::PlaceChanged);
	}

	ShowList();
	ShowSelected();

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpList);
	M_Pointer(mpScrollPane);
	M_Pointer(mpFormPane);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpCleanUp);
	M_Pointer(mpName);
	M_Pointer(mpToolchain);
	M_Pointer(mpProgram);
	M_Pointer(mpProject);
	M_Embedded(maChecks);
	M_Embedded(maFields);
	M_Embedded(maPlaces);
	M_Pointer(mpNote);
	M_Pointer(mpError);
	U_Unknown(CArrayInt, maiEditors);
	U_Pointer(mpcSolution);
	U_Unknown(CBuildOptionEdits, mcOptions);
	U_Unknown(CArrayIDEBuildDialogEntries, masEntries);
	U_Int32(miSelected);
	U_Size(muiProject);
	U_Bool(mbUpdating);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so the
// dialog takes itself off the widgets still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::Free(void)
{
	size	i;

	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearApprover();
	}
	if (mpList.IsNotNull())
	{
		mpList->GetSelectionEvent()->Remove(this);
	}
	if (mpName.IsNotNull())
	{
		mpName->GetChangeEvent()->Remove(this);
	}
	if (mpProject.IsNotNull())
	{
		mpProject->GetSelectionEvent()->Remove(this);
	}
	if (mpAdd.IsNotNull())
	{
		mpAdd->GetClickEvent()->Remove(this);
	}
	if (mpRemove.IsNotNull())
	{
		mpRemove->GetClickEvent()->Remove(this);
	}
	if (mpCleanUp.IsNotNull())
	{
		mpCleanUp->GetClickEvent()->Remove(this);
	}
	for (i = 0; i < maChecks.NumElements(); i++)
	{
		maChecks.Get(i)->GetChangeEvent()->Remove(this);
	}
	for (i = 0; i < maFields.NumElements(); i++)
	{
		maFields.Get(i)->GetChangeEvent()->Remove(this);
	}
	for (i = 0; i < maPlaces.NumElements(); i++)
	{
		maPlaces.Get(i)->GetSelectionEvent()->Remove(this);
	}
	for (i = 0; i < masEntries.NumElements(); i++)
	{
		masEntries.Get(i)->Kill();
	}
	masEntries.Kill();
	maiEditors.Kill();
	mcOptions.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDEBuildConfigurationsDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The list and its buttons on the left; on the right the page, which
// scrolls: the name, the whole program's options, the project and its own
// options, and what is to be said under them.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEBuildConfigurationsDialog::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CGridContainer>		pLeft;
	Ptr<CGridContainer>		pButtons;
	Ptr<CGridContainer>		pForm;
	Ptr<CFillContainer>		pGap;
	SInsets					sInsets;
	int32					iRow;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	sInsets.Init(0, 0);
	mpList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpList->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpScrollPane->SetContent(mpList);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpScrollPane->SetDesiredSize(IDE_BUILD_DIALOG_LIST_WIDTH, -1);

	mpAdd = OMalloc<CButton>(pWindow, psLook->pcTheme, "Add");
	mpAdd->SetAlignment(CSH_Left, CSV_Center);
	mpRemove = OMalloc<CButton>(pWindow, psLook->pcTheme, "Remove");
	mpRemove->SetAlignment(CSH_Left, CSV_Center);
	mpCleanUp = OMalloc<CButton>(pWindow, psLook->pcTheme, IDE_BUILD_DIALOG_CLEAN_UP);
	mpCleanUp->SetAlignment(CSH_Left, CSV_Center);
	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	pButtons->AddComponent(mpAdd, 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_BUILD_DIALOG_GAP / 2, 1);
	pButtons->AddComponent(pGap, 1, 0);
	pButtons->AddComponent(mpRemove, 2, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_BUILD_DIALOG_GAP / 2, 1);
	pButtons->AddComponent(pGap, 3, 0);
	pButtons->AddComponent(mpCleanUp, 4, 0);
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 5, 0, 1, 1, 1, 0);  //Keeps the buttons on the left.

	pLeft = OMalloc<CGridContainer>(pWindow);
	pLeft->SetAlignment(CSH_Stretch, CSV_Stretch);
	pLeft->AddComponent(mpScrollPane, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_BUILD_DIALOG_GAP / 2);
	pLeft->AddComponent(pGap, 0, 1);
	pLeft->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	mpName = CreateField();
	mpToolchain = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpToolchain->SetAlignment(CSH_Left, CSV_Center);
	mpProgram = OMalloc<CLabel>(pWindow, psLook->pcDimFont, "");
	mpProgram->SetAlignment(CSH_Left, CSV_Center);
	mpProject = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpProject->SetAlignment(CSH_Stretch, CSV_Center);
	mpNote = OMalloc<CLabel>(pWindow, psLook->pcDimFont, "");
	mpNote->SetAlignment(CSH_Left, CSV_Center);
	mpError = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpError->SetAlignment(CSH_Left, CSV_Center);

	pForm = OMalloc<CGridContainer>(pWindow);
	pForm->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pForm, &iRow, "Name", mpName, NULL);
	AddFormRow(pForm, &iRow, "Toolchain", mpToolchain, NULL);
	AddFormRow(pForm, &iRow, NULL, mpProgram, NULL);
	AddOptionRows(pForm, &iRow, true);
	AddFormRow(pForm, &iRow, "One project's own code", mpProject, NULL);
	AddOptionRows(pForm, &iRow, false);
	AddFormRow(pForm, &iRow, NULL, mpNote, NULL);
	AddFormRow(pForm, &iRow, NULL, mpError, NULL);

	//The page is as high as its rows need, however high the dialog is: a scroll pane lays its content out at the
	//view's size on an axis it has no desired size on.
	pForm->SetRequiredSize();
	pForm->SetDesiredSize(-1, pForm->GetRequiredSize().y);
	mpFormPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpFormPane->SetContent(pForm);
	mpFormPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->AddComponent(pLeft, 0, 0, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_BUILD_DIALOG_GAP, 1);
	pBody->AddComponent(pGap, 1, 0);
	pBody->AddComponent(mpFormPane, 2, 0, 1, 1, 1, 1);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
// A row for each option of the whole program (bProgram) or of a project's
// own code: a box to tick, or a label and a field, and beside it the
// drop-down of where its value comes from.  The widgets are kept by the
// option's index.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::AddOptionRows(Ptr<CGridContainer> pForm, int32* piRow, bool bProgram)
{
	Ptr<CWindow>		pWindow;
	SDialogLook*		psLook;
	const SBuildOption*	psOption;
	Ptr<CCheckBox>		pCheck;
	Ptr<CTextField>		pField;
	Ptr<CComboBox>		pPlace;
	size				i;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();
	while (maiEditors.NumElements() < NumBuildOptions())
	{
		maiEditors.Add(-1);
		pPlace = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
		pPlace->SetAlignment(CSH_Stretch, CSV_Center);
		pPlace->SetDesiredSize(IDE_BUILD_DIALOG_PLACE_WIDTH, -1);
		maPlaces.Add(pPlace);
	}
	for (i = 0; i < NumBuildOptions(); i++)
	{
		psOption = GetBuildOption(i);
		if (psOption->bProgram != bProgram)
		{
			continue;
		}
		if (psOption->bBoolean)
		{
			pCheck = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, psOption->szLabel);
			pCheck->SetAlignment(CSH_Left, CSV_Center);
			maiEditors.SetValue(i, (int)maChecks.NumElements());
			maChecks.Add(pCheck);
			AddFormRow(pForm, piRow, NULL, pCheck, maPlaces.Get(i));
		}
		else
		{
			pField = CreateField();
			maiEditors.SetValue(i, (int)maFields.NumElements());
			maFields.Add(pField);
			AddFormRow(pForm, piRow, psOption->szLabel, pField, maPlaces.Get(i));
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A label and its editor, as CDialog's own rows are, and what goes on the
// right of them if anything does.  Without a label the editor is across
// both its columns.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor, Ptr<CComponent> pPlace)
{
	Ptr<CWindow>		pWindow;
	SDialogLook*		psLook;
	Ptr<CLabel>			pLabel;
	Ptr<CFillContainer>	pGap;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();
	if (*piRow > 0)
	{
		pGap = OMalloc<CFillContainer>(pWindow);
		pGap->SetDesiredSize(1, psLook->pcTheme->GetMetric(TM_DialogRowGap));
		pForm->AddComponent(pGap, 0, *piRow);
		(*piRow)++;
	}
	if (szLabel)
	{
		pLabel = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), szLabel);
		pLabel->SetDesiredSize(pLabel->GetRequiredSize().x + psLook->pcTheme->GetMetric(TM_DialogLabelGap), -1);
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		pForm->AddComponent(pLabel, 0, *piRow);
		pForm->AddComponent(pEditor, 1, *piRow, 1, 1, 1, 0);
	}
	else
	{
		pForm->AddComponent(pEditor, 0, *piRow, pPlace.IsNotNull() ? 2 : 4, 1, 1, 0);
	}
	if (pPlace.IsNotNull())
	{
		pGap = OMalloc<CFillContainer>(pWindow);
		pGap->SetDesiredSize(IDE_BUILD_DIALOG_GAP, 1);
		pForm->AddComponent(pGap, 2, *piRow);
		pForm->AddComponent(pPlace, 3, *piRow);
	}
	(*piRow)++;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextField> CIDEBuildConfigurationsDialog::CreateField(void)
{
	Ptr<CTextField>	pField;

	pField = OMalloc<CTextField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, "");
	pField->SetColumns(10);
	pField->SetAlignment(CSH_Stretch, CSV_Center);
	return pField;
}


//////////////////////////////////////////////////////////////////////////
//
// A row for each configuration, the selected one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ShowList(void)
{
	Ptr<CLabel>	pLabel;
	size		i;

	mbUpdating = true;
	mpList->RemoveAllComponents();
	for (i = 0; i < masEntries.NumElements(); i++)
	{
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), masEntries.Get(i)->mszName.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpList->AddItem(pLabel);
	}
	mpList->Select(miSelected);
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A configuration's platform: what is after its last |, so a name typed
// with one in it (which OK refuses) doesn't change it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::GetPlatform(const char* szName, CChars* pszDest)
{
	const char*	szBar;

	szBar = strrchr(szName, '|');
	pszDest->Set(szBar ? szBar + 1 : "");
}


//////////////////////////////////////////////////////////////////////////
//
// The selected configuration on the page, which is empty if there are
// none: its name (without its platform, which is its toolchain's and
// stays), its toolchain, the projects it builds to choose from, and the
// options.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ShowSelected(void)
{
	CIDEBuildDialogEntry*	pcEntry;
	EBuildToolchain			eToolchain;
	CChars					szText;
	CChars					szPlatform;
	size					uiStartup;
	size					i;

	mbUpdating = true;
	pcEntry = (miSelected >= 0) ? masEntries.Get(miSelected) : NULL;
	szText.Init(pcEntry ? pcEntry->mszName.Text() : "");
	szPlatform.Init();
	GetPlatform(szText.Text(), &szPlatform);
	if (szText.Contains('|'))
	{
		szText.RemoveEnd(szText.FindFromEnd('|'));
	}
	mpName->SetText(szText.Text());

	szText.Set("");
	if (pcEntry)
	{
		eToolchain = BT_MSVC;
		CBuildConfiguration::FindToolchainOfPlatform(szPlatform.Text(), &eToolchain);
		szText.Append(CBuildConfiguration::GetToolchainName(eToolchain))->Append(" (")->Append(szPlatform)->Append(")");
	}
	mpToolchain->SetText(szText.Text());

	//The project whose own options show: the one that was showing if this configuration builds it, else its first.
	szText.Set((mpProject->GetSelectedText() != NULL) ? mpProject->GetSelectedText() : "");
	mpProject->RemoveAllItems();
	muiProject = MAX_SIZE;
	uiStartup = MAX_SIZE;
	if (pcEntry)
	{
		uiStartup = mcOptions.GetStartupProject(miSelected);
		for (i = 0; i < mcOptions.NumProjects(miSelected); i++)
		{
			mpProject->AddItem(mcOptions.GetProject(miSelected, i)->mszName.Text());
			if ((muiProject == MAX_SIZE) || mcOptions.GetProject(miSelected, i)->mszName.Equals(szText))
			{
				muiProject = i;
			}
		}
		if (muiProject != MAX_SIZE)
		{
			mpProject->Select((int32)muiProject);
		}
	}

	szText.Set("The whole program");
	if (pcEntry && (uiStartup != MAX_SIZE))
	{
		szText.Append(": the options of the startup project, ")->Append(mcOptions.GetProject(miSelected, uiStartup)->mszName);
	}
	mpProgram->SetText(szText.Text());
	mpError->SetText("");
	szPlatform.Kill();
	szText.Kill();
	mbUpdating = false;
	ShowOptions();
}


//////////////////////////////////////////////////////////////////////////
//
// The project an option is shown for: the startup project for one of the
// whole program's, else the one chosen.  MAX_SIZE if there is none.
//
//////////////////////////////////////////////////////////////////////////
size CIDEBuildConfigurationsDialog::GetOptionProject(size uiOption)
{
	if (miSelected < 0)
	{
		return MAX_SIZE;
	}
	return GetBuildOption(uiOption)->bProgram ? mcOptions.GetStartupProject(miSelected) : muiProject;
}


//////////////////////////////////////////////////////////////////////////
//
// Each option's value and where it is from, as the edits have them now,
// and the note of what OK will do besides.  An option with no project to
// be shown for is blank.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ShowOptions(void)
{
	EBuildOptionPlace	aePlaces[BOP_NumPlaces];
	const SBuildOption*	psOption;
	Ptr<CComboBox>		pPlace;
	CChars				szText;
	const char*			szValue;
	size				uiProject;
	size				uiPlaces;
	size				i;
	size				j;

	mbUpdating = true;
	szText.Init();
	for (i = 0; i < NumBuildOptions(); i++)
	{
		psOption = GetBuildOption(i);
		uiProject = GetOptionProject(i);
		szValue = (uiProject != MAX_SIZE) ? mcOptions.GetValue(miSelected, uiProject, i) : "";
		if (psOption->bBoolean)
		{
			maChecks.Get(maiEditors.GetValue(i))->SetTicked(strcmp(szValue, "true") == 0);
		}
		else if (strcmp(maFields.Get(maiEditors.GetValue(i))->GetText(), szValue) != 0)
		{
			maFields.Get(maiEditors.GetValue(i))->SetText(szValue);
		}
		pPlace = maPlaces.Get(i);
		pPlace->RemoveAllItems();
		if (uiProject != MAX_SIZE)
		{
			uiPlaces = mcOptions.GetPlaces(miSelected, uiProject, i, aePlaces);
			for (j = 0; j < uiPlaces; j++)
			{
				mcOptions.GetPlaceText(miSelected, uiProject, i, aePlaces[j], &szText);
				pPlace->AddItem(szText.Text());
				if (aePlaces[j] == mcOptions.GetPlace(miSelected, uiProject, i))
				{
					pPlace->Select((int32)j);
				}
			}
		}
	}

	szText.Set("");
	if (miSelected >= 0)
	{
		mcOptions.GetNote(miSelected, &szText);
		if (mcOptions.NumProjects(miSelected) == 0)
		{
			szText.Set("These are the HCP65 compiler's options: only an HCP65 configuration has them.");
		}
		else if (mcOptions.GetStartupProject(miSelected) == MAX_SIZE)
		{
			szText.Insert(0, "No startup project is chosen, and the whole program's options are its.  ");
		}
	}
	mpNote->SetText(szText.Text());
	szText.Kill();
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// An option's new value, where its value is from.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::SetOptionValue(size uiOption, const char* szValue)
{
	size	uiProject;

	uiProject = GetOptionProject(uiOption);
	if (uiProject != MAX_SIZE)
	{
		mcOptions.SetValue(miSelected, uiProject, uiOption, szValue);
	}
	ShowOptions();
}


//////////////////////////////////////////////////////////////////////////
//
// Where an option's value is to be from.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::SetOptionPlace(size uiOption, EBuildOptionPlace ePlace)
{
	EBuildOptionPlace	aePlaces[BOP_NumPlaces];
	size				uiProject;
	size				uiPlaces;
	size				i;

	uiProject = GetOptionProject(uiOption);
	if (uiProject != MAX_SIZE)
	{
		uiPlaces = mcOptions.GetPlaces(miSelected, uiProject, uiOption, aePlaces);
		for (i = 0; i < uiPlaces; i++)
		{
			if (aePlaces[i] == ePlace)
			{
				mcOptions.SetPlace(miSelected, uiProject, uiOption, ePlace);
			}
		}
	}
	ShowOptions();
}


//////////////////////////////////////////////////////////////////////////
//
// A box ticked or unticked, a field typed in, a place chosen: the edits
// have it, and every row shows what that makes of it (a value changed in
// a sheet is another row's too; a default changed becomes the project's).
// A drop-down's rows are made again from outside its own event.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::CheckChanged(CCheckBox* pcCheckBox)
{
	size	i;

	for (i = 0; !mbUpdating && (i < NumBuildOptions()); i++)
	{
		if (GetBuildOption(i)->bBoolean && (&maChecks.Get(maiEditors.GetValue(i)) == pcCheckBox))
		{
			SetOptionValue(i, pcCheckBox->IsTicked() ? "true" : "false");
			return;
		}
	}
}


void CIDEBuildConfigurationsDialog::FieldChanged(CTextField* pcField)
{
	size	i;

	for (i = 0; !mbUpdating && (i < NumBuildOptions()); i++)
	{
		if (!GetBuildOption(i)->bBoolean && (&maFields.Get(maiEditors.GetValue(i)) == pcField))
		{
			SetOptionValue(i, pcField->GetText());
			return;
		}
	}
}


void CIDEBuildConfigurationsDialog::PlaceChanged(CComboBox* pcComboBox)
{
	EBuildOptionPlace	aePlaces[BOP_NumPlaces];
	size				uiProject;
	size				uiPlaces;
	size				i;

	for (i = 0; !mbUpdating && (i < NumBuildOptions()); i++)
	{
		uiProject = GetOptionProject(i);
		if ((&maPlaces.Get(i) == pcComboBox) && (uiProject != MAX_SIZE))
		{
			uiPlaces = mcOptions.GetPlaces(miSelected, uiProject, i, aePlaces);
			if ((pcComboBox->GetSelected() >= 0) && ((size)pcComboBox->GetSelected() < uiPlaces))
			{
				mcOptions.SetPlace(miSelected, uiProject, i, aePlaces[pcComboBox->GetSelected()]);
				mpDialog->GetWindow()->Post(this, &CIDEBuildConfigurationsDialog::ShowOptions);
			}
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Another project's own options.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ProjectChanged(void)
{
	if (mbUpdating || (miSelected < 0) || (mpProject->GetSelected() < 0))
	{
		return;
	}
	muiProject = (size)mpProject->GetSelected();
	mpDialog->GetWindow()->Post(this, &CIDEBuildConfigurationsDialog::ShowOptions);
}


void CIDEBuildConfigurationsDialog::SelectProject(const char* szName)
{
	size	i;

	for (i = 0; (miSelected >= 0) && (i < mcOptions.NumProjects(miSelected)); i++)
	{
		if (mcOptions.GetProject(miSelected, i)->mszName.Equals(szName))
		{
			muiProject = i;
			mbUpdating = true;
			mpProject->Select((int32)i);
			mbUpdating = false;
		}
	}
	ShowOptions();
}


//////////////////////////////////////////////////////////////////////////
//
// Another row selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::SelectionChanged(void)
{
	if (mbUpdating || (mpList->GetSelected() < 0))
	{
		return;
	}
	miSelected = mpList->GetSelected();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The selected configuration's name, which is what is before its
// platform: that stays.  The list shows it as it is typed, and so do the
// places that say it ("This project, Fast only").
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::SetName(const char* szName)
{
	CIDEBuildDialogEntry*	pcEntry;
	CChars					szPlatform;

	if (miSelected < 0)
	{
		return;
	}
	pcEntry = masEntries.Get(miSelected);
	szPlatform.Init();
	GetPlatform(pcEntry->mszName.Text(), &szPlatform);
	pcEntry->mszName.Set(szName);
	pcEntry->mszName.Append('|')->Append(szPlatform);
	szPlatform.Kill();
	mcOptions.SetName(miSelected, pcEntry->mszName.Text());
	ShowList();
	ShowOptions();
}


void CIDEBuildConfigurationsDialog::NameChanged(void)
{
	if (!mbUpdating)
	{
		SetName(mpName->GetText());
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::AddClicked(CButton* pcButton)
{
	AddConfiguration();
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpName);
}


void CIDEBuildConfigurationsDialog::RemoveClicked(CButton* pcButton) { RemoveConfiguration(); }


//////////////////////////////////////////////////////////////////////////
//
// Clean Up saves what the dialog has, as OK does, and closes it as itself:
// what is cleaned up is then found in the files as they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::CleanUpClicked(CButton* pcButton)
{
	if (Approve())
	{
		mpDialog->Close(IDE_BUILD_DIALOG_CLEAN_UP);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A copy of the one selected, on its toolchain, called so it can be told
// apart: it is read from the rows the selected one was, so it builds what
// that builds.  There is nothing to copy without one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::AddConfiguration(void)
{
	CIDEBuildDialogEntry*	pcSelected;
	CChars					szBase;
	CChars					szPlatform;
	CChars					szName;
	CChars					szSource;
	int						iNumber;
	size					i;
	bool					bUsed;

	if (miSelected < 0)
	{
		return;
	}
	pcSelected = masEntries.Get(miSelected);
	szPlatform.Init();
	GetPlatform(pcSelected->mszName.Text(), &szPlatform);
	szBase.Init(pcSelected->mszName);
	if (szBase.Contains('|'))
	{
		szBase.RemoveEnd(szBase.FindFromEnd('|'));
	}
	szBase.Append(" Copy");
	szName.Init();
	iNumber = 1;
	do
	{
		szName.Set(szBase);
		if (iNumber > 1)
		{
			szName.Append(' ')->Append(iNumber);
		}
		szName.Append('|')->Append(szPlatform);
		bUsed = false;
		for (i = 0; i < masEntries.NumElements(); i++)
		{
			bUsed |= masEntries.Get(i)->mszName.Equals(szName.Text());
		}
		iNumber++;
	} while (bUsed);

	szSource.Init(pcSelected->mszSource);
	masEntries.Add()->Init(szName.Text(), szSource.Text(), true);
	mcOptions.Add(szName.Text(), szSource.Text());
	miSelected = (int32)masEntries.NumElements() - 1;
	szSource.Kill();
	szName.Kill();
	szBase.Kill();
	szPlatform.Kill();

	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The selected one goes, and the one after it (or now last) is selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::RemoveConfiguration(void)
{
	if (miSelected < 0)
	{
		return;
	}
	masEntries.Get(miSelected)->Kill();
	masEntries.RemoveAt(miSelected);
	mcOptions.Remove(miSelected);
	if (miSelected >= (int32)masEntries.NumElements())
	{
		miSelected = (int32)masEntries.NumElements() - 1;
	}
	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// Why the files couldn't have the entry at uiEntry, NULL if they could.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEBuildConfigurationsDialog::FindProblem(size uiEntry)
{
	CIDEBuildDialogEntry*	pcEntry;
	size					i;

	pcEntry = masEntries.Get(uiEntry);
	if (pcEntry->mszName.StartsWith('|') || pcEntry->mszName.Empty())
	{
		return "A build configuration needs a name.";
	}
	if ((pcEntry->mszName.Find('|') != pcEntry->mszName.FindFromEnd('|')) || pcEntry->mszName.Contains("=") || pcEntry->mszName.StartsWith('~'))
	{
		return "A build configuration's name can't have | or = in it, or start with ~.";
	}
	for (i = 0; i < uiEntry; i++)
	{
		if (masEntries.Get(i)->mszName.Equals(pcEntry->mszName.Text()))
		{
			return "Another build configuration has this name.";
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// OK closes the dialog only if the files could have it all; otherwise the
// first configuration that couldn't is selected and the page says why: a
// name, or linking that a build couldn't read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::Approve(void)
{
	CChars		szError;
	const char*	szProblem;
	size		i;

	mpError->SetText("");
	szError.Init();
	for (i = 0; i < masEntries.NumElements(); i++)
	{
		szProblem = FindProblem(i);
		if ((szProblem == NULL) && !mcOptions.Check(i, &szError))
		{
			szProblem = szError.Text();
		}
		if (szProblem != NULL)
		{
			miSelected = (int32)i;
			ShowList();
			ShowSelected();
			mpError->SetText(szProblem);
			szError.Kill();
			return false;
		}
	}
	szError.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The edits written: the solution's rows, then each option in its
// project or sheet (CBuildOptionEdits::Apply).  pszNote is given what was
// done besides what was asked.  False if a file couldn't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::Write(CChars* pszNote)
{
	return mcOptions.Apply(mpcSolution->GetFileName(), pszNote);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether anything is to be written: a configuration added, removed or
// renamed, or an option edited.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::IsEdited(void)
{
	size	i;
	bool	bEdited;

	bEdited = mcOptions.IsEdited() || (masEntries.NumElements() != mpcSolution->NumConfigurations());
	for (i = 0; i < masEntries.NumElements(); i++)
	{
		bEdited |= !masEntries.Get(i)->mszName.Equals(masEntries.Get(i)->mszSource);
	}
	return bEdited;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, the selected configuration's options as the page has
// them: each one's value and where it is from, the whole program's and
// then the chosen project's own, and the note.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::PrintOptions(void)
{
	CChars	szPlace;
	size	uiProject;
	size	i;

	szPlace.Init();
	printf("Build dialog: %s, project %s\n", (miSelected >= 0) ? masEntries.Get(miSelected)->mszName.Text() : "(none)", (muiProject != MAX_SIZE) ? mcOptions.GetProject(miSelected, muiProject)->mszName.Text() : "(none)");
	for (i = 0; i < NumBuildOptions(); i++)
	{
		uiProject = GetOptionProject(i);
		if (uiProject != MAX_SIZE)
		{
			mcOptions.GetPlaceText(miSelected, uiProject, i, mcOptions.GetPlace(miSelected, uiProject, i), &szPlace);
			printf("Build option: %s [%s] from [%s]\n", GetBuildOption(i)->szName, mcOptions.GetValue(miSelected, uiProject, i), szPlace.Text());
		}
	}
	printf("Build note: [%s]\n", mpNote->GetText());
	fflush(stdout);
	szPlace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do in the dialog, separated by
// semicolons: add, remove, select:<row>, name:<text>, project:<name>,
// set:<Option>=<value> (NoFloats=true, Entry=Start), place:<Option>=<place>
// (configuration, platform, project, sheet, program or default), print,
// which prints the options (PrintOptions), check, which prints whether OK
// would close it and why not, and cleanup, which clicks Clean Up.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	CChars		szOption;
	size		i;
	size		uiOption;
	char*		sz;
	char*		szEquals;
	int			iPlace;
	bool		bApproved;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		szEquals = strchr(sz, '=');
		uiOption = MAX_SIZE;
		if (szEquals && ((strncmp(sz, "set:", 4) == 0) || (strncmp(sz, "place:", 6) == 0)))
		{
			szOption.Init();
			szOption.AppendSubString(strchr(sz, ':') + 1, szEquals);
			uiOption = FindBuildOption(szOption.Text());
			szOption.Kill();
		}
		if (strcmp(sz, "add") == 0)
		{
			AddConfiguration();
		}
		else if (strcmp(sz, "remove") == 0)
		{
			RemoveConfiguration();
		}
		else if (strncmp(sz, "select:", 7) == 0)
		{
			mpList->Select(atoi(&sz[7]));
		}
		else if (strncmp(sz, "name:", 5) == 0)
		{
			mpName->SetText(&sz[5]);
		}
		else if (strncmp(sz, "project:", 8) == 0)
		{
			SelectProject(&sz[8]);
		}
		else if ((strncmp(sz, "set:", 4) == 0) && (uiOption != MAX_SIZE))
		{
			SetOptionValue(uiOption, szEquals + 1);
		}
		else if ((strncmp(sz, "place:", 6) == 0) && (uiOption != MAX_SIZE))
		{
			for (iPlace = 0; iPlace < BOP_NumPlaces; iPlace++)
			{
				if (strcmp(gaszIDEBuildPlaces[iPlace], szEquals + 1) == 0)
				{
					SetOptionPlace(uiOption, (EBuildOptionPlace)iPlace);
				}
			}
		}
		else if (strcmp(sz, "print") == 0)
		{
			PrintOptions();
		}
		else if (strcmp(sz, "check") == 0)
		{
			bApproved = Approve();
			printf("Build dialog: %s [%s]\n", bApproved ? "OK" : "refused", mpError->GetText());
			fflush(stdout);
		}
	}
	aszEdits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// True if OK closed it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::Show(void)
{
	return mpDialog->Show(IDE_BUILD_DIALOG_WIDTH, IDE_BUILD_DIALOG_HEIGHT);
}


Ptr<CDialog> CIDEBuildConfigurationsDialog::GetDialog(void) const { return mpDialog; }
size CIDEBuildConfigurationsDialog::NumEntries(void) const { return masEntries.NumElements(); }
CIDEBuildDialogEntry* CIDEBuildConfigurationsDialog::GetEntry(size uiIndex) { return masEntries.Get(uiIndex); }
