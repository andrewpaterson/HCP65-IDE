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
#include <string.h>
#include <stdlib.h>
#include "BaseLib/ArrayChars.h"
#include "WindowLib/FillContainer.h"
#include "IDEContent.h"
#include "IDEEditorPage.h"


#define IDE_EDITOR_PAGE_GAP		8


//////////////////////////////////////////////////////////////////////////
//
// The page shows the settings as they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::Init(CIDEContent* pcContent, Ptr<CDialog> pDialog)
{
	Ptr<CWindow>		pWindow;
	SDialogLook*		psLook;
	Ptr<CFillContainer>	pGap;
	Ptr<CIDESettings>	pSettings;

	PreInit();
	CSettingsPage::Init();
	mpcContent = pcContent;
	mpDialog = pDialog;
	pSettings = pcContent->GetSettings();
	pWindow = pDialog->GetWindow();
	psLook = pDialog->GetLook();

	mpBreadcrumbs = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Show breadcrumbs: what the caret is in, under the code", pcContent->GetStructure()->IsBreadcrumbs());
	mpBreadcrumbs->SetAlignment(CSH_Left, CSV_Center);
	mpStickyLines = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Show sticky lines: the first lines of what the top of the view is in", pcContent->GetStructure()->IsStickyLines());
	mpStickyLines->SetAlignment(CSH_Left, CSV_Center);
	mpKeyRepeat = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Repeat held keys: the caret's moves, Page Up and Down, deleting and typing", pSettings->IsKeyRepeat());
	mpKeyRepeat->SetAlignment(CSH_Left, CSV_Center);
	mpRepeatDelay = CreateNumberField(pSettings->GetKeyRepeatDelay());
	mpRepeatRate = CreateNumberField(pSettings->GetKeyRepeatRate());

	mpBody = OMalloc<CGridContainer>(pWindow);
	mpBody->SetAlignment(CSH_Stretch, CSV_Top);
	mpBody->AddComponent(mpBreadcrumbs, 0, 0, 3, 1, 1, 0);
	mpBody->AddComponent(mpStickyLines, 0, 1, 3, 1, 1, 0);
	mpBody->AddComponent(mpKeyRepeat, 0, 2, 3, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Held before repeating (milliseconds):"), 0, 3);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_EDITOR_PAGE_GAP, 1);
	mpBody->AddComponent(pGap, 1, 3);
	mpBody->AddComponent(mpRepeatDelay, 2, 3, 1, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Repeats a second:"), 0, 4);
	mpBody->AddComponent(mpRepeatRate, 2, 4, 1, 1, 1, 0);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::Class(void)
{
	CSettingsPage::Class();
	U_Pointer(mpcContent);
	M_Pointer(mpBreadcrumbs);
	M_Pointer(mpStickyLines);
	M_Pointer(mpKeyRepeat);
	M_Pointer(mpRepeatDelay);
	M_Pointer(mpRepeatRate);
	M_Pointer(mpDialog);
	M_Pointer(mpBody);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::Free(void)
{
	CSettingsPage::Free();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CLabel> CIDEEditorPage::CreateLabel(const char* szText)
{
	Ptr<CLabel>	pLabel;

	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szText);
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	return pLabel;
}


Ptr<CTextField> CIDEEditorPage::CreateNumberField(uint32 uiNumber)
{
	Ptr<CTextField>	pField;
	CChars			szNumber;

	szNumber.Init();
	szNumber.Append(uiNumber);
	pField = OMalloc<CTextField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, szNumber.Text());
	pField->SetAlignment(CSH_Stretch, CSV_Center);
	szNumber.Kill();
	return pField;
}


//////////////////////////////////////////////////////////////////////////
//
// The number in szText if it is from uiMinimum to uiMaximum, otherwise
// uiWas.
//
//////////////////////////////////////////////////////////////////////////
uint32 CIDEEditorPage::ReadNumber(const char* szText, uint32 uiMinimum, uint32 uiMaximum, uint32 uiWas)
{
	int	iNumber;

	iNumber = atoi(szText);
	if ((iNumber < (int)uiMinimum) || (iNumber > (int)uiMaximum))
	{
		return uiWas;
	}
	return (uint32)iNumber;
}


//////////////////////////////////////////////////////////////////////////
//
// OK: what is ticked, and the key repeat's numbers (one out of range
// keeps what there was).
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::Keep(void)
{
	Ptr<CIDESettings>	pSettings;

	pSettings = mpcContent->GetSettings();
	mpcContent->GetStructure()->SetEditorSettings(mpBreadcrumbs->IsTicked(), mpStickyLines->IsTicked());
	pSettings->SetKeyRepeat(mpKeyRepeat->IsTicked(),
							ReadNumber(mpRepeatDelay->GetText(), IDE_KEY_REPEAT_MIN_DELAY, IDE_KEY_REPEAT_MAX_DELAY, pSettings->GetKeyRepeatDelay()),
							ReadNumber(mpRepeatRate->GetText(), IDE_KEY_REPEAT_MIN_RATE, IDE_KEY_REPEAT_MAX_RATE, pSettings->GetKeyRepeatRate()));
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE's checks' edits, as if ticked or typed (HCP65_IDE_EDITOR_EDITS):
// "breadcrumbs:false;sticky:true;repeat:true;delay:300;rate:20".
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	size		i;
	char*		sz;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		if (strncmp(sz, "breadcrumbs:", 12) == 0)
		{
			mpBreadcrumbs->SetTicked(strcmp(&sz[12], "true") == 0);
		}
		else if (strncmp(sz, "sticky:", 7) == 0)
		{
			mpStickyLines->SetTicked(strcmp(&sz[7], "true") == 0);
		}
		else if (strncmp(sz, "repeat:", 7) == 0)
		{
			mpKeyRepeat->SetTicked(strcmp(&sz[7], "true") == 0);
		}
		else if (strncmp(sz, "delay:", 6) == 0)
		{
			mpRepeatDelay->SetText(&sz[6]);
		}
		else if (strncmp(sz, "rate:", 5) == 0)
		{
			mpRepeatRate->SetText(&sz[5]);
		}
	}
	aszEdits.Kill();
}


bool CIDEEditorPage::Save(CObjectWriter* pcFile) { return false; }
bool CIDEEditorPage::Load(CObjectReader* pcFile) { return false; }
const char* CIDEEditorPage::GetTitle(void) const { return "Editor"; }
Ptr<CComponent> CIDEEditorPage::GetBody(void) const { return mpBody; }
Ptr<CComponent> CIDEEditorPage::GetFirstEditor(void) const { return mpBreadcrumbs; }

