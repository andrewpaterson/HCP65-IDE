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
#include "IDEChangeSignatureDialog.h"


#define IDE_SIGNATURE_DIALOG_WIDTH		820
#define IDE_SIGNATURE_DIALOG_HEIGHT		420
#define IDE_SIGNATURE_DIALOG_GAP		12

//The parameters' table's columns.
#define IDE_SIGNATURE_COLUMN_TYPE		0
#define IDE_SIGNATURE_COLUMN_NAME		1
#define IDE_SIGNATURE_COLUMN_DEFAULT	2
#define IDE_SIGNATURE_COLUMN_VALUE		3


//////////////////////////////////////////////////////////////////////////
//
// A row for each of the function's parameters, as its first declaration
// writes them.  The owner and the signature aren't held.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CIDEChangeSignature* pcOwner, SIDESignature* psSignature)
{
	Ptr<CComponent>		pBody;
	SIDENewParameter*	psRow;
	size				i;

	PreInit();
	mpcOwner = pcOwner;
	mpsSignature = psSignature;
	mbUpdating = false;
	masRows.Init();
	for (i = 0; i < psSignature->aszOldTypes.NumElements(); i++)
	{
		psRow = masRows.Add();
		psRow->Init((int32)i);
		psRow->szType.Set(psSignature->aszOldTypes.GetText(i));
		psRow->szName.Set(psSignature->aszOldNames.GetText(i));
		psRow->szDefault.Set(psSignature->aszOldDefaults.GetText(i));
	}
	miSelected = (masRows.NumElements() > 0) ? 0 : -1;

	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Change Signature");
	mpDialog->SetResizable(IDE_SIGNATURE_DIALOG_WIDTH / 2, IDE_SIGNATURE_DIALOG_HEIGHT / 2);
	//The body is made first: it makes the component that has the keys when the dialog opens.
	pBody = CreateBody();
	mpDialog->SetBody(pBody, mpFunction);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEChangeSignatureDialog::Approve);

	mpTable->GetSelectionEvent()->Add(this, &CIDEChangeSignatureDialog::SelectionChanged);
	mpTable->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::CellChanged);
	mpFunction->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpReturn->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::RemoveClicked);
	mpUp->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::UpClicked);
	mpDown->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::DownClicked);

	mbUpdating = true;
	mpFunction->SetText(psSignature->szOldName.Text());
	mpReturn->SetText(psSignature->szOldReturn.Text());
	mbUpdating = false;
	ShowTable();
	ShowPreview();

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpFunction);
	M_Pointer(mpReturn);
	M_Pointer(mpTable);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpUp);
	M_Pointer(mpDown);
	M_Pointer(mpPreview);
	M_Pointer(mpError);
	U_Pointer(mpcOwner);
	U_Pointer(mpsSignature);
	U_Unknown(CArrayIDENewParameters, masRows);
	U_Int32(miSelected);
	U_Bool(mbUpdating);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::Free(void)
{
	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearApprover();
	}
	if (mpTable.IsNotNull())
	{
		mpTable->GetSelectionEvent()->Remove(this);
		mpTable->GetChangeEvent()->Remove(this);
	}
	StopListening(mpFunction);
	StopListening(mpReturn);
	StopListening(mpAdd);
	StopListening(mpRemove);
	StopListening(mpUp);
	StopListening(mpDown);
	KillIDENewParameters(&masRows);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::StopListening(Ptr<CTextField> pField)
{
	if (pField.IsNotNull())
	{
		pField->GetChangeEvent()->Remove(this);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::StopListening(Ptr<CButton> pButton)
{
	if (pButton.IsNotNull())
	{
		pButton->GetClickEvent()->Remove(this);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignatureDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDEChangeSignatureDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextField> CIDEChangeSignatureDialog::CreateField(void)
{
	Ptr<CTextField>	pField;

	pField = OMalloc<CTextField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, "");
	pField->SetColumns(10);
	pField->SetAlignment(CSH_Stretch, CSV_Center);
	return pField;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDEChangeSignatureDialog::CreateButton(const char* szCaption)
{
	Ptr<CButton>	pButton;

	pButton = OMalloc<CButton>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, szCaption);
	pButton->SetAlignment(CSH_Left, CSV_Center);
	return pButton;
}


//////////////////////////////////////////////////////////////////////////
//
// The space between two of the list's buttons.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEChangeSignatureDialog::CreateButtonGap(void)
{
	Ptr<CFillContainer>	pGap;

	pGap = OMalloc<CFillContainer>(mpDialog->GetWindow());
	pGap->SetDesiredSize(IDE_SIGNATURE_DIALOG_GAP / 2, 1);
	return pGap;
}


//////////////////////////////////////////////////////////////////////////
//
// A label and its editor, as CDialog's own rows are.  Without a label the
// editor is across both columns.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor)
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
// The function's name and what it returns across the top; the
// parameters' table with its buttons under it; the signature as it would
// be, and what is wrong with it, across the bottom.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEChangeSignatureDialog::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CGridContainer>		pTop;
	Ptr<CGridContainer>		pMiddle;
	Ptr<CGridContainer>		pButtons;
	Ptr<CGridContainer>		pBottom;
	Ptr<CFillContainer>		pGap;
	int32					iRow;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	mpFunction = CreateField();
	mpReturn = CreateField();
	pTop = OMalloc<CGridContainer>(pWindow);
	pTop->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pTop, &iRow, "Name", mpFunction);
	AddFormRow(pTop, &iRow, "Return type", mpReturn);

	//The parameters: a row each, edited where they are.
	mpTable = OMalloc<CTable>(pWindow, psLook->pcTheme);
	mpTable->AddColumn("Type", TCK_Text, 3);
	mpTable->AddColumn("Name", TCK_Text, 2);
	mpTable->AddColumn("Default value", TCK_Text, 2);
	mpTable->AddColumn("Value in calls", TCK_Text, 2);
	mpTable->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpAdd = CreateButton("Add");
	mpRemove = CreateButton("Remove");
	mpUp = CreateButton("Up");
	mpDown = CreateButton("Down");
	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	pButtons->AddComponent(mpAdd, 0, 0);
	pButtons->AddComponent(CreateButtonGap(), 1, 0);
	pButtons->AddComponent(mpRemove, 2, 0);
	pButtons->AddComponent(CreateButtonGap(), 3, 0);
	pButtons->AddComponent(mpUp, 4, 0);
	pButtons->AddComponent(CreateButtonGap(), 5, 0);
	pButtons->AddComponent(mpDown, 6, 0);
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 7, 0, 1, 1, 1, 0);  //Keeps the buttons on the left.

	pMiddle = OMalloc<CGridContainer>(pWindow);
	pMiddle->SetAlignment(CSH_Stretch, CSV_Stretch);
	pMiddle->AddComponent(mpTable, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_SIGNATURE_DIALOG_GAP / 2);
	pMiddle->AddComponent(pGap, 0, 1);
	pMiddle->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	mpPreview = OMalloc<CLabel>(pWindow, psLook->pcDimFont, "");
	mpPreview->SetAlignment(CSH_Left, CSV_Center);
	mpError = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpError->SetAlignment(CSH_Left, CSV_Center);
	pBottom = OMalloc<CGridContainer>(pWindow);
	pBottom->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pBottom, &iRow, NULL, mpPreview);
	AddFormRow(pBottom, &iRow, NULL, mpError);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->AddComponent(pTop, 0, 0, 1, 1, 1, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_SIGNATURE_DIALOG_GAP);
	pBody->AddComponent(pGap, 0, 1);
	pBody->AddComponent(pMiddle, 0, 2, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_SIGNATURE_DIALOG_GAP);
	pBody->AddComponent(pGap, 0, 3);
	pBody->AddComponent(pBottom, 0, 4, 1, 1, 1, 0);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
// A parameter as its row says it: "const char* szName = 0".
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::AppendRow(SIDENewParameter* psRow, CChars* pszDest)
{
	pszDest->Append(psRow->szType);
	if (!psRow->szName.Empty())
	{
		pszDest->Append(' ')->Append(psRow->szName);
	}
	if (!psRow->szDefault.Empty())
	{
		pszDest->Append(" = ")->Append(psRow->szDefault);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A row of the table for each parameter, the selected one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::ShowTable(void)
{
	size	i;

	mbUpdating = true;
	mpTable->RemoveAllRows();
	for (i = 0; i < masRows.NumElements(); i++)
	{
		mpTable->AddRow();
		ShowRow((int32)i);
	}
	if (miSelected >= 0)
	{
		mpTable->SelectCell(miSelected, (mpTable->GetSelectedColumn() >= 0) ? mpTable->GetSelectedColumn() : 0);
	}
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A parameter's row of the table as its row here says it.  Only a new
// parameter has a value in the calls there already are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::ShowRow(int32 iRow)
{
	SIDENewParameter*	psRow;

	psRow = masRows.Get((size)iRow);
	mpTable->SetCellText(iRow, IDE_SIGNATURE_COLUMN_TYPE, psRow->szType.Text());
	mpTable->SetCellText(iRow, IDE_SIGNATURE_COLUMN_NAME, psRow->szName.Text());
	mpTable->SetCellText(iRow, IDE_SIGNATURE_COLUMN_DEFAULT, psRow->szDefault.Text());
	mpTable->SetCellText(iRow, IDE_SIGNATURE_COLUMN_VALUE, psRow->szCallValue.Text());
	mpTable->SetCellEditable(iRow, IDE_SIGNATURE_COLUMN_VALUE, psRow->iOld < 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The signature as it would be declared, and what is wrong with it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::ShowPreview(void)
{
	SIDESignatureChange	sChange;
	CChars				szText;

	sChange.Init();
	GetChange(&sChange);
	szText.Init();
	mpcOwner->AppendNewSignature(mpsSignature, &sChange, &szText);
	mpPreview->SetText(szText.Text());
	szText.Kill();
	szText.Init();
	mpcOwner->FindProblem(mpsSignature, &sChange, &szText);
	mpError->SetText(szText.Text());
	szText.Kill();
	sChange.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A cell of the table changed by the user: it is its parameter's type,
// name, default value or value in calls, and the signature shown is as
// it is now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::CellChanged(CTable* pcTable)
{
	SIDENewParameter*	psRow;
	CChars*				pszField;
	int32				iRow;

	iRow = pcTable->GetChangedRow();
	if (mbUpdating || (iRow < 0) || (iRow >= (int32)masRows.NumElements()))
	{
		return;
	}
	psRow = masRows.Get((size)iRow);
	switch (pcTable->GetChangedColumn())
	{
	case IDE_SIGNATURE_COLUMN_TYPE:
		pszField = &psRow->szType;
		break;
	case IDE_SIGNATURE_COLUMN_NAME:
		pszField = &psRow->szName;
		break;
	case IDE_SIGNATURE_COLUMN_DEFAULT:
		pszField = &psRow->szDefault;
		break;
	default:
		pszField = &psRow->szCallValue;
		break;
	}
	pszField->Set(pcTable->GetCellText(iRow, pcTable->GetChangedColumn()));
	pszField->StripWhitespace();
	ShowPreview();
}


//////////////////////////////////////////////////////////////////////////
//
// What the dialog asks for, with only what differs from the function as
// it is: an old parameter's name, type and default value are given only
// where they were changed, and the function's name and what it returns
// likewise.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::GetChange(SIDESignatureChange* psDest)
{
	SIDENewParameter*	psRow;
	SIDENewParameter*	psNew;
	CChars				szText;
	size				i;

	for (i = 0; i < masRows.NumElements(); i++)
	{
		psRow = masRows.Get(i);
		psNew = psDest->asParameters.Add();
		psNew->Init(psRow->iOld);
		if (psRow->iOld < 0)
		{
			psNew->szType.Set(psRow->szType);
			psNew->szName.Set(psRow->szName);
			psNew->szDefault.Set(psRow->szDefault);
			psNew->szCallValue.Set(psRow->szCallValue);
			continue;
		}
		if (!psRow->szType.Equals(mpsSignature->aszOldTypes.GetText((size)psRow->iOld)))
		{
			psNew->szType.Set(psRow->szType);
		}
		if (!psRow->szName.Equals(mpsSignature->aszOldNames.GetText((size)psRow->iOld)))
		{
			psNew->szName.Set(psRow->szName);
		}
		if (!psRow->szDefault.Equals(mpsSignature->aszOldDefaults.GetText((size)psRow->iOld)))
		{
			psNew->szDefault.Set(psRow->szDefault);
			psNew->bDefaultChanged = true;
		}
	}
	szText.Init(mpFunction->GetText());
	szText.StripWhitespace();
	if (!szText.Equals(mpsSignature->szOldName))
	{
		psDest->szName.Set(szText);
	}
	szText.Set(mpReturn->GetText());
	szText.StripWhitespace();
	if (!szText.Equals(mpsSignature->szOldReturn))
	{
		psDest->szReturn.Set(szText);
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// OK closes it only if what is written can be the signature: the name
// and, for a function that returns something, the return type written,
// and nothing FindProblem refuses.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignatureDialog::Approve(void)
{
	SIDESignatureChange	sChange;
	CChars				szProblem;
	CChars				szText;
	bool				bProblem;

	//What is being typed in a cell is the cell's.
	mpTable->EndEdit(true);
	szProblem.Init();
	szText.Init(mpFunction->GetText());
	szText.StripWhitespace();
	bProblem = szText.Empty();
	if (bProblem)
	{
		szProblem.Set("The function needs a name.");
	}
	szText.Set(mpReturn->GetText());
	szText.StripWhitespace();
	if (!bProblem && szText.Empty() && !mpsSignature->szOldReturn.Empty())
	{
		szProblem.Set("The function needs a return type.");
		bProblem = true;
	}
	szText.Kill();
	if (!bProblem)
	{
		sChange.Init();
		GetChange(&sChange);
		bProblem = mpcOwner->FindProblem(mpsSignature, &sChange, &szProblem);
		sChange.Kill();
	}
	mpError->SetText(szProblem.Text());
	szProblem.Kill();
	return !bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// Another row selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::SelectionChanged(CTable* pcTable)
{
	if (mbUpdating || (pcTable->GetSelectedRow() < 0))
	{
		return;
	}
	miSelected = pcTable->GetSelectedRow();
}


//////////////////////////////////////////////////////////////////////////
//
// The function's name or what it returns typed: the signature shown is
// as it is typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::FieldChanged(void)
{
	if (mbUpdating)
	{
		return;
	}
	ShowPreview();
}


void CIDEChangeSignatureDialog::RemoveClicked(CButton* pcButton) { RemoveParameter(); }
void CIDEChangeSignatureDialog::UpClicked(CButton* pcButton) { MoveParameter(-1); }
void CIDEChangeSignatureDialog::DownClicked(CButton* pcButton) { MoveParameter(1); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::AddClicked(CButton* pcButton)
{
	AddParameter();
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpTable->GetList());
	mpTable->BeginEdit();
}


//////////////////////////////////////////////////////////////////////////
//
// A new parameter after the selected one (at the end, with none), to be
// given its type and name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::AddParameter(void)
{
	SIDENewParameter	sRow;

	mpTable->EndEdit(true);
	sRow.Init(-1);
	miSelected = (miSelected >= 0) ? miSelected + 1 : (int32)masRows.NumElements();
	masRows.InsertAt(&sRow, (size)miSelected);
	mbUpdating = true;
	mpTable->InsertRow(miSelected);
	ShowRow(miSelected);
	mpTable->SelectCell(miSelected, IDE_SIGNATURE_COLUMN_TYPE);
	mbUpdating = false;
	ShowPreview();
}


//////////////////////////////////////////////////////////////////////////
//
// The selected parameter taken out; the one after it is selected, or the
// last.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::RemoveParameter(void)
{
	int32	iRemoved;

	if (miSelected < 0)
	{
		return;
	}
	mpTable->EndEdit(false);
	iRemoved = miSelected;
	masRows.Get(miSelected)->Kill();
	masRows.RemoveAt((size)miSelected);
	if (miSelected >= (int32)masRows.NumElements())
	{
		miSelected = (int32)masRows.NumElements() - 1;
	}
	mbUpdating = true;
	mpTable->RemoveRow(iRemoved);
	if (miSelected >= 0)
	{
		mpTable->SelectCell(miSelected, (mpTable->GetSelectedColumn() >= 0) ? mpTable->GetSelectedColumn() : 0);
	}
	mbUpdating = false;
	ShowPreview();
}


//////////////////////////////////////////////////////////////////////////
//
// The selected parameter moved one place up (-1) or down (1), if there
// is a place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::MoveParameter(int32 iBy)
{
	SIDENewParameter	sRow;
	int32				iTo;

	iTo = miSelected + iBy;
	if ((miSelected < 0) || (iTo < 0) || (iTo >= (int32)masRows.NumElements()))
	{
		return;
	}
	mpTable->EndEdit(true);
	sRow = *masRows.Get(miSelected);
	*masRows.Get(miSelected) = *masRows.Get(iTo);
	*masRows.Get(iTo) = sRow;
	mbUpdating = true;
	mpTable->MoveRow(miSelected, iTo);
	miSelected = iTo;
	mpTable->SelectCell(miSelected, (mpTable->GetSelectedColumn() >= 0) ? mpTable->GetSelectedColumn() : 0);
	mbUpdating = false;
	ShowPreview();
}


//////////////////////////////////////////////////////////////////////////
//
// A cell of the selected parameter's row given a text as if the user had
// typed it there (for the checks' edits).
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::SetField(int32 iColumn, const char* szText)
{
	SIDENewParameter*	psRow;
	CChars*				pszField;

	if ((miSelected < 0) || !mpTable->IsCellEditable(miSelected, iColumn))
	{
		return;
	}
	psRow = masRows.Get((size)miSelected);
	pszField = (iColumn == IDE_SIGNATURE_COLUMN_TYPE) ? &psRow->szType : ((iColumn == IDE_SIGNATURE_COLUMN_NAME) ? &psRow->szName : ((iColumn == IDE_SIGNATURE_COLUMN_DEFAULT) ? &psRow->szDefault : &psRow->szCallValue));
	pszField->Set(szText);
	pszField->StripWhitespace();
	mpTable->SetCellText(miSelected, iColumn, pszField->Text());
	ShowPreview();
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do in the dialog, separated by
// semicolons: add, remove, up, down, select:<row>, type:<text>,
// name:<text>, default:<text>, value:<text> (the selected parameter's
// fields), function:<text>, return:<text>, print, which prints the rows
// and the signature shown, and check, which prints whether OK would
// close it and why not.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	CChars		szRow;
	size		i;
	size		j;
	char*		sz;
	bool		bApproved;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		if (strcmp(sz, "add") == 0)
		{
			AddParameter();
		}
		else if (strcmp(sz, "remove") == 0)
		{
			RemoveParameter();
		}
		else if (strcmp(sz, "up") == 0)
		{
			MoveParameter(-1);
		}
		else if (strcmp(sz, "down") == 0)
		{
			MoveParameter(1);
		}
		else if (strncmp(sz, "select:", 7) == 0)
		{
			mpTable->SelectCell(atoi(&sz[7]), 0);
		}
		else if (strncmp(sz, "type:", 5) == 0)
		{
			SetField(IDE_SIGNATURE_COLUMN_TYPE, &sz[5]);
		}
		else if (strncmp(sz, "name:", 5) == 0)
		{
			SetField(IDE_SIGNATURE_COLUMN_NAME, &sz[5]);
		}
		else if (strncmp(sz, "default:", 8) == 0)
		{
			SetField(IDE_SIGNATURE_COLUMN_DEFAULT, &sz[8]);
		}
		else if (strncmp(sz, "value:", 6) == 0)
		{
			SetField(IDE_SIGNATURE_COLUMN_VALUE, &sz[6]);
		}
		else if (strncmp(sz, "function:", 9) == 0)
		{
			mpFunction->SetText(&sz[9]);
		}
		else if (strncmp(sz, "return:", 7) == 0)
		{
			mpReturn->SetText(&sz[7]);
		}
		else if (strcmp(sz, "print") == 0)
		{
			printf("Signature dialog:");
			for (j = 0; j < masRows.NumElements(); j++)
			{
				szRow.Init();
				AppendRow(masRows.Get(j), &szRow);
				printf("%s %s%s", (j > 0) ? " |" : "", ((int32)j == miSelected) ? ">" : "", szRow.Text());
				szRow.Kill();
			}
			printf("\nSignature shown: %s\n", mpPreview->GetText());
			fflush(stdout);
		}
		else if (strcmp(sz, "check") == 0)
		{
			bApproved = Approve();
			printf("Signature dialog: %s [%s]\n", bApproved ? "OK" : "refused", mpError->GetText());
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
bool CIDEChangeSignatureDialog::Show(void)
{
	bool	bResult;

	bResult = mpDialog->Show(IDE_SIGNATURE_DIALOG_WIDTH, IDE_SIGNATURE_DIALOG_HEIGHT);
	mpTable->EndEdit(bResult);
	return bResult;
}


Ptr<CDialog> CIDEChangeSignatureDialog::GetDialog(void) { return mpDialog; }
