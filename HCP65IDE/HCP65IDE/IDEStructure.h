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
#include "IDEOutline.h"


class CIDEContent;
class CIDEEditorTab;


//Getting about a file by what it declares (NavigationPlan.txt's stage B), as IntelliJ IDEA's: File Structure
//(Control+F12), a chooser of the active file's outline, nested, each row its kind's icon, its name with its parameters
//and its line, filtered as it is typed in, the entry the caret is in selected to begin with; and Next and Previous
//Method (Alt+Down and Alt+Up), the caret put on the name of the next function the file declares or defines after
//the caret, or the last before it.  They read the active tab's outline (CIDEOutline, parsed now if the file was
//edited since).  And Go to Line (Control+G): a prompt over the editor, "Line[:column]" with the caret's place in it,
//that puts the caret there.
class CIDEStructure : public CObject
{
CONSTRUCTABLE(CIDEStructure);
DESTRUCTABLE(CIDEStructure);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CIDEChooser>	mpChooser;  //File Structure's, made when it is first shown.
	Ptr<CPromptPopup>	mpPrompt;  //Go to Line's, made when it is first shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddNavigateItems(Ptr<CMenuContainer> pMenu);
	bool	ShowFileStructure(void);
	bool	NextMethod(void);
	bool	PreviousMethod(void);
	bool	ShowGoToLine(void);
	bool	GoToLine(const char* szPlace);
	Ptr<CPromptPopup>	GetPrompt(void) const;
	bool	HasEditor(void);
	bool	GoToEntry(Ptr<CIDEEditorTab> pTab, SIDEOutlineEntry* psEntry, bool bSelectName);
	uint8	GetEntrySymbolKind(SIDEOutlineEntry* psEntry);
	void	GetEntryLocation(Ptr<CIDEEditorTab> pTab, SIDEOutlineEntry* psEntry, SIDELocation* psDest);

protected:
	bool	StepMethod(bool bNext);
	void	LineAccepted(void);
};


bool ParseIDELinePlace(const char* szPlace, size* puiLine, size* puiColumn);


#endif // __IDE_STRUCTURE_H__

