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
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEResultsPane.h"


//////////////////////////////////////////////////////////////////////////
//
// szNoun is what a result is called when there are none to step to
// ("usages", "matches").  With bSelectOnOpen a result opened from the tree
// has its text selected, as one stepped to always has.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Init(CIDEContent* pcContent, CIDEResultsSource* pcSource, const char* szNoun, bool bSelectOnOpen)
{
	PreInit();
	mpcContent = pcContent;
	mpcSource = pcSource;
	mszNoun.Init(szNoun);
	mbSelectOnOpen = bSelectOnOpen;

	mpTree = OMalloc<CTreeContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpTree->SetAlignment(CSH_Stretch, CSV_Top);
	mpTree->GetActivationEvent()->Add(this, &CIDEResultsPane::ResultActivated);

	//As IntelliJ's: only Jump to Source (F4) and double clicking go to a result, not space or enter, and escape goes back
	//to the editor.
	mpTree->SetInputActionsEnabled(CIC_Space, false);
	mpTree->SetInputActionsEnabled(CIC_Enter, false);
	mpEscapeAction = mpTree->AddInputAction(CIC_ClearSelection, this, &CIDEResultsPane::ReturnToEditor);
	mpScrollPane = OMalloc<CScrollPane>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpScrollPane->SetContent(mpTree);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Class(void)
{
	U_Pointer(mpcContent);
	U_Pointer(mpcSource);
	M_Pointer(mpTree);
	M_Pointer(mpScrollPane);
	M_Pointer(mpEscapeAction);
	U_String(mszNoun);
	U_Bool(mbSelectOnOpen);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Free(void)
{
	if (mpTree.IsNotNull())
	{
		mpTree->GetActivationEvent()->Remove(this);
		if (mpEscapeAction.IsNotNull())
		{
			mpTree->RemoveInputAction(mpEscapeAction);
			mpEscapeAction = NULL;
		}
	}
	mszNoun.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsPane::Save(CObjectWriter* pcFile) { return false; }
bool CIDEResultsPane::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The tree is emptied for its source to make its nodes again.  The
// selected result's data, for EndShow, if bKeepSelected and one is
// selected; else 0.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEResultsPane::BeginShow(bool bKeepSelected)
{
	int64	iSelectedData;

	iSelectedData = 0;
	if (bKeepSelected && (mpTree->GetSelectedNode() != TREE_NO_NODE))
	{
		iSelectedData = mpTree->GetNodeData(mpTree->GetSelectedNode());
	}
	mpTree->RemoveAllNodes();
	return iSelectedData;
}


//////////////////////////////////////////////////////////////////////////
//
// The nodes are made: the result that was selected is selected again,
// or without one (or it has gone) the first result.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::EndShow(int64 iSelectedData)
{
	int32	iNode;

	iNode = (iSelectedData > 0) ? FindResultNode(iSelectedData) : TREE_NO_NODE;
	mpTree->SelectNode((iNode != TREE_NO_NODE) ? iNode : FindResultNode(0));
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Clear(void)
{
	mpTree->RemoveAllNodes();
}


//////////////////////////////////////////////////////////////////////////
//
// A node that groups results (a usage type, a project, a file): its name,
// then szDetail (how many are in it, its directory; with the spaces that
// part it from the name; NULL for none) dim.  With szFileName it is that
// file's node and has its file type's icon.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEResultsPane::AddGroup(int32 iParent, const char* szName, const char* szDetail, const char* szFileName)
{
	Ptr<CImageCel>	pIcon;
	CChars			szText;
	int32			iNode;

	if (szFileName)
	{
		pIcon = mpcContent->GetFileTypeIcon((size)CIDEIcons::GetFileType(szFileName));
	}
	szText.Init(szName);
	szText.Append(szDetail ? szDetail : "");
	iNode = mpTree->AddNode(iParent, szText.Text(), pIcon, 0);
	if (szText.Length() > strlen(szName))
	{
		mpTree->AddNodeRange(iNode, strlen(szName), szText.Length() - strlen(szName), mpcContent->GetDimFont());
	}
	szText.Kill();
	return iNode;
}


//////////////////////////////////////////////////////////////////////////
//
// A result's node: its line's number, dim, and the line as it is now
// (from pcLines, without the white space at its ends), the text found
// (iLength of the line from iColumn) in the match font; then szNote (with
// the spaces that part it from the line; NULL for none) dim.  iData,
// above 0, is what its source knows it by.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEResultsPane::AddResult(int32 iParent, CIDEFileLines* pcLines, const char* szFileName, int32 iLine, int32 iColumn, int32 iLength, const char* szNote, int64 iData)
{
	CChars	szText;
	size	uiNumber;
	size	uiTrimmed;
	size	uiLineEnd;
	size	uiMatch;
	int32	iNode;

	szText.Init();
	szText.Append(iLine + 1);
	szText.Append(": ");
	uiNumber = szText.Length();
	pcLines->AppendLine(szFileName, iLine, &szText, &uiTrimmed);
	uiLineEnd = szText.Length();
	szText.Append(szNote ? szNote : "");
	iNode = mpTree->AddNode(iParent, szText.Text(), NULL, iData);

	mpTree->AddNodeRange(iNode, 0, uiNumber, mpcContent->GetDimFont());
	if ((iColumn >= 0) && ((size)iColumn >= uiTrimmed) && (iLength > 0))
	{
		uiMatch = uiNumber + (size)iColumn - uiTrimmed;
		if ((uiMatch < uiLineEnd) && ((size)iLength <= uiLineEnd - uiMatch))
		{
			mpTree->AddNodeRange(iNode, uiMatch, (size)iLength, mpcContent->GetMatchFont());
		}
	}
	if (szText.Length() > uiLineEnd)
	{
		mpTree->AddNodeRange(iNode, uiLineEnd, szText.Length() - uiLineEnd, mpcContent->GetDimFont());
	}
	szText.Kill();
	return iNode;
}


//////////////////////////////////////////////////////////////////////////
//
// The tree's node for a result (its data), or with 0 its first result's:
// TREE_NO_NODE if there is none.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEResultsPane::FindResultNode(int64 iData)
{
	int32	iNode;
	int64	iNodeData;

	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		iNodeData = mpTree->GetNodeData(iNode);
		if ((iNodeData > 0) && ((iData == 0) || (iNodeData == iData)))
		{
			return iNode;
		}
	}
	return TREE_NO_NODE;
}


//////////////////////////////////////////////////////////////////////////
//
// The data of the result a node is the line of, 0 if it is a group's
// node or none.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEResultsPane::GetResultData(int32 iNode)
{
	int64	iData;

	if ((iNode < 0) || (iNode >= mpTree->NumNodes()))
	{
		return 0;
	}
	iData = mpTree->GetNodeData(iNode);
	return (iData > 0) ? iData : 0;
}


bool CIDEResultsPane::HasResults(void) { return FindResultNode(0) != TREE_NO_NODE; }


//////////////////////////////////////////////////////////////////////////
//
// A result's place is opened in its editor, which gets the keys; with
// bSelect its text is selected.  False if the node isn't a result's or
// its place can't be opened.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsPane::OpenResult(int32 iNode, bool bSelect)
{
	SIDELocation	sLocation;
	int64			iData;
	bool			bResult;

	iData = GetResultData(iNode);
	if ((iData == 0) || !mpcSource->GetResultLocation(this, iData, &sLocation))
	{
		return false;
	}
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, bSelect);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A line's node, double clicked or jumped to (F4), opens its place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ResultActivated(void)
{
	OpenResult(mpTree->GetSelectedNode(), mbSelectOnOpen);
}


//////////////////////////////////////////////////////////////////////////
//
// Next or Previous Occurrence (CIDEOccurrences): the result below (or
// above) the selected one in the tree is selected and its place opened in
// its editor, which has the keys unless bKeepKeys (the tree was stepped
// in).  Past the last (or first) nothing moves.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsPane::Step(bool bNext, bool bKeepKeys)
{
	CChars	szStatus;
	int32	iNode;
	int32	iStep;

	iStep = bNext ? 1 : -1;
	iNode = mpTree->GetSelectedNode();
	if (iNode == TREE_NO_NODE)
	{
		iNode = bNext ? -1 : mpTree->NumNodes();
	}
	for (iNode += iStep; (iNode >= 0) && (iNode < mpTree->NumNodes()); iNode += iStep)
	{
		if (GetResultData(iNode) != 0)
		{
			mpTree->SelectNode(iNode);
			mpTree->ShowRow(mpTree->FindNodeRow(iNode));
			OpenResult(iNode, true);
			if (bKeepKeys)
			{
				Focus();
			}
			return true;
		}
	}
	szStatus.Init("No ");
	szStatus.Append(mszNoun);
	szStatus.Append(bNext ? " after this one." : " before this one.");
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The selected row is scrolled into view.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ShowSelected(void)
{
	if (mpTree->GetSelected() >= 0)
	{
		mpTree->ShowRow(mpTree->GetSelected());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The tree gets the keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Focus(void)
{
	mpcContent->GetWindow()->GetFocus()->SetFocus(mpTree);
}


//////////////////////////////////////////////////////////////////////////
//
// Escape in the tree: the keys go back to the active editor, as
// IntelliJ's tool windows give them back.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ReturnToEditor(void)
{
	mpcContent->GetDocuments()->FocusActiveEditor();
}


Ptr<CTreeContainer> CIDEResultsPane::GetTree(void) { return mpTree; }
Ptr<CComponent> CIDEResultsPane::GetComponent(void) { return mpScrollPane; }

