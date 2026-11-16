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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "WindowLib/FillContainer.h"
#include "IDEContent.h"
#include "IDEDuplicatesPage.h"


#define IDE_DUPLICATES_PAGE_GAP		8


//////////////////////////////////////////////////////////////////////////
//
// The page shows Find Duplicates' settings as they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicatesPage::Init(CIDEContent* pcContent, Ptr<CDialog> pDialog)
{
	Ptr<CWindow>		pWindow;
	Ptr<CFillContainer>	pGap;
	Ptr<CIDEDuplicates>	pDuplicates;
	SDialogLook*		psLook;
	CChars				szNumber;

	PreInit();
	CSettingsPage::Init();
	mpcContent = pcContent;
	mpDialog = pDialog;
	pWindow = pDialog->GetWindow();
	psLook = pDialog->GetLook();
	pDuplicates = pcContent->GetDuplicates();

	szNumber.Init();
	szNumber.Append((uint32)pDuplicates->GetMinimumStatements());
	mpStatements = OMalloc<CTextField>(pWindow, psLook->pcTheme, szNumber.Text());
	mpStatements->SetAlignment(CSH_Stretch, CSV_Center);
	szNumber.Clear();
	szNumber.Append((uint32)pDuplicates->GetMinimumTokens());
	mpTokens = OMalloc<CTextField>(pWindow, psLook->pcTheme, szNumber.Text());
	mpTokens->SetAlignment(CSH_Stretch, CSV_Center);
	szNumber.Kill();
	mpLiterals = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Literals may differ");
	mpLiterals->SetAlignment(CSH_Left, CSV_Center);
	mpLiterals->SetTicked(pDuplicates->IsLiteralsFree());

	mpBody = OMalloc<CGridContainer>(pWindow);
	mpBody->SetAlignment(CSH_Stretch, CSV_Top);
	mpBody->AddComponent(CreateLabel("Smallest run of a function's statements listed:"), 0, 0, 3, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Statements (and those inside them):"), 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_DUPLICATES_PAGE_GAP, 1);
	mpBody->AddComponent(pGap, 1, 1);
	mpBody->AddComponent(mpStatements, 2, 1, 1, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Tokens:"), 0, 2);
	mpBody->AddComponent(mpTokens, 2, 2, 1, 1, 1, 0);
	mpBody->AddComponent(mpLiterals, 0, 3, 3, 1, 1, 0);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicatesPage::Class(void)
{
	CSettingsPage::Class();
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpStatements);
	M_Pointer(mpTokens);
	M_Pointer(mpLiterals);
	M_Pointer(mpBody);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicatesPage::Free(void)
{
	CSettingsPage::Free();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CLabel> CIDEDuplicatesPage::CreateLabel(const char* szText)
{
	Ptr<CLabel>	pLabel;

	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szText);
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	return pLabel;
}


//////////////////////////////////////////////////////////////////////////
//
// OK: the minimums (a number that isn't one keeps what there was) and
// the tick.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicatesPage::Keep(void)
{
	Ptr<CIDEDuplicates>	pDuplicates;
	int					iStatements;
	int					iTokens;

	pDuplicates = mpcContent->GetDuplicates();
	iStatements = atoi(mpStatements->GetText());
	iTokens = atoi(mpTokens->GetText());
	pDuplicates->SetSettings(pDuplicates->GetScope(), pDuplicates->GetDirectory(), mpLiterals->IsTicked());
	pDuplicates->SetMinimums((iStatements > 0) ? (size)iStatements : pDuplicates->GetMinimumStatements(), (iTokens > 0) ? (size)iTokens : pDuplicates->GetMinimumTokens());
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE's checks' edits, as if typed (HCP65_IDE_DUPLICATES_EDITS):
// "statements:4;tokens:10;literals:on".
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicatesPage::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	size		i;
	char*		sz;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		if (strncmp(sz, "statements:", 11) == 0)
		{
			mpStatements->SetText(&sz[11]);
		}
		else if (strncmp(sz, "tokens:", 7) == 0)
		{
			mpTokens->SetText(&sz[7]);
		}
		else if (strncmp(sz, "literals:", 9) == 0)
		{
			mpLiterals->SetTicked(strcmp(&sz[9], "on") == 0);
		}
	}
	aszEdits.Kill();
}


bool CIDEDuplicatesPage::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDuplicatesPage::Load(CObjectReader* pcFile) { return false; }
const char* CIDEDuplicatesPage::GetTitle(void) { return "Find Duplicates"; }
Ptr<CComponent> CIDEDuplicatesPage::GetBody(void) { return mpBody; }
Ptr<CComponent> CIDEDuplicatesPage::GetFirstEditor(void) { return mpStatements; }

