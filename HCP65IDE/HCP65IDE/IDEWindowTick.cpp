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
#include "SupportLib/PNGWriter.h"
#include "WindowLib/Window.h"
#include "IDEWindowTick.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEWindowTick::Init(void)
{
	PreInit();

	CWindowTick::Init();
	mszSnapshot.Init();
	miSnapshotTicks = -1;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEWindowTick::Class(void)
{
	CWindowTick::Class();
	U_String(mszSnapshot);
	U_Int32(miSnapshotTicks);
	M_Pointer(mpContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEWindowTick::Free(void)
{
	mszSnapshot.Kill();
	CWindowTick::Free();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEWindowTick::SetSnapshot(const char* szFilename, int iTicks)
{
	mszSnapshot.Set(szFilename);
	miSnapshotTicks = iTicks;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEWindowTick::Tick(Ptr<CWindow> pWindow, int64 iUpdateTimeInMillieconds, int64 iTotalTimeInMillieconds)
{
	if (mpContent.IsNotNull())
	{
		mpContent->Tick(pWindow);
	}
	if ((miSnapshotTicks >= 0) && mpContent.IsNotNull() && (mpContent->GetDebugging()->IsDebugScriptRunning() || mpContent->GetAskClaude()->IsClaudeRunning()))
	{
		//The snapshot is of the window once the debugger, or Claude, has done what it was asked to.
		pWindow->RequestTick(20);
	}
	else if (miSnapshotTicks >= 0)
	{
		if (miSnapshotTicks == 0)
		{
			SavePNG(pWindow->GetCanvas()->GetImageOrNull(), mszSnapshot.Text());
			pWindow->Stop();
		}
		else
		{
			//The window would otherwise wait for input before the next tick.
			pWindow->Invalidate();
			pWindow->RequestTick(0);
		}
		miSnapshotTicks--;
	}
}



//////////////////////////////////////////////////////////////////////////
//
// The content is ticked with the window.
//
//////////////////////////////////////////////////////////////////////////
void CIDEWindowTick::SetContent(Ptr<CIDEContent> pContent)
{
	mpContent = pContent;
}
