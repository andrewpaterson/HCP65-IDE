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
#ifndef __IDE_RESULTS_PANE_H__
#define __IDE_RESULTS_PANE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/TreeContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/SplitContainer.h"
#include "WindowLib/CodePreview.h"
#include "WindowLib/Toolbar.h"
#include "WindowLib/CommandSet.h"
#include "IDEIcons.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"
#include "IDECodeModel.h"
#include "IDEDocuments.h"


class CIDEContent;
class CIDEResultsPane;


//Whatever fills a results pane with its tree (Find Usages, Find in Files): it knows where each result is.
class CIDEResultsSource
{
public:
	//The place of the result a node's data (above 0) is of.  psDest isn't initialised: its file name is made, for the
	//caller to kill, only if it answers true.
	virtual bool	GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) =0;

	//Where what the results are of is declared (the symbol Find Usages found the usages of), made as a result's is.
	//False if they are of nothing that is declared, as text found is.
	virtual bool	GetDeclarationLocation(CIDEResultsPane* pcPane, SIDELocation* psDest);

	//Whether what was looked for can be looked for again, and doing so (the pane's Rerun).
	virtual bool	CanRerunResults(CIDEResultsPane* pcPane);
	virtual void	RerunResults(CIDEResultsPane* pcPane);
};


//What a tool window of places found shows (Find Usages', Find in Files' Find): a tree whose nodes with data above 0
//are results, each a line of a file, under nodes that group them (with data 0).  Its source makes the nodes (between
//BeginShow and EndShow) and says where each result is; the pane opens a result's place when it is double clicked or
//enter is pressed on it, steps through the results for Next and Previous Occurrence, and gives the keys back to the
//editor on escape.  As IntelliJ's, Jump to Source (F4) goes to where what the results are of is declared, whichever
//row is selected (or, for results of nothing declared, opens the selected one as enter does); a click or the arrow
//keys only select; and a character typed, space too, isn't the tree's: the active editor gets the keys and has it
//typed in it.  With Navigate with Single Click (SetNavigateOnSelect, off to start with) a result is opened as it is
//selected, by a click or the arrow keys, the tree keeping the keys.
//
//Its rows are made as IntelliJ's are (AddGroup, AddResult): a group's name with what is said of it after (how many
//are in it, its directory) dim, a file's with its file type's icon, and a result's line with its line number dim, the
//text found in the match font and anything noted of it dim.
//
//Right of the tree, across a sash, is a preview of the selected result (a CCodePreview): its file's code at its line,
//the text found coloured, as its editor has it if it is open, and coloured as an editor is by the edits thread a
//moment after.  Whether it is shown, and the tree's share of the pane, are settings its owner keeps (ReadSettings,
//WriteSettings) and are written when they change.
//
//Down its left is a toolbar, as IntelliJ's: Rerun (its source looks again), Previous and Next Occurrence, Expand All
//and Collapse All, then whatever its source adds (Find Usages' groupings; AddToolbarCommand), then Preview and
//Navigate with Single Click (AddViewCommands, called by its owner after its own).  Their commands are the pane's own
//(GetCommands), not the application's: they have no keys.  A button is on while what it turns on is, and dim while it
//can't be used.
//
//Whatever lets go of a pane before the IDE closes calls Detach first.
class CIDEResultsPane : public CObject, public CIDEPreviewAsker
{
CONSTRUCTABLE(CIDEResultsPane);
DESTRUCTABLE(CIDEResultsPane);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds what holds the pane.
	CIDEResultsSource*	mpcSource;  //Not held: it holds the pane.
	Ptr<CTreeContainer>	mpTree;
	Ptr<CScrollPane>	mpScrollPane;
	Ptr<CInputAction>	mpEscapeAction;  //Escape in the tree goes back to the editor.
	Ptr<CInputAction>	mpJumpAction;  //F4 in it goes to the declaration.
	Ptr<CInputAction>	mpSpaceAction;  //Space, and a character typed, in it are typed in the editor.
	Ptr<CInputAction>	mpTypeAction;
	Ptr<CGridContainer>	mpPanel;  //The toolbar and, right of it, the split.
	Ptr<CToolbar>		mpToolbar;
	Ptr<CCommandSet>	mpCommands;  //The toolbar's.
	Ptr<CSplitContainer>	mpSplit;  //The tree and, right of it, the preview.
	Ptr<CCodePreview>	mpPreview;
	size				muiPreviewChanges;  //The previewed file's editor's change count when its text was taken, MAX_SIZE if it wasn't open.
	CChars				mszNoun;  //What a result is called in the status line: "usages", "matches".
	bool				mbSelectOnOpen;  //A result opened from the tree has its text selected in its editor.
	bool				mbNavigateOnSelect;  //A result is opened as it is selected.
	bool				mbShowing;  //The tree is being made, or stepped through: what is selected isn't the user's doing.

public:
	void	Init(CIDEContent* pcContent, CIDEResultsSource* pcSource, const char* szNoun, bool bSelectOnOpen);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	Ptr<CTreeContainer>	GetTree(void);
	Ptr<CComponent>		GetComponent(void);

	int64				BeginShow(bool bKeepSelected);
	void				EndShow(int64 iSelectedData);
	void				Clear(void);
	int32				AddGroup(int32 iParent, const char* szName, const char* szDetail, const char* szFileName = NULL);
	int32				AddResult(int32 iParent, CIDEFileLines* pcLines, const char* szFileName, int32 iLine, int32 iColumn, int32 iLength, const char* szNote, int64 iData);
	int32				FindResultNode(int64 iData);
	int64				GetResultData(int32 iNode);
	bool				HasResults(void);
	bool				OpenResult(int32 iNode, bool bSelect);
	bool				Step(bool bNext, bool bKeepKeys);
	void				ShowSelected(void);
	void				Focus(void);
	void				Detach(void);

	Ptr<CCommandSet>	GetCommands(void);
	Ptr<CToolbar>		GetToolbar(void);
	void				AddToolbarCommand(Ptr<CCommand> pCommand, EIDEToolIcon eIcon);
	void				AddToolbarSeparator(void);
	void				AddViewCommands(void);
	void				RefreshToolbar(void);
	bool				CanStep(bool bNext);

	void				SetNavigateOnSelect(bool bNavigate);
	bool				IsNavigateOnSelect(void);
	void				SetPreviewShown(bool bShown);
	bool				IsPreviewShown(void);
	Ptr<CCodePreview>	GetPreview(void);
	Ptr<CSplitContainer>	GetSplit(void);
	void				ShowPreview(void);
	void				PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights) override;
	void				ReadSettings(CMarkupTag* pcTag);
	void				WriteSettings(CXMLWriter* pcWriter);

protected:
	void				ResultActivated(void);
	void				ResultSelected(void);
	void				ShareChanged(void);
	void				AddCommand(const char* szName, const char* szCaption, EIDEToolIcon eIcon, void(CIDEResultsPane::*fRun)(void), bool(CIDEResultsPane::*fEnabled)(void), bool(CIDEResultsPane::*fChecked)(void));
	void				Rerun(void);
	bool				CanRerun(void);
	void				StepPrevious(void);
	void				StepNext(void);
	bool				CanStepPrevious(void);
	bool				CanStepNext(void);
	void				ExpandAll(void);
	void				CollapseAll(void);
	void				TogglePreview(void);
	void				ToggleNavigateOnSelect(void);
	void				ReturnToEditor(void);
	void				JumpToSource(void);
	void				TypeSpace(void);
	void				TypeCharacter(SComponentInputEvent* psEvent);
};


#endif // __IDE_RESULTS_PANE_H__

