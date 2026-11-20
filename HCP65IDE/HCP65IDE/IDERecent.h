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
#ifndef __IDE_RECENT_H__
#define __IDE_RECENT_H__
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Object.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDEChooser.h"
#include "IDENavigationHistory.h"
#include "IDEUserFile.h"


class CIDEContent;
class CIDEEditorTab;
struct SIDEPlaceChange;


#define IDE_RECENT_FILES		50	//The most files kept as recent.
#define IDE_RECENT_EDITS		50	//The most places edited kept.
#define IDE_RECENT_EDIT_LINES	3	//An edit this near the last one, in lines, is the same place edited.
#define IDE_RECENT_WRITE		2000	//Milliseconds after the recent files change before the user's file is written.


//A file that was shown in an editor: where its caret was when it was last looked at here.
struct SIDERecentFile
{
	CChars	szFile;
	int32	iLine;
	int32	iColumn;
};


typedef CArrayTemplate<SIDERecentFile>	CArrayIDERecentFiles;


//What the chooser is choosing among.
enum EIDERecentChooser
{
	IRC_None,
	IRC_Files,
	IRC_Locations,
	IRC_Switcher,
	IRC_SelectIn,
};


//Getting about between files by where the user has been (NavigationPlan.txt's stage E), as IntelliJ IDEA's.  It
//keeps the files shown in editors, the most recently shown first, the closed ones too (heard from the content as a
//tab is chosen or closed), and the places the text was edited (heard from the documents).
//
//  - Recent Files (Control+E): a chooser of them, the open ones first, the file shown before this one selected so
//    Control+E then Enter goes back to it; typing filters.  A closed one opens where its caret was.
//  - Recent Locations (Control+Shift+E): a chooser of the places Back has, the newest first, each its line's text
//    with its file and line and a preview of the code; its button lists only the places edited, and back.
//  - The Switcher (Control+Tab, with Shift backwards): the open files, most recent first, then the tool windows.
//    Held open while Control is down, each Tab (or Down, Up) moving on; letting go chooses.  A quick Control+Tab
//    so goes straight to the file before this one.
//  - Select Next Tab and Select Previous Tab (Alt+Right, Alt+Left): the tab beside the active one, round the ends.
//  - Reopen Closed Tab: the tab closed last, where its caret was.
//  - Last Edit Location (Control+Shift+Backspace) goes back through the places edited, Next Edit Location forward.
//  - Select In (Alt+F1): a chooser of where the active file can be shown selected: Solution Explorer, Structure.
//
//The recent files are the workspace's user's, kept in its .hcp65user (a part of CIDEUserFile) where settings are
//kept; the places edited, as Back's, are the session's.
class CIDERecent : public CObject, public CIDEUserPart
{
CONSTRUCTABLE(CIDERecent);
DESTRUCTABLE(CIDERecent);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	CArrayIDERecentFiles		masFiles;  //The most recently shown first.
	CArrayChars					maszClosed;  //The files of tabs that were closed, the last closed last.
	CArrayIDENavigationPlaces	masEdits;  //Where the text was edited, the last last.
	size						muiEdit;  //The place Last Edit Location is at: their number when it hasn't gone back.
	Ptr<CIDEChooser>			mpChooser;
	EIDERecentChooser			meChooser;
	CArrayInt					maiChoices;  //For the Switcher: a place's tool window (the dock's pane), -1 for a file.
	bool						mbChangedOnly;  //Recent Locations lists the places edited.
	bool						mbDirty;  //The recent files have changed since the user's file was written, at miDirty.
	int64						miDirty;
	bool						mbLettingGo;  //The workspace is closing: its tabs' closing isn't the user's.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddNavigateItems(Ptr<CMenuContainer> pMenu);
	void	AddWindowItems(Ptr<CMenuContainer> pMenu);

	void	FileShown(Ptr<CIDEEditorTab> pTab);
	void	FileClosed(Ptr<CIDEEditorTab> pTab);
	void	FileEdited(const char* szFile, SIDEPlaceChange* psChange);
	void	AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	void	LetGo(void);
	void	Clear(void);
	int64	Poll(int64 iNow);

	bool	ShowRecentFiles(void);
	bool	ShowRecentLocations(void);
	bool	ShowSwitcher(void);
	bool	ShowSwitcherBackward(void);
	bool	OpenSwitcher(bool bBackward, bool bHeld);
	bool	SelectNextTab(void);
	bool	SelectPreviousTab(void);
	bool	ReopenClosedTab(void);
	bool	CanReopenClosedTab(void);
	bool	GoToLastEdit(void);
	bool	GoToNextEdit(void);
	bool	ShowSelectIn(void);
	bool	HasEditor(void);

	size				NumFiles(void) const;
	SIDERecentFile*		GetFile(size uiIndex);
	size				NumEdits(void) const;
	Ptr<CIDEChooser>	GetChooser(void) const;
	void				Print(CChars* pszDest);

	void	WriteUser(CXMLWriter* pcWriter) override;
	void	ReadUser(CMarkupTag* pcRoot) override;

protected:
	int32	FindFile(const char* szFile);
	void	Remember(const char* szFile, int32 iLine, int32 iColumn, bool bFront);
	void	RemoveAll(void);
	void	RecentChanged(void);
	void	BeginChooser(EIDERecentChooser eChooser, const char* szTitle, bool bFilter, bool bPreview, const char* szButton);
	bool	OpenChooser(size uiSelected);
	void	AddFilePlace(const char* szFile, int32 iLine, int32 iColumn);
	void	Chosen(void);
	void	ButtonClicked(void);
	void	ReopenLocations(void);
	void	SwitcherReleased(SComponentInputEvent* psEvent);
	void	SwitcherUp(SComponentInputEvent* psEvent);
	void	SwitcherDown(SComponentInputEvent* psEvent);
	bool	StepSwitcher(bool bBackward);
	bool	StepTab(bool bNext);
	bool	GoToEdit(size uiEdit);
};


#endif // __IDE_RECENT_H__

