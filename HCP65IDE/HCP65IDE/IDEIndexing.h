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
#ifndef __IDE_INDEXING_H__
#define __IDE_INDEXING_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/DockManager.h"
#include "IDEBuildPane.h"
#include "IDEMessageOutput.h"
#include "IDESteppable.h"


class CIDEContent;


//The Indexing tab beside Build: how far the code model's index has got, on a bar as the build's (blue while it is
//built, then green, or red if some units had problems), over what they couldn't include, each once, and how long
//it took.  It follows each indexing the code model starts.  Its problems name files by their full paths, and are gone
//to as the Build tab's messages are (CIDEMessageOutput).
class CIDEIndexing : public CObject, public CIDESteppable
{
CONSTRUCTABLE(CIDEIndexing);
DESTRUCTABLE(CIDEIndexing);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CIDEBuildPane>	mpPane;
	Ptr<CIDEMessageOutput>	mpMessages;  //Its problems.
	Ptr<CDockPane>		mpTool;
	size				muiShown;		//The code model's indexing shown (CIDECodeModel::NumIndexings), 0 for none.
	bool				mbFinished;		//Its end has been written.

public:
	void				Init(CIDEContent* pcContent);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				CreatePane(SDockPlace* psPlace);
	bool				Update(void);

	Ptr<CIDEBuildPane>	GetPane(void) const;
	Ptr<CDockPane>		GetTool(void) const;
	Ptr<CIDEMessageOutput>	GetMessages(void) const;

	Ptr<CDockPane>		GetSteppablePane(void) const override;
	const char*			GetSteppableName(void) override;
	bool				HasSteps(void) override;
	bool				Step(bool bNext, bool bKeepKeys) override;
};


#endif // __IDE_INDEXING_H__

