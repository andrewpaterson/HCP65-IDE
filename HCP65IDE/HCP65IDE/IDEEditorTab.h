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
#ifndef __IDE_EDITOR_TAB_H__
#define __IDE_EDITOR_TAB_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/CodeEditor.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TabComponent.h"
#include "WindowLib/CodeEditorListener.h"
#include "CppParserLib/CPPHighlighter.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "IDEListingZone.h"


#define IDE_LISTING_MARK		'+'		//In the gutter: the line has code in the listings.
#define IDE_LISTING_OPEN_MARK	'-'		//And its code is shown under it.


class CIDEEditorTab;


//Hears the gutter being clicked next to a line.
class CIDEGutterListener : public CCodeEditorListener
{
CONSTRUCTABLE(CIDEGutterListener);
public:
	CIDEEditorTab*	mpcTab;

	void	Init(CIDEEditorTab* pcTab);
	void	GutterClicked(CUnknown* pcSource, void* pvContext) override;
};


//A file open in the editor's tabs: its text in a fixed width font with line numbers, scrolled in its page, with what
//is known of it for highlighting it again as it is edited.  Given an HCP65 build's listings (SetListings), each line
//with code is marked in the gutter; clicking a mark shows the line's code under it (a CIDEListingZone), clicking again
//hides it.  Editing the file puts the listings out of date until they are given again: the marks go and the zones are
//greyed.
class CIDEEditorTab : public CObject
{
CONSTRUCTABLE(CIDEEditorTab);
DESTRUCTABLE(CIDEEditorTab);
protected:
	Ptr<CWindow>		mpWindow;
	Ptr<CWindowTheme>	mpTheme;
	Ptr<CFont>			mpTitleFont;	//The zones' headers'.
	Ptr<CFontFamily>	mpFamily;
	Ptr<CCodeEditor>	mpCodeEditor;
	Ptr<CTextArea>		mpEditor;		//The code editor's text area.
	Ptr<CScrollPane>	mpPane;			//The tab's page.
	Ptr<CTabComponent>	mpTab;
	CChars				mszFileName;
	size				muiProject;		//The solution's project it is in, MAX_SIZE if none.
	size				muiSeenChanges;	//The editor's change count when it was last looked at.
	int64				miChangedTime;	//When the text last changed (GetDebugTickCount).
	bool				mbRecolour;		//The text changed and hasn't been highlighted since.
	bool				mbEdited;		//The text was changed since the file was opened.
	CIDEGutterListener	mcGutterListener;
	CCPP65ListingIndex*	mpcListings;	//The listings given, NULL for none; the content keeps them.
	bool				mbListingsOutOfDate;	//The text changed since they were given.
	CArray<CIDEListingZone>	maZones;	//Shown under their lines.

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pTitleFont, Ptr<CFontFamily> pFamily, ARGB32 sColour, const char* szFileName, size uiProject, const char* szText);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				SetTab(Ptr<CTabComponent> pTab);
	void				SetHighlights(CArrayHighlights* pacHighlights);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				SetCaretPlace(int iLine, int iColumn);
	bool				NeedsRecolour(int64 iNow, int64* piWait);

	void				SetListings(CCPP65ListingIndex* pcListings);
	bool				ToggleListing(size uiLine);
	size				NumZones(void);
	Ptr<CIDEListingZone>	GetZone(size uiIndex);
	Ptr<CIDEListingZone>	FindZone(size uiLine);
	bool				IsListingsOutOfDate(void);

	Ptr<CTextArea>		GetEditor(void);
	Ptr<CCodeEditor>	GetCodeEditor(void);
	Ptr<CScrollPane>	GetPane(void);
	Ptr<CTabComponent>	GetTab(void);
	char*				GetFileName(void);
	size				GetProject(void);
	bool				IsEdited(void);

protected:
	void				ListingsOutOfDate(void);
	void				RemoveZone(Ptr<CIDEListingZone> pZone);
	void				MarkLine(size uiLine);
};


#endif // __IDE_EDITOR_TAB_H__

