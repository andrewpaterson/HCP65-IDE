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
#ifndef __IDE_NAVIGATOR_H__
#define __IDE_NAVIGATOR_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/MapStringInt.h"
#include "WindowLib/GoToDialog.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/CommandSet.h"
#include "IDEIcons.h"
#include "IDELocationPreview.h"
#include "IDEChooser.h"


class CIDEContent;


//Getting about the code, as IntelliJ: Navigate's Go to Class (Control+N), File (Control+Shift+N) and Symbol
//(Control+Alt+Shift+N), whose dialog's searcher it is; Go to Source (Control+B), the file an #include names or a
//definition; and Alt+O, between a .cpp and its header.  The files it searches are the code model's list of the
//projects' files (CIDECodeModel::GetProjectFile).
//
//And the "go to"s that can have more than one answer (NavigationPlan.txt, S1): Go to Implementation (Control+Alt+B)
//Go to Super Method (Control+U) and Go to Type Declaration (Control+Shift+B).  Each finds its targets from the symbol
//at the caret and ends in GoToTargets; Go to Related Symbol (Control+Alt+Home) lists them all under headings, with
//the file's other half and the symbol's other declarations.  Control+click on a name in an editor (the text area's
//link event) puts the caret on it and goes where Go to Source would from there.  A "go to" ends in GoToTargets:
//none, and the status line says so; one, and it is gone to; several, and a chooser under the caret lists them with a
//preview of each (CIDEChooser).
#define IDE_GO_TO_RECENT			30			//The most of a tab's choices kept to show first.
#define IDE_GO_TO_RECENT_SCORE	1000000		//What puts an item before those only matched.


class CIDENavigator : public CObject, public CGoToSearcher
{
friend class CIDEScript;
CONSTRUCTABLE(CIDENavigator);
DESTRUCTABLE(CIDENavigator);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>	mpNavigateMenu;
	Ptr<CGoToDialog>	mpGoTo;  //While Go to Class, File or Symbol is shown.
	CChars				mszGoToPattern;  //What was typed in Go to last.
	int32				miGoToFileIcon;  //The dialog's first file icon, by EIDEFileType from it.
	int32				maiGoToSymbolIcons[IST_Count];
	int32				miQueuedGoTo;  //The Go to tab posted to be shown at the next tick (ShowQueuedGoTo).
	Ptr<CIDEChooser>	mpTargets;  //The chooser of a "go to" with several targets.
	Ptr<CIDELocationPreview>	mpGoToPreview;  //Fills the Go to dialog's preview, while it is shown.
	CArrayChars			maszRecentClasses;  //The keys of what Go to Class went to, the last one last,
	CArrayChars			maszRecentSymbols;  //and Go to Symbol.
	CChars				mszQueuedAction;  //The command Go to Action chose, run at the next tick.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	Ptr<CMenuContainer>	CreateNavigateMenu(void);
	void				AddEditorItems(Ptr<CMenuContainer> pMenu);
	void				GoToClass(void);
	void				GoToFile(void);
	void				GoToSymbol(void);
	void				GoToAction(void);
	void				GoToSelected(void);
	void				RunQueuedAction(void);
	void				RememberGoTo(size uiTab, size uiSymbol);
	void				ReleaseGoTo(void);
	void				PutGoToFirst(CGoToItems* pcDest, size uiData, size uiRank);
	void				FindGoToActions(CNameMatcher* pcMatcher, CGoToItems* pcDest);
	void				GoToSearch(size uiTab, CNameMatcher* pcMatcher, CGoToItems* pcDest) override;
	bool				ShowGoTo(size uiTab);
	void				QueueGoTo(size uiTab);
	bool				CreateGoTo(size uiTab);
	bool				OpenGoToChoice(void);
	Ptr<CGoToDialog>	GetGoTo(void) const;
	bool				GoToSource(void);
	bool				CanGoToSource(void);
	bool				SwitchHeaderSource(void);
	bool				GoToImplementation(void);
	bool				GoToSuperMethod(void);
	bool				GoToTypeDeclaration(void);
	bool				GoToRelatedSymbol(void);
	void				LinkPressed(CTextArea* pcEditor, size uiPosition);
	bool				FindHeaderSource(CChars* pszDest);
	bool				FindTypeOf(const char* szKey, uint8 uiKind, CChars* pszTypeKey);
	bool				CanGoToImplementation(void);
	bool				CanGoToSuperMethod(void);
	bool				FindCaretSymbol(CChars* pszKey, uint8* puiKind);
	bool				FindCaretFunction(CChars* pszKey, uint8* puiKind);
	bool				GoToTargets(const char* szWhat, const char* szOf, CArrayIDETargets* pasTargets);
	Ptr<CIDEChooser>	GetTargetsChooser(void) const;
	void				Detach(void);

protected:
	bool				GetIncludeAtCaret(CChars* pszName, bool* pbSystem);
	bool				FindIncludedFile(CChars* pszDest);
	bool				FindProjectFileEnding(const char* szEnd, CChars* pszDest);
	bool				OpenProjectFile(const char* szFileName);
	void				ShowQueuedGoTo(void);
};


#endif // __IDE_NAVIGATOR_H__

