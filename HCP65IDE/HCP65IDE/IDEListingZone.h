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
#ifndef __IDE_LISTING_ZONE_H__
#define __IDE_LISTING_ZONE_H__
#include "StandardLib/Object.h"
#include "SupportLib/FontFamily.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/Button.h"
#include "WindowLib/ButtonListener.h"
#include "WindowLib/TextArea.h"


class CIDEListingZone;


//Hears the zone's < and > buttons.
class CIDEListingZoneListener : public CButtonListener
{
CONSTRUCTABLE(CIDEListingZoneListener);
public:
	CIDEListingZone*	mpcZone;

	void	Init(CIDEListingZone* pcZone);
	void	ButtonClicked(CUnknown* pcSource, void* pvContext) override;
};


//What a line of C++ compiled to, shown under it in the editor (a line component of its CCodeEditor): a header, < and >
//to step through the places the line's code is in then the place's listing and function, "Main.lst - _main - 1 of 3",
//over the place's assembly.  A line in pieces in its function (a for's parts) shows them all, "..." between.  Once the
//file is edited after the build the zone is out of date: greyed, until the next build.
class CIDEListingZone : public CObject
{
CONSTRUCTABLE(CIDEListingZone);
DESTRUCTABLE(CIDEListingZone);
protected:
	Ptr<CGridContainer>			mpPanel;
	Ptr<CLabel>					mpTitle;
	Ptr<CButton>				mpPrevious;
	Ptr<CButton>				mpNext;
	Ptr<CTextArea>				mpCode;
	CIDEListingZoneListener		mcListener;
	CCPP65ListingIndex*			mpcIndex;	//The listings the places are in; the editor's owner keeps it.
	CArrayCPP65ListingPlaces	masPlaces;
	size						muiPlace;	//The one shown.
	bool						mbOutOfDate;

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pTitleFont, Ptr<CFontFamily> pCodeFamily);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				SetPlaces(CCPP65ListingIndex* pcIndex, CArrayCPP65ListingPlaces* pasPlaces);
	void				Step(int iDirection);
	void				SetOutOfDate(bool bOutOfDate);
	void				ButtonClicked(CUnknown* pcButton);

	Ptr<CComponent>		GetComponent(void);
	size				NumPlaces(void);
	size				GetPlace(void);
	char*				GetTitle(void);
	char*				GetCode(void);
	bool				IsOutOfDate(void);

protected:
	void				Show(void);
	void				GetTitle(CChars* pszDest);
	void				GetCode(CChars* pszDest);
};


#endif // __IDE_LISTING_ZONE_H__

