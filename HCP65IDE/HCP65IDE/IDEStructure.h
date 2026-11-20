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
#ifndef __IDE_STRUCTURE_H__
#define __IDE_STRUCTURE_H__
#include "StandardLib/Object.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/PromptPopup.h"
#include "IDEChooser.h"
#include "WindowLib/DockManager.h"
#include "IDEOutline.h"
#include "IDEStructurePane.h"


class CIDEContent;
class CIDEEditorTab;
class CMarkupTag;
class CXMLWriter;


//Getting about a file by what it declares (NavigationPlan.txt's stage B), as IntelliJ IDEA's: File Structure
//(Control+F12), a chooser of the active file's outline, nested, each row its kind's icon, its name with its parameters
//and its line, filtered as it is typed in, the entry the caret is in selected to begin with; and Next and Previous
//Method (Alt+Down and Alt+Up), the caret put on the name of the next function the file declares or defines after
//the caret, or the last before it.  They read the active tab's outline (CIDEOutline, parsed now if the file was
//edited since).  And Go to Line (Control+G): a prompt over the editor, "Line[:column]" with the caret's place in it,
//that puts the caret there.
//
//The Structure tool window (Alt+7, tabbed with Solution Explorer) is the outline as a tree that follows the caret
//(CIDEStructurePane).
//
//It also keeps each editor's breadcrumbs (CIDEBreadcrumbs) and sticky lines (CIDEStickyLines) up to date as the
//window ticks (Poll), and has the settings that say whether editors show them (both, until turned off).
class CIDEStructure : public CObject
{
CONSTRUCTABLE(CIDEStructure);
DESTRUCTABLE(CIDEStructure);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CIDEChooser>	mpChooser;  //File Structure's, made when it is first shown.
	Ptr<CPromptPopup>	mpPrompt;  //Go to Line's, made when it is first shown.
	bool				mbBreadcrumbs;  //The settings': an editor shows what its caret is in, under its code.
	bool				mbStickyLines;  //And keeps the first lines of what the top of its view is in.
	Ptr<CIDEStructurePane>	mpPane;  //The Structure tool window's tree.
	Ptr<CDockPane>		mpTool;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	CreatePane(SDockPlace* psPlace);
	bool	ShowStructureWindow(void);
	void	SetStructureSettings(bool bSortByName, bool bShowFields);
	Ptr<CIDEStructurePane>	GetStructurePane(void) const;
	Ptr<CDockPane>	GetTool(void) const;
	bool	ShowSiblings(Ptr<CIDEEditorTab> pTab, int32 iEntry, SInt32Vec2 sPosition);
	Ptr<CIDEChooser>	GetChooser(void) const;
	void	AddNavigateItems(Ptr<CMenuContainer> pMenu);
	bool	ShowFileStructure(void);
	bool	NextMethod(void);
	bool	PreviousMethod(void);
	bool	ShowGoToLine(void);
	bool	GoToLine(const char* szPlace);
	Ptr<CPromptPopup>	GetPrompt(void) const;
	void	Poll(void);
	bool	IsBreadcrumbs(void) const;
	bool	IsStickyLines(void) const;
	void	SetEditorSettings(bool bBreadcrumbs, bool bStickyLines);
	void	ReadSettings(CMarkupTag* pcSettings);
	void	WriteSettings(CXMLWriter* pcWriter);
	bool	HasEditor(void);
	bool	GoToEntry(Ptr<CIDEEditorTab> pTab, SIDEOutlineEntry* psEntry, bool bSelectName);
	uint8	GetEntrySymbolKind(SIDEOutlineEntry* psEntry);
	void	GetEntryLocation(Ptr<CIDEEditorTab> pTab, SIDEOutlineEntry* psEntry, SIDELocation* psDest);

protected:
	bool	StepMethod(bool bNext);
	bool	OpenOutlineChooser(Ptr<CIDEEditorTab> pTab, CIDEOutline* pcOutline, const char* szTitle, int32 iSiblingsOf, int32 iSelectedEntry, SInt32Vec2 sPosition, bool bAbove);
	void	LineAccepted(void);
};


bool ParseIDELinePlace(const char* szPlace, size* puiLine, size* puiColumn);


//A place in a file's text, from 0, and the places a command steps the caret through, in the file's order (methods'
//names; changed lines, bookmarks and highlighted usages use it too).
struct SIDEStepPlace
{
	int32	iLine;
	int32	iColumn;
	int32	iEndColumn;  //Where what is at the place ends on its line: a caret in it is at it, not after it.
};
typedef CArrayTemplate<SIDEStepPlace>	CArrayIDEStepPlaces;


int32 FindIDEStepPlace(CArrayIDEStepPlaces* pasPlaces, int32 iLine, int32 iColumn, bool bNext, bool bWrap);


#endif // __IDE_STRUCTURE_H__

