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
#ifndef __IDE_BREADCRUMBS_H__
#define __IDE_BREADCRUMBS_H__
#include "BaseLib/ArrayInt.h"
#include "StandardLib/Object.h"
#include "WindowLib/Button.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/TextArea.h"
#include "IDEOutline.h"


class CIDEContent;
class CIDEEditorTab;


//An editor tab's breadcrumbs, as IntelliJ IDEA's: a strip under the code with what the caret is in, outermost first,
//from the tab's outline ("CShape > Move"): its namespace, its class, its function.  A press on one puts the caret on
//its name and opens a chooser of what is beside it (CIDEStructure::ShowSiblings).  They follow the caret, and the outline as the edits thread parses it again (Update, at each tick of the
//active tab): they are made again only when what the caret is in has changed.
class CIDEBreadcrumbs : public CObject
{
CONSTRUCTABLE(CIDEBreadcrumbs);
DESTRUCTABLE(CIDEBreadcrumbs);
protected:
	CIDEContent*		mpcContent;  //Not held.
	CIDEEditorTab*		mpcTab;  //Not held: the tab holds this.
	Ptr<CGridContainer>	mpRow;  //What the tab's lower strip holds.
	CArray<CButton>		maButtons;  //One for each entry, outermost first.
	CArrayInt			maiEntries;  //The outline's entry each is of.
	size				muiShownChanges;  //The outline's change count when they were made, MAX_SIZE for never.
	int32				miShownEntry;  //The entry the caret was in then.

public:
	void			Init(CIDEContent* pcContent, CIDEEditorTab* pcTab);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			Update(void);
	Ptr<CComponent>	GetComponent(void) const;
	size			NumCrumbs(void) const;
	bool			Click(size uiCrumb);
	void			Print(CChars* pszDest);

protected:
	void			RemoveButtons(void);
	void			CrumbClicked(CButton* pcButton);
};


//An editor tab's sticky lines, as IntelliJ IDEA's: while the start of something the top of the view is in (a
//namespace, a class, a function's definition) is scrolled out of sight, its first line is kept at the top of the
//view, over the code, the outermost first and at most IDE_STICKY_LINES of them.  A press on one puts the caret on
//its name.  They follow the scrolling and the outline (Update, at each tick of the active tab).
class CIDEStickyLines : public CObject
{
CONSTRUCTABLE(CIDEStickyLines);
DESTRUCTABLE(CIDEStickyLines);
protected:
	CIDEContent*		mpcContent;  //Not held.
	CIDEEditorTab*		mpcTab;  //Not held: the tab holds this.
	Ptr<CFillContainer>	mpPanel;  //Laid over the top of the tab's view while there are lines to show.
	Ptr<CGridContainer>	mpRows;
	Ptr<CFillContainer>	mpGap;  //As wide as the editor's gutter, so the lines are over their code.
	Ptr<CTextArea>		mpText;
	CArrayInt			maiEntries;  //The outline's entry of each line shown.
	size				muiShownChanges;  //The outline's change count when they were worked out, MAX_SIZE for never.
	int32				miShownTop;  //The first line in view then.
	bool				mbShown;  //The panel is over the view.

public:
	void			Init(CIDEContent* pcContent, CIDEEditorTab* pcTab);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			Update(void);
	void			Hide(void);
	size			NumLines(void) const;
	bool			Click(size uiLine);
	void			Print(CChars* pszDest);

protected:
	void			Pressed(SComponentInputEvent* psEvent);
	int32			GetLineHeight(void);
};


#define IDE_STICKY_LINES	5	//The most lines kept at the top of a view, as IntelliJ's default.


#endif // __IDE_BREADCRUMBS_H__

