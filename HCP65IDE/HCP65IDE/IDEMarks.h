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
#ifndef __IDE_MARKS_H__
#define __IDE_MARKS_H__
#include "StandardLib/Object.h"
#include "WindowLib/CodeEditor.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextArea.h"


class CIDEContent;
class CIDEEditorTab;


//The sources of an editor's scroll bar's marks (CScrollBar::SetMarks) that are the line marks': the changed lines,
//by what changed, and the usages kept highlighted.  (The Find bar's are IDE_FIND_MARKS, the bookmarks'
//IDE_BOOKMARK_MARKS.)
#define IDE_CHANGE_ADDED_MARKS		3
#define IDE_CHANGE_MODIFIED_MARKS	4
#define IDE_CHANGE_REMOVED_MARKS	5
#define IDE_USAGES_MARKS			6

#define IDE_OVERRIDE_ENTRIES		2000	//The most of a file's functions and classes looked up for override marks.
#define IDE_OVERRIDES_UP			'^'		//The gutter's mark of a function that overrides.
#define IDE_OVERRIDES_DOWN			'v'		//Of a function that is overridden, or a class with derived classes.
#define IDE_OVERRIDES_BOTH			'x'		//Of a function that does both.


//What marks an editor's lines beside the bookmarks (NavigationPlan.txt's stage C), as IntelliJ IDEA's:
//
//Next Change and Previous Change (Control+Alt+Shift+Down and Up) put the caret on the next or the previous run of
//lines changed since the last commit (the gutter's change bars).  The changed lines are marked on the scroll bar too,
//in the bars' colours.
//
//Highlight Usages in File (Control+Shift+F7) keeps the usages of the symbol at the caret highlighted in its file (a
//layer of the editor's) while the caret goes elsewhere, marks them on the scroll bar, and says how many there are.
//Find Next and Find Previous (F3, Shift+F3) step through them while no Find bar is open; Escape in the editor, or an
//edit, ends it.
//
//A function that overrides one of a base class has a mark in the gutter's second icon column (^), one that is
//overridden in a derived class has another (v), one that does both a third (x), and so has a class with derived
//classes (v).  A press on a mark goes there: Go to Super Method, Go to Implementation(s), or for both Go to Related
//Symbol.  They are worked out from the file's outline and the index, when both have the file as it is (Poll).
class CIDEMarks : public CObject
{
CONSTRUCTABLE(CIDEMarks);
DESTRUCTABLE(CIDEMarks);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.

	//The scroll bar's marks of the changed lines: the tab they were last made for, and its bars then.
	Ptr<CIDEEditorTab>	mpChangesTab;
	size				muiChangesBars;
	bool				mbChangesDue;  //Its lines weren't laid out.

	//The usages kept highlighted: in which tab, where, and its text's change count then.
	Ptr<CIDEEditorTab>	mpUsagesTab;
	CArrayTextRange		masUsages;
	size				muiUsagesChanges;

	//The override marks: the tab they were last made for, its outline then and the index then.
	Ptr<CIDEEditorTab>	mpOverridesTab;
	size				muiOverridesOutline;
	size				muiOverridesIndexings;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddNavigateItems(Ptr<CMenuContainer> pMenu);
	bool	NextChange(void);
	bool	PreviousChange(void);
	bool	HighlightUsages(void);
	bool	HasKeptUsages(Ptr<CIDEEditorTab> pTab);
	bool	StepUsage(bool bNext);
	void	ClearUsages(void);
	void	EscapeAction(SComponentInputEvent* psEvent);
	void	IconClicked(CCodeEditor* pcEditor, SInt32Vec2 sIcon);
	void	TabClosed(Ptr<CIDEEditorTab> pTab);
	void	Poll(void);
	bool	HasEditor(void);
	size	NumKeptUsages(void) const;

protected:
	bool	StepChange(bool bNext);
	void	ShowChangeMarks(Ptr<CIDEEditorTab> pTab);
	void	ShowOverrides(Ptr<CIDEEditorTab> pTab);
};


#endif // __IDE_MARKS_H__

