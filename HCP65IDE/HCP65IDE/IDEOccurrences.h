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
#ifndef __IDE_OCCURRENCES_H__
#define __IDE_OCCURRENCES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/CommandSet.h"
#include "IDESteppable.h"


#define IDE_NUM_STEPPABLES	2


class CIDEContent;
//Next and Previous Occurrence (Control+Alt+Down and Up), as IntelliJ IDEA's: the steppable tool window with the keys
//steps, keeping them; from a source editor, the one steppable tool window that can be seen and has something to step
//to steps, and the editor it opens has the keys.  None, or more than one, and the status line says so.
class CIDEOccurrences : public CObject
{
CONSTRUCTABLE(CIDEOccurrences);
DESTRUCTABLE(CIDEOccurrences);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.

public:
	void			Init(CIDEContent* pcContent);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			AddCommands(Ptr<CCommandSet> pCommands);
	CIDESteppable*	FindSteppable(bool* pbKeepKeys, bool bSay);
	CIDESteppable*	FindFocussedSteppable(void);

protected:
	void			NextOccurrence(void);
	void			PreviousOccurrence(void);
	void			Step(bool bNext);
	bool			CanStep(void);
	size			GetSteppables(CIDESteppable** apcDest);
	bool			IsInSourceEditor(Ptr<CComponent> pComponent);
};


#endif // __IDE_OCCURRENCES_H__
