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
#ifndef __IDE_RENAME_H__
#define __IDE_RENAME_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/TextField.h"
#include "WindowLib/Label.h"
#include "IDECodeModel.h"


class CIDEContent;


//IntelliJ's Rename (Shift+F6): the symbol at the caret (or the name the selection is in) renamed everywhere the index
//has it, a class's constructors and destructor too, in each file's editor as one edit and across the files as one
//undo group.  The new name is asked for in a dialog; nothing changes if a file changed since it was indexed.
class CIDERename : public CObject
{
CONSTRUCTABLE(CIDERename);
DESTRUCTABLE(CIDERename);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpRenameDialog;  //While Rename is shown.
	Ptr<CTextField>		mpRenameField;
	Ptr<CLabel>			mpRenameError;
	SIDERenameTarget	msRenameTarget;  //What Rename's dialog is renaming: its key and name.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	FindRenameTarget(SIDERenameTarget* psDest);
	void	QueueRename(void);
	void	ShowQueuedRename(void);
	bool	ShowRename(void);
	bool	ApproveRename(void);
	bool	RenameSymbol(SIDERenameTarget* psTarget, const char* szNewName);

protected:
};


bool IsIDEIdentifier(const char* szName);
size FindIDENameInKey(const char* szKey, const char* szName);


#endif // __IDE_RENAME_H__

