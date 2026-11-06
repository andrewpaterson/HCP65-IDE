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
#ifndef __IDE_CLOSE_GUARD_H__
#define __IDE_CLOSE_GUARD_H__
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/DockPane.h"


class CIDEContent;


//What is done once the files with changes have been saved, or their changes dropped (CIDEContent::AskToSave).
enum EIDEAfterSave
{
	IAS_ClosePanes,		//The documents asked to close.
	IAS_CloseSolution,	//muiAfterSolution closed.
	IAS_OpenSolution,	//mszAfterFile opened instead.
	IAS_Exit,
};


//Asks to save files with changes before they close, as Visual Studio's Save Changes: a document closing (PaneClosing),
//a solution closing or another opening (ApproveLettingGo), and the IDE exiting (RequestExit, the window's close
//approver).  The asking is done at the next tick (a dialog can't be shown from the input that closes them), then what
//was asked for.
class CIDECloseGuard : public CObject
{
CONSTRUCTABLE(CIDECloseGuard);
DESTRUCTABLE(CIDECloseGuard);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds the guard.
	int32			miAfterSave;  //EIDEAfterSave asked to save before, -1 for nothing.
	bool			mbCloseConfirmed;  //Saved or dropped: closing doesn't ask (for good while exiting).
	CArrayChars		maszSaveFiles;  //The files with changes asked about.
	CArrayChars		maszCloseIDs;  //The panes to close then (IAS_ClosePanes).
	size			muiAfterSolution;
	CChars			mszAfterFile;
	Ptr<CDialog>	mpSaveDialog;  //While it's shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	PaneClosing(CDockPane* pcPane);
	bool	RequestExit(void);
	bool	ApproveLettingGo(size uiSolution, const char* szFile);
	void	AskToSave(void);

protected:
	bool	FindUnsavedFiles(size uiSolution, CArrayChars* paszDest);
	void	QueueAskToSave(EIDEAfterSave eAfter);
};


#endif // __IDE_CLOSE_GUARD_H__

