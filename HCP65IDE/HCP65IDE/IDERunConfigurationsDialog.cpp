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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDERunConfigurationsDialog.h"


#define IDE_RUN_DIALOG_WIDTH		760
#define IDE_RUN_DIALOG_HEIGHT		400
#define IDE_RUN_DIALOG_LIST_WIDTH	240
#define IDE_RUN_DIALOG_GAP			12
#define IDE_RUN_DIALOG_NEW_NAME		"New Configuration"


//////////////////////////////////////////////////////////////////////////
//
// Copies of pcWorkspace's run configurations, szSelected selected.  The
// workspace isn't held.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CBuildWorkspace* pcWorkspace, const char* szSelected)
{
	Ptr<CComponent>	pBody;
	size	i;

	PreInit();

	masEdited.Init();
	maszBuildConfigurations.Init();
	miSelected = -1;
	mbUpdating = false;
	pcWorkspace->GetConfigurationNames(&maszBuildConfigurations);
	for (i = 0; i < pcWorkspace->NumRunConfigurations(); i++)
	{
		masEdited.Add()->Init(pcWorkspace->GetRunConfiguration(i));
		if ((miSelected == -1) || (StringCompare(pcWorkspace->GetRunConfiguration(i)->GetName(), szSelected) == 0))
		{
			miSelected = (int32)i;
		}
	}

	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Run Configurations");
	mpDialog->SetResizable(IDE_RUN_DIALOG_WIDTH / 2, IDE_RUN_DIALOG_HEIGHT / 2);
	//The body is made first: it makes the component that has the keys when the dialog opens.
	pBody = CreateBody();
	mpDialog->SetBody(pBody, mpName);
	mpDialog->SetApprover(this, &CIDERunConfigurationsDialog::Approve);

	mpList->GetSelectionEvent()->Add(this, &CIDERunConfigurationsDialog::SelectionChanged);
	mpName->GetChangeEvent()->Add(this, &CIDERunConfigurationsDialog::NameChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDERunConfigurationsDialog::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDERunConfigurationsDialog::RemoveClicked);

	ShowList();
	ShowSelected();

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpList);
	M_Pointer(mpScrollPane);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpName);
	M_Pointer(mpBuildConfiguration);
	M_Pointer(mpTool);
	M_Pointer(mpDirectory);
	M_Pointer(mpArguments);
	M_Pointer(mpError);
	U_Unknown(CArrayRunConfigurations, masEdited);
	U_Unknown(CArrayChars, maszBuildConfigurations);
	U_Int32(miSelected);
	U_Bool(mbUpdating);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so the
// dialog takes itself off the widgets still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Free(void)
{
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
	if (mpAdd.IsNotNull())
	{
		mpAdd->GetClickEvent()->Remove(this);
	}
	if (mpRemove.IsNotNull())
	{
		mpRemove->GetClickEvent()->Remove(this);
	}
	KillEdited();
	masEdited.Kill();
	maszBuildConfigurations.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunConfigurationsDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDERunConfigurationsDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::KillEdited(void)
{
	size	i;

	for (i = 0; i < masEdited.NumElements(); i++)
	{
		masEdited.Get(i)->Kill();
	}
	masEdited.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The list and its buttons on the left, the form on the right.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDERunConfigurationsDialog::CreateBody(void)
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
	size					i;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	sInsets.Init(0, 0);
	mpList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpList->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpScrollPane->SetContent(mpList);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpScrollPane->SetDesiredSize(IDE_RUN_DIALOG_LIST_WIDTH, -1);

	mpAdd = OMalloc<CButton>(pWindow, psLook->pcTheme, "Add");
	mpAdd->SetAlignment(CSH_Left, CSV_Center);
	mpRemove = OMalloc<CButton>(pWindow, psLook->pcTheme, "Remove");
	mpRemove->SetAlignment(CSH_Left, CSV_Center);
	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	pButtons->AddComponent(mpAdd, 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_RUN_DIALOG_GAP / 2, 1);
	pButtons->AddComponent(pGap, 1, 0);
	pButtons->AddComponent(mpRemove, 2, 0);
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 3, 0, 1, 1, 1, 0);  //Keeps the buttons on the left.

	pLeft = OMalloc<CGridContainer>(pWindow);
	pLeft->SetAlignment(CSH_Stretch, CSV_Stretch);
	pLeft->AddComponent(mpScrollPane, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_RUN_DIALOG_GAP / 2);
	pLeft->AddComponent(pGap, 0, 1);
	pLeft->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	mpName = CreateField();
	mpBuildConfiguration = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpBuildConfiguration->SetAlignment(CSH_Stretch, CSV_Center);
	for (i = 0; i < maszBuildConfigurations.NumElements(); i++)
	{
		mpBuildConfiguration->AddItem(maszBuildConfigurations.GetText(i));
	}
	mpTool = CreateField();
	mpDirectory = CreateField();
	mpArguments = CreateField();
	mpError = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpError->SetAlignment(CSH_Left, CSV_Center);

	pForm = OMalloc<CGridContainer>(pWindow);
	pForm->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pForm, &iRow, "Name", mpName);
	AddFormRow(pForm, &iRow, "Build configuration", mpBuildConfiguration);
	AddFormRow(pForm, &iRow, "Tool", mpTool);
	AddFormRow(pForm, &iRow, "Working directory", mpDirectory);
	AddFormRow(pForm, &iRow, "Arguments", mpArguments);
	AddFormRow(pForm, &iRow, NULL, mpError);

	//What is left over goes under the rows, which keeps them at the top.
	pGap = OMalloc<CFillContainer>(pWindow);
	pForm->AddComponent(pGap, 0, iRow, 2, 1, 1, 1);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->AddComponent(pLeft, 0, 0, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_RUN_DIALOG_GAP, 1);
	pBody->AddComponent(pGap, 1, 0);
	pBody->AddComponent(pForm, 2, 0, 1, 1, 1, 1);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
// A label and its editor, as CDialog's own rows are.  Without a label the
// editor is across both columns.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor)
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
		pForm->AddComponent(pEditor, 0, *piRow, 2, 1, 1, 0);
	}
	(*piRow)++;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextField> CIDERunConfigurationsDialog::CreateField(void)
{
	Ptr<CTextField>	pField;

	pField = OMalloc<CTextField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, "");
	pField->SetColumns(10);
	pField->SetAlignment(CSH_Stretch, CSV_Center);
	return pField;
}


//////////////////////////////////////////////////////////////////////////
//
// A row for each run configuration, the selected one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ShowList(void)
{
	Ptr<CLabel>	pLabel;
	size		i;

	mbUpdating = true;
	mpList->RemoveAllComponents();
	for (i = 0; i < masEdited.NumElements(); i++)
	{
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), masEdited.Get(i)->GetName());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpList->AddItem(pLabel);
	}
	mpList->Select(miSelected);
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The selected run configuration in the form, which is empty if there are
// none.  A build configuration that no solution has any more is added, so
// that it isn't lost by looking at it.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ShowSelected(void)
{
	CRunConfiguration*	pcRun;

	mbUpdating = true;
	pcRun = (miSelected >= 0) ? masEdited.Get(miSelected) : NULL;
	mpName->SetText(pcRun ? pcRun->GetName() : "");
	mpTool->SetText(pcRun ? pcRun->GetTool() : "");
	mpDirectory->SetText(pcRun ? pcRun->GetDirectory() : "");
	mpArguments->SetText(pcRun ? pcRun->GetArguments() : "");
	if (pcRun)
	{
		SelectBuildConfiguration(pcRun->GetConfiguration());
	}
	mpError->SetText("");
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::SelectBuildConfiguration(const char* szConfiguration)
{
	size	i;

	for (i = 0; i < mpBuildConfiguration->NumItems(); i++)
	{
		if (StringCompare(mpBuildConfiguration->GetItemText(i), szConfiguration) == 0)
		{
			mpBuildConfiguration->Select((int32)i);
			return;
		}
	}
	if (szConfiguration[0] != '\0')
	{
		mpBuildConfiguration->Select((int32)mpBuildConfiguration->AddItem(szConfiguration));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the form shows is kept in the selected run configuration.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Keep(void)
{
	CRunConfiguration*	pcRun;

	if (miSelected < 0)
	{
		return;
	}
	pcRun = masEdited.Get(miSelected);
	pcRun->SetName(mpName->GetText());
	if (mpBuildConfiguration->GetSelectedText() != NULL)
	{
		pcRun->SetConfiguration(mpBuildConfiguration->GetSelectedText());
	}
	pcRun->SetTool(mpTool->GetText(), mpDirectory->GetText(), mpArguments->GetText());
}


//////////////////////////////////////////////////////////////////////////
//
// Another row selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::SelectionChanged(void)
{
	if (mbUpdating || (mpList->GetSelected() < 0))
	{
		return;
	}
	Keep();
	miSelected = mpList->GetSelected();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The name typed shows in the list as it is typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::NameChanged(void)
{
	if (mbUpdating || (miSelected < 0))
	{
		return;
	}
	Keep();
	ShowList();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddClicked(CButton* pcButton)
{
	AddConfiguration();
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpName);
}


void CIDERunConfigurationsDialog::RemoveClicked(CButton* pcButton) { RemoveConfiguration(); }


//////////////////////////////////////////////////////////////////////////
//
// A new one, named so it can be told apart, for the build configuration of
// the one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddConfiguration(void)
{
	CChars	szName;
	CChars	szConfiguration;
	int		iNumber;
	size	i;
	bool	bUsed;

	Keep();
	szName.Init(IDE_RUN_DIALOG_NEW_NAME);
	iNumber = 1;
	do
	{
		bUsed = false;
		for (i = 0; i < masEdited.NumElements(); i++)
		{
			bUsed |= StringCompare(masEdited.Get(i)->GetName(), szName.Text()) == 0;
		}
		if (bUsed)
		{
			iNumber++;
			szName.Set(IDE_RUN_DIALOG_NEW_NAME " ");
			szName.Append(iNumber);
		}
	} while (bUsed);

	szConfiguration.Init();
	if (miSelected >= 0)
	{
		szConfiguration.Set(masEdited.Get(miSelected)->GetConfiguration());
	}
	else if (maszBuildConfigurations.NumElements() > 0)
	{
		szConfiguration.Set(maszBuildConfigurations.GetText(0));
	}
	masEdited.Add()->Init(szName.Text(), szConfiguration.Text());
	miSelected = (int32)masEdited.NumElements() - 1;
	szConfiguration.Kill();
	szName.Kill();

	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The selected one goes, and the one after it (or now last) is selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::RemoveConfiguration(void)
{
	if (miSelected < 0)
	{
		return;
	}
	masEdited.Get(miSelected)->Kill();
	masEdited.RemoveAt(miSelected);
	if (miSelected >= (int32)masEdited.NumElements())
	{
		miSelected = (int32)masEdited.NumElements() - 1;
	}
	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// OK closes the dialog only if every run configuration has a name of its
// own; otherwise the first that hasn't is selected and the form says why.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunConfigurationsDialog::Approve(void)
{
	size		i;
	size		j;
	const char*	szProblem;

	Keep();
	mpError->SetText("");
	szProblem = NULL;
	for (i = 0; (i < masEdited.NumElements()) && (szProblem == NULL); i++)
	{
		if (masEdited.Get(i)->GetName()[0] == '\0')
		{
			szProblem = "A run configuration needs a name.";
		}
		for (j = 0; (j < i) && (szProblem == NULL); j++)
		{
			if (StringCompare(masEdited.Get(i)->GetName(), masEdited.Get(j)->GetName()) == 0)
			{
				szProblem = "Another run configuration has this name.";
			}
		}
		if (szProblem != NULL)
		{
			miSelected = (int32)i;
			ShowList();
			ShowSelected();
			mpError->SetText(szProblem);
		}
	}
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do in the dialog, separated by
// semicolons: add, remove, select:<row>, name:<text>, build:<text>,
// tool:<text>, directory:<text>, arguments:<text>, and check, which prints
// whether OK would close it and why not.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	size		i;
	char*		sz;
	bool		bApproved;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
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
		else if (strncmp(sz, "build:", 6) == 0)
		{
			SelectBuildConfiguration(&sz[6]);
		}
		else if (strncmp(sz, "tool:", 5) == 0)
		{
			mpTool->SetText(&sz[5]);
		}
		else if (strncmp(sz, "directory:", 10) == 0)
		{
			mpDirectory->SetText(&sz[10]);
		}
		else if (strncmp(sz, "arguments:", 10) == 0)
		{
			mpArguments->SetText(&sz[10]);
		}
		else if (strcmp(sz, "check") == 0)
		{
			bApproved = Approve();
			printf("Run dialog: %s [%s]\n", bApproved ? "OK" : "refused", mpError->GetText());
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
bool CIDERunConfigurationsDialog::Show(void)
{
	bool	bResult;

	bResult = mpDialog->Show(IDE_RUN_DIALOG_WIDTH, IDE_RUN_DIALOG_HEIGHT);
	if (bResult)
	{
		Keep();
	}
	return bResult;
}


Ptr<CDialog> CIDERunConfigurationsDialog::GetDialog(void) { return mpDialog; }
CArrayRunConfigurations* CIDERunConfigurationsDialog::GetEdited(void) { return &masEdited; }
char* CIDERunConfigurationsDialog::GetSelectedName(void) { return (miSelected >= 0) ? masEdited.Get(miSelected)->GetName() : (char*)""; }

