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
#ifndef __IDE_EDITOR_TAB_H__
#define __IDE_EDITOR_TAB_H__
#include "IDEOutline.h"
#include "IDEBreadcrumbs.h"


//The columns of icons in an editor's gutter (CCodeEditor::SetIconColumns), after the debugger's glyphs.
#define IDE_GUTTER_BOOKMARKS	0	//A bookmark's.
#define IDE_GUTTER_OVERRIDES	1	//What a function overrides, or is overridden by.
#define IDE_GUTTER_ICON_COLUMNS	2


//The colours of a changed line's bar in the gutter and of its mark on the scroll bar, as IntelliJ IDEA's.
#define IDE_CHANGE_ADDED		Set32BitColour((uint8)0x54, (uint8)0x91, (uint8)0x59)
#define IDE_CHANGE_MODIFIED		Set32BitColour((uint8)0x37, (uint8)0x5F, (uint8)0xAD)
#define IDE_CHANGE_REMOVED		Set32BitColour((uint8)0x86, (uint8)0x8A, (uint8)0x91)


#include "IDEFind.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/CodeEditor.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/DockPane.h"
#include "WindowLib/LineZones.h"
#include "CppCompilerLib/CPPHighlighter.h"
#include "CppCompilerLib/CPP65ListingIndex.h"
#include "GitLib/GitStatus.h"
#include "GitLib/GitLineChanges.h"
#include "IDEListingSource.h"


//A file open in the editor's tabs: its text in a fixed width font with line numbers, scrolled in its page, with what
//is known of it for highlighting it again as it is edited.  Given an HCP65 build's listings (SetListings), the lines'
//code is shown under them as CLineZones do: clicking a line's mark in the gutter shows its code.  Given the file's text
//in the last commit (SetBaseText), the lines changed since are marked in the gutter as IntelliJ IDEA marks them.
//
//SaveFile writes the text back with the file's line ends: lines typed into a file whose lines end with carriage returns
//and line feeds get them too.  The tab's name ends with a '*' while the text has changed since it was opened or saved.
class CIDEEditorTab : public CObject, public CTextSelectionRanges
{
CONSTRUCTABLE(CIDEEditorTab);
DESTRUCTABLE(CIDEEditorTab);
protected:
	Ptr<CWindow>		mpWindow;
	Ptr<CCodeEditor>	mpCodeEditor;
	Ptr<CTextArea>		mpEditor;		//The code editor's text area.
	Ptr<CScrollPane>	mpPane;			//The code, scrolled.
	Ptr<CGridContainer>	mpHolder;		//The tab's page: a strip, the view with the code in it, and a strip.
	Ptr<CFillContainer>	mpTopStrip;		//Over the code, empty and no height until something is put in it (SetTopBar).
	Ptr<CFillContainer>	mpView;			//The code, and what is laid over it (SetOverlay).
	Ptr<CFillContainer>	mpBottomStrip;	//Under the code (SetBottomBar).
	Ptr<CComponent>		mpTopBar;
	Ptr<CComponent>		mpBottomBar;
	Ptr<CComponent>		mpOverlay;
	Ptr<CIDEBreadcrumbs>	mpBreadcrumbs;	//Made when breadcrumbs are first shown.
	Ptr<CIDEStickyLines>	mpStickyLines;	//Made when sticky lines are first shown.
	Ptr<CIDEFindBar>	mpFindBar;		//Made when Find is first opened in it.
	size				muiKeptUsagesLayer;	//The editor's layer of highlights for usages kept highlighted, MAX_SIZE until made.
	size				muiLinkLayer;		//Its layer for the name underlined as a link, MAX_SIZE until made.
	Ptr<CDockPane>		mpDockPane;  //The document pane it is shown in.
	CChars				mszFileName;
	size				muiProject;		//The workspace's project it is in, MAX_SIZE if none.
	size				muiSeenChanges;	//The editor's change count when it was last looked at.
	int64				miChangedTime;	//When the text last changed (GetDebugTickCount).
	bool				mbRecolour;		//The text changed and hasn't been given to be highlighted since (TakeEdits).
	bool				mbRecolourNow;	//It is highlighted at once, not after a pause in typing (RequestRecolour).
	bool				mbEdited;		//The text was changed since the file was opened.
	bool				mbIndexStale;	//The text changed since the index was given it (TakeEdits).
	size				muiIndexedChanges;	//The editor's change count the index has its text from, MAX_SIZE if it hasn't the editor's text.
	bool				mbCarriageReturns;	//The file's lines end with carriage returns and line feeds.
	size				muiSavedUndo;	//The editor's undo id when the file was opened or last saved: undone back to it, it isn't modified.
	bool				mbShownModified;	//The tab's name ends with a '*'.
	Ptr<CLineZones>		mpZones;		//The lines' code, from mcListings.
	CIDEListingSource	mcListings;
	EGitFileState		meGitState;		//As of the last git status read.
	bool				mbBase;			//The file has a text in the last commit to mark changes from.
	CChars				mszBaseText;	//That text.
	CChars				mszBaseHead;	//The commit it is from.
	size				muiBarChanges;	//The editor's change count when the change bars were worked out, MAX_SIZE for again.
	CArrayTextRange		masSyntaxRanges;	//What each node of the text's syntax was parsed from, by start then end, for Extend Selection.
	size				muiSyntaxChanges;	//The editor's change count they were parsed at, MAX_SIZE for none.
	CIDEOutline			mcOutline;			//What the text declares, as it was last parsed.
	size				muiOutlineChanges;	//The editor's change count it was parsed at, MAX_SIZE for none.
	CComponentEvent<CIDEEditorTab>	mcSyntaxBehind;	//Extend Selection wants the syntax and the ranges are behind the text.

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pTitleFont, Ptr<CFontFamily> pFamily, ARGB32 sColour, const char* szFileName, size uiProject, const char* szText);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				SetDockPane(Ptr<CDockPane> pPane);
	void				SetHighlights(CArrayHighlights* pacHighlights);
	void				SetSyntaxRanges(CArrayCPPSyntaxRanges* pasRanges, size uiChanges);
	void				SetOutline(CIDEOutline* pcOutline, size uiChanges);
	CIDEOutline*		GetOutline(void);
	CIDEOutline*		GetLastOutline(void);
	bool				IsOutlineCurrent(void);
	size				GetOutlineChanges(void) const;
	Ptr<CComponent>		GetHolder(void) const;
	void				SetTopBar(Ptr<CComponent> pBar);
	void				SetBottomBar(Ptr<CComponent> pBar);
	void				SetOverlay(Ptr<CComponent> pOverlay);
	void				ShowBreadcrumbs(CIDEContent* pcContent, bool bShown);
	void				ShowStickyLines(CIDEContent* pcContent, bool bShown);
	Ptr<CIDEBreadcrumbs>	GetBreadcrumbs(void) const;
	Ptr<CIDEStickyLines>	GetStickyLines(void) const;
	void				SetFindBar(Ptr<CIDEFindBar> pFindBar);
	Ptr<CIDEFindBar>	GetFindBar(void) const;
	size				GetKeptUsagesLayer(ARGB32 sColour);
	size				GetLinkLayer(ARGB32 sColour);
	size				GetBarChanges(void) const;
	CComponentEvent<CIDEEditorTab>*	GetSyntaxBehindEvent(void);
	void				AddSelectionRanges(size uiStart, size uiEnd, CArrayTextRange* pasDest) override;
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				SetCaretPlace(int iLine, int iColumn);
	size				FindPosition(int iLine, int iColumn);
	bool				TakeEdits(int64 iNow, bool bNow, bool bCanIndex, bool* pbHighlight, bool* pbIndex, int64* piWait);
	void				IndexRestarted(void);
	void				Indexed(size uiChanges);
	bool				IsIndexCurrent(void);
	void				RequestRecolour(void);

	void				SetListings(CCPP65ListingIndex* pcListings);
	bool				ToggleListing(size uiLine);
	Ptr<CLineZones>		GetZones(void) const;

	void				SetBaseText(const char* szText, size uiLength, const char* szHead);
	void				ClearBaseText(void);
	bool				HasBaseText(void) const;
	CChars*				GetBaseText(void);
	char*				GetBaseHead(void);
	void				SetGitState(EGitFileState eState);
	EGitFileState		GetGitState(void) const;
	bool				NeedsChangeBars(void);
	void				SetChangeBars(CGitLineChanges* pcChanges);

	Ptr<CTextArea>		GetEditor(void) const;
	Ptr<CCodeEditor>	GetCodeEditor(void) const;
	Ptr<CScrollPane>	GetPane(void) const;
	Ptr<CDockPane>		GetDockPane(void) const;
	char*				GetFileName(void);
	void				SetFileName(const char* szFileName);
	size				GetProject(void);
	void				SetProject(size uiProject);
	bool				IsEdited(void) const;
	void				SeeChanges(int64 iNow);

	bool				SaveFile(void);
	void				Reload(const char* szText);
	bool				IsModified(void);
	void				ForgetModified(void);
	bool				UsesCarriageReturns(void);
	void				ShowModified(void);
};


bool UsesIDECarriageReturns(const char* szText);
void ApplyIDEHighlights(Ptr<CTextArea> pTextArea, CArrayHighlights* pacHighlights);


#endif // __IDE_EDITOR_TAB_H__

