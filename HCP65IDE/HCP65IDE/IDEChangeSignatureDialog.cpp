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
#include "IDEChangeSignatureDialog.h"


#define IDE_SIGNATURE_DIALOG_WIDTH		820
#define IDE_SIGNATURE_DIALOG_HEIGHT		420
#define IDE_SIGNATURE_DIALOG_LIST_WIDTH	320
#define IDE_SIGNATURE_DIALOG_GAP		12


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

	mpList->GetSelectionEvent()->Add(this, &CIDEChangeSignatureDialog::SelectionChanged);
	mpFunction->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpReturn->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpType->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpName->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpDefault->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpCallValue->GetChangeEvent()->Add(this, &CIDEChangeSignatureDialog::FieldChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::RemoveClicked);
	mpUp->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::UpClicked);
	mpDown->GetClickEvent()->Add(this, &CIDEChangeSignatureDialog::DownClicked);

	mbUpdating = true;
	mpFunction->SetText(psSignature->szOldName.Text());
	mpReturn->SetText(psSignature->szOldReturn.Text());
	mbUpdating = false;
	ShowList();
	ShowSelected();
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
	M_Pointer(mpList);
	M_Pointer(mpScrollPane);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpUp);
	M_Pointer(mpDown);
	M_Pointer(mpType);
	M_Pointer(mpName);
	M_Pointer(mpDefault);
	M_Pointer(mpCallValue);
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
	if (mpList.IsNotNull())
	{
		mpList->GetSelectionEvent()->Remove(this);
	}
	StopListening(mpFunction);
	StopListening(mpReturn);
	StopListening(mpType);
	StopListening(mpName);
	StopListening(mpDefault);
	StopListening(mpCallValue);
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
// parameters' list with its buttons on the left and the selected one's
// form on the right; the signature as it would be, and what is wrong
// with it, across the bottom.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEChangeSignatureDialog::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CGridContainer>		pTop;
	Ptr<CGridContainer>		pMiddle;
	Ptr<CGridContainer>		pLeft;
	Ptr<CGridContainer>		pButtons;
	Ptr<CGridContainer>		pForm;
	Ptr<CGridContainer>		pBottom;
	Ptr<CFillContainer>		pGap;
	SInsets					sInsets;
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

	sInsets.Init(0, 0);
	mpList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpList->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpScrollPane->SetContent(mpList);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpScrollPane->SetDesiredSize(IDE_SIGNATURE_DIALOG_LIST_WIDTH, -1);

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

	pLeft = OMalloc<CGridContainer>(pWindow);
	pLeft->SetAlignment(CSH_Stretch, CSV_Stretch);
	pLeft->AddComponent(mpScrollPane, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_SIGNATURE_DIALOG_GAP / 2);
	pLeft->AddComponent(pGap, 0, 1);
	pLeft->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	mpType = CreateField();
	mpName = CreateField();
	mpDefault = CreateField();
	mpCallValue = CreateField();
	pForm = OMalloc<CGridContainer>(pWindow);
	pForm->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pForm, &iRow, "Type", mpType);
	AddFormRow(pForm, &iRow, "Name", mpName);
	AddFormRow(pForm, &iRow, "Default value", mpDefault);
	AddFormRow(pForm, &iRow, "Value in calls", mpCallValue);
	//What is left over goes under the rows, which keeps them at the top.
	pGap = OMalloc<CFillContainer>(pWindow);
	pForm->AddComponent(pGap, 0, iRow, 2, 1, 1, 1);

	pMiddle = OMalloc<CGridContainer>(pWindow);
	pMiddle->SetAlignment(CSH_Stretch, CSV_Stretch);
	pMiddle->AddComponent(pLeft, 0, 0, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_SIGNATURE_DIALOG_GAP, 1);
	pMiddle->AddComponent(pGap, 1, 0);
	pMiddle->AddComponent(pForm, 2, 0, 1, 1, 1, 1);

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
// A row for each parameter, the selected one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::ShowList(void)
{
	Ptr<CLabel>	pLabel;
	CChars		szText;
	size		i;

	mbUpdating = true;
	mpList->RemoveAllComponents();
	for (i = 0; i < masRows.NumElements(); i++)
	{
		szText.Init();
		AppendRow(masRows.Get(i), &szText);
		if (szText.Empty())
		{
			szText.Append("(new parameter)");
		}
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szText.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpList->AddItem(pLabel);
		szText.Kill();
	}
	mpList->Select(miSelected);
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The selected parameter in the form, which is empty if there are none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::ShowSelected(void)
{
	SIDENewParameter*	psRow;

	mbUpdating = true;
	psRow = (miSelected >= 0) ? masRows.Get(miSelected) : NULL;
	mpType->SetText(psRow ? psRow->szType.Text() : "");
	mpName->SetText(psRow ? psRow->szName.Text() : "");
	mpDefault->SetText(psRow ? psRow->szDefault.Text() : "");
	mpCallValue->SetText(psRow ? psRow->szCallValue.Text() : "");
	mbUpdating = false;
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
// The form's fields kept in the selected row.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::Keep(void)
{
	SIDENewParameter*	psRow;

	if (miSelected < 0)
	{
		return;
	}
	psRow = masRows.Get(miSelected);
	psRow->szType.Set(mpType->GetText());
	psRow->szType.StripWhitespace();
	psRow->szName.Set(mpName->GetText());
	psRow->szName.StripWhitespace();
	psRow->szDefault.Set(mpDefault->GetText());
	psRow->szDefault.StripWhitespace();
	psRow->szCallValue.Set(mpCallValue->GetText());
	psRow->szCallValue.StripWhitespace();
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

	Keep();
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
void CIDEChangeSignatureDialog::SelectionChanged(void)
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
// What is typed shows in the list and in the signature as it is typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignatureDialog::FieldChanged(void)
{
	if (mbUpdating)
	{
		return;
	}
	Keep();
	ShowList();
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
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpType);
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

	Keep();
	sRow.Init(-1);
	miSelected = (miSelected >= 0) ? miSelected + 1 : (int32)masRows.NumElements();
	masRows.InsertAt(&sRow, (size)miSelected);
	ShowList();
	ShowSelected();
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
	if (miSelected < 0)
	{
		return;
	}
	masRows.Get(miSelected)->Kill();
	masRows.RemoveAt((size)miSelected);
	if (miSelected >= (int32)masRows.NumElements())
	{
		miSelected = (int32)masRows.NumElements() - 1;
	}
	ShowList();
	ShowSelected();
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
	Keep();
	sRow = *masRows.Get(miSelected);
	*masRows.Get(miSelected) = *masRows.Get(iTo);
	*masRows.Get(iTo) = sRow;
	miSelected = iTo;
	ShowList();
	ShowSelected();
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
			mpList->Select(atoi(&sz[7]));
		}
		else if (strncmp(sz, "type:", 5) == 0)
		{
			mpType->SetText(&sz[5]);
		}
		else if (strncmp(sz, "name:", 5) == 0)
		{
			mpName->SetText(&sz[5]);
		}
		else if (strncmp(sz, "default:", 8) == 0)
		{
			mpDefault->SetText(&sz[8]);
		}
		else if (strncmp(sz, "value:", 6) == 0)
		{
			mpCallValue->SetText(&sz[6]);
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
	if (bResult)
	{
		Keep();
	}
	return bResult;
}


Ptr<CDialog> CIDEChangeSignatureDialog::GetDialog(void) { return mpDialog; }
