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
#ifndef __IDE_WINDOW_TICK_H__
#define __IDE_WINDOW_TICK_H__
#include "BaseLib/Chars.h"
#include "WindowLib/WindowTick.h"
#include "IDEContent.h"


//The IDE's window tick: the content's (so it can colour edited text again), and with a snapshot set it saves the
//window as a PNG after that many ticks and closes it.
class CIDEWindowTick : public CWindowTick
{
CONSTRUCTABLE(CIDEWindowTick);
DESTRUCTABLE(CIDEWindowTick);
protected:
	CChars				mszSnapshot;
	int					miSnapshotTicks;
	Ptr<CIDEContent>	mpContent;

public:
	void	Init(void);
	void	Class(void) override;
	void	Free(void) override;
	void	SetSnapshot(const char* szFilename, int iTicks);
	void	SetContent(Ptr<CIDEContent> pContent);
	void	Tick(Ptr<CWindow> pWindow, int64 iUpdateTimeInMillieconds, int64 iTotalTimeInMillieconds) override;
};


#endif // __IDE_WINDOW_TICK_H__

