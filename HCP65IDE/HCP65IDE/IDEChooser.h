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
#ifndef __IDE_CHOOSER_H__
#define __IDE_CHOOSER_H__
#include "StandardLib/Object.h"
#include "WindowLib/ChooserPopup.h"
#include "IDECodeModel.h"
#include "IDEIcons.h"
#include "IDELocationPreview.h"


class CIDEContent;


//A chooser of places in the code (WindowLib's CChooserPopup, as IntelliJ IDEA's popups that list places are): the
//targets of a "go to" that has several, a symbol's usages, a file's structure, the recent files.  Each row is a
//place: its icon (a file's type or a symbol's kind), its name, where it is, dimmer, and the place itself, which the
//preview beside the rows shows the code of and which is gone to when its row is chosen (CIDEDocuments::OpenLocation,
//so it is on Back).  Headings go over groups of rows.
//
//It is filled between Begin (which says what it has: a title, a field that filters, a preview, a button) and one of
//the opens: under the active editor's caret (OpenAtCaret) or at a point of the window (OpenAt).  A chooser begun
//again forgets the places it had.  A chooser that doesn't go to what is chosen itself (SetGoesToChosen) leaves that
//to whoever hears its chosen event (GetChosenPlace).
class CIDEChooser : public CObject
{
CONSTRUCTABLE(CIDEChooser);
DESTRUCTABLE(CIDEChooser);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds what holds this.
	Ptr<CChooserPopup>			mpPopup;  //From Begin on.
	Ptr<CIDELocationPreview>	mpPreview;  //If it has a preview.
	CArrayIDELocations			masPlaces;  //A row's item's data is its place's index.
	int32						miFileIcon;  //The popup's first file icon, by EIDEFileType from it.
	int32						maiSymbolIcons[IST_Count];
	bool						mbSelectName;  //The place's name is selected when it is gone to.
	bool						mbGoesToChosen;
	int32						miChosen;  //The place chosen, -1 until one is.

	CComponentEvent<CIDEChooser>	mcChosen;
	CComponentEvent<CIDEChooser>	mcButtonClicked;

public:
	void			Init(CIDEContent* pcContent);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			Begin(const char* szTitle, bool bFilter, bool bPreview, const char* szButton);
	void			AddHeading(const char* szName);
	size			AddPlace(const char* szName, const char* szWhere, int32 iIcon, SIDELocation* psLocation);
	size			AddTarget(SIDETarget* psTarget);
	void			SetSelectName(bool bSelectName);
	void			SetGoesToChosen(bool bGoesToChosen);
	void			SetSortMatches(bool bSortMatches);
	bool			OpenAtCaret(size uiSelectedPlace = 0);
	bool			OpenAt(SInt32Vec2 sPosition, Ptr<CComponent> pOwner, size uiSelectedPlace = 0);
	void			Close(void);
	bool			IsOpen(void);
	void			Detach(void);

	size			NumPlaces(void) const;
	SIDELocation*	GetPlace(size uiIndex);
	SIDELocation*	GetChosenPlace(void);
	int32			GetChosenIndex(void) const;
	int32			GetFileIcon(const char* szFileName);
	int32			GetSymbolIcon(uint8 uiKind);
	Ptr<CChooserPopup>	GetPopup(void) const;

	CComponentEvent<CIDEChooser>*	GetChosenEvent(void);
	CComponentEvent<CIDEChooser>*	GetButtonClickEvent(void);

protected:
	void			RowSelected(void);
	void			PlaceChosen(void);
	void			ButtonClicked(void);
	void			PopupClosed(void);
	void			ReleasePopup(void);
};


void AppendIDEPlaceName(SIDELocation* psLocation, CChars* pszDest);


#endif // __IDE_CHOOSER_H__

