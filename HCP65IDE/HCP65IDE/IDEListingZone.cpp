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
#include "BaseLib/FileUtil.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "WindowLib/Canvas.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/JewelColours.h"
#include "IDEListingZone.h"


#define IDE_LISTING_ZONE_GAP	"\t\t...\n"		//Between a line's pieces.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZoneListener::Init(CIDEListingZone* pcZone)
{
	mpcZone = pcZone;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZoneListener::ButtonClicked(CUnknown* pcSource, void* pvContext)
{
	mpcZone->ButtonClicked(pcSource);
}


//////////////////////////////////////////////////////////////////////////
//
// The header in pTitleFont over the assembly in pCodeFamily's fixed
// width font, on the panels' colour so it stands apart from the code.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pTitleFont, Ptr<CFontFamily> pCodeFamily)
{
	Ptr<CCanvas>	pBackground;

	PreInit();

	mcListener.Init(this);
	mpcIndex = NULL;
	masPlaces.Init();
	muiPlace = 0;
	mbOutOfDate = false;

	mpTitle = OMalloc<CLabel>(pWindow, pTitleFont, "");
	mpTitle->SetAlignment(CSH_Left, CSV_Center);
	mpPrevious = OMalloc<CButton>(pWindow, pTheme, "<");
	mpPrevious->AddListener(&mcListener);
	mpNext = OMalloc<CButton>(pWindow, pTheme, ">");
	mpNext->AddListener(&mcListener);
	mpCode = OMalloc<CTextArea>(pWindow, pCodeFamily, "");
	mpCode->SetEditable(false);
	mpCode->SetTiledText(true);
	mpCode->SetAlignment(CSH_Left, CSV_Top);
	pBackground = OMalloc<CCanvas>(pWindow, OMalloc<CClearCanvasDraw>(JEWEL_PANEL));
	pBackground->SetAlignment(CSH_Stretch, CSV_Stretch);

	//< and > before the title: the zone is as wide as the code, which can be wider than the view.
	mpPanel = OMalloc<CGridContainer>(pWindow);
	mpPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel->AddComponent(pBackground, 0, 0, 3, 2, 1, 1);
	mpPanel->AddComponent(mpPrevious, 0, 0);
	mpPanel->AddComponent(mpNext, 1, 0);
	mpPanel->AddComponent(mpTitle, 2, 0, 1, 1, 1, 0);
	mpPanel->AddComponent(mpCode, 0, 1, 3, 1, 1, 1);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::Class(void)
{
	M_Pointer(mpPanel);
	M_Pointer(mpTitle);
	M_Pointer(mpPrevious);
	M_Pointer(mpNext);
	M_Pointer(mpCode);
	U_Unknown(CIDEListingZoneListener, mcListener);
	U_Pointer(mpcIndex);
	U_Data(CArrayCPP65ListingPlaces, masPlaces);
	U_Size(muiPlace);
	U_Bool(mbOutOfDate);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::Free(void)
{
	mcListener.RemoveFromListenerCalls();
	masPlaces.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A zone isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEListingZone::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEListingZone::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The places of the line's code, in pcIndex's listings: the place shown
// stays if there are as many, otherwise the first is.  The zone is up to
// date.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::SetPlaces(CCPP65ListingIndex* pcIndex, CArrayCPP65ListingPlaces* pasPlaces)
{
	mpcIndex = pcIndex;
	masPlaces.ReInit();
	masPlaces.Copy(pasPlaces);
	if (muiPlace >= masPlaces.NumElements())
	{
		muiPlace = 0;
	}
	mbOutOfDate = false;
	Show();
}


//////////////////////////////////////////////////////////////////////////
//
// To the next place (iDirection 1) or the one before (-1), round from the
// last to the first.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::Step(int iDirection)
{
	size	uiPlaces;

	uiPlaces = masPlaces.NumElements();
	if (uiPlaces < 2)
	{
		return;
	}
	muiPlace = (muiPlace + uiPlaces + (iDirection < 0 ? uiPlaces - 1 : 1)) % uiPlaces;
	Show();
}


//////////////////////////////////////////////////////////////////////////
//
// The file was edited since the build: what is shown may not be what the
// line compiles to now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::SetOutOfDate(bool bOutOfDate)
{
	if (mbOutOfDate != bOutOfDate)
	{
		mbOutOfDate = bOutOfDate;
		Show();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::ButtonClicked(CUnknown* pcButton)
{
	Step((pcButton == &mpPrevious) ? -1 : 1);
}


//////////////////////////////////////////////////////////////////////////
//
// The header and code of the place shown, in grey when out of date, and
// the zone as high as they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::Show(void)
{
	CChars	sz;
	int32	iHeader;

	sz.Init();
	GetTitle(&sz);
	mpTitle->SetText(sz.Text());
	sz.Clear();
	GetCode(&sz);
	mpCode->SetText(sz.Text());
	mpCode->SetColour(mbOutOfDate ? JEWEL_LINE_NUMBER : JEWEL_TEXT);
	sz.Kill();

	//A grid is variable sized: the code editor's gap is as high as this.
	iHeader = mpTitle->GetDesiredSize().y;
	if (mpNext->GetDesiredSize().y > iHeader)
	{
		iHeader = mpNext->GetDesiredSize().y;
	}
	mpPanel->SetDesiredSize(-1, iHeader + mpCode->GetTextSize().y);
}


//////////////////////////////////////////////////////////////////////////
//
// "Main.lst - _main - 2 of 3", and "(out of date: build again)" after.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::GetTitle(CChars* pszDest)
{
	SCPP65ListingPlace*	psPlace;
	CFileUtil			cFileUtil;
	CChars				szListing;

	if (masPlaces.NumElements() == 0)
	{
		pszDest->Append("No code for this line.");
	}
	else
	{
		psPlace = masPlaces.Get(muiPlace);
		szListing.Init();
		cFileUtil.GetFileName(&szListing, mpcIndex->GetListingName(psPlace->uiListing));
		pszDest->Append(szListing);
		pszDest->Append(" - ");
		pszDest->Append(mpcIndex->GetFunction(psPlace->uiFunction));
		pszDest->Append(" - ");
		pszDest->Append((uint32)(muiPlace + 1));
		pszDest->Append(" of ");
		pszDest->Append((uint32)masPlaces.NumElements());
		szListing.Kill();
	}
	if (mbOutOfDate)
	{
		pszDest->Append("  (out of date: build again)");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The place's pieces, "..." between them, without a line end after the
// last.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingZone::GetCode(CChars* pszDest)
{
	SCPP65ListingPlace*	psPlace;
	SCPP65ListingRange*	psRange;
	size				i;

	if (masPlaces.NumElements() == 0)
	{
		return;
	}
	psPlace = masPlaces.Get(muiPlace);
	for (i = 0; i < psPlace->uiNumRanges; i++)
	{
		if (i > 0)
		{
			pszDest->Append(IDE_LISTING_ZONE_GAP);
		}
		psRange = mpcIndex->GetRange(psPlace->uiFirstRange + i);
		mpcIndex->AppendLines(psRange->uiListing, psRange->uiFirstLine, psRange->uiEndLine, pszDest);
	}
	if (pszDest->EndsWith('\n'))
	{
		pszDest->RemoveLastCharacter();
	}
}


Ptr<CComponent> CIDEListingZone::GetComponent(void) { return mpPanel; }
size CIDEListingZone::NumPlaces(void) { return masPlaces.NumElements(); }
size CIDEListingZone::GetPlace(void) { return muiPlace; }
char* CIDEListingZone::GetTitle(void) { return mpTitle->GetText(); }
char* CIDEListingZone::GetCode(void) { return mpCode->GetText(); }
bool CIDEListingZone::IsOutOfDate(void) { return mbOutOfDate; }

