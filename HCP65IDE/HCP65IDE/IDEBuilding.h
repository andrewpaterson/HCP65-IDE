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
#ifndef __IDE_BUILDING_H__
#define __IDE_BUILDING_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "WindowLib/Window.h"
#include "WindowLib/Button.h"
#include "WindowLib/DockManager.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "IDEBuildPane.h"


class CIDEContent;


//Build (Control+Shift+B, or the toolbar's Build): the solutions a build configuration builds, in order, one build
//step after another while the IDE goes on, the Build tab's output and progress, and the HCP65 listings the last build
//wrote, shown in the editors' gutters.
class CIDEBuilding : public CObject
{
friend class CIDEContent;
friend class CIDEScript;
CONSTRUCTABLE(CIDEBuilding);
DESTRUCTABLE(CIDEBuilding);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CDockPane>				mpBuildTool;
	Ptr<CButton>				mpBuildButton;
	Ptr<CIDEBuildPane>			mpBuildPane;
	size						muiBuildStep;		//The one being built.
	CChars						mszLibc65;			//The HCP65 compiler's system headers.
	size						muiListingToShow;	//The line to show the code of after the next build, MAX_SIZE for none.
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				CreatePane(SDockPlace* psPlace);
	Ptr<CButton>		CreateBuildButton(void);
	Ptr<CDockPane>	GetBuildTool(void);
	Ptr<CIDEBuildPane>	GetBuildPane(void);
	void				ShowBuildTab(void);
	void				SetLibc65Directory(const char* szDirectory);
	bool				Build(void);
	bool				IsBuilding(void);
	void				ShowListingAfterBuild(size uiLine);
	CCPP65ListingIndex*	GetListings(void);
	void				BuildClicked(void);
	void				SetListings(bool bListings);

protected:
	bool				StartBuildStep(CArrayChars* paszLines);
	void				ShowBuildProgress(void);
	void				FinishBuildSteps(CArrayChars* paszLines);
	void				PollBuild(Ptr<CWindow> pWindow);
	void				ReadListings(CArrayChars* paszLines);
};


#endif // __IDE_BUILDING_H__

