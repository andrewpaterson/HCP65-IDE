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
#include "BaseLib/TextFile.h"
#include "IDEContent.h"
#include "IDEResultsPane.h"


#define IDE_RESULTS_NAVIGATE_ATTRIBUTE	"NavigateWithSingleClick"
#define IDE_RESULTS_PREVIEW_ATTRIBUTE	"Preview"
#define IDE_RESULTS_SHARE_ATTRIBUTE		"PreviewShare"
#define IDE_RESULTS_TREE_SHARE			450		//Of the pane, out of SPLIT_CONTAINER_WHOLE, until the sash is dragged.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsSource::GetDeclarationLocation(CIDEResultsPane* pcPane, SIDELocation* psDest)
{
	return false;
}


bool CIDEResultsSource::CanRerunResults(CIDEResultsPane* pcPane) { return false; }
void CIDEResultsSource::RerunResults(CIDEResultsPane* pcPane) {}


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
	mbNavigateOnSelect = false;
	mbShowing = false;

	mpTree = OMalloc<CTreeContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpTree->SetAlignment(CSH_Stretch, CSV_Top);
	mpTree->GetActivationEvent()->Add(this, &CIDEResultsPane::ResultActivated);

	//As IntelliJ's: enter and double clicking go to a result (the list's own actions), Jump to Source (F4) goes to the
	//declaration, space is typed in the editor as any character is, and escape goes back to the editor.
	mpTree->SetInputActionsEnabled(CIC_Space, false);
	mpTree->SetInputActionsEnabled(CIC_JumpToSource, false);
	mpEscapeAction = mpTree->AddInputAction(CIC_ClearSelection, this, &CIDEResultsPane::ReturnToEditor);
	mpJumpAction = mpTree->AddInputAction(CIC_JumpToSource, this, &CIDEResultsPane::JumpToSource);
	mpSpaceAction = mpTree->AddInputAction(CIC_Space, this, &CIDEResultsPane::TypeSpace);
	mpTypeAction = mpTree->AddInputAction(IAF_Printable, NULL, this, &CIDEResultsPane::TypeCharacter);
	mpScrollPane = OMalloc<CScrollPane>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpScrollPane->SetContent(mpTree);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	//The selected result's code, right of the tree.
	muiPreviewChanges = MAX_SIZE;
	mpPreview = OMalloc<CCodePreview>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetDimFont(), mpcContent->GetCodeFamily());
	mpSplit = OMalloc<CSplitContainer>(mpcContent->GetWindow(), mpcContent->GetTheme(), true);
	mpSplit->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpSplit->SetComponents(mpScrollPane, mpPreview);
	mpSplit->SetShare(IDE_RESULTS_TREE_SHARE);
	mpSplit->GetShareEvent()->Add(this, &CIDEResultsPane::ShareChanged);
	mpTree->GetSelectionEvent()->Add(this, &CIDEResultsPane::ResultSelected);

	//The toolbar, down the left.
	mpCommands = OMalloc<CCommandSet>();
	mpToolbar = OMalloc<CToolbar>(mpcContent->GetWindow(), mpcContent->GetTheme(), true);
	mpToolbar->SetAlignment(CSH_Left, CSV_Stretch);
	AddCommand("Results.Rerun", "Rerun", ITI_Rerun, &CIDEResultsPane::Rerun, &CIDEResultsPane::CanRerun, NULL);
	AddCommand("Results.Previous", "Previous Occurrence", ITI_Previous, &CIDEResultsPane::StepPrevious, &CIDEResultsPane::CanStepPrevious, NULL);
	AddCommand("Results.Next", "Next Occurrence", ITI_Next, &CIDEResultsPane::StepNext, &CIDEResultsPane::CanStepNext, NULL);
	AddCommand("Results.ExpandAll", "Expand All", ITI_ExpandAll, &CIDEResultsPane::ExpandAll, &CIDEResultsPane::HasResults, NULL);
	AddCommand("Results.CollapseAll", "Collapse All", ITI_CollapseAll, &CIDEResultsPane::CollapseAll, &CIDEResultsPane::HasResults, NULL);

	mpPanel = OMalloc<CGridContainer>(mpcContent->GetWindow());
	mpPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel->AddComponent(mpToolbar, 0, 0, 1, 1, 0, 1);
	mpPanel->AddComponent(mpSplit, 1, 0, 1, 1, 1, 1);
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
	M_Pointer(mpJumpAction);
	M_Pointer(mpSpaceAction);
	M_Pointer(mpTypeAction);
	M_Pointer(mpPanel);
	M_Pointer(mpToolbar);
	M_Pointer(mpCommands);
	M_Pointer(mpSplit);
	M_Pointer(mpPreview);
	U_Size(muiPreviewChanges);
	U_String(mszNoun);
	U_Bool(mbSelectOnOpen);
	U_Bool(mbNavigateOnSelect);
	U_Bool(mbShowing);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Free(void)
{
	if (mpSplit.IsNotNull())
	{
		mpSplit->GetShareEvent()->Remove(this);
	}
	if (mpTree.IsNotNull())
	{
		mpTree->GetSelectionEvent()->Remove(this);
		mpTree->GetActivationEvent()->Remove(this);
		if (mpEscapeAction.IsNotNull())
		{
			mpTree->RemoveInputAction(mpEscapeAction);
			mpTree->RemoveInputAction(mpJumpAction);
			mpTree->RemoveInputAction(mpSpaceAction);
			mpTree->RemoveInputAction(mpTypeAction);
			mpEscapeAction = NULL;
			mpJumpAction = NULL;
			mpSpaceAction = NULL;
			mpTypeAction = NULL;
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
	mpPreview->Clear();
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
	mbShowing = true;
	mpTree->SelectNode((iNode != TREE_NO_NODE) ? iNode : FindResultNode(0));
	mbShowing = false;
	ShowPreview();
	RefreshToolbar();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Clear(void)
{
	mpTree->RemoveAllNodes();
	mpPreview->Clear();
	RefreshToolbar();
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
// A line's node, double clicked or with enter pressed on it, opens its
// place.
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
			mbShowing = true;
			mpTree->SelectNode(iNode);
			mbShowing = false;
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
// Jump to Source (F4) in the tree, as IntelliJ's: where what the results
// are of is declared is opened, whichever row is selected.  Results of
// nothing declared (text found): the selected one is opened, as enter
// does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::JumpToSource(void)
{
	SIDELocation	sLocation;

	if (mpcSource->GetDeclarationLocation(this, &sLocation))
	{
		mpcContent->GetDocuments()->OpenLocation(&sLocation, false);
		sLocation.szFileName.Kill();
	}
	else
	{
		OpenResult(mpTree->GetSelectedNode(), mbSelectOnOpen);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A character typed in the tree, or space, isn't the tree's: the active
// editor gets the keys and has it typed in it.  With no editor it is
// lost.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::TypeCharacter(SComponentInputEvent* psEvent)
{
	if (mpcContent->GetDocuments()->FocusActiveEditor())
	{
		mpcContent->GetWindow()->InputPrintable(psEvent->cPrintable, psEvent->bInsert);
	}
}


void CIDEResultsPane::TypeSpace(void)
{
	if (mpcContent->GetDocuments()->FocusActiveEditor())
	{
		mpcContent->GetWindow()->InputCommand(CIC_Space, true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Another row of the tree is selected: its result is previewed and, with
// Navigate with Single Click, opened in its editor, the tree keeping the
// keys.  Not when the pane selected it itself (the tree made again, a
// step, which opens it anyway).
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ResultSelected(void)
{
	ShowPreview();
	RefreshToolbar();
	if (mbNavigateOnSelect && !mbShowing && OpenResult(mpTree->GetSelectedNode(), true))
	{
		Focus();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where a line and column (from 0, the column in bytes) are in szText.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEResultPosition(const char* szText, size uiLength, int32 iLine, int32 iColumn)
{
	const char*	szNewLine;
	size		uiPosition;
	size		uiLineEnd;

	uiPosition = 0;
	for (; iLine > 0; iLine--)
	{
		szNewLine = (const char*)memchr(&szText[uiPosition], '\n', uiLength - uiPosition);
		if (szNewLine == NULL)
		{
			return uiLength;
		}
		uiPosition = (size)(szNewLine - szText) + 1;
	}
	szNewLine = (const char*)memchr(&szText[uiPosition], '\n', uiLength - uiPosition);
	uiLineEnd = szNewLine ? (size)(szNewLine - szText) : uiLength;
	uiPosition += (iColumn > 0) ? (size)iColumn : 0;
	return (uiPosition < uiLineEnd) ? uiPosition : uiLineEnd;
}


//////////////////////////////////////////////////////////////////////////
//
// The preview shows the selected result: its file's code (as its editor
// has it if it is open, else as it is on the disk), at its place, the
// text found coloured.  The code is only taken again when the file isn't
// the one shown or its editor has changed it since; it is then given to
// the edits thread to be coloured.  A group's row, or none, shows nothing.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ShowPreview(void)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDELocation		sLocation;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	size				uiChanges;
	int64				iData;

	if (!IsPreviewShown())
	{
		return;
	}
	iData = GetResultData(mpTree->GetSelectedNode());
	if ((iData == 0) || !mpcSource->GetResultLocation(this, iData, &sLocation))
	{
		mpPreview->Clear();
		return;
	}

	pTab = mpcContent->GetDocuments()->FindTab(sLocation.szFileName.Text());
	uiChanges = pTab.IsNotNull() ? pTab->GetEditor()->GetChangeCount() : MAX_SIZE;
	if (!mpPreview->IsShowing(sLocation.szFileName.Text()) || (uiChanges != muiPreviewChanges))
	{
		cFile.Init();
		szText = NULL;
		if (pTab.IsNotNull())
		{
			szText = pTab->GetEditor()->GetText();
		}
		else if (cFile.Read(sLocation.szFileName.Text()))
		{
			szText = cFile.Text();
		}
		if (szText == NULL)
		{
			cFile.Kill();
			sLocation.szFileName.Kill();
			mpPreview->Clear();
			return;
		}
		mpPreview->SetCode(sLocation.szFileName.Text(), szText);
		muiPreviewChanges = uiChanges;
		mpcContent->GetDocuments()->HighlightPreview(this, sLocation.szFileName.Text(), szText);
		cFile.Kill();
	}

	szText = mpPreview->GetTextArea()->GetText();
	uiLength = strlen(szText);
	mpPreview->ShowPlace(FindIDEResultPosition(szText, uiLength, sLocation.iLine, sLocation.iColumn), (sLocation.iLength > 0) ? (size)sLocation.iLength : 0);
	sLocation.szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The edits thread highlighted a file for the preview: if the preview
// still shows it, it is coloured as its editor would be.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights)
{
	if (mpPreview->IsShowing(szFileName))
	{
		ApplyIDEHighlights(mpPreview->GetTextArea(), pacHighlights);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The preview is shown beside the tree, or the tree has the whole pane.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::SetPreviewShown(bool bShown)
{
	if (bShown != IsPreviewShown())
	{
		mpSplit->SetSecondShown(bShown);
		if (bShown)
		{
			ShowPreview();
		}
		else
		{
			mpPreview->Clear();
		}
		RefreshToolbar();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Navigate with Single Click: a result is opened as it is selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::SetNavigateOnSelect(bool bNavigate)
{
	mbNavigateOnSelect = bNavigate;
	RefreshToolbar();
}


bool CIDEResultsPane::IsNavigateOnSelect(void) { return mbNavigateOnSelect; }


//////////////////////////////////////////////////////////////////////////
//
// The sash was dragged: the share is kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ShareChanged(void)
{
	mpcContent->GetSettings()->WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a result is opened as it is selected, whether the preview is
// shown and the tree's share of the pane, from and to its owner's tag of
// the IDE's settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ReadSettings(CMarkupTag* pcTag)
{
	char*	szValue;
	int32	iShare;

	szValue = pcTag->GetStringAttribute(IDE_RESULTS_NAVIGATE_ATTRIBUTE);
	if (szValue)
	{
		mbNavigateOnSelect = (StringCompare(szValue, "true") == 0);
	}
	szValue = pcTag->GetStringAttribute(IDE_RESULTS_PREVIEW_ATTRIBUTE);
	if (szValue)
	{
		SetPreviewShown(StringCompare(szValue, "true") == 0);
	}
	szValue = pcTag->GetStringAttribute(IDE_RESULTS_SHARE_ATTRIBUTE);
	if (szValue)
	{
		iShare = atoi(szValue);
		if ((iShare > 0) && (iShare < SPLIT_CONTAINER_WHOLE))
		{
			mpSplit->SetShare(iShare);
		}
	}
}


void CIDEResultsPane::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Attribute(IDE_RESULTS_NAVIGATE_ATTRIBUTE, mbNavigateOnSelect);
	pcWriter->Attribute(IDE_RESULTS_PREVIEW_ATTRIBUTE, IsPreviewShown());
	pcWriter->Attribute(IDE_RESULTS_SHARE_ATTRIBUTE, mpSplit->GetShare());
}


//////////////////////////////////////////////////////////////////////////
//
// One of the pane's own commands, and its button on the toolbar with its
// icon, dim while it can't be used.  fEnabled and fChecked can be NULL.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::AddCommand(const char* szName, const char* szCaption, EIDEToolIcon eIcon, void(CIDEResultsPane::*fRun)(void), bool(CIDEResultsPane::*fEnabled)(void), bool(CIDEResultsPane::*fChecked)(void))
{
	Ptr<CCommand>	pCommand;

	pCommand = mpCommands->Add(szName, szCaption, "", mpcContent->GetToolIcon(eIcon), this, fRun);
	if (fEnabled)
	{
		pCommand->SetEnabledCheck(this, fEnabled);
	}
	if (fChecked)
	{
		pCommand->SetCheckedCheck(this, fChecked);
	}
	AddToolbarCommand(pCommand, eIcon);
}


//////////////////////////////////////////////////////////////////////////
//
// A button for a command of the pane's (GetCommands) that its source
// made, after those there are: its icon is eIcon's, dim while it can't
// be used.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::AddToolbarCommand(Ptr<CCommand> pCommand, EIDEToolIcon eIcon)
{
	mpToolbar->AddCommand(pCommand, mpcContent->GetToolIcon(eIcon, true));
}


void CIDEResultsPane::AddToolbarSeparator(void) { mpToolbar->AddSeparator(); }


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's last buttons, after its source's: Preview and Navigate
// with Single Click.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::AddViewCommands(void)
{
	mpToolbar->AddSeparator();
	AddCommand("Results.Preview", "Preview", ITI_Preview, &CIDEResultsPane::TogglePreview, NULL, &CIDEResultsPane::IsPreviewShown);
	AddCommand("Results.NavigateWithSingleClick", "Navigate with Single Click", ITI_SingleClick, &CIDEResultsPane::ToggleNavigateOnSelect, NULL, &CIDEResultsPane::IsNavigateOnSelect);
}


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's buttons are as their commands are now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::RefreshToolbar(void)
{
	if (mpToolbar.IsNotNull())
	{
		mpToolbar->Refresh();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's commands.  Rerun has the source look again.  Previous
// and Next Occurrence step as the application's do, the tree keeping the
// keys.  Preview and Navigate with Single Click are kept in the
// settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Rerun(void) { mpcSource->RerunResults(this); }
bool CIDEResultsPane::CanRerun(void) { return mpcSource->CanRerunResults(this); }
void CIDEResultsPane::StepPrevious(void) { Step(false, true); }
void CIDEResultsPane::StepNext(void) { Step(true, true); }
bool CIDEResultsPane::CanStepPrevious(void) { return CanStep(false); }
bool CIDEResultsPane::CanStepNext(void) { return CanStep(true); }
void CIDEResultsPane::ExpandAll(void) { mpTree->ExpandAll(); }
void CIDEResultsPane::CollapseAll(void) { mpTree->CollapseAll(); }


void CIDEResultsPane::TogglePreview(void)
{
	SetPreviewShown(!IsPreviewShown());
	mpcContent->GetSettings()->WriteSettings();
}


void CIDEResultsPane::ToggleNavigateOnSelect(void)
{
	SetNavigateOnSelect(!mbNavigateOnSelect);
	mpcContent->GetSettings()->WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether there is a result after (or before) the selected row to step
// to; with none selected, whether there is one at all.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsPane::CanStep(bool bNext)
{
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
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The pane is being let go of before the IDE closes: what it asked to be
// highlighted isn't given to it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Detach(void)
{
	mpcContent->GetDocuments()->ForgetPreviewAsker(this);
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
Ptr<CComponent> CIDEResultsPane::GetComponent(void) { return mpPanel; }
Ptr<CCommandSet> CIDEResultsPane::GetCommands(void) { return mpCommands; }
Ptr<CToolbar> CIDEResultsPane::GetToolbar(void) { return mpToolbar; }
Ptr<CCodePreview> CIDEResultsPane::GetPreview(void) { return mpPreview; }
Ptr<CSplitContainer> CIDEResultsPane::GetSplit(void) { return mpSplit; }
bool CIDEResultsPane::IsPreviewShown(void) { return mpSplit->IsSecondShown(); }

