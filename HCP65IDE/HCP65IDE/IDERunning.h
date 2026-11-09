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
#ifndef __IDE_RUNNING_H__
#define __IDE_RUNNING_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "ThreadLib/Process.h"
#include "WindowLib/Button.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/Window.h"
#include "IDEBuildPane.h"


class CIDEContent;


//Runs the chosen run configuration's external tool without the debugger, as IntelliJ IDEA's Run does: the toolbar's
//Start (Stop while it runs) and the Run tab, which shows what the tool writes and, along its bottom, that it is running
//(blue) or has exited well (green) or not (red).  The tool is stopped when the IDE closes.
class CIDERunning : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDERunning);
DESTRUCTABLE(CIDERunning);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	CProcess			mcProcess;
	Ptr<CIDEBuildPane>	mpRunPane;
	Ptr<CDockPane>		mpRunTool;
	Ptr<CButton>		mpStartButton;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void			CreatePane(SDockPlace* psPlace);
	void			AddCommands(Ptr<CCommandSet> pCommands);
	Ptr<CButton>	CreateStartButton(void);
	Ptr<CButton>	GetStartButton(void);
	Ptr<CIDEBuildPane>	GetRunPane(void);
	bool			IsRunning(void);
	void			PollRun(Ptr<CWindow> pWindow);

protected:
	bool			StartOrStop(void);
	bool			Start(void);
	bool			Stop(void);
	void			Finished(CArrayChars* paszLines);
	void			ShowMessage(const char* szMessage);
};


#endif // __IDE_RUNNING_H__

