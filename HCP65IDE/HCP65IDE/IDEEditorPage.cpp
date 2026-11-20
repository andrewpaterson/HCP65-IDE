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
#include "BaseLib/ArrayChars.h"
#include "IDEContent.h"
#include "IDEEditorPage.h"


//////////////////////////////////////////////////////////////////////////
//
// The page shows the settings as they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::Init(CIDEContent* pcContent, Ptr<CDialog> pDialog)
{
	Ptr<CWindow>	pWindow;
	SDialogLook*	psLook;

	PreInit();
	CSettingsPage::Init();
	mpcContent = pcContent;
	pWindow = pDialog->GetWindow();
	psLook = pDialog->GetLook();

	mpBreadcrumbs = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Show breadcrumbs: what the caret is in, under the code", pcContent->GetStructure()->IsBreadcrumbs());
	mpBreadcrumbs->SetAlignment(CSH_Left, CSV_Center);
	mpStickyLines = OMalloc<CCheckBox>(pWindow, psLook->pcTheme, "Show sticky lines: the first lines of what the top of the view is in", pcContent->GetStructure()->IsStickyLines());
	mpStickyLines->SetAlignment(CSH_Left, CSV_Center);

	mpBody = OMalloc<CGridContainer>(pWindow);
	mpBody->SetAlignment(CSH_Stretch, CSV_Top);
	mpBody->AddComponent(mpBreadcrumbs, 0, 0, 1, 1, 1, 0);
	mpBody->AddComponent(mpStickyLines, 0, 1, 1, 1, 1, 0);
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
// OK: what is ticked.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEditorPage::Keep(void)
{
	mpcContent->GetStructure()->SetEditorSettings(mpBreadcrumbs->IsTicked(), mpStickyLines->IsTicked());
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE's checks' edits, as if ticked (HCP65_IDE_EDITOR_EDITS):
// "breadcrumbs:false;sticky:true".
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
	}
	aszEdits.Kill();
}


bool CIDEEditorPage::Save(CObjectWriter* pcFile) { return false; }
bool CIDEEditorPage::Load(CObjectReader* pcFile) { return false; }
const char* CIDEEditorPage::GetTitle(void) const { return "Editor"; }
Ptr<CComponent> CIDEEditorPage::GetBody(void) const { return mpBody; }
Ptr<CComponent> CIDEEditorPage::GetFirstEditor(void) const { return mpBreadcrumbs; }

