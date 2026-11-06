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
#ifndef __IDE_USAGES_H__
#define __IDE_USAGES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/TreeContainer.h"
#include "WindowLib/DockManager.h"
#include "IDECodeModel.h"


class CIDEContent;


//IntelliJ's Find Usages (Alt+F7): every use of the symbol at the caret, not its declarations, in the Find Usages tool
//window beside Build, a tree of the project each is in, then the class it is in (or the file), then its line, which
//opens the place.
class CIDEUsages : public CObject
{
CONSTRUCTABLE(CIDEUsages);
DESTRUCTABLE(CIDEUsages);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CTreeContainer>	mpUsages;  //What Find Usages found: projects, classes, lines.
	Ptr<CDockPane>		mpUsagesTool;
	CArrayIDELocations	masUsages;  //Each line's place, by its node's data less one.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				CreatePane(SDockPlace* psPlace);
	bool				FindUsages(void);
	void				UsageActivated(void);
	Ptr<CTreeContainer>	GetUsagesTree(void);

protected:
};


#endif // __IDE_USAGES_H__

