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
#ifndef __IDE_FIND_H__
#define __IDE_FIND_H__
#include "BaseLib/TextSearch.h"
#include "StandardLib/Object.h"
#include "WindowLib/Button.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/TextField.h"


class CIDEContent;
class CIDEEditorTab;
class CMarkupTag;
class CXMLWriter;


#define IDE_FIND_MAX_MATCHES	100000	//The most matches a Find bar keeps of one search.
#define IDE_FIND_MARKS			1		//The Find bar's marks' source on an editor's scroll bar (CScrollBar::SetMarks).
#define IDE_FIND_NONE			MAX_SIZE


//An editor's Find bar, as IntelliJ IDEA's (NextIDEWork.txt's item 2; NavigationPlan.txt's stage D): a strip over the
//code, not a dialog.  What is typed in its field is looked for in the file as it is typed: every match is
//highlighted (a layer of the editor's, CTextArea::AddHighlightLayer) and marked on the scroll bar, the first from where
//the caret was is selected and shown, and the bar says which it is of how many ("3 of 17").  Enter goes to the next
//and Shift+Enter to the previous, round the ends; Escape closes the bar and leaves the match selected.  Its ticks are
//Match Case, Words and Regex (BaseLib's CRegex), which every bar shares and the settings keep, and In Selection, which
//keeps the search to the text that was selected when the bar opened (ticked by itself when that was more than a
//line).  A regular expression that isn't one is said to be wrong, in the bar.
//
//Opened to replace it has a second row: what to replace with (a regular expression's $1 for a group), Replace (the
//current match, then on to the next) and Replace All (every match as one edit: one undo puts them all back).
//
//After the file is edited it searches again at the next tick (Update): the highlights are of the text as it is.
class CIDEFindBar : public CObject
{
CONSTRUCTABLE(CIDEFindBar);
DESTRUCTABLE(CIDEFindBar);
protected:
	CIDEContent*		mpcContent;  //Not held.
	CIDEEditorTab*		mpcTab;  //Not held: the tab holds this.
	Ptr<CGridContainer>	mpBar;  //What the tab's upper strip holds while it is open.
	Ptr<CGridContainer>	mpReplaceRow;
	Ptr<CTextField>		mpFindField;
	Ptr<CTextField>		mpReplaceField;
	Ptr<CLabel>			mpCount;
	Ptr<CCheckBox>		mpMatchCase;
	Ptr<CCheckBox>		mpWords;
	Ptr<CCheckBox>		mpRegex;
	Ptr<CCheckBox>		mpInSelection;
	Ptr<CButton>		mpPrevious;
	Ptr<CButton>		mpNext;
	Ptr<CButton>		mpClose;
	Ptr<CButton>		mpReplace;
	Ptr<CButton>		mpReplaceAll;
	CTextSearch			mcSearch;
	CArrayTextRange		masMatches;
	CChars				mszCount;  //What the bar says of the search.
	size				muiCurrent;  //The match that is selected, IDE_FIND_NONE for none.
	size				muiLayer;  //The editor's layer of highlights, IDE_FIND_NONE until it is made.
	size				muiSearchedChanges;  //The editor's change count the matches are of.
	size				muiSearchedLength;  //And its text's length then.
	size				muiAnchor;  //Where the first match to select is looked for from.
	STextRange			msScope;  //The text that was selected when it was opened, for In Selection.
	bool				mbOpen;
	bool				mbReplacing;  //It has its second row.
	bool				mbSetting;  //It is setting its own fields and ticks: their events aren't the user's.

public:
	void			Init(CIDEContent* pcContent, CIDEEditorTab* pcTab);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			Open(bool bReplace);
	void			Close(void);
	bool			IsOpen(void) const;
	bool			IsReplacing(void) const;
	void			Update(void);
	void			Search(bool bSelect);
	bool			Next(void);
	bool			Previous(void);
	bool			ReplaceCurrent(void);
	size			ReplaceAll(void);

	void			SetFindText(const char* szText);
	const char*		GetFindText(void);
	void			SetReplaceText(const char* szText);
	size			NumMatches(void) const;
	size			GetCurrent(void) const;
	Ptr<CTextField>	GetFindField(void) const;
	Ptr<CTextField>	GetReplaceField(void) const;
	Ptr<CCheckBox>	GetTick(const char* szName);
	void			Print(CChars* pszDest);

protected:
	Ptr<CButton>	CreateButton(const char* szCaption);
	void			CreateBar(void);
	void			ShowOptions(void);
	void			FieldChanged(void);
	void			TickChanged(void);
	void			ScopeChanged(void);
	void			NextClicked(void);
	void			PreviousClicked(void);
	void			ReplaceClicked(void);
	void			ReplaceAllClicked(void);
	void			SelectCurrent(void);
	void			ShowMatches(void);
	void			ClearMatches(void);
	bool			GetReplacement(size uiMatch, CChars* pszDest);
};


//Find and Replace in the active editor (see CIDEFindBar): Find (Control+F) and Replace (Control+R) open its bar,
//filled from the selection or the word at the caret; Find Next (F3) and Find Previous (Shift+F3) step through the
//bar's matches, or with the bar closed look for what was last searched for from the caret; Find Word at Caret
//(Control+F3) looks for the word the caret is in.  It keeps what every bar shares, written with the settings: Match
//Case, Words and Regex, and what was last searched for.
class CIDEFind : public CObject
{
CONSTRUCTABLE(CIDEFind);
DESTRUCTABLE(CIDEFind);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	CChars			mszLast;  //What was last searched for, for Find Next with no bar open.
	bool			mbMatchCase;
	bool			mbWords;
	bool			mbRegex;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditItems(Ptr<CMenuContainer> pMenu);
	bool	ShowFind(void);
	bool	ShowReplace(void);
	bool	FindNext(void);
	bool	FindPrevious(void);
	bool	FindWordAtCaret(void);
	void	Poll(void);

	bool	IsMatchCase(void) const;
	bool	IsWords(void) const;
	bool	IsRegex(void) const;
	void	SetOptions(bool bMatchCase, bool bWords, bool bRegex);
	void	SetLast(const char* szText);
	const char*	GetLast(void);
	Ptr<CIDEFindBar>	GetBar(Ptr<CIDEEditorTab> pTab);
	Ptr<CIDEFindBar>	GetActiveBar(void);
	void	ReadSettings(CMarkupTag* pcSettings);
	void	WriteSettings(CXMLWriter* pcWriter);
	bool	HasEditor(void);

protected:
	bool	ShowBar(bool bReplace);
	bool	Step(bool bNext);
};


bool FindIDEWordAt(const char* szText, size uiPosition, size* puiStart, size* puiEnd);


#endif // __IDE_FIND_H__

