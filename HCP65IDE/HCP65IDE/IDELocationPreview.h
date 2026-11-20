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
#ifndef __IDE_LOCATION_PREVIEW_H__
#define __IDE_LOCATION_PREVIEW_H__
#include "StandardLib/Object.h"
#include "WindowLib/CodePreview.h"
#include "IDEDocuments.h"


class CIDEContent;


//A code preview (CCodePreview) showing places the IDE found, for a results pane and a chooser: a place's file's code
//(as its editor has it if it is open, else as it is on the disk), at its line, the text found coloured.  The code is
//only taken again when the file isn't the one shown or its editor has changed it since; it is then given to the edits
//thread to be coloured as an editor is, a moment after.  Whoever holds it calls Detach before the IDE closes, or
//before letting it go: the edits thread may have a file of its still to colour.
class CIDELocationPreview : public CObject, public CIDEPreviewAsker
{
CONSTRUCTABLE(CIDELocationPreview);
DESTRUCTABLE(CIDELocationPreview);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds what holds this.
	Ptr<CCodePreview>	mpPreview;
	size				muiPreviewChanges;  //The previewed file's editor's change count when its text was taken, MAX_SIZE if it wasn't open.

public:
	void				Init(CIDEContent* pcContent, Ptr<CCodePreview> pPreview);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	bool				ShowLocation(SIDELocation* psLocation);
	void				Clear(void);
	void				Detach(void);
	void				PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights) override;
	Ptr<CCodePreview>	GetPreview(void) const;
};


size FindIDETextPosition(const char* szText, size uiLength, int32 iLine, int32 iColumn);


#endif // __IDE_LOCATION_PREVIEW_H__

