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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDEKeymapPage.h"


#define IDE_KEYMAP_CAPTURE_WIDTH	460
#define IDE_KEYMAP_GAP				6
#define IDE_KEYMAP_COLUMN_GAP		24


//The groups of the keymap, in the order they're shown: the part of a command's name before its dot and what's shown.
struct SIDEKeymapGroup
{
	const char*	szPrefix;
	const char*	szCaption;
};


static SIDEKeymapGroup	gasIDEKeymapGroups[] =
{
	{ "File", "File" },
	{ "Edit", "Edit" },
	{ "Navigate", "Navigate" },
	{ "Refactor", "Refactor" },
	{ "View", "View" },
	{ "Git", "Git" },
	{ "Build", "Build" },
	{ "Run", "Run" },
	{ "Debug", "Debug" },
	{ "Window", "Window" },
	{ "Explorer", "Solution Explorer" },
	{ "Editor", "Editor Actions" },
};


//////////////////////////////////////////////////////////////////////////
//
// Whether szName is in the group, so "File.Save" is in "File".
//
//////////////////////////////////////////////////////////////////////////
static bool IsCommandInGroup(const char* szName, const char* szPrefix)
{
	size	uiLength;

	uiLength = strlen(szPrefix);
	return (strncmp(szName, szPrefix, uiLength) == 0) && (szName[uiLength] == '.');
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szFilter is in szText, whatever the case.
//
//////////////////////////////////////////////////////////////////////////
static bool ContainsIgnoringCase(const char* szText, const char* szFilter)
{
	size	uiFilter;
	size	i;
	size	j;

	uiFilter = strlen(szFilter);
	for (i = 0; szText[i] != '\0'; i++)
	{
		for (j = 0; (j < uiFilter) && (szText[i + j] != '\0') && (tolower((unsigned char)szText[i + j]) == tolower((unsigned char)szFilter[j])); j++)
		{
		}
		if (j == uiFilter)
		{
			return true;
		}
	}
	return uiFilter == 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeyCaptureDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CIDEKeymapPage* pcPage, size uiRow)
{
	PreInit();
	mpcPage = pcPage;
	muiRow = uiRow;
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Add Shortcut");
	mpField = OMalloc<CKeyCaptureField>(mpDialog->GetWindow(), psLook->pcTheme);
	mpField->SetAlignment(CSH_Stretch, CSV_Center);
	mpMessage = OMalloc<CLabel>(mpDialog->GetWindow(), psLook->pcTheme->GetFont(), "Press the keys.");
	mpMessage->SetAlignment(CSH_Left, CSV_Center);
	mpDialog->AddEditor("Shortcut", mpField);
	mpDialog->AddEditor("", mpMessage);
	mpDialog->SetApprover(this, &CIDEKeyCaptureDialog::Approve);
	mpField->GetCapturedEvent()->Add(this, &CIDEKeyCaptureDialog::KeysCaptured);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeyCaptureDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpField);
	M_Pointer(mpMessage);
	U_Pointer(mpcPage);
	U_Size(muiRow);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeyCaptureDialog::Free(void)
{
	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearApprover();
	}
	if (mpField.IsNotNull())
	{
		mpField->GetCapturedEvent()->Remove(this);
	}
}


bool CIDEKeyCaptureDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDEKeyCaptureDialog::Load(CObjectReader* pcFile) { return false; }
bool CIDEKeyCaptureDialog::Show(void) { return mpDialog->Show(IDE_KEYMAP_CAPTURE_WIDTH); }
char* CIDEKeyCaptureDialog::GetKeys(void) { return mpField->GetKeys(); }
void CIDEKeyCaptureDialog::SetKeys(const char* szKeys) { mpField->SetKeys(szKeys); }
Ptr<CDialog> CIDEKeyCaptureDialog::GetDialog(void) { return mpDialog; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeyCaptureDialog::KeysCaptured(void)
{
	CChars	szMessage;

	szMessage.Init();
	mpcPage->DescribeKeys(muiRow, mpField->GetKeys(), &szMessage);
	mpMessage->SetText(szMessage.Text());
	szMessage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// OK only closes it for keys the command can have.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEKeyCaptureDialog::Approve(void)
{
	CChars	szMessage;
	bool	bResult;

	szMessage.Init();
	bResult = mpcPage->DescribeKeys(muiRow, mpField->GetKeys(), &szMessage);
	szMessage.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// pDialog is what the page is on.  The commands and input aren't held.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::Init(Ptr<CDialog> pDialog, Ptr<CCommandSet> pCommands, CComponentInput* pcInput, Ptr<CIDEScript> pScript)
{
	PreInit();
	CIDESettingsPage::Init();
	mpDialog = pDialog;
	mpCommands = pCommands;
	mpcInput = pcInput;
	mpScript = pScript;
	mszScriptedKeys.Init();
	mbUpdating = false;
	masRows.Init();
	maiListRows.Init();
	AddRows();
	mpBody = CreateBody();

	mpFilter->GetChangeEvent()->Add(this, &CIDEKeymapPage::FilterChanged);
	mpList->GetActivationEvent()->Add(this, &CIDEKeymapPage::RowActivated);
	mpAdd->GetClickEvent()->Add(this, &CIDEKeymapPage::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDEKeymapPage::RemoveClicked);
	mpReset->GetClickEvent()->Add(this, &CIDEKeymapPage::ResetClicked);
	mpResetAll->GetClickEvent()->Add(this, &CIDEKeymapPage::ResetAllClicked);

	ShowList();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::Class(void)
{
	CIDESettingsPage::Class();
	M_Pointer(mpDialog);
	M_Pointer(mpCommands);
	U_Pointer(mpcInput);
	M_Pointer(mpScript);
	U_String(mszScriptedKeys);
	M_Pointer(mpBody);
	M_Pointer(mpFilter);
	M_Pointer(mpList);
	M_Pointer(mpScrollPane);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpReset);
	M_Pointer(mpResetAll);
	M_Pointer(mpStatus);
	M_Pointer(mpCapture);
	U_Unknown(CArrayIDEKeymapRows, masRows);
	U_Unknown(CArrayInt, maiListRows);
	U_Bool(mbUpdating);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so the page
// takes itself off the widgets still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::Free(void)
{
	size	i;

	if (mpDialog.IsNotNull())
	{
		mpDialog->GetWindow()->CancelPosted(this);
	}
	if (mpFilter.IsNotNull())
	{
		mpFilter->GetChangeEvent()->Remove(this);
	}
	if (mpList.IsNotNull())
	{
		mpList->GetActivationEvent()->Remove(this);
	}
	if (mpAdd.IsNotNull())
	{
		mpAdd->GetClickEvent()->Remove(this);
		mpRemove->GetClickEvent()->Remove(this);
		mpReset->GetClickEvent()->Remove(this);
		mpResetAll->GetClickEvent()->Remove(this);
	}
	for (i = 0; i < masRows.NumElements(); i++)
	{
		masRows.Get(i)->aszKeys.Kill();
	}
	masRows.Kill();
	maiListRows.Kill();
	mszScriptedKeys.Kill();
	CIDESettingsPage::Free();
}


bool CIDEKeymapPage::Save(CObjectWriter* pcFile) { return false; }
bool CIDEKeymapPage::Load(CObjectReader* pcFile) { return false; }
const char* CIDEKeymapPage::GetTitle(void) { return "Keymap"; }
Ptr<CComponent> CIDEKeymapPage::GetBody(void) { return mpBody; }
Ptr<CComponent> CIDEKeymapPage::GetFirstEditor(void) { return mpFilter; }


//////////////////////////////////////////////////////////////////////////
//
// Copies of the commands' keys, a group's together.  A command in no group
// goes after them all.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::AddRows(void)
{
	SIDEKeymapRow*	psRow;
	Ptr<CCommand>	pCommand;
	size			uiNumGroups;
	size			uiGroup;
	size			i;
	size			j;
	bool			bInGroup;

	uiNumGroups = sizeof(gasIDEKeymapGroups) / sizeof(gasIDEKeymapGroups[0]);
	for (uiGroup = 0; uiGroup <= uiNumGroups; uiGroup++)
	{
		for (i = 0; i < mpCommands->NumCommands(); i++)
		{
			pCommand = mpCommands->GetCommand(i);
			if (uiGroup < uiNumGroups)
			{
				bInGroup = IsCommandInGroup(pCommand->GetName(), gasIDEKeymapGroups[uiGroup].szPrefix);
			}
			else
			{
				bInGroup = true;
				for (j = 0; j < uiNumGroups; j++)
				{
					bInGroup = bInGroup && !IsCommandInGroup(pCommand->GetName(), gasIDEKeymapGroups[j].szPrefix);
				}
			}
			if (bInGroup)
			{
				psRow = masRows.Add();
				psRow->pcCommand = &pCommand;
				psRow->aszKeys.Init();
				for (j = 0; j < pCommand->NumKeys(); j++)
				{
					psRow->aszKeys.Add(pCommand->GetKeys(j));
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The filter over the list, its buttons under it and what's been said
// under them.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEKeymapPage::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CGridContainer>		pButtons;
	Ptr<CFillContainer>		pGap;
	SInsets					sInsets;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	mpFilter = OMalloc<CTextField>(pWindow, psLook->pcTheme, "");
	mpFilter->SetAlignment(CSH_Stretch, CSV_Center);

	sInsets.Init(0, 0);
	mpList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpList->SetAlignment(CSH_Stretch, CSV_Top);
	mpList->SetDoubleClickActivates(true);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpScrollPane->SetContent(mpList);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	mpAdd = CreateButton(pButtons, 0, "Add Shortcut...");
	mpRemove = CreateButton(pButtons, 2, "Remove");
	mpReset = CreateButton(pButtons, 4, "Reset");
	mpResetAll = CreateButton(pButtons, 6, "Reset All");
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 7, 0, 1, 1, 1, 0);  //Keeps the buttons on the left.

	mpStatus = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpStatus->SetAlignment(CSH_Left, CSV_Center);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->SetAlignment(CSH_Stretch, CSV_Stretch);
	pBody->AddComponent(mpFilter, 0, 0, 1, 1, 1, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_KEYMAP_GAP);
	pBody->AddComponent(pGap, 0, 1);
	pBody->AddComponent(mpScrollPane, 0, 2, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_KEYMAP_GAP);
	pBody->AddComponent(pGap, 0, 3);
	pBody->AddComponent(pButtons, 0, 4, 1, 1, 1, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_KEYMAP_GAP);
	pBody->AddComponent(pGap, 0, 5);
	pBody->AddComponent(mpStatus, 0, 6, 1, 1, 1, 0);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
// A button, with a gap in the column after it.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDEKeymapPage::CreateButton(Ptr<CGridContainer> pButtons, int32 iColumn, const char* szCaption)
{
	Ptr<CButton>		pButton;
	Ptr<CFillContainer>	pGap;

	pButton = OMalloc<CButton>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, szCaption);
	pButton->SetAlignment(CSH_Left, CSV_Center);
	pButtons->AddComponent(pButton, iColumn, 0);
	pGap = OMalloc<CFillContainer>(mpDialog->GetWindow());
	pGap->SetDesiredSize(IDE_KEYMAP_GAP, 1);
	pButtons->AddComponent(pGap, iColumn + 1, 0);
	return pButton;
}


//////////////////////////////////////////////////////////////////////////
//
// The group a command is shown under, "Other" if it's in none.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEKeymapPage::GetGroupCaption(CCommand* pcCommand)
{
	size	i;

	for (i = 0; i < sizeof(gasIDEKeymapGroups) / sizeof(gasIDEKeymapGroups[0]); i++)
	{
		if (IsCommandInGroup(pcCommand->GetName(), gasIDEKeymapGroups[i].szPrefix))
		{
			return gasIDEKeymapGroups[i].szCaption;
		}
	}
	return "Other";
}


//////////////////////////////////////////////////////////////////////////
//
// The row's keys as a menu shows them, separated by commas.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ShowKeys(size uiRow, CChars* pszDest)
{
	SIDEKeymapRow*	psRow;
	size			i;

	psRow = masRows.Get(uiRow);
	for (i = 0; i < psRow->aszKeys.NumElements(); i++)
	{
		if (i > 0)
		{
			pszDest->Append(", ");
		}
		CComponentInput::AppendShortcutText(pszDest, psRow->aszKeys.GetText(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the filter lets the row be shown: its caption, its name, its
// group or its keys have what's typed in them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEKeymapPage::IsShown(size uiRow)
{
	SIDEKeymapRow*	psRow;
	CChars			szKeys;
	char*			szFilter;
	bool			bResult;

	psRow = masRows.Get(uiRow);
	szFilter = mpFilter->GetText();
	szKeys.Init();
	ShowKeys(uiRow, &szKeys);
	bResult = ContainsIgnoringCase(psRow->pcCommand->GetCaption(), szFilter) ||
			  ContainsIgnoringCase(psRow->pcCommand->GetName(), szFilter) ||
			  ContainsIgnoringCase(GetGroupCaption(psRow->pcCommand), szFilter) ||
			  ContainsIgnoringCase(szKeys.Text(), szFilter);
	szKeys.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The rows the filter lets through under their groups' headings, with the
// row selected kept selected if it's still there.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ShowList(void)
{
	Ptr<CWindow>	pWindow;
	SDialogLook*	psLook;
	SIDEKeymapRow*	psRow;
	Ptr<CLabel>		pLabel;
	CChars			szCaption;
	CChars			szKeys;
	const char*		szGroup;
	int32			iSelected;
	int32			iListRow;
	size			i;

	iSelected = GetSelectedRow();
	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();
	mbUpdating = true;
	mpList->RemoveAllComponents();
	maiListRows.ReInit();
	szGroup = NULL;
	for (i = 0; i < masRows.NumElements(); i++)
	{
		if (!IsShown(i))
		{
			continue;
		}
		psRow = masRows.Get(i);
		if ((szGroup == NULL) || (StringCompare(szGroup, GetGroupCaption(psRow->pcCommand)) != 0))
		{
			szGroup = GetGroupCaption(psRow->pcCommand);
			pLabel = OMalloc<CLabel>(pWindow, psLook->pcDimFont, szGroup);
			pLabel->SetAlignment(CSH_Left, CSV_Center);
			iListRow = mpList->AddRow(false);
			mpList->AddToRow(iListRow, pLabel, 0, 2, 1);
			maiListRows.Add(-1);
		}

		szCaption.Init(psRow->pcCommand->GetCaption());
		szKeys.Init();
		ShowKeys(i, &szKeys);
		pLabel = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), szCaption.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		iListRow = mpList->AddRow(true);
		mpList->AddToRow(iListRow, pLabel, 0, 1, 1);
		pLabel = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), szKeys.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		pLabel->SetDesiredSize(pLabel->GetRequiredSize().x + IDE_KEYMAP_COLUMN_GAP, -1);
		mpList->AddToRow(iListRow, pLabel, 1);
		maiListRows.Add((int)i);
		szKeys.Kill();
		szCaption.Kill();
	}
	if (iSelected >= 0)
	{
		SelectRow((size)iSelected);
	}
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The row of masRows selected in the list, -1 if none is.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEKeymapPage::GetSelectedRow(void)
{
	int32	iListRow;

	iListRow = mpList->GetSelected();
	if ((iListRow < 0) || (iListRow >= (int32)maiListRows.NumElements()))
	{
		return -1;
	}
	return maiListRows.GetValue((size)iListRow);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::SelectRow(size uiRow)
{
	size	i;

	for (i = 0; i < maiListRows.NumElements(); i++)
	{
		if (maiListRows.GetValue(i) == (int)uiRow)
		{
			mpList->Select((int32)i);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What giving szKeys to the row's command would mean, for saying so, and
// false if it can't have them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEKeymapPage::DescribeKeys(size uiRow, const char* szKeys, CChars* pszMessage)
{
	SIDEKeymapRow*	psRow;
	CChars			szShown;
	CChars			szName;
	size			i;
	bool			bResult;

	pszMessage->Clear();
	if (StrEmpty(szKeys))
	{
		pszMessage->Append("Press the keys.");
		return false;
	}

	psRow = masRows.Get(uiRow);
	szShown.Init();
	szName.Init();
	CComponentInput::AppendShortcutText(&szShown, szKeys);
	CComponentInput::NameKeys(szKeys, &szName);
	pszMessage->Append(szShown.Text());
	bResult = false;
	if (psRow->aszKeys.Contains(szName.Text()))
	{
		pszMessage->Append(" is already its shortcut.");
	}
	else if (mpcInput->IsTypingKeys(szName.Text()))
	{
		pszMessage->Append(" types a character: add Ctrl or Alt.");
	}
	else if ((psRow->pcCommand->GetKeyScope() != CKS_Editor) && CComponentInput::IsWidgetCommand(mpcInput->GetStandardCommand(szName.Text())))
	{
		pszMessage->Append(" is needed by dialogs and text fields.");
	}
	else
	{
		bResult = true;
		pszMessage->Append(" is free.");
		for (i = 0; i < masRows.NumElements(); i++)
		{
			if ((i != uiRow) && masRows.Get(i)->aszKeys.Contains(szName.Text()))
			{
				pszMessage->Clear();
				pszMessage->Append(szShown.Text());
				pszMessage->Append(" moves here from ");
				pszMessage->Append(GetGroupCaption(masRows.Get(i)->pcCommand));
				pszMessage->Append(": ");
				pszMessage->Append(masRows.Get(i)->pcCommand->GetCaption());
				pszMessage->Append(".");
				break;
			}
		}
	}
	szName.Kill();
	szShown.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// szKeys go to the row's command, from whichever had them.  False, with
// why under the list, if it can't have them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEKeymapPage::AddKeys(size uiRow, const char* szKeys)
{
	CChars	szMessage;
	CChars	szName;
	size	i;
	bool	bResult;

	szMessage.Init();
	bResult = DescribeKeys(uiRow, szKeys, &szMessage);
	mpStatus->SetText(szMessage.Text());
	szMessage.Kill();
	if (bResult)
	{
		szName.Init();
		CComponentInput::NameKeys(szKeys, &szName);
		for (i = 0; i < masRows.NumElements(); i++)
		{
			if (masRows.Get(i)->aszKeys.Contains(szName.Text()))
			{
				masRows.Get(i)->aszKeys.Remove(szName.Text());
			}
		}
		masRows.Get(uiRow)->aszKeys.Add(szName.Text());
		szName.Kill();
		ShowList();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::RemoveKeys(size uiRow)
{
	masRows.Get(uiRow)->aszKeys.ReInit();
	mpStatus->SetText("");
	ShowList();
}


//////////////////////////////////////////////////////////////////////////
//
// The command has the keys it was made with, taking them from any other
// that has them now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ResetKeys(size uiRow)
{
	SIDEKeymapRow*	psRow;
	size			i;
	size			j;
	char*			szKeys;

	psRow = masRows.Get(uiRow);
	psRow->aszKeys.ReInit();
	for (i = 0; i < psRow->pcCommand->NumDefaultKeys(); i++)
	{
		szKeys = psRow->pcCommand->GetDefaultKeys(i);
		for (j = 0; j < masRows.NumElements(); j++)
		{
			if (masRows.Get(j)->aszKeys.Contains(szKeys))
			{
				masRows.Get(j)->aszKeys.Remove(szKeys);
			}
		}
		psRow->aszKeys.Add(szKeys);
	}
	mpStatus->SetText("");
	ShowList();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ResetAllKeys(void)
{
	SIDEKeymapRow*	psRow;
	size			i;
	size			j;

	for (i = 0; i < masRows.NumElements(); i++)
	{
		psRow = masRows.Get(i);
		psRow->aszKeys.ReInit();
		for (j = 0; j < psRow->pcCommand->NumDefaultKeys(); j++)
		{
			psRow->aszKeys.Add(psRow->pcCommand->GetDefaultKeys(j));
		}
	}
	mpStatus->SetText("");
	ShowList();
}


//////////////////////////////////////////////////////////////////////////
//
// OK closed the dialog: the commands have what's here, to be bound and
// written by whoever showed it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::Keep(void)
{
	size	i;

	for (i = 0; i < masRows.NumElements(); i++)
	{
		masRows.Get(i)->pcCommand->SetKeys(&masRows.Get(i)->aszKeys);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Add Shortcut for the selected row, from the dialog's tick: a dialog
// can't be shown from an input action.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ShowCapture(void)
{
	int32	iRow;
	CChars	szKeys;
	bool	bOK;

	iRow = GetSelectedRow();
	if (iRow < 0)
	{
		mpStatus->SetText("Select a command first.");
		return;
	}
	mpCapture = OMalloc<CIDEKeyCaptureDialog>(mpDialog->GetWindow(), mpDialog->GetLook(), this, (size)iRow);
	if (!mszScriptedKeys.Empty())
	{
		mpCapture->SetKeys(mszScriptedKeys.Text());
	}
	mpScript->WatchDialog(mpCapture->GetDialog());
	bOK = mpCapture->Show();
	mpScript->StopWatchingDialog();
	if (bOK)
	{
		szKeys.Init(mpCapture->GetKeys());
		AddKeys((size)iRow, szKeys.Text());
		szKeys.Kill();
	}
	mpCapture = NULL;
	mpScript->WatchDialog(mpDialog);  //Again, as watching Add Shortcut replaced it.
}


void CIDEKeymapPage::FilterChanged(void) { ShowList(); }
void CIDEKeymapPage::RowActivated(void) { mpDialog->GetWindow()->Post(this, &CIDEKeymapPage::ShowCapture); }
void CIDEKeymapPage::AddClicked(CButton* pcButton) { mpDialog->GetWindow()->Post(this, &CIDEKeymapPage::ShowCapture); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::RemoveClicked(CButton* pcButton)
{
	if (GetSelectedRow() >= 0)
	{
		RemoveKeys((size)GetSelectedRow());
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ResetClicked(CButton* pcButton)
{
	if (GetSelectedRow() >= 0)
	{
		ResetKeys((size)GetSelectedRow());
	}
}


void CIDEKeymapPage::ResetAllClicked(CButton* pcButton) { ResetAllKeys(); }


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do on the page, separated by semicolons:
// select:<command's name>, add:<keys>, capture:<keys>, which goes through
// Add Shortcut with them pressed, remove, reset, resetall, filter:<text>,
// and show, which prints the rows the list has.
//
//////////////////////////////////////////////////////////////////////////
void CIDEKeymapPage::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	CChars		szKeys;
	size		i;
	size		j;
	char*		sz;
	int32		iRow;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		iRow = GetSelectedRow();
		if (strncmp(sz, "select:", 7) == 0)
		{
			for (j = 0; j < masRows.NumElements(); j++)
			{
				if (StringCompare(masRows.Get(j)->pcCommand->GetName(), &sz[7]) == 0)
				{
					SelectRow(j);
				}
			}
		}
		else if ((strncmp(sz, "add:", 4) == 0) && (iRow >= 0))
		{
			AddKeys((size)iRow, &sz[4]);
			printf("Keymap: %s\n", mpStatus->GetText());
		}
		else if (strncmp(sz, "capture:", 8) == 0)
		{
			mszScriptedKeys.Set(&sz[8]);
			mpScript->SetDialogDrawnCall(this, &CIDEKeymapPage::ShowCapture);
		}
		else if ((strcmp(sz, "remove") == 0) && (iRow >= 0))
		{
			RemoveKeys((size)iRow);
		}
		else if ((strcmp(sz, "reset") == 0) && (iRow >= 0))
		{
			ResetKeys((size)iRow);
		}
		else if (strcmp(sz, "resetall") == 0)
		{
			ResetAllKeys();
		}
		else if (strncmp(sz, "filter:", 7) == 0)
		{
			mpFilter->SetText(&sz[7]);
			ShowList();
		}
		else if (strcmp(sz, "show") == 0)
		{
			for (j = 0; j < maiListRows.NumElements(); j++)
			{
				if (maiListRows.GetValue(j) >= 0)
				{
					szKeys.Init();
					ShowKeys((size)maiListRows.GetValue(j), &szKeys);
					printf("Keymap row: %s [%s]\n", masRows.Get((size)maiListRows.GetValue(j))->pcCommand->GetName(), szKeys.Text());
					szKeys.Kill();
				}
			}
		}
		fflush(stdout);
	}
	aszEdits.Kill();
}

