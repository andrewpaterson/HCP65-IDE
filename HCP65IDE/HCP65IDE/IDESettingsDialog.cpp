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
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDESettingsDialog.h"


#define IDE_SETTINGS_DIALOG_WIDTH		860
#define IDE_SETTINGS_DIALOG_HEIGHT		620
#define IDE_SETTINGS_DIALOG_LIST_WIDTH	160
#define IDE_SETTINGS_DIALOG_GAP			12


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettingsPage::Init(void)
{
	PreInit();
	PostInit();
}


void CIDESettingsPage::Class(void) {}
void CIDESettingsPage::Free(void) {}
bool CIDESettingsPage::Save(CObjectWriter* pcFile) { return false; }
bool CIDESettingsPage::Load(CObjectReader* pcFile) { return false; }
const char* CIDESettingsPage::GetTitle(void) { return ""; }
Ptr<CComponent> CIDESettingsPage::GetBody(void) { return NULL; }
Ptr<CComponent> CIDESettingsPage::GetFirstEditor(void) { return NULL; }
void CIDESettingsPage::Keep(void) {}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettingsDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook)
{
	PreInit();
	maPages.Init();
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Settings");
	mpDialog->SetResizable(IDE_SETTINGS_DIALOG_WIDTH / 2, IDE_SETTINGS_DIALOG_HEIGHT / 2);
	CreateBody();
	mpPageList->GetSelectionEvent()->Add(this, &CIDESettingsDialog::PageSelected);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettingsDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpPageList);
	M_Pointer(mpPageHolder);
	M_Embedded(maPages);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so the
// dialog takes itself off the widgets still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettingsDialog::Free(void)
{
	if (mpPageList.IsNotNull())
	{
		mpPageList->GetSelectionEvent()->Remove(this);
	}
}


bool CIDESettingsDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDESettingsDialog::Load(CObjectReader* pcFile) { return false; }
Ptr<CDialog> CIDESettingsDialog::GetDialog(void) { return mpDialog; }


//////////////////////////////////////////////////////////////////////////
//
// The pages' list on the left, what the selected one shows on the right.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDESettingsDialog::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CFillContainer>		pGap;
	SInsets					sInsets;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	sInsets.Init(0, 0);
	mpPageList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpPageList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpPageList->SetAlignment(CSH_Stretch, CSV_Top);
	mpPageList->SetDesiredSize(IDE_SETTINGS_DIALOG_LIST_WIDTH, -1);

	mpPageHolder = OMalloc<CGridContainer>(pWindow);
	mpPageHolder->SetAlignment(CSH_Stretch, CSV_Stretch);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->AddComponent(mpPageList, 0, 0, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_SETTINGS_DIALOG_GAP, 1);
	pBody->AddComponent(pGap, 1, 0);
	pBody->AddComponent(mpPageHolder, 2, 0, 1, 1, 1, 1);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettingsDialog::AddPage(Ptr<CIDESettingsPage> pPage)
{
	Ptr<CLabel>	pLabel;

	maPages.Add(pPage);
	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), pPage->GetTitle());
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	mpPageList->AddItem(pLabel);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettingsDialog::PageSelected(void)
{
	Ptr<CIDESettingsPage>	pPage;
	int32					iSelected;

	iSelected = mpPageList->GetSelected();
	if ((iSelected < 0) || ((size)iSelected >= maPages.NumElements()))
	{
		return;
	}
	pPage = maPages.Get((size)iSelected);
	mpPageHolder->RemoveAllComponents();
	mpPageHolder->AddComponent(pPage->GetBody(), 0, 0, 1, 1, 1, 1);
}


//////////////////////////////////////////////////////////////////////////
//
// The first page is shown.  True, once every page has kept what it was
// given, if OK closed it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESettingsDialog::Show(void)
{
	bool	bResult;
	size	i;

	mpDialog->SetBody(mpPageHolder->GetParent(), maPages.Get(0)->GetFirstEditor());
	mpPageList->Select(0);
	bResult = mpDialog->Show(IDE_SETTINGS_DIALOG_WIDTH, IDE_SETTINGS_DIALOG_HEIGHT);
	if (bResult)
	{
		for (i = 0; i < maPages.NumElements(); i++)
		{
			maPages.Get(i)->Keep();
		}
	}
	return bResult;
}

