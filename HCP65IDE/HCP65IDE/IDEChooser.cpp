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
#include <string.h>
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEChooser.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::Init(CIDEContent* pcContent)
{
	size	i;

	PreInit();
	mpcContent = pcContent;
	masPlaces.Init();
	miFileIcon = -1;
	for (i = 0; i < IST_Count; i++)
	{
		maiSymbolIcons[i] = -1;
	}
	mbSelectName = false;
	mbGoesToChosen = true;
	miChosen = -1;
	mcChosen.Init();
	mcButtonClicked.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpPopup);
	M_Pointer(mpPreview);
	U_Unknown(CArrayIDELocations, masPlaces);
	U_Int32(miFileIcon);
	U_Int32Array(maiSymbolIcons, IST_Count);
	U_Bool(mbSelectName);
	U_Bool(mbGoesToChosen);
	U_Int32(miChosen);
	U_Unknown(CComponentEvent<CIDEChooser>, mcChosen);
	U_Unknown(CComponentEvent<CIDEChooser>, mcButtonClicked);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::Free(void)
{
	//The popup isn't told to close: the window may have gone before the chooser.
	if (mpPopup.IsNotNull())
	{
		mpPopup->GetSelectedEvent()->Remove(this);
		mpPopup->GetChosenEvent()->Remove(this);
		mpPopup->GetButtonClickEvent()->Remove(this);
		mpPopup->GetClosedEvent()->Remove(this);
	}
	mcButtonClicked.Kill();
	mcChosen.Kill();
	KillIDELocations(&masPlaces);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChooser::Save(CObjectWriter* pcFile) { return false; }
bool CIDEChooser::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The popup it had, if any, is closed and let go of.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::ReleasePopup(void)
{
	if (mpPopup.IsNotNull())
	{
		mpPopup->Close();
		mpPopup->GetSelectedEvent()->Remove(this);
		mpPopup->GetChosenEvent()->Remove(this);
		mpPopup->GetButtonClickEvent()->Remove(this);
		mpPopup->GetClosedEvent()->Remove(this);
		mpPopup = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE is closing, or the chooser is being let go of: the edits
// thread isn't to give its preview anything.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::Detach(void)
{
	if (mpPreview.IsNotNull())
	{
		mpPreview->Detach();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A chooser with nothing in it yet: its title, whether it has a field
// that filters its rows and a preview of the selected place's code, and
// its button's caption (NULL for no button).
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::Begin(const char* szTitle, bool bFilter, bool bPreview, const char* szButton)
{
	size	i;

	ReleasePopup();
	Detach();
	mpPreview = NULL;
	KillIDELocations(&masPlaces);
	masPlaces.Init();
	miChosen = -1;
	mbSelectName = false;
	mbGoesToChosen = true;

	mpPopup = OMalloc<CChooserPopup>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), mpcContent->GetMatchFont());
	mpPopup->SetTitle(szTitle);
	mpPopup->SetOpensAbove(false);
	if (bFilter)
	{
		mpPopup->AddFilter();
	}
	if (bPreview)
	{
		mpPopup->AddPreview(mpcContent->GetDimFont(), mpcContent->GetCodeFamily());
		mpPreview = OMalloc<CIDELocationPreview>(mpcContent, mpPopup->GetPreview());
	}
	if (szButton)
	{
		mpPopup->AddButton(szButton);
	}
	mpPopup->GetSelectedEvent()->Add(this, &CIDEChooser::RowSelected);
	mpPopup->GetChosenEvent()->Add(this, &CIDEChooser::PlaceChosen);
	mpPopup->GetButtonClickEvent()->Add(this, &CIDEChooser::ButtonClicked);
	mpPopup->GetClosedEvent()->Add(this, &CIDEChooser::PopupClosed);

	//The icons a Go to dialog has, in its order: the kinds of file, then the kinds of symbol.
	miFileIcon = -1;
	for (i = 0; i < mpcContent->NumFileTypeIcons(); i++)
	{
		if (i == 0)
		{
			miFileIcon = mpPopup->AddIcon(mpcContent->GetFileTypeIcon(i));
		}
		else
		{
			mpPopup->AddIcon(mpcContent->GetFileTypeIcon(i));
		}
	}
	for (i = 0; i < IST_Count; i++)
	{
		maiSymbolIcons[i] = mpPopup->AddIcon(mpcContent->GetSymbolIcon(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A heading over the places added after it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::AddHeading(const char* szName)
{
	mpPopup->AddHeading(szName);
}


//////////////////////////////////////////////////////////////////////////
//
// A place: the row's name, where it says it is (NULL for the file's name
// and line), its icon (GetFileIcon, GetSymbolIcon; -1 for none) and the
// place, which is copied.  Its index is returned.
//
//////////////////////////////////////////////////////////////////////////
size CIDEChooser::AddPlace(const char* szName, const char* szWhere, int32 iIcon, SIDELocation* psLocation)
{
	SIDELocation*	psPlace;
	CChars			szDefault;

	psPlace = masPlaces.Add();
	memcpy(psPlace, psLocation, sizeof(SIDELocation));
	psPlace->szFileName.Init(psLocation->szFileName);

	if (szWhere == NULL)
	{
		szDefault.Init();
		AppendIDEPlaceName(psLocation, &szDefault);
		mpPopup->AddItem(szName, szDefault.Text(), iIcon, masPlaces.NumElements() - 1);
		szDefault.Kill();
	}
	else
	{
		mpPopup->AddItem(szName, szWhere, iIcon, masPlaces.NumElements() - 1);
	}
	return masPlaces.NumElements() - 1;
}


//////////////////////////////////////////////////////////////////////////
//
// A symbol a "go to" found: its name with its scope, "CDrum::Fill(int)",
// its kind's icon and where it is, "Drum.cpp:22".
//
//////////////////////////////////////////////////////////////////////////
size CIDEChooser::AddTarget(SIDETarget* psTarget)
{
	CChars	szName;
	size	uiPlace;

	szName.Init();
	GetIDEScopedName(psTarget->szKey.Text(), &szName);
	uiPlace = AddPlace(szName.Text(), NULL, GetSymbolIcon(psTarget->uiKind), &psTarget->sLocation);
	szName.Kill();
	return uiPlace;
}


//////////////////////////////////////////////////////////////////////////
//
// "Drum.cpp:22": a place's file's name and its line, from 1.
//
//////////////////////////////////////////////////////////////////////////
void AppendIDEPlaceName(SIDELocation* psLocation, CChars* pszDest)
{
	pszDest->Append(GetIDEFileNamePart(psLocation->szFileName.Text()));
	pszDest->Append(':');
	pszDest->Append(psLocation->iLine + 1);
}


//////////////////////////////////////////////////////////////////////////
//
// Opened under the active editor's caret, as IntelliJ's choosers are,
// place uiSelectedPlace selected.  False if there is no editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChooser::OpenAtCaret(size uiSelectedPlace)
{
	Ptr<CIDEEditorTab>	pTab;
	SInt32Vec2			sPosition;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !mpcContent->GetDocuments()->GetCaretPoint(&sPosition))
	{
		return false;
	}
	return OpenAt(sPosition, pTab->GetEditor(), uiSelectedPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Opened with its top left at sPosition, in the window's client space,
// for pOwner.  False if it has no places.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChooser::OpenAt(SInt32Vec2 sPosition, Ptr<CComponent> pOwner, size uiSelectedPlace)
{
	size	i;
	size	uiItem;

	if (mpPopup.IsNull() || (masPlaces.NumElements() == 0))
	{
		return false;
	}
	miChosen = -1;

	//The item that is the place: headings are items too.
	uiItem = 0;
	for (i = 0; i < mpPopup->NumItems(); i++)
	{
		if (mpPopup->GetItem(i)->uiData == uiSelectedPlace)
		{
			uiItem = i;
			break;
		}
	}
	mpPopup->Open(sPosition, pOwner, uiItem);
	mpcContent->GetScript()->ChooserOpened(this);
	RowSelected();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::Close(void)
{
	if (mpPopup.IsNotNull())
	{
		mpPopup->Close();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The preview shows the selected row's place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::RowSelected(void)
{
	SGoToItem*	psItem;

	if (mpPreview.IsNull() || mpPopup.IsNull() || !mpPopup->IsOpen())
	{
		return;
	}
	psItem = mpPopup->GetSelectedItem();
	if ((psItem == NULL) || (psItem->uiData >= masPlaces.NumElements()))
	{
		mpPreview->Clear();
		return;
	}
	mpPreview->ShowLocation(masPlaces.Get(psItem->uiData));
}


//////////////////////////////////////////////////////////////////////////
//
// A row was chosen: its place is gone to, unless that is left to whoever
// hears the chosen event.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::PlaceChosen(void)
{
	SGoToItem*	psItem;

	psItem = mpPopup->GetChosen();
	if ((psItem == NULL) || (psItem->uiData >= masPlaces.NumElements()))
	{
		return;
	}
	miChosen = (int32)psItem->uiData;
	if (mbGoesToChosen)
	{
		mpcContent->GetDocuments()->OpenLocation(masPlaces.Get(psItem->uiData), mbSelectName);
	}
	mcChosen.Call(this);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::ButtonClicked(void)
{
	mcButtonClicked.Call(this);
}


//////////////////////////////////////////////////////////////////////////
//
// The popup has gone: its preview's file isn't wanted coloured any more.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChooser::PopupClosed(void)
{
	mpcContent->GetScript()->ChooserClosed(this);
	Detach();
}


//////////////////////////////////////////////////////////////////////////
//
// The icon of a file's type, or of a symbol's kind (-1 for a kind that
// has none), for AddPlace.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEChooser::GetFileIcon(const char* szFileName)
{
	return (miFileIcon < 0) ? -1 : miFileIcon + (int32)CIDEIcons::GetFileType(szFileName);
}


int32 CIDEChooser::GetSymbolIcon(uint8 uiKind)
{
	int	iType;

	iType = GetIDESymbolIconType(uiKind);
	return (iType < 0) ? -1 : maiSymbolIcons[iType];
}


//////////////////////////////////////////////////////////////////////////
//
// The place chosen, NULL if none was since it was last opened.
//
//////////////////////////////////////////////////////////////////////////
SIDELocation* CIDEChooser::GetChosenPlace(void)
{
	return ((miChosen < 0) || ((size)miChosen >= masPlaces.NumElements())) ? NULL : masPlaces.Get((size)miChosen);
}


void CIDEChooser::SetSelectName(bool bSelectName) { mbSelectName = bSelectName; }  //The place's name (its length) is selected when it is gone to.
void CIDEChooser::SetGoesToChosen(bool bGoesToChosen) { mbGoesToChosen = bGoesToChosen; }
void CIDEChooser::SetOpensAbove(bool bOpensAbove) { mpPopup->SetOpensAbove(bOpensAbove); }
void CIDEChooser::SetSortMatches(bool bSortMatches) { mpPopup->SetSortMatches(bSortMatches); }
bool CIDEChooser::IsOpen(void) { return mpPopup.IsNotNull() && mpPopup->IsOpen(); }
size CIDEChooser::NumPlaces(void) const { return masPlaces.NumElements(); }
SIDELocation* CIDEChooser::GetPlace(size uiIndex) { return masPlaces.Get(uiIndex); }
int32 CIDEChooser::GetChosenIndex(void) const { return miChosen; }
Ptr<CChooserPopup> CIDEChooser::GetPopup(void) const { return mpPopup; }
CComponentEvent<CIDEChooser>* CIDEChooser::GetChosenEvent(void) { return &mcChosen; }
CComponentEvent<CIDEChooser>* CIDEChooser::GetButtonClickEvent(void) { return &mcButtonClicked; }

