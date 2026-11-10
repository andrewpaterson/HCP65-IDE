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
#ifndef __IDE_NAVIGATION_HISTORY_H__
#define __IDE_NAVIGATION_HISTORY_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"


#define IDE_NAVIGATION_MAXIMUM		100		//The most places Back keeps; the oldest are dropped.
#define IDE_NAVIGATION_FAR_LINES	10		//A move in a file this many lines or more is a place to come back to.
#define IDE_NAVIGATION_SETTLE		1000	//Milliseconds the caret stays put for where it is to be a place.


//A place in a file: where its caret was.  It follows the file's edits while the file is open (CIDEDocuments tells them).
struct SIDENavigationPlace
{
	CChars	szFile;
	size	uiProject;
	int32	iLine;		//From 0.
	int32	iColumn;	//From 0.

	void	Init(void);
	void	Init(SIDENavigationPlace* psPlace);
	void	Kill(void);
	void	Set(SIDENavigationPlace* psPlace);
};


typedef CArrayTemplate<SIDENavigationPlace>	CArrayIDENavigationPlaces;


class CIDEContent;
struct SIDEPlaceChange;
//IntelliJ IDEA's Back and Forward (Control+Alt+Left and Right): the places the caret was, as a browser's history.  A
//jump (CIDEDocuments::OpenLocation and OpenFile: Go to, Find Usages, F4, Next Occurrence, Solution Explorer...) puts
//where it left on Back, as does a move of the caret far from where it had stayed (a click, Control+End) or another
//tab chosen; either clears Forward.  Back goes to the last place, putting where it was on Forward, and reopens its
//file if it was closed.  It is kept for the session only.
class CIDENavigationHistory : public CObject
{
CONSTRUCTABLE(CIDENavigationHistory);
DESTRUCTABLE(CIDENavigationHistory);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	CArrayIDENavigationPlaces	masBack;  //The oldest first.
	CArrayIDENavigationPlaces	masForward;  //The nearest last.
	SIDENavigationPlace			msSettled;  //Where the caret last stayed put, if mbSettled.
	bool						mbSettled;
	SIDENavigationPlace			msLast;  //Where it was when last looked at, since miLastChange, if mbLast.
	bool						mbLast;
	int64						miLastChange;
	int32						miJumps;  //Jumps begun and not ended; only the outermost is recorded.
	bool						mbMoving;  //Back or Forward is moving: nothing is recorded.

public:
	void			Init(CIDEContent* pcContent);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			AddCommands(Ptr<CCommandSet> pCommands);
	void			AddNavigateItems(Ptr<CMenuContainer> pMenu);

	bool			BeginJump(SIDENavigationPlace* psFrom);
	void			EndJump(SIDENavigationPlace* psFrom, bool bJumped);
	void			CaretPlaced(void);
	int64			Poll(int64 iNow);

	bool			GoBack(void);
	bool			GoForward(void);
	bool			CanGoBack(void);
	bool			CanGoForward(void);

	void			AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);

	size			NumBack(void);
	size			NumForward(void);
	void			Print(CChars* pszDest);

protected:
	void			NavigateBack(void);
	void			NavigateForward(void);
	bool			GetCurrentPlace(SIDENavigationPlace* psDest);
	bool			IsFar(SIDENavigationPlace* psFrom, SIDENavigationPlace* psTo);
	void			Push(SIDENavigationPlace* psPlace, bool bClearForward);
	bool			Go(SIDENavigationPlace* psPlace);
	void			Arrived(void);
	void			AdjustPlace(SIDENavigationPlace* psPlace, const char* szFile, SIDEPlaceChange* psChange);
	void			KillPlaces(CArrayIDENavigationPlaces* pasPlaces);
	void			PrintPlaces(CArrayIDENavigationPlaces* pasPlaces, CChars* pszDest);
};


#endif // __IDE_NAVIGATION_HISTORY_H__
