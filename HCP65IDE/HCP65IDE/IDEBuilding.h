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
#include "BaseLib/ArrayInt.h"
#include "BuildLib/SolutionBuild.h"


class CIDEContent;


//Build (Control+Shift+B, or the toolbar's Build): the solutions a build configuration builds, in order, one build
//step after another while the IDE goes on, the Build tab's output and progress, and the HCP65 listings the last build
//wrote, shown in the editors' gutters.
class CIDEBuilding : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEBuilding);
DESTRUCTABLE(CIDEBuilding);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	CSolutionBuild				mcBuild;			//The build running, or the last.
	CArrayInt					maiBuildOrder;		//The solutions a build builds, in order.
	CCPP65ListingIndex			mcListings;			//The last HCP65 build's listings, given to the open files.
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
	void				SetLibc65Directory(const char* szDirectory);
	bool				Build(void);
	void				StopBuild(void);
	bool				IsBuilding(void);
	void				ShowListingAfterBuild(size uiLine);
	CCPP65ListingIndex*	GetListings(void);
	void				SetListings(bool bListings);
	void				PollBuild(Ptr<CWindow> pWindow);

protected:
	bool				StartBuildStep(CArrayChars* paszLines);
	void				ShowBuildProgress(void);
	void				FinishBuildSteps(CArrayChars* paszLines);
	void				ReadListings(CArrayChars* paszLines);
	void				ShowBuildTab(void);
	void				BuildClicked(void);
};


#endif // __IDE_BUILDING_H__

