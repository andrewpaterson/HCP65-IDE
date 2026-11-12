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
#include "WindowLib/Clipboard.h"
#include "IDEContent.h"
#include "IDEPasteHistory.h"


//////////////////////////////////////////////////////////////////////////
//
// It is the chooser of the window's clipboard's history from now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEPasteHistory::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpDialog = NULL;
	PostInit();
	GetHistory()->SetChooser(this);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEPasteHistory::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEPasteHistory::Free(void)
{
	if (mpcContent && mpcContent->GetWindow().IsNotNull())
	{
		GetHistory()->SetChooser(NULL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What is shown is for the session only.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEPasteHistory::Save(CObjectWriter* pcFile) { return false; }
bool CIDEPasteHistory::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Choose Content to Paste shown over pOwner.  The dialog a check was
// watching, if the keys were pressed in one, isn't watched again after:
// whatever showed it does that.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEPasteHistory::ChooseContent(Ptr<CWindow> pOwner, Ptr<CClipboardHistory> pHistory, CChars* pszDest)
{
	bool	bChosen;

	if (mpDialog.IsNull())
	{
		mpDialog = OMalloc<CPasteHistoryDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), mpcContent->GetCodeFamily());
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	bChosen = mpDialog->ChooseContent(pOwner, pHistory, pszDest);
	mpcContent->GetScript()->StopWatchingDialog();
	return bChosen;
}


Ptr<CPasteHistoryDialog> CIDEPasteHistory::GetDialog(void) { return mpDialog; }
Ptr<CClipboardHistory> CIDEPasteHistory::GetHistory(void) { return mpcContent->GetWindow()->GetClipboard()->GetHistory(); }

