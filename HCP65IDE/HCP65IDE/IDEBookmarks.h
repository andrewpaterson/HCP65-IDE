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
#ifndef __IDE_BOOKMARKS_H__
#define __IDE_BOOKMARKS_H__
#include "BaseLib/ArrayInt.h"
#include "StandardLib/Object.h"
#include "WindowLib/CodeEditor.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/PromptPopup.h"
#include "IDEResultsPane.h"
#include "IDEUserFile.h"


class CIDEContent;
class CIDEEditorTab;
struct SIDEPlaceChange;


#define IDE_BOOKMARK_MARKS		2	//The bookmarks' marks' source on an editor's scroll bar (CScrollBar::SetMarks).
#define IDE_BOOKMARKS_REFRESH	500	//Milliseconds after the last edit that moved one before the list and the file catch up.


//A line a user has marked to come back to: its file, its line (from 0) and its mnemonic, a digit or a capital letter,
//'\0' for a plain one.
struct SIDEBookmark
{
	CChars	szFile;
	int32	iLine;
	int32	iColumn;
	char	cMnemonic;
};


typedef CArrayTemplate<SIDEBookmark>	CArrayIDEBookmarks;


//Bookmarks, as IntelliJ IDEA's (NextIDEWork.txt's item 7, with the user's keys; NavigationPlan.txt's stage C).
//Toggle Bookmark (Control+F11) marks the caret's line, or unmarks it.  Toggle Bookmark with Mnemonic
//(Control+Shift+F11) asks for a digit or a letter in a prompt over the editor and gives the line that mnemonic,
//taking it from the line that had it; Control+0 to Control+9 go to the bookmark with that digit, in whichever file.
//Next Bookmark (F2) and Previous Bookmark (Shift+F2) go through the active file's, round its ends.
//
//A bookmark is shown in its editor's gutter (the first icon column: its mnemonic on a cell of the accent colour, the
//cell alone for a plain one; a press on it takes the bookmark off) and marked on its scroll bar.  The Bookmarks tool
//window lists them by file, each with its line's text and a preview, as the Find tool window lists what it found;
//Enter or a double click goes to one.
//
//They are the workspace's user's: kept in its .hcp65user (CIDEUserFile) and read when it opens.  They move with
//their lines as their files are edited (AdjustPlaces): the gutter at once, the list and the file a moment after the
//edits stop.
class CIDEBookmarks : public CObject, public CIDEUserPart, public CIDEResultsSource
{
CONSTRUCTABLE(CIDEBookmarks);
DESTRUCTABLE(CIDEBookmarks);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	CArrayIDEBookmarks		masBookmarks;  //In the order they were made.
	CArrayInt				maiListed;  //The list's order: by file, then line.  A result's data is its place here and one.
	Ptr<CIDEResultsPane>	mpPane;
	Ptr<CDockPane>			mpTool;
	Ptr<CPromptPopup>		mpPrompt;  //The mnemonic's, made when it is first shown.
	bool					mbMoved;  //Edits have moved bookmarks since the list was shown, the last at miMoved.
	int64					miMoved;
	bool					mbMarksDue;  //A tab's scroll bar marks couldn't be placed: its lines weren't laid out.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddNavigateItems(Ptr<CMenuContainer> pMenu);
	void	AddEditorItems(Ptr<CMenuContainer> pMenu);
	void	CreatePane(SDockPlace* psPlace);

	bool	ToggleBookmark(void);
	int32	GetSelectedBookmark(void);
	bool	CanRemoveSelectedBookmark(void);
	void	RemoveSelectedBookmark(void);
	bool	ToggleMnemonicBookmark(void);
	bool	SetMnemonicBookmark(char cMnemonic);
	bool	NextBookmark(void);
	bool	PreviousBookmark(void);
	bool	GoToMnemonic(char cMnemonic);
	bool	GoToBookmark0(void);
	bool	GoToBookmark1(void);
	bool	GoToBookmark2(void);
	bool	GoToBookmark3(void);
	bool	GoToBookmark4(void);
	bool	GoToBookmark5(void);
	bool	GoToBookmark6(void);
	bool	GoToBookmark7(void);
	bool	GoToBookmark8(void);
	bool	GoToBookmark9(void);

	size			NumBookmarks(void) const;
	SIDEBookmark*	GetBookmark(size uiIndex);
	int32			FindBookmark(const char* szFile, int32 iLine);
	int32			FindMnemonic(char cMnemonic);
	void			ShowBookmarks(Ptr<CIDEEditorTab> pTab);
	void			IconClicked(CCodeEditor* pcEditor, SInt32Vec2 sIcon);
	void			AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	int64			Poll(int64 iNow);
	void			Clear(void);
	void			Print(CChars* pszDest);
	Ptr<CIDEResultsPane>	GetResultsPane(void) const;
	Ptr<CDockPane>			GetTool(void) const;
	Ptr<CPromptPopup>		GetPrompt(void) const;

	void	WriteUser(CXMLWriter* pcWriter) override;
	void	ReadUser(CMarkupTag* pcRoot) override;
	bool	GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;
	bool	HasEditor(void);

protected:
	void	AddBookmark(const char* szFile, int32 iLine, char cMnemonic);
	void	RemoveBookmark(size uiIndex);
	void	RemoveAll(void);
	void	Changed(const char* szFile, const char* szOtherFile);
	void	ShowList(bool bKeepSelected);
	void	ShowFile(const char* szFile);
	bool	Step(bool bNext);
	void	MnemonicAccepted(void);
};


#endif // __IDE_BOOKMARKS_H__

