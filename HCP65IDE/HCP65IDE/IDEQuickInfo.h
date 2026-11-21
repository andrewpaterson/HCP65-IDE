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


//What the popup over the caret's line is showing.
enum EIDEQuickInfo
{
	IQI_None,
	IQI_Parameters,
	IQI_Type,
};


//What a name is, shown where it is written (NavigationPlan.txt's stage F), as IntelliJ IDEA's:
//
//  - With Control held, the name under the pointer that Control+click would go from is underlined as a link, and
//    its declaration is shown under it with where it is declared.  Letting Control go, or moving off the name,
//    takes both away.
//  - Quick Definition (Control+Shift+I): the definition of the symbol at the caret, its code previewed in a chooser
//    with its implementations if it has them; Enter goes to the one selected, Escape closes it.
//  - Parameter Info (Control+P): the parameters of the call the caret is in, a row for each function of that name
//    its class declares, the parameter the caret's argument is given to brighter; it follows the caret, and goes when
//    the caret leaves the call, on Escape or a press.
//  - Type Info (Control+Shift+P): the type of the selected expression (found as Introduce Variable finds it), or of
//    the symbol at the caret as it is declared; it goes when the caret moves.
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

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddViewItems(Ptr<CMenuContainer> pMenu);
	void	Poll(void);
	void	TabClosed(Ptr<CIDEEditorTab> pTab);
	void	EscapeAction(SComponentInputEvent* psEvent);
	void	Detach(void);

	bool	ShowQuickDefinition(void);
	bool	ShowParameterInfo(void);
	bool	ShowTypeInfo(void);
	bool	HasEditor(void);

	bool	ShowLinkAt(Ptr<CIDEEditorTab> pTab, size uiPosition);
	void	HoldLink(bool bHeld);
	bool	ClearLink(void);
	bool	HideInfo(void);
	bool	IsLinkShown(void);
	bool	IsInfoShown(void);
	void	Print(CChars* pszDest);

	Ptr<CIDEChooser>	GetDefinitionsChooser(void) const;

protected:
	void	PollLink(void);
	void	PollInfo(void);
	bool	UpdateParameterInfo(bool bSay);
	void	FindOverloads(const char* szKey);
	void	ShowParameters(void);
	void	ShowOverCaret(void);
	void	KillOverloads(void);
};


#endif // __IDE_QUICK_INFO_H__

