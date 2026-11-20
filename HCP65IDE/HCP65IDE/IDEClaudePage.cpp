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
#include "WindowLib/FillContainer.h"
#include "IDEContent.h"
#include "IDEClaudePage.h"


#define IDE_CLAUDE_PAGE_GAP		8


//////////////////////////////////////////////////////////////////////////
//
// The page shows where Claude is started as it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePage::Init(CIDEContent* pcContent, Ptr<CDialog> pDialog)
{
	Ptr<CWindow>		pWindow;
	Ptr<CFillContainer>	pGap;
	Ptr<CIDEAskClaude>	pAskClaude;
	SDialogLook*		psLook;
	size				i;

	PreInit();
	CSettingsPage::Init();
	mpcContent = pcContent;
	mpDialog = pDialog;
	pWindow = pDialog->GetWindow();
	psLook = pDialog->GetLook();
	pAskClaude = pcContent->GetAskClaude();

	mpDirectory = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	for (i = 0; i < ICD_NumDirectories; i++)
	{
		mpDirectory->AddItem(GetIDEClaudeDirectoryText((EIDEClaudeDirectory)i));
	}
	mpDirectory->Select((int32)pAskClaude->GetDirectorySetting());
	mpDirectory->SetAlignment(CSH_Stretch, CSV_Center);
	mpFolder = OMalloc<CTextField>(pWindow, psLook->pcTheme, pAskClaude->GetFolderSetting());
	mpFolder->SetAlignment(CSH_Stretch, CSV_Center);

	mpBody = OMalloc<CGridContainer>(pWindow);
	mpBody->SetAlignment(CSH_Stretch, CSV_Top);
	mpBody->AddComponent(CreateLabel("Claude Code is started in:"), 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_CLAUDE_PAGE_GAP, 1);
	mpBody->AddComponent(pGap, 1, 0);
	mpBody->AddComponent(mpDirectory, 2, 0, 1, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Folder:"), 0, 1);
	mpBody->AddComponent(mpFolder, 2, 1, 1, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Folder is for \"The folder below\"; one that isn't a full path is from the startup solution's folder."), 0, 2, 3, 1, 1, 0);
	mpBody->AddComponent(CreateLabel("Wherever it is started, Claude can also work in the repositories of the open solutions' projects."), 0, 3, 3, 1, 1, 0);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePage::Class(void)
{
	CSettingsPage::Class();
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpDirectory);
	M_Pointer(mpFolder);
	M_Pointer(mpBody);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePage::Free(void)
{
	CSettingsPage::Free();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CLabel> CIDEClaudePage::CreateLabel(const char* szText)
{
	Ptr<CLabel>	pLabel;

	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szText);
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	return pLabel;
}


//////////////////////////////////////////////////////////////////////////
//
// OK: where Claude is started, and the folder typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePage::Keep(void)
{
	int32	iSelected;

	iSelected = mpDirectory->GetSelected();
	if ((iSelected < 0) || (iSelected >= ICD_NumDirectories))
	{
		iSelected = ICD_Solution;
	}
	mpcContent->GetAskClaude()->SetDirectorySettings((EIDEClaudeDirectory)iSelected, mpFolder->GetText());
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE's checks' edits, as if chosen and typed (HCP65_IDE_CLAUDE_EDITS):
// "directory:Folder;folder:/home/me/Code", the directory by its name in
// the settings file (Solution, Repository or Folder).
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePage::ApplyEdits(const char* szEdits)
{
	CArrayChars			aszEdits;
	size				i;
	char*				sz;
	EIDEClaudeDirectory	eDirectory;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		if (strncmp(sz, "directory:", 10) == 0)
		{
			if (FindIDEClaudeDirectory(&sz[10], &eDirectory))
			{
				mpDirectory->Select((int32)eDirectory);
			}
		}
		else if (strncmp(sz, "folder:", 7) == 0)
		{
			mpFolder->SetText(&sz[7]);
		}
	}
	aszEdits.Kill();
}


bool CIDEClaudePage::Save(CObjectWriter* pcFile) { return false; }
bool CIDEClaudePage::Load(CObjectReader* pcFile) { return false; }
const char* CIDEClaudePage::GetTitle(void) const { return "Claude"; }
Ptr<CComponent> CIDEClaudePage::GetBody(void) const { return mpBody; }
Ptr<CComponent> CIDEClaudePage::GetFirstEditor(void) const { return mpDirectory; }

