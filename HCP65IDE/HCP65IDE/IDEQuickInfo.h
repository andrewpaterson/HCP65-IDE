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
#ifndef __IDE_QUICK_INFO_H__
#define __IDE_QUICK_INFO_H__
#include "StandardLib/Object.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/InfoPopup.h"
#include "WindowLib/MenuContainer.h"
#include "IDEChooser.h"
#include "IDESymbolInfo.h"


class CIDEContent;
class CIDEEditorTab;


#define IDE_DOCUMENTATION_LINES	16		//The lines of Quick Documentation shown before it scrolls.
#define IDE_TIP_COMMENT_LINES	3		//The lines of a comment a name's tool tip shows.
#define IDE_TIP_DELAY			600		//Milliseconds the pointer rests on a name before its tool tip shows.


//A part of Quick Documentation's text that goes somewhere when it is pressed.
struct SIDEDocumentationLink
{
	size			uiStart;
	size			uiLength;
	SIDELocation	sPlace;
};


typedef CArrayTemplate<SIDEDocumentationLink>	CArrayIDEDocumentationLinks;


//What the popup over the caret's line is showing.
enum EIDEQuickInfo
{
	IQI_None,
	IQI_Parameters,
	IQI_Type,
};


//What a name is, shown where it is written (NavigationPlan.txt's stage F), as IntelliJ IDEA's:
//
//  - With Control held, the name under the pointer that Control+click would go from is underlined as a link, the
//    pointer is a hand over the editor, and the name's declaration is shown under it with where it is declared.
//    Letting Control go, or moving off the name, takes them away.
//  - Quick Definition (Control+Shift+I): the definition of the symbol at the caret, its code previewed in a chooser
//    with its implementations if it has them; Enter goes to the one selected, Escape closes it.
//  - Parameter Info (Control+P): the parameters of the call the caret is in, a row for each function of that name
//    its class declares, the parameter the caret's argument is given to brighter; it follows the caret, and goes when
//    the caret leaves the call, on Escape or a press.
//  - Type Info (Control+Shift+P): the type of the selected expression (found as Introduce Variable finds it), or of
//    the symbol at the caret as it is declared; it goes when the caret moves.
//
//  - Quick Documentation (Control+Q): what is written about the symbol at the caret, under its line: its declaration
//    in the code's colours, where it is declared, the comment over it as it is written, and the documentation pages
//    that name it (ReadIDESymbolDocumentation).  A press on where it is declared, or on a page, goes there.  It is
//    no taller than IDE_DOCUMENTATION_LINES lines: the wheel over it, Page Up and Page Down, and Control with Up and
//    Down scroll it.  It goes when the caret moves, on Escape or a press elsewhere; Control+Q again while it shows
//    keeps it, and then it follows the caret to each symbol it comes to, until Control+Q once more or Escape.
//  - The tool tip over a name: the pointer resting on a name of the active editor for IDE_TIP_DELAY shows the same,
//    shorter (the declaration, where, and the first lines of the comment), under the name.  It goes when the pointer
//    leaves the name, the caret moves or the text changes, on Escape or a press, and isn't shown while Control is
//    held (the link is) or another popup is open.  Settings' Editor page turns it off.
//
//The popups are CInfoPopups, which take no keys; what they say is read from the declaring file's text
//(ReadIDESymbolInfo).  Poll, called each window tick, follows the pointer and the caret.
class CIDEQuickInfo : public CObject
{
CONSTRUCTABLE(CIDEQuickInfo);
DESTRUCTABLE(CIDEQuickInfo);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CInfoPopup>			mpLinkPopup;  //The declaration of the name Control is held over.
	Ptr<CInfoPopup>			mpInfoPopup;  //Parameter Info or Type Info, over the caret's line.
	Ptr<CIDEChooser>		mpDefinitions;  //Quick Definition's.

	Ptr<CIDEEditorTab>		mpLinkTab;  //The tab with a name underlined, NULL for none.
	size					muiLinkStart;  //The name,
	size					muiLinkEnd;
	size					muiLinkChanges;  //and its editor's change count then.
	CChars					mszLinkName;
	ENativeCursor			meLinkCursor;  //The editor's pointer shape before the hand: put back when the link goes.
	Ptr<CIDEEditorTab>		mpAskedTab;  //Where the pointer was when the name under it was last looked for.
	size					muiAskedPosition;
	size					muiAskedChanges;
	bool					mbLinkHeld;  //Shown by a check's script: the pointer isn't followed.

	EIDEQuickInfo			meInfo;
	Ptr<CIDEEditorTab>		mpInfoTab;
	size					muiInfoCaret;  //The caret and the change count it was shown for.
	size					muiInfoChanges;
	size					muiInfoOpen;  //Parameter Info: the call's bracket, its name, the caret's argument,
	CChars					mszInfoName;
	size					muiInfoArgument;
	CArrayIDESymbolInfos	masOverloads;  //and the functions of that name.
	CArrayInt				maiBold;  //The caret's parameter in each of the popup's rows: a start and a length each.

	Ptr<CInfoPopup>			mpDocumentation;  //Quick Documentation, under the caret's line.
	Ptr<CIDEEditorTab>		mpDocumentationTab;  //The tab it is shown for, NULL for none.
	size					muiDocumentationCaret;  //The caret and the change count it was shown for.
	size					muiDocumentationChanges;
	bool					mbDocumentationPinned;  //It follows the caret rather than going when it moves.
	CChars					mszDocumentationName;  //The name it is about.
	CArrayIDEDocumentationLinks	masDocumentationLinks;
	bool					mbDocumentationPressed;  //A press on it waits for the next tick,
	size					muiDocumentationPressed;  //on this character of its text, or MAX_SIZE.

	Ptr<CInfoPopup>			mpTip;  //The tool tip of the name the pointer rests on.
	Ptr<CIDEEditorTab>		mpTipTab;  //The tab with a name tipped, NULL for none.
	CChars					mszTipName;
	bool					mbToolTips;  //The settings': names have tool tips.
	bool					mbTipHeld;  //Shown by a check's script: the pointer isn't followed.
	Ptr<CIDEEditorTab>		mpRestTab;  //The name the pointer is on: its tab, where it starts,
	size					muiRestStart;
	size					muiRestChanges;  //its editor's change count and caret then,
	size					muiRestCaret;
	SInt32Vec2				msRestPointer;  //where the pointer was last seen and when it last moved,
	int64					miRestTime;
	bool					mbRestDone;  //and whether its tip has been shown or looked for: not again until another name.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddViewItems(Ptr<CMenuContainer> pMenu);
	int64	Poll(int64 iNow);
	void	TabClosed(Ptr<CIDEEditorTab> pTab);
	void	EscapeAction(SComponentInputEvent* psEvent);
	void	ScrollAction(SComponentInputEvent* psEvent);
	void	Detach(void);

	bool	ShowQuickDefinition(void);
	bool	ShowParameterInfo(void);
	bool	ShowTypeInfo(void);
	bool	ShowQuickDocumentation(void);
	bool	HasEditor(void);

	bool	ShowLinkAt(Ptr<CIDEEditorTab> pTab, size uiPosition);
	void	HoldLink(bool bHeld);
	bool	ClearLink(void);
	bool	HideInfo(void);
	bool	IsLinkShown(void);
	bool	IsInfoShown(void);
	bool	HideDocumentation(void);
	bool	IsDocumentationShown(void);
	bool	IsDocumentationPinned(void) const;
	size	NumDocumentationLinks(void) const;
	bool	PressDocumentationLink(size uiLink);
	bool	ShowTipAt(Ptr<CIDEEditorTab> pTab, size uiPosition);
	void	HoldTip(bool bHeld);
	bool	HideTip(void);
	bool	IsTipShown(void);
	void	SetToolTips(bool bToolTips);
	bool	IsToolTips(void) const;
	void	Print(CChars* pszDest);

	Ptr<CIDEChooser>	GetDefinitionsChooser(void) const;

protected:
	void	PollLink(void);
	void	PollInfo(void);
	void	PollDocumentation(void);
	int64	PollTip(int64 iNow);
	bool	ShowDocumentationAt(Ptr<CIDEEditorTab> pTab, size uiPosition);
	bool	WriteDocumentation(Ptr<CIDEEditorTab> pTab, size uiPosition, bool bTip, Ptr<CInfoPopup> pPopup, CChars* pszName, size* puiStart);
	void	DocumentationPressed(CInfoPopup* pcPopup, size uiPosition);
	void	KillDocumentationLinks(void);
	bool	UpdateParameterInfo(bool bSay);
	void	FindOverloads(const char* szKey);
	void	ShowParameters(void);
	void	ShowOverCaret(void);
	void	KillOverloads(void);
};


#endif // __IDE_QUICK_INFO_H__

