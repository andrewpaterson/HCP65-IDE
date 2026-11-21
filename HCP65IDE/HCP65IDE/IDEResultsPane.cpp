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
#include "StandardLib/ClassDefines.h"
#include "BaseLib/TextFile.h"
#include "IDEContent.h"
#include "IDEResultsPane.h"


#define IDE_RESULTS_NAVIGATE_ATTRIBUTE	"NavigateWithSingleClick"
#define IDE_RESULTS_AUTOSCROLL_ATTRIBUTE	"AutoscrollFromSource"
#define IDE_RESULTS_LINE_COLOURS		64		//The most coloured parts a result's line has.
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
	mbAutoscroll = false;

	mpTree = OMalloc<CTreeContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpTree->SetAlignment(CSH_Stretch, CSV_Top);
	mpTree->GetActivationEvent()->Add(this, &CIDEResultsPane::ResultActivated);

	//As IntelliJ's: enter and double clicking go to a result (the list's own actions), Jump to Source (F4) goes to the
	//declaration, and escape goes back to the editor.  Space doesn't open a result as a list's does: it does
	//nothing here.
	mpTree->SetInputActionsEnabled(CIC_Space, false);
	mpTree->SetInputActionsEnabled(CIC_JumpToSource, false);
	mpEscapeAction = mpTree->AddInputAction(CIC_ClearSelection, this, &CIDEResultsPane::ReturnToEditor);
	mpJumpAction = mpTree->AddInputAction(CIC_JumpToSource, this, &CIDEResultsPane::JumpToSource);

	//Typing finds a row, as in every tree.  Asked for after the escape action above is added, so that one is called
	//first and can leave the keys here when escape only ends a find (ReturnToEditor).
	mpTree->SetTypeToFind(true);
	mpScrollPane = OMalloc<CScrollPane>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpScrollPane->SetContent(mpTree);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	//The selected result's code, right of the tree.
	mpPreview = OMalloc<CCodePreview>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetDimFont(), mpcContent->GetCodeFamily());
	mpLocationPreview = OMalloc<CIDELocationPreview>(mpcContent, mpPreview);
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
	M_Pointer(mpPanel);
	M_Pointer(mpToolbar);
	M_Pointer(mpCommands);
	M_Pointer(mpSplit);
	M_Pointer(mpPreview);
	M_Pointer(mpLocationPreview);
	U_String(mszNoun);
	U_Bool(mbSelectOnOpen);
	U_Bool(mbNavigateOnSelect);
	U_Bool(mbShowing);
	U_Bool(mbAutoscroll);
	M_Pointer(mpExportDialog);
	M_Pointer(mpExportChooser);
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
			mpEscapeAction = NULL;
			mpJumpAction = NULL;
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
// A result that is a symbol, not a line (a class in a hierarchy, a
// function that calls another): its name with its kind's icon, then
// szDetail (with the spaces that part it from the name; NULL for none)
// dim.  It can have results under it.  iData, above 0, is what its source
// knows it by.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEResultsPane::AddSymbol(int32 iParent, const char* szName, const char* szDetail, uint8 uiKind, int64 iData)
{
	Ptr<CImageCel>	pIcon;
	CChars			szText;
	int32			iNode;
	int				iType;

	iType = GetIDESymbolIconType(uiKind);
	if (iType >= 0)
	{
		pIcon = mpcContent->GetSymbolIcon((size)iType);
	}
	szText.Init(szName);
	szText.Append(szDetail ? szDetail : "");
	iNode = mpTree->AddNode(iParent, szText.Text(), pIcon, iData);
	if (szText.Length() > strlen(szName))
	{
		mpTree->AddNodeRange(iNode, strlen(szName), szText.Length() - strlen(szName), mpcContent->GetDimFont());
	}
	szText.Kill();
	return iNode;
}


//A part of a line of code that has a colour of its own: where it is in the line and what it is.
struct SIDELineColour
{
	size			uiStart;
	size			uiLength;
	ECPPHighlight	eKind;
};


//A part of a result's row in a font of its own.
struct SIDERowRange
{
	size		uiStart;
	size		uiLength;
	CFont*		pcFont;
};


//////////////////////////////////////////////////////////////////////////
//
// The parts of a line of C or C++ that its own text says the colour of:
// a directive at its start, comments, strings, characters, numbers and
// keywords, in order.  What a name is takes a parse of its file, and a
// line inside a comment that started on another doesn't know it is.  At
// most uiMaximum are found; how many.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDELineColours(const char* szLine, size uiLength, SIDELineColour* asDest, size uiMaximum)
{
	static const char*	aszKeywords[] = { "alignas", "alignof", "auto", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class", "const", "const_cast", "constexpr",
										  "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "extern", "false", "final", "float",
										  "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "nullptr", "operator", "override",
										  "private", "protected", "public", "register", "reinterpret_cast", "return", "short", "signed", "sizeof", "static", "static_assert",
										  "static_cast", "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
										  "using", "virtual", "void", "volatile", "wchar_t", "while" };
	size			uiFound;
	size			i;
	size			j;
	size			k;
	ECPPHighlight	eKind;
	bool			bKeyword;
	char			cQuote;

	uiFound = 0;
	i = 0;
	while ((i < uiLength) && ((szLine[i] == ' ') || (szLine[i] == '\t')))
	{
		i++;
	}
	if ((i < uiLength) && (szLine[i] == '#'))
	{
		j = i + 1;
		while ((j < uiLength) && ((szLine[j] == ' ') || (szLine[j] == '\t')))
		{
			j++;
		}
		while ((j < uiLength) && IsIDENameChar(szLine[j]))
		{
			j++;
		}
		asDest[uiFound].uiStart = i;
		asDest[uiFound].uiLength = j - i;
		asDest[uiFound].eKind = CPPH_Directive;
		uiFound++;
		i = j;
	}
	while ((i < uiLength) && (uiFound < uiMaximum))
	{
		j = i;
		eKind = CPPH_Identifier;
		if ((szLine[i] == '/') && (i + 1 < uiLength) && (szLine[i + 1] == '/'))
		{
			j = uiLength;
			eKind = CPPH_Comment;
		}
		else if ((szLine[i] == '/') && (i + 1 < uiLength) && (szLine[i + 1] == '*'))
		{
			j = i + 2;
			while ((j + 1 < uiLength) && !((szLine[j] == '*') && (szLine[j + 1] == '/')))
			{
				j++;
			}
			j = (j + 2 <= uiLength) ? j + 2 : uiLength;
			eKind = CPPH_Comment;
		}
		else if ((szLine[i] == '"') || (szLine[i] == '\''))
		{
			cQuote = szLine[i];
			j = i + 1;
			while ((j < uiLength) && (szLine[j] != cQuote))
			{
				j += ((szLine[j] == '\\') && (j + 1 < uiLength)) ? 2 : 1;
			}
			j = (j < uiLength) ? j + 1 : uiLength;
			eKind = (cQuote == '"') ? CPPH_String : CPPH_Character;
		}
		else if ((szLine[i] >= '0') && (szLine[i] <= '9'))
		{
			while ((j < uiLength) && (IsIDENameChar(szLine[j]) || (szLine[j] == '.')))
			{
				j++;
			}
			eKind = CPPH_Number;
		}
		else if (IsIDENameChar(szLine[i]))
		{
			while ((j < uiLength) && IsIDENameChar(szLine[j]))
			{
				j++;
			}
			bKeyword = false;
			for (k = 0; (k < sizeof(aszKeywords) / sizeof(const char*)) && !bKeyword; k++)
			{
				bKeyword = (strlen(aszKeywords[k]) == j - i) && (memcmp(aszKeywords[k], &szLine[i], j - i) == 0);
			}
			if (bKeyword)
			{
				eKind = CPPH_Keyword;
			}
		}
		else
		{
			j = i + 1;
		}
		if (eKind != CPPH_Identifier)
		{
			asDest[uiFound].uiStart = i;
			asDest[uiFound].uiLength = j - i;
			asDest[uiFound].eKind = eKind;
			uiFound++;
		}
		i = j;
	}
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// A result's node: its line's number, dim, and the line as it is now
// (from pcLines, without the white space at its ends), the text found
// (iLength of the line from iColumn) in the match font and, in a C or C++
// file, the rest coloured as code where its own text says how
// (FindIDELineColours); then szNote (with the spaces that part it from
// the line; NULL for none) dim.  iData, above 0, is what its source knows
// it by.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEResultsPane::AddResult(int32 iParent, CIDEFileLines* pcLines, const char* szFileName, int32 iLine, int32 iColumn, int32 iLength, const char* szNote, int64 iData)
{
	SIDELineColour	asColours[IDE_RESULTS_LINE_COLOURS];
	SIDERowRange	asRanges[IDE_RESULTS_LINE_COLOURS * 2 + 1];
	SIDERowRange	sRange;
	EIDEFileType	eType;
	CChars			szText;
	size			uiNumber;
	size			uiTrimmed;
	size			uiLineEnd;
	size			uiMatch;
	size			uiMatchEnd;
	size			uiColours;
	size			uiRanges;
	size			uiStart;
	size			uiEnd;
	size			i;
	size			j;
	int32			iNode;

	szText.Init();
	szText.Append(iLine + 1);
	szText.Append(": ");
	uiNumber = szText.Length();
	pcLines->AppendLine(szFileName, iLine, &szText, &uiTrimmed);
	uiLineEnd = szText.Length();
	szText.Append(szNote ? szNote : "");
	iNode = mpTree->AddNode(iParent, szText.Text(), NULL, iData);

	mpTree->AddNodeRange(iNode, 0, uiNumber, mpcContent->GetDimFont());

	//The text found.
	uiRanges = 0;
	uiMatch = uiLineEnd;
	uiMatchEnd = uiLineEnd;
	if ((iColumn >= 0) && ((size)iColumn >= uiTrimmed) && (iLength > 0))
	{
		uiMatch = uiNumber + (size)iColumn - uiTrimmed;
		if ((uiMatch < uiLineEnd) && ((size)iLength <= uiLineEnd - uiMatch))
		{
			uiMatchEnd = uiMatch + (size)iLength;
			asRanges[uiRanges].uiStart = uiMatch;
			asRanges[uiRanges].uiLength = (size)iLength;
			asRanges[uiRanges].pcFont = &mpcContent->GetMatchFont();
			uiRanges++;
		}
		else
		{
			uiMatch = uiLineEnd;
		}
	}

	//The line's colours, round the text found.
	eType = CIDEIcons::GetFileType(szFileName);
	if ((eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header))
	{
		uiColours = FindIDELineColours(&szText.Text()[uiNumber], uiLineEnd - uiNumber, asColours, IDE_RESULTS_LINE_COLOURS);
		for (i = 0; i < uiColours; i++)
		{
			uiStart = uiNumber + asColours[i].uiStart;
			uiEnd = uiStart + asColours[i].uiLength;
			sRange.pcFont = &mpcContent->GetFontColours()->GetFont(GetHighlightColour(asColours[i].eKind));
			if ((uiStart < uiMatch) && (uiRanges < IDE_RESULTS_LINE_COLOURS * 2))
			{
				sRange.uiStart = uiStart;
				sRange.uiLength = ((uiEnd < uiMatch) ? uiEnd : uiMatch) - uiStart;
				asRanges[uiRanges] = sRange;
				uiRanges++;
			}
			if ((uiEnd > uiMatchEnd) && (uiRanges < IDE_RESULTS_LINE_COLOURS * 2))
			{
				sRange.uiStart = (uiStart > uiMatchEnd) ? uiStart : uiMatchEnd;
				sRange.uiLength = uiEnd - sRange.uiStart;
				asRanges[uiRanges] = sRange;
				uiRanges++;
			}
		}
	}

	//In the order of the text, as a node's ranges must be.
	for (i = 1; i < uiRanges; i++)
	{
		for (j = i; (j > 0) && (asRanges[j].uiStart < asRanges[j - 1].uiStart); j--)
		{
			sRange = asRanges[j];
			asRanges[j] = asRanges[j - 1];
			asRanges[j - 1] = sRange;
		}
	}
	for (i = 0; i < uiRanges; i++)
	{
		mpTree->AddNodeRange(iNode, asRanges[i].uiStart, asRanges[i].uiLength, asRanges[i].pcFont);
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
// The preview shows the selected result: its file's code (as its editor
// has it if it is open, else as it is on the disk), at its place, the
// text found coloured.  The code is only taken again when the file isn't
// the one shown or its editor has changed it since; it is then given to
// the edits thread to be coloured.  A group's row, or none, shows nothing.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ShowPreview(void)
{
	SIDELocation	sLocation;
	int64			iData;

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
	mpLocationPreview->ShowLocation(&sLocation);
	sLocation.szFileName.Kill();
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
	szValue = pcTag->GetStringAttribute(IDE_RESULTS_AUTOSCROLL_ATTRIBUTE);
	if (szValue)
	{
		mbAutoscroll = (StringCompare(szValue, "true") == 0);
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


//////////////////////////////////////////////////////////////////////////
//
// The same settings as another pane has, as a new tab's pane starts with
// the last's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::CopySettings(Ptr<CIDEResultsPane> pSource)
{
	mbNavigateOnSelect = pSource->mbNavigateOnSelect;
	mbAutoscroll = pSource->mbAutoscroll;
	SetPreviewShown(pSource->IsPreviewShown());
	mpSplit->SetShare(pSource->mpSplit->GetShare());
	RefreshToolbar();
}


void CIDEResultsPane::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Attribute(IDE_RESULTS_NAVIGATE_ATTRIBUTE, mbNavigateOnSelect);
	if (mbAutoscroll)
	{
		pcWriter->Attribute(IDE_RESULTS_AUTOSCROLL_ATTRIBUTE, mbAutoscroll);
	}
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
	AddCommand("Results.AutoscrollFromSource", "Autoscroll from Source", ITI_Autoscroll, &CIDEResultsPane::ToggleAutoscroll, NULL, &CIDEResultsPane::IsAutoscrollFromSource);
	AddCommand("Results.Export", "Export...", ITI_Export, &CIDEResultsPane::Export, &CIDEResultsPane::CanExport, NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// Autoscroll from Source, as IntelliJ's: while it is on, the result
// nearest the active editor's caret in its file is the selected one.  The
// setting is its owner's to keep, as Navigate with Single Click is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ToggleAutoscroll(void)
{
	Ptr<CIDEEditorTab>	pTab;
	int					iLine;
	int					iColumn;

	mbAutoscroll = !mbAutoscroll;
	RefreshToolbar();
	mpcContent->GetSettings()->WriteSettings();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (mbAutoscroll && pTab.IsNotNull() && pTab->GetCaretPlace(&iLine, &iColumn))
	{
		AutoscrollFromSource(pTab->GetFileName(), iLine);
	}
}


void CIDEResultsPane::SetAutoscrollFromSource(bool bAutoscroll) { mbAutoscroll = bAutoscroll; RefreshToolbar(); }
bool CIDEResultsPane::IsAutoscrollFromSource(void) { return mbAutoscroll; }


//////////////////////////////////////////////////////////////////////////
//
// The caret of the editor of szFileName is on iLine: with Autoscroll from
// Source the result of that file nearest it is selected and shown (the
// last at or before the line; with none, the first after it), without
// being opened and without the keys.  False if nothing was selected: it
// is off, the file has no result here, or that one is selected already.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsPane::AutoscrollFromSource(const char* szFileName, int32 iLine)
{
	SIDELocation	sLocation;
	int32			iNode;
	int32			iBefore;
	int32			iAfter;
	int32			iBeforeLine;
	int32			iAfterLine;
	int64			iData;

	if (!mbAutoscroll || mbShowing)
	{
		return false;
	}
	iBefore = TREE_NO_NODE;
	iAfter = TREE_NO_NODE;
	iBeforeLine = -1;
	iAfterLine = 0;
	for (iNode = 0; iNode < mpTree->NumNodes(); iNode++)
	{
		iData = mpTree->GetNodeData(iNode);
		if ((iData <= 0) || !mpcSource->GetResultLocation(this, iData, &sLocation))
		{
			continue;
		}
		if (StringCompare(sLocation.szFileName.Text(), szFileName) == 0)
		{
			if ((sLocation.iLine <= iLine) && (sLocation.iLine > iBeforeLine))
			{
				iBefore = iNode;
				iBeforeLine = sLocation.iLine;
			}
			else if ((sLocation.iLine > iLine) && ((iAfter == TREE_NO_NODE) || (sLocation.iLine < iAfterLine)))
			{
				iAfter = iNode;
				iAfterLine = sLocation.iLine;
			}
		}
		sLocation.szFileName.Kill();
	}
	iNode = (iBefore != TREE_NO_NODE) ? iBefore : iAfter;
	if ((iNode == TREE_NO_NODE) || (iNode == mpTree->GetSelectedNode()))
	{
		return false;
	}
	mbShowing = true;
	mpTree->SelectNode(iNode);
	mbShowing = false;
	mpcContent->GetWindow()->Layout();
	ShowSelected();
	ShowPreview();
	RefreshToolbar();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The tree's rows as they are shown (the open nodes' and no others),
// appended as text: each on a line, two spaces in for each depth.  How
// many.
//
//////////////////////////////////////////////////////////////////////////
size CIDEResultsPane::AppendShownText(CChars* pszDest)
{
	CChars	szNode;
	int32	iRow;
	int32	iNode;
	int32	iDepth;

	mpTree->UpdateRows();
	for (iRow = 0; iRow < mpTree->NumRows(); iRow++)
	{
		iNode = mpTree->GetRowNode(iRow);
		for (iDepth = mpTree->GetNodeDepth(iNode); iDepth > 0; iDepth--)
		{
			pszDest->Append("  ");
		}
		szNode.Init();
		mpTree->GetNodeText(iNode, &szNode);
		pszDest->Append(szNode);
		szNode.Kill();
		pszDest->Append('\n');
	}
	return (size)mpTree->NumRows();
}


//////////////////////////////////////////////////////////////////////////
//
// Export, on the toolbar: its dialog is shown at the next tick, as a
// dialog must be.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::Export(void)
{
	mpcContent->GetWindow()->Post(this, &CIDEResultsPane::ShowQueuedExport);
}


void CIDEResultsPane::ShowQueuedExport(void) { ShowExport(); }
bool CIDEResultsPane::CanExport(void) { return mpTree->NumNodes() > 0; }


//////////////////////////////////////////////////////////////////////////
//
// Export, as IntelliJ's Export to Text File: a dialog asks whether the
// rows shown are to be saved to a file (a file chooser then asks which)
// or copied to the clipboard.  The status line says what was done.  False
// if nothing was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEResultsPane::ShowExport(void)
{
	static SFileChooserType	asTypes[] =
	{
		{ "Text Files", "*.txt" },
		{ "All Files", NULL },
	};
	CTextFile	cFile;
	CChars		szText;
	CChars		szMessage;
	CChars		szClicked;
	CChars		szChosen;
	size		uiRows;
	bool		bSave;
	bool		bResult;

	szText.Init();
	uiRows = AppendShownText(&szText);
	if (uiRows == 0)
	{
		szText.Kill();
		return false;
	}

	szMessage.Init("Export the ");
	szMessage.Append((uint32)uiRows);
	szMessage.Append((uiRows == 1) ? " row shown, as text." : " rows shown, as text.");
	mpExportDialog = OMalloc<CMessageDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Export Results", szMessage.Text());
	mpExportDialog->GetDialog()->SetOKCaption("Save to File...");
	mpExportDialog->GetDialog()->AddExtraButton("Copy to Clipboard");
	mpcContent->GetScript()->WatchDialog(mpExportDialog->GetDialog());
	bSave = mpExportDialog->Show();
	szClicked.Init(mpExportDialog->GetClicked());
	mpcContent->GetScript()->StopWatchingDialog();
	mpExportDialog = NULL;

	bResult = false;
	szMessage.Clear();
	if (szClicked.Equals("Copy to Clipboard"))
	{
		bResult = mpcContent->GetWindow()->GetClipboard()->SetText(szText.Text());
		szMessage.Append(bResult ? "Copied " : "Couldn't copy ");
		szMessage.Append((uint32)uiRows);
		szMessage.Append((uiRows == 1) ? " row to the clipboard." : " rows to the clipboard.");
	}
	else if (bSave)
	{
		szChosen.Init();
		mpExportChooser = OMalloc<CFileChooser>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Export to Text File", true, "", asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpcContent->GetFolderIcon(), mpcContent->GetFileIcon());
		if (mpExportChooser->Show())
		{
			szChosen.Append(mpExportChooser->GetChosen());
		}
		mpExportChooser = NULL;
		if (!szChosen.Empty())
		{
			cFile.Init(szText.Text());
			bResult = cFile.Write(szChosen.Text());
			cFile.Kill();
			szMessage.Append(bResult ? "Exported " : "Couldn't export ");
			szMessage.Append((uint32)uiRows);
			szMessage.Append((uiRows == 1) ? " row to " : " rows to ");
			szMessage.Append(szChosen);
			szMessage.Append('.');
		}
		szChosen.Kill();
	}
	if (!szMessage.Empty())
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szClicked.Kill();
	szMessage.Kill();
	szText.Kill();
	return bResult;
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
	mpLocationPreview->Detach();
}


//////////////////////////////////////////////////////////////////////////
//
// Escape in the tree: the keys go back to the active editor, as
// IntelliJ's tool windows give them back.
//
//////////////////////////////////////////////////////////////////////////
void CIDEResultsPane::ReturnToEditor(void)
{
	if (mpTree->IsFinding())
	{
		//Escape ends finding a row by what was typed; the next one goes back to the editor.
		mpTree->StopFinding();
		return;
	}
	mpcContent->GetDocuments()->FocusActiveEditor();
}


Ptr<CTreeContainer> CIDEResultsPane::GetTree(void) const { return mpTree; }
Ptr<CComponent> CIDEResultsPane::GetComponent(void) const { return mpPanel; }
Ptr<CCommandSet> CIDEResultsPane::GetCommands(void) const { return mpCommands; }
Ptr<CToolbar> CIDEResultsPane::GetToolbar(void) const { return mpToolbar; }
Ptr<CCodePreview> CIDEResultsPane::GetPreview(void) const { return mpPreview; }
Ptr<CSplitContainer> CIDEResultsPane::GetSplit(void) const { return mpSplit; }
bool CIDEResultsPane::IsPreviewShown(void) { return mpSplit->IsSecondShown(); }

