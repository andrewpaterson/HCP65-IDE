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
#include "IDECodeModel.h"


class CIDEContent;
class CIDEResultsPane;


//Whatever fills a results pane with its tree (Find Usages, Find in Files): it knows where each result is.
class CIDEResultsSource
{
public:
	//The place of the result a node's data (above 0) is of.  psDest isn't initialised: its file name is made, for the
	//caller to kill, only if it answers true.
	virtual bool	GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) =0;
};


//What a tool window of places found shows (Find Usages', Find in Files' Find): a tree whose nodes with data above 0
//are results, each a line of a file, under nodes that group them (with data 0).  Its source makes the nodes (between
//BeginShow and EndShow) and says where each result is; the pane opens a result's place when it is double clicked or
//jumped to (F4), steps through the results for Next and Previous Occurrence, and gives the keys back to the editor on
//escape.  As IntelliJ's, space and enter don't open a result.
class CIDEResultsPane : public CObject
{
CONSTRUCTABLE(CIDEResultsPane);
DESTRUCTABLE(CIDEResultsPane);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds what holds the pane.
	CIDEResultsSource*	mpcSource;  //Not held: it holds the pane.
	Ptr<CTreeContainer>	mpTree;
	Ptr<CScrollPane>	mpScrollPane;
	Ptr<CInputAction>	mpEscapeAction;  //Escape in the tree goes back to the editor.
	CChars				mszNoun;  //What a result is called in the status line: "usages", "matches".
	bool				mbSelectOnOpen;  //A result opened from the tree has its text selected in its editor.

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
	int32				FindResultNode(int64 iData);
	int64				GetResultData(int32 iNode);
	bool				HasResults(void);
	bool				OpenResult(int32 iNode, bool bSelect);
	bool				Step(bool bNext, bool bKeepKeys);
	void				ShowSelected(void);
	void				Focus(void);

protected:
	void				ResultActivated(void);
	void				ReturnToEditor(void);
};


#endif // __IDE_RESULTS_PANE_H__

