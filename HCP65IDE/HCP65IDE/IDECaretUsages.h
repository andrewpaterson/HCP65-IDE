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
#ifndef __IDE_CARET_USAGES_H__
#define __IDE_CARET_USAGES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "IDEEditorTab.h"


#define IDE_CARET_USAGES_REST	300		//Milliseconds the caret stays put on a symbol before its usages are shown.
#define IDE_CARET_USAGES_WAIT	100		//Milliseconds between looks while the index catches up with the editor.


class CIDEContent;
//The usages of the symbol at the active editor's caret in its file, as IntelliJ IDEA highlights them: once the caret
//has stayed on a symbol the index knows for IDE_CARET_USAGES_REST, its usages are drawn in the occurrence colour and
//where it is declared in the write colour (CTextArea::SetOccurrenceRanges).  They go when the caret leaves them or the
//text is edited, and come again only once the index has the edited text (it is given it a pause after the edits;
//CIDEEditorTab::IsIndexCurrent).  A selection hides them; the selected text's own occurrences show instead.
class CIDECaretUsages : public CObject
{
CONSTRUCTABLE(CIDECaretUsages);
DESTRUCTABLE(CIDECaretUsages);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CIDEEditorTab>	mpTab;  //The tab watched: the active one when last looked at.
	size				muiCaret;  //Its caret then, MAX_SIZE for none.
	size				muiChanges;  //Its editor's change count then.
	int64				miMoved;  //When the caret or text last changed.
	bool				mbLookedUp;  //The usages for where the caret is now have been looked for.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	int64	Poll(int64 iNow);

protected:
	void	Watch(Ptr<CIDEEditorTab> pTab);
	bool	IsOnShownUsage(Ptr<CTextArea> pEditor, size uiCaret);
	void	ShowUsages(Ptr<CIDEEditorTab> pTab);
};


#endif // __IDE_CARET_USAGES_H__

