/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_TOOL_WINDOWS_H__
#define __IDE_TOOL_WINDOWS_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "StandardLib/Object.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/DockPane.h"
#include "WindowLib/MenuContainer.h"


class CIDEContent;


//A tool pane Hide All Tool Windows hid: where it was docked then, which a pane shown and hidden again since has lost,
//and whether it was its group's tab in front.
struct SIDEHiddenToolWindow
{
	CChars			szID;
	SDockRestore	sRestore;
	bool			bFront;
};


typedef CArrayTemplate<SIDEHiddenToolWindow>	CArrayIDEHiddenToolWindows;


//The tool windows' keys (NavigationPlan.txt's follow-up 3e), as IntelliJ IDEA's:
//
//Alt and a digit shows a tool window and gives it the keys; pressed while that window has the keys, it hides it and
//the editor has them again.  2 Bookmarks, 3 Find Usages, 4 Run, 5 Debug, 6 Build, 9 Find, and Structure's 7 and
//Hierarchy's 8 (their own commands, which ask here whether to hide).  Solution Explorer's Alt+1 is its own and doesn't
//hide: it selects the active file.  Duplicates, Indexing and Claude have no keys.  View has them all, each with its
//keys beside its name.
//
//Hide All Tool Windows (Control+Shift+F12) hides every docked tool window, so the editor has the window's room, and
//pressed again with none shown puts back the ones it hid, as they were: their tabs' order and the tab in front.
//What it hid is the session's, and is kept until it is put back: a window shown by itself in between and hidden by
//the command again doesn't take the others' place.  A floating tool window and one auto hidden to the edge are left:
//neither takes the editor's room, and a floating one hidden would come back docked.
//
//Jump to Last Tool Window (no key of its own: IntelliJ IDEA's F12 is Go to Definition here) shows the tool window
//that last had the keys, one hidden since too, and gives them back to it.
class CIDEToolWindows : public CObject
{
CONSTRUCTABLE(CIDEToolWindows);
DESTRUCTABLE(CIDEToolWindows);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	CArrayIDEHiddenToolWindows	masHidden;  //Hide All's: the panes it hid, in the order they were hidden.
	CChars						mszLast;  //The tool pane that last had the keys (its ID), empty for none.

public:
	void		Init(CIDEContent* pcContent);
	void		Class(void) override;
	void		Free(void) override;
	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	void		AddCommands(Ptr<CCommandSet> pCommands);
	void		AddViewItems(Ptr<CMenuContainer> pMenu);
	const char*	GetPaneKeys(const char* szID);

	bool		HasKeys(Ptr<CDockPane> pPane);
	bool		Toggle(const char* szID);
	bool		HideWithKeys(const char* szID);
	bool		HideAll(void);
	bool		JumpToLast(void);
	bool		HasLast(void);
	void		Poll(void);

	void		Print(CChars* pszDest);

protected:
	bool		IsHidden(const char* szID);
	void		ForgetHidden(void);
	bool		ToggleBookmarks(void);
	bool		ToggleUsages(void);
	bool		ToggleRun(void);
	bool		ToggleDebug(void);
	bool		ToggleBuild(void);
	bool		ToggleFind(void);
};


#endif // __IDE_TOOL_WINDOWS_H__
