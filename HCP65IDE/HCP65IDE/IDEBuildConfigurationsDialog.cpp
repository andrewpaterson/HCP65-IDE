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
#include "BaseLib/PrimitiveTypes.h"
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDEBuildConfigurationsDialog.h"


#define IDE_BUILD_DIALOG_WIDTH		800
#define IDE_BUILD_DIALOG_HEIGHT		500
#define IDE_BUILD_DIALOG_LIST_WIDTH	220
#define IDE_BUILD_DIALOG_GAP		12
#define IDE_BUILD_DIALOG_NEW_NAME	"New|W65C816"


//////////////////////////////////////////////////////////////////////////
//
// An edit for each of pcConfigurations, szSelected selected.  The solution
// and the configurations aren't held.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, const char* szSolution, CVcxSolution* pcSolution, CBuildConfigurations* pcConfigurations, const char* szSelected)
{
	CChars	szTitle;
	size	i;

	PreInit();

	mpcConfigurations = pcConfigurations;
	mpcSolution = pcSolution;
	masEdits.Init();
	miSelected = -1;
	mbUpdating = false;
	for (i = 0; i < pcConfigurations->NumConfigurations(); i++)
	{
		masEdits.Add()->Init(pcConfigurations->GetConfiguration(i), i);
		if ((miSelected == -1) || (StringCompare(pcConfigurations->GetConfiguration(i)->GetName(), szSelected) == 0))
		{
			miSelected = (int32)i;
		}
	}

	szTitle.Init("Build Configurations: ");
	szTitle.Append(szSolution);
	mpDialog = OMalloc<CDialog>(pOwner, psLook, szTitle.Text());
	szTitle.Kill();
	mpDialog->SetResizable(IDE_BUILD_DIALOG_WIDTH / 2, IDE_BUILD_DIALOG_HEIGHT / 2);
	mpDialog->SetBody(CreateBody(), mpName);
	mpDialog->AddExtraButton(IDE_BUILD_DIALOG_OPEN_FILE);
	mpDialog->SetApprover(this, &CIDEBuildConfigurationsDialog::Approve);

	mpList->GetSelectionEvent()->Add(this, &CIDEBuildConfigurationsDialog::SelectionChanged);
	mpName->GetChangeEvent()->Add(this, &CIDEBuildConfigurationsDialog::NameChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDEBuildConfigurationsDialog::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDEBuildConfigurationsDialog::RemoveClicked);

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
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpName);
	M_Pointer(mpToolchain);
	M_Pointer(mpSolution);
	M_Pointer(mpBreakpointSpace);
	M_Pointer(mpNoFloats);
	M_Pointer(mpMathTables);
	M_Pointer(mpPreferSize);
	M_Pointer(mpCard);
	M_Pointer(mpCardPath);
	M_Pointer(mpContents);
	M_Pointer(mpError);
	U_Pointer(mpcConfigurations);
	U_Pointer(mpcSolution);
	U_Unknown(CArrayBuildConfigurationEdits, masEdits);
	U_Int32(miSelected);
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
	if (mpAdd.IsNotNull())
	{
		mpAdd->GetClickEvent()->Remove(this);
	}
	if (mpRemove.IsNotNull())
	{
		mpRemove->GetClickEvent()->Remove(this);
	}
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		masEdits.Get(i)->Kill();
	}
	masEdits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDEBuildConfigurationsDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The list and its buttons on the left, the form on the right.
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
	int						i;
	size					uiItem;

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
	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	pButtons->AddComponent(mpAdd, 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_BUILD_DIALOG_GAP / 2, 1);
	pButtons->AddComponent(pGap, 1, 0);
	pButtons->AddComponent(mpRemove, 2, 0);
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 3, 0, 1, 1, 1, 0);  //Keeps the buttons on the left.

	pLeft = OMalloc<CGridContainer>(pWindow);
	pLeft->SetAlignment(CSH_Stretch, CSV_Stretch);
	pLeft->AddComponent(mpScrollPane, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_BUILD_DIALOG_GAP / 2);
	pLeft->AddComponent(pGap, 0, 1);
	pLeft->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	mpName = CreateField();
	mpToolchain = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpToolchain->SetAlignment(CSH_Stretch, CSV_Center);
	for (i = 0; i < BT_NumToolchains; i++)
	{
		mpToolchain->AddItem(CBuildConfiguration::GetToolchainName((EBuildToolchain)i));
	}
	mpSolution = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpSolution->SetAlignment(CSH_Stretch, CSV_Center);
	for (uiItem = 0; uiItem < mpcSolution->NumConfigurations(); uiItem++)
	{
		mpSolution->AddItem(mpcSolution->GetConfiguration(uiItem));
	}
	mpBreakpointSpace = CreateField();
	mpNoFloats = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "No floating point");
	mpNoFloats->SetAlignment(CSH_Left, CSV_Center);
	mpMathTables = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Maths tables in memory");
	mpMathTables->SetAlignment(CSH_Left, CSV_Center);
	mpPreferSize = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Prefer size to speed");
	mpPreferSize->SetAlignment(CSH_Left, CSV_Center);
	mpCard = CreateField();
	mpCardPath = CreateField();
	mpContents = OMalloc<CLabel>(pWindow, psLook->pcDimFont, "");
	mpContents->SetAlignment(CSH_Left, CSV_Center);
	mpError = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpError->SetAlignment(CSH_Left, CSV_Center);

	pForm = OMalloc<CGridContainer>(pWindow);
	pForm->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pForm, &iRow, "Name", mpName);
	AddFormRow(pForm, &iRow, "Toolchain", mpToolchain);
	AddFormRow(pForm, &iRow, "Solution configuration", mpSolution);
	AddFormRow(pForm, &iRow, "Breakpoint space", mpBreakpointSpace);
	AddFormRow(pForm, &iRow, "", mpNoFloats);
	AddFormRow(pForm, &iRow, "", mpMathTables);
	AddFormRow(pForm, &iRow, "", mpPreferSize);
	AddFormRow(pForm, &iRow, "Card", mpCard);
	AddFormRow(pForm, &iRow, "Path on card", mpCardPath);
	AddFormRow(pForm, &iRow, NULL, mpContents);
	AddFormRow(pForm, &iRow, NULL, mpError);

	//What is left over goes under the rows, which keeps them at the top.
	pGap = OMalloc<CFillContainer>(pWindow);
	pForm->AddComponent(pGap, 0, iRow, 2, 1, 1, 1);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->AddComponent(pLeft, 0, 0, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_BUILD_DIALOG_GAP, 1);
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
void CIDEBuildConfigurationsDialog::AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor)
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
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), masEdits.Get(i)->mszName.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpList->AddItem(pLabel);
	}
	mpList->Select(miSelected);
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The selected configuration in the form, which is empty if there are
// none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ShowSelected(void)
{
	CBuildConfigurationEdit*	pcEdit;

	mbUpdating = true;
	pcEdit = (miSelected >= 0) ? masEdits.Get(miSelected) : NULL;
	mpName->SetText(pcEdit ? pcEdit->mszName.Text() : "");
	mpBreakpointSpace->SetText(pcEdit ? pcEdit->mszBreakpointSpace.Text() : "");
	mpCard->SetText(pcEdit ? pcEdit->mszCard.Text() : "");
	mpCardPath->SetText(pcEdit ? pcEdit->mszCardPath.Text() : "");
	mpNoFloats->SetTicked(pcEdit ? pcEdit->mbNoFloats : false);
	mpMathTables->SetTicked(pcEdit ? pcEdit->mbMathTables : true);
	mpPreferSize->SetTicked(pcEdit ? pcEdit->mbPreferSize : false);
	if (pcEdit)
	{
		SelectItem(mpToolchain, CBuildConfiguration::GetToolchainName(pcEdit->meToolchain));
		SelectItem(mpSolution, pcEdit->mszSolution.Text());
	}
	ShowContents(pcEdit);
	mpError->SetText("");
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// What it has inside it, which this dialog doesn't edit.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ShowContents(CBuildConfigurationEdit* pcEdit)
{
	CBuildConfiguration*	pcSource;
	CChars					szText;

	szText.Init();
	pcSource = ((pcEdit != NULL) && (pcEdit->muiSource != MAX_SIZE)) ? mpcConfigurations->GetConfiguration(pcEdit->muiSource) : NULL;
	if ((pcSource != NULL) && (pcSource->GetProgram() != BP_None))
	{
		szText.Append("Links ");
		szText.Append(pcSource->GetProgramFile());
	}
	else if (pcEdit != NULL)
	{
		szText.Append("Links nothing");
	}
	if (pcSource != NULL)
	{
		szText.Append(", loads ");
		szText.Append((uint32)pcSource->NumLoads());
	}
	if (pcEdit != NULL)
	{
		szText.Append(" (in the file)");
	}
	mpContents->SetText(szText.Text());
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szText selected in pBox; it is added if it isn't one of its items, so
// that it isn't lost by looking at it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::SelectItem(Ptr<CComboBox> pBox, const char* szText)
{
	size	i;

	for (i = 0; i < pBox->NumItems(); i++)
	{
		if (StringCompare(pBox->GetItemText(i), szText) == 0)
		{
			pBox->Select((int32)i);
			return;
		}
	}
	if (szText[0] != '\0')
	{
		pBox->Select((int32)pBox->AddItem(szText));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the form shows is kept in the selected configuration's edit.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::Keep(void)
{
	CBuildConfigurationEdit*	pcEdit;

	if (miSelected < 0)
	{
		return;
	}
	pcEdit = masEdits.Get(miSelected);
	pcEdit->mszName.Set(mpName->GetText());
	if (mpToolchain->GetSelectedText() != NULL)
	{
		CBuildConfiguration::FindToolchain(mpToolchain->GetSelectedText(), &pcEdit->meToolchain);
	}
	if (mpSolution->GetSelectedText() != NULL)
	{
		pcEdit->mszSolution.Set(mpSolution->GetSelectedText());
	}
	pcEdit->mszBreakpointSpace.Set(mpBreakpointSpace->GetText());
	pcEdit->mbNoFloats = mpNoFloats->IsTicked();
	pcEdit->mbMathTables = mpMathTables->IsTicked();
	pcEdit->mbPreferSize = mpPreferSize->IsTicked();
	pcEdit->mszCard.Set(mpCard->GetText());
	pcEdit->mszCardPath.Set(mpCardPath->GetText());
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
	Keep();
	miSelected = mpList->GetSelected();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The name typed shows in the list as it is typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::NameChanged(void)
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
void CIDEBuildConfigurationsDialog::AddClicked(CButton* pcButton)
{
	AddConfiguration();
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpName);
}


void CIDEBuildConfigurationsDialog::RemoveClicked(CButton* pcButton) { RemoveConfiguration(); }


//////////////////////////////////////////////////////////////////////////
//
// A copy of the one selected, called so it can be told apart; with none to
// copy a W65C816 one of the solution's first configuration.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::AddConfiguration(void)
{
	CChars						szBase;
	CChars						szName;
	CBuildConfigurationEdit*	pcNew;
	int							iNumber;
	size						i;
	bool						bUsed;

	Keep();
	szBase.Init((miSelected >= 0) ? masEdits.Get(miSelected)->mszName.Text() : IDE_BUILD_DIALOG_NEW_NAME);
	if (miSelected >= 0)
	{
		szBase.Append(" Copy");
	}
	szName.Init(szBase);
	iNumber = 1;
	do
	{
		bUsed = false;
		for (i = 0; i < masEdits.NumElements(); i++)
		{
			bUsed |= masEdits.Get(i)->mszName.Equals(szName.Text());
		}
		if (bUsed)
		{
			iNumber++;
			szName.Set(szBase);
			szName.Append(' ');
			szName.Append(iNumber);
		}
	} while (bUsed);

	pcNew = masEdits.Add();
	if (miSelected >= 0)
	{
		pcNew->Init(masEdits.Get(miSelected), szName.Text());
	}
	else
	{
		pcNew->Init(szName.Text(), BT_W65C816, (mpcSolution->NumConfigurations() > 0) ? mpcSolution->GetConfiguration(0) : "");
	}
	miSelected = (int32)masEdits.NumElements() - 1;
	szName.Kill();
	szBase.Kill();

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
	masEdits.Get(miSelected)->Kill();
	masEdits.RemoveAt(miSelected);
	if (miSelected >= (int32)masEdits.NumElements())
	{
		miSelected = (int32)masEdits.NumElements() - 1;
	}
	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// Why the file couldn't have pcEdit, the one at uiEdit, NULL if it could.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEBuildConfigurationsDialog::FindProblem(CBuildConfigurationEdit* pcEdit, size uiEdit)
{
	size	i;
	bool	bHCP65;

	if (pcEdit->mszName.Empty())
	{
		return "A build configuration needs a name.";
	}
	for (i = 0; i < uiEdit; i++)
	{
		if (masEdits.Get(i)->mszName.Equals(pcEdit->mszName.Text()))
		{
			return "Another build configuration has this name.";
		}
	}
	if (pcEdit->mszSolution.Empty())
	{
		return "A build configuration needs a solution configuration.";
	}
	if (pcEdit->mszCard.Empty() != pcEdit->mszCardPath.Empty())
	{
		return "A card and a path on it go together.";
	}
	bHCP65 = (pcEdit->meToolchain == BT_W65C816) || (pcEdit->meToolchain == BT_65C64);
	if (!bHCP65 && (pcEdit->mbNeedsHCP65 || !pcEdit->mszCard.Empty()))
	{
		return "Only an HCP65 toolchain can link a program, load files or use a card.";
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// OK closes the dialog only if the file could have them all; otherwise the
// first that couldn't is selected and the form says why.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildConfigurationsDialog::Approve(void)
{
	size		i;
	const char*	szProblem;

	Keep();
	mpError->SetText("");
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		szProblem = FindProblem(masEdits.Get(i), i);
		if (szProblem != NULL)
		{
			miSelected = (int32)i;
			ShowList();
			ShowSelected();
			mpError->SetText(szProblem);
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do in the dialog, separated by
// semicolons: add, remove, select:<row>, name:<text>, toolchain:<text>,
// solution:<text>, breakpoint:<text>, nofloats:<0 or 1>, mathtables:<0 or
// 1>, card:<text>, cardpath:<text>, and check, which prints whether OK
// would close it and why not.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildConfigurationsDialog::ApplyEdits(const char* szEdits)
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
		else if (strncmp(sz, "toolchain:", 10) == 0)
		{
			SelectItem(mpToolchain, &sz[10]);
		}
		else if (strncmp(sz, "solution:", 9) == 0)
		{
			SelectItem(mpSolution, &sz[9]);
		}
		else if (strncmp(sz, "breakpoint:", 11) == 0)
		{
			mpBreakpointSpace->SetText(&sz[11]);
		}
		else if (strncmp(sz, "nofloats:", 9) == 0)
		{
			mpNoFloats->SetTicked(sz[9] == '1');
		}
		else if (strncmp(sz, "mathtables:", 11) == 0)
		{
			mpMathTables->SetTicked(sz[11] == '1');
		}
		else if (strncmp(sz, "prefersize:", 11) == 0)
		{
			mpPreferSize->SetTicked(sz[11] == '1');
		}
		else if (strncmp(sz, "card:", 5) == 0)
		{
			mpCard->SetText(&sz[5]);
		}
		else if (strncmp(sz, "cardpath:", 9) == 0)
		{
			mpCardPath->SetText(&sz[9]);
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
	bool	bResult;

	bResult = mpDialog->Show(IDE_BUILD_DIALOG_WIDTH, IDE_BUILD_DIALOG_HEIGHT);
	if (bResult)
	{
		Keep();
	}
	return bResult;
}


Ptr<CDialog> CIDEBuildConfigurationsDialog::GetDialog(void) { return mpDialog; }
CArrayBuildConfigurationEdits* CIDEBuildConfigurationsDialog::GetEdits(void) { return &masEdits; }

