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
#ifndef __IDE_SCRIPT_H__
#define __IDE_SCRIPT_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"


class CIDEContent;


//How the IDE is driven to check it, from the command line's environment and the debug script, all in one place:
//
//  HCP65_IDE_EDIT        a comma separated list of edits (RunEditStep): open:<file>, type:<text>, undo, redo, undoall,
//                        redoall, group, endgroup, menu, explorer, renamable, rename:<name>, usages and print.
//  HCP65_IDE_GOTO        <tab>:<pattern>, Go to's results printed and the first opened (RunGoTo).
//  HCP65_IDE_DOCK        a comma separated list of dock moves: <action>:<pane ID> or reset (RunDockMove).
//  HCP65_IDE_DEBUG       the debug script: the debugger's commands (CIDEContent::RunDebugScript) and these, the IDE's
//                        (RunCommand): goto:<tab>[:<pattern>], rename, closetab, closeall, exit, gotosource,
//                        switchfile, printtabs, tabmenu and contextmenu.
//
//A dialog the IDE shows (Go to, Rename, Save Changes) is watched (WatchDialog): with HCP65_IDE_DIALOG_SNAPSHOT set it is
//saved to that PNG once drawn, and with HCP65_IDE_DIALOG_ANSWER (or the snapshot) it is answered with that button's
//caption, Cancel if none is given.
class CIDEScript : public CObject
{
CONSTRUCTABLE(CIDEScript);
DESTRUCTABLE(CIDEScript);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds the script.
	Ptr<CDialog>	mpDialog;  //The dialog being watched.
	int32			miDialogTicks;
	size			muiSpans;  //The active editor's spans when printspans last looked.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	IsCommand(const char* szCommand);
	void	RunCommand(const char* szCommand);
	void	PrintMenu(const char* szName, Ptr<CMenuContainer> pMenu);
	void	ClickMenu(const char* szPath);
	void	RunEditStep(const char* szStep);
	void	RunGoTo(const char* szGoTo);
	void	RunDockMove(const char* szMove);

	void	WatchDialog(Ptr<CDialog> pDialog);
	void	StopWatchingDialog(void);
	void	DialogTick(void);
};


#endif // __IDE_SCRIPT_H__

