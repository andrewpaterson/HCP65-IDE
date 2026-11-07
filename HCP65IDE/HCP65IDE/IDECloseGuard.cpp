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
#include "BaseLib/FileUtil.h"
#include "SupportLib/PNGWriter.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDECloseGuard.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECloseGuard::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	miAfterSave = -1;
	mbCloseConfirmed = false;
	maszSaveFiles.Init();
	maszCloseIDs.Init();
	muiAfterSolution = 0;
	mszAfterFile.Init();
	mpSaveDialog = NULL;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECloseGuard::Class(void)
{
	U_Pointer(mpcContent);
	U_Int32(miAfterSave);
	U_Bool(mbCloseConfirmed);
	U_Unknown(CArrayChars, maszSaveFiles);
	U_Unknown(CArrayChars, maszCloseIDs);
	U_Size(muiAfterSolution);
	U_String(mszAfterFile);
	M_Pointer(mpSaveDialog);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECloseGuard::Free(void)
{
	mszAfterFile.Kill();
	maszCloseIDs.Kill();
	maszSaveFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDECloseGuard::Save(CObjectWriter* pcFile) { return false; }
bool CIDECloseGuard::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The modified files' names, of solution uiSolution's projects or, with
// MAX_SIZE, of all of them.  False if none is modified.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECloseGuard::FindUnsavedFiles(size uiSolution, CArrayChars* paszDest)
{
	size				i;
	Ptr<CIDEEditorTab>	pTab;
	bool				bFound;

	bFound = false;
	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		pTab = mpcContent->GetDocuments()->GetTab(i);
		if (pTab->IsModified() && ((uiSolution == MAX_SIZE) || mpcContent->GetSolutions()->IsSolutionFile(uiSolution, pTab->GetProject())))
		{
			paszDest->Add(pTab->GetFileName());
			bFound = true;
		}
	}
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The files with changes are asked about at the next tick (a dialog can't
// be shown from the input that closes them), then eAfter is done: closing
// the panes, closing a solution, opening another or exiting.
//
//////////////////////////////////////////////////////////////////////////
void CIDECloseGuard::QueueAskToSave(EIDEAfterSave eAfter)
{
	miAfterSave = eAfter;
	mpcContent->GetWindow()->Post(this, &CIDECloseGuard::AskToSave);
}


//////////////////////////////////////////////////////////////////////////
//
// A document closing: a file with changes isn't closed yet, it is asked
// about first (and closed with any others closed at the same time, Close
// All say).  Anything else closes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECloseGuard::PaneClosing(CDockPane* pcPane)
{
	Ptr<CIDEEditorTab>	pTab;

	if (mbCloseConfirmed)
	{
		return true;
	}
	pTab = mpcContent->GetDocuments()->FindTab(pcPane);
	if (pTab.IsNull() || !pTab->IsModified())
	{
		return true;
	}
	if ((miAfterSave >= 0) && (miAfterSave != IAS_ClosePanes))
	{
		//Already asking, before something that closes it anyway.
		return false;
	}
	maszSaveFiles.AddIfUnique(pTab->GetFileName());
	maszCloseIDs.AddIfUnique((char*)pcPane->GetID());
	QueueAskToSave(IAS_ClosePanes);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// File, Exit and the window's close button: true if the IDE can close
// now; with changes it asks first and closes itself.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECloseGuard::RequestExit(void)
{
	if (mbCloseConfirmed)
	{
		return true;
	}
	maszSaveFiles.ReInit();
	if (!FindUnsavedFiles(MAX_SIZE, &maszSaveFiles))
	{
		return true;
	}
	QueueAskToSave(IAS_Exit);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Before a solution closes (uiSolution) or another is opened (MAX_SIZE,
// szFile): true if nothing it closes has changes, else it is asked about
// and done afterwards.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECloseGuard::ApproveLettingGo(size uiSolution, const char* szFile)
{
	if (mbCloseConfirmed)
	{
		return true;
	}
	maszSaveFiles.ReInit();
	if (!FindUnsavedFiles(((uiSolution == MAX_SIZE) || (mpcContent->GetWorkspace()->NumSolutions() == 1)) ? MAX_SIZE : uiSolution, &maszSaveFiles))
	{
		return true;
	}
	muiAfterSolution = uiSolution;
	mszAfterFile.Set(szFile ? szFile : "");
	QueueAskToSave((uiSolution == MAX_SIZE) ? IAS_OpenSolution : IAS_CloseSolution);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Visual Studio's Save Changes: the files with changes listed; Save saves
// them, Don't Save drops their changes, and either then does what was
// asked for (closes them, the solution or the IDE); Cancel does nothing.
//
//////////////////////////////////////////////////////////////////////////
void CIDECloseGuard::AskToSave(void)
{
	CChars				szName;
	CFileUtil			cFileUtil;
	size				i;
	CChars				szClicked;
	bool				bSave;
	bool				bSaved;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CDockPane>		pPane;

	if (miAfterSave < 0)
	{
		return;
	}

	mpSaveDialog = OMalloc<CMessageDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Save Changes", "Save changes to the following files?");
	for (i = 0; i < maszSaveFiles.NumElements(); i++)
	{
		szName.Init();
		cFileUtil.GetFileName(&szName, maszSaveFiles.GetText(i));
		mpSaveDialog->AddItem(szName.Text());
		szName.Kill();
	}
	mpSaveDialog->GetDialog()->SetOKCaption("Save");
	mpSaveDialog->GetDialog()->AddExtraButton("Don't Save");
	mpcContent->GetScript()->WatchDialog(mpSaveDialog->GetDialog());
	bSave = mpSaveDialog->Show();
	szClicked.Init(mpSaveDialog->GetClicked());
	mpcContent->GetScript()->StopWatchingDialog();
	mpSaveDialog = NULL;

	if (bSave || szClicked.Equals("Don't Save"))
	{
		bSaved = true;
		for (i = 0; bSave && (i < maszSaveFiles.NumElements()); i++)
		{
			pTab = mpcContent->GetDocuments()->FindTab(maszSaveFiles.GetText(i));
			if (pTab.IsNotNull() && !mpcContent->GetDocuments()->SaveFile(pTab))
			{
				bSaved = false;
			}
		}

		//What was asked for, now without asking.
		if (bSaved)
		{
			mbCloseConfirmed = true;
			switch (miAfterSave)
			{
			case IAS_ClosePanes:
				for (i = 0; i < maszCloseIDs.NumElements(); i++)
				{
					pPane = mpcContent->GetDock()->GetPane(maszCloseIDs.GetText(i));
					if (pPane.IsNotNull())
					{
						mpcContent->GetDock()->Close(pPane);
					}
				}
				break;
			case IAS_CloseSolution:
				mpcContent->GetSolutions()->CloseSolution(muiAfterSolution);
				break;
			case IAS_OpenSolution:
				if (!mpcContent->GetSolutions()->OpenSolution(mszAfterFile.Text()))
				{
					gcLogger.Error2(__METHOD__, " Could not open [", mszAfterFile.Text(), "].", NULL);
				}
				break;
			case IAS_Exit:
				mpcContent->GetWindow()->Stop();
				break;
			}

			//Exiting, everything closes without asking again.
			mbCloseConfirmed = (miAfterSave == IAS_Exit);
		}
		else
		{
			mpcContent->ShowStatus("A file couldn't be saved: nothing was closed.");
		}
	}
	szClicked.Kill();
	miAfterSave = -1;
	maszSaveFiles.ReInit();
	maszCloseIDs.ReInit();
}
