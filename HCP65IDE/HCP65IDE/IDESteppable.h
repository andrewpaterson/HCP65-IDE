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
#ifndef __IDE_STEPPABLE_H__
#define __IDE_STEPPABLE_H__
#include "StandardLib/Pointer.h"
#include "WindowLib/DockPane.h"


//A tool window whose items are places in the source that Next and Previous Occurrence (Control+Alt+Down and Up, see
//CIDEOccurrences) step through, as IntelliJ IDEA's Find and Build windows are: Find Usages' usages, Build's messages.
class CIDESteppable
{
public:
	virtual Ptr<CDockPane>	GetSteppablePane(void) =0;
	virtual const char*		GetSteppableName(void) =0;  //As the status line names it: "Find Usages".
	virtual bool			HasSteps(void) =0;  //Whether it has anything to step to.

	//The next (or previous) item after the current one becomes current and its place is opened in its editor.  With
	//bKeepKeys the pane keeps the keys (it was stepped in); without, the editor opened has them.  False, with the status
	//line saying so, if there is nothing after (or before) it.
	virtual bool			Step(bool bNext, bool bKeepKeys) =0;
};


#endif // __IDE_STEPPABLE_H__
