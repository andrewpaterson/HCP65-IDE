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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/DebugOutput.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLWriter.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDEBookmarks.h"


#define IDE_BOOKMARK_TAG	"Bookmark"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	masBookmarks.Init();
	maiListed.Init();
	mbMoved = false;
	miMoved = 0;
	mbMarksDue = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayIDEBookmarks, masBookmarks);
	U_Unknown(CArrayInt, maiListed);
	M_Pointer(mpPane);
	M_Pointer(mpTool);
	M_Pointer(mpPrompt);
	U_Bool(mbMoved);
	U_Int64(miMoved);
	U_Bool(mbMarksDue);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::Free(void)
{
	if (mpPrompt.IsNotNull())
	{
		mpPrompt->GetAcceptedEvent()->Remove(this);
	}
	RemoveAll();
	maiListed.Kill();
	masBookmarks.Kill();
}


bool CIDEBookmarks::Save(CObjectWriter* pcFile) { return false; }
bool CIDEBookmarks::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Navigate's, with the user's keys (NextIDEWork.txt's item 7) and
// IntelliJ IDEA's Control+digit.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Bookmarks.Toggle", "Toggle Bookmark", "Control+F11", Ptr<CImageCel>(), this, &CIDEBookmarks::ToggleBookmark)->SetEnabledCheck(this, &CIDEBookmarks::HasEditor);
	pCommands->Add("Bookmarks.ToggleMnemonic", "Toggle Bookmark with Mnemonic...", "Control+Shift+F11", Ptr<CImageCel>(), this, &CIDEBookmarks::ToggleMnemonicBookmark)->SetEnabledCheck(this, &CIDEBookmarks::HasEditor);
	pCommands->Add("Bookmarks.Next", "Next Bookmark", "F2", Ptr<CImageCel>(), this, &CIDEBookmarks::NextBookmark)->SetEnabledCheck(this, &CIDEBookmarks::HasEditor);
	pCommands->Add("Bookmarks.Previous", "Previous Bookmark", "Shift+F2", Ptr<CImageCel>(), this, &CIDEBookmarks::PreviousBookmark)->SetEnabledCheck(this, &CIDEBookmarks::HasEditor);
	pCommands->Add("Bookmarks.GoTo0", "Go to Bookmark 0", "", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark0);
	pCommands->Add("Bookmarks.GoTo1", "Go to Bookmark 1", "Control+1", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark1);
	pCommands->Add("Bookmarks.GoTo2", "Go to Bookmark 2", "Control+2", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark2);
	pCommands->Add("Bookmarks.GoTo3", "Go to Bookmark 3", "Control+3", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark3);
	pCommands->Add("Bookmarks.GoTo4", "Go to Bookmark 4", "Control+4", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark4);
	pCommands->Add("Bookmarks.GoTo5", "Go to Bookmark 5", "Control+5", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark5);
	pCommands->Add("Bookmarks.GoTo6", "Go to Bookmark 6", "Control+6", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark6);
	pCommands->Add("Bookmarks.GoTo7", "Go to Bookmark 7", "Control+7", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark7);
	pCommands->Add("Bookmarks.GoTo8", "Go to Bookmark 8", "Control+8", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark8);
	pCommands->Add("Bookmarks.GoTo9", "Go to Bookmark 9", "Control+9", Ptr<CImageCel>(), this, &CIDEBookmarks::GoToBookmark9);
}


void CIDEBookmarks::AddEditorItems(Ptr<CMenuContainer> pMenu) { pMenu->AddItem(mpcContent->GetCommands()->Find("Bookmarks.Toggle")); }


void CIDEBookmarks::AddNavigateItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Bookmarks.Toggle"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Bookmarks.ToggleMnemonic"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Bookmarks.Next"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Bookmarks.Previous"));
}


//////////////////////////////////////////////////////////////////////////
//
// The bookmark selected in the list, -1 if a file's row or none is.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEBookmarks::GetSelectedBookmark(void)
{
	int64	iData;

	iData = mpPane->GetResultData(mpPane->GetTree()->GetSelectedNode());
	if ((iData <= 0) || ((size)iData > maiListed.NumElements()))
	{
		return -1;
	}
	return (int32)maiListed.GetValue((size)iData - 1);
}


bool CIDEBookmarks::CanRemoveSelectedBookmark(void) { return GetSelectedBookmark() >= 0; }


void CIDEBookmarks::RemoveSelectedBookmark(void)
{
	CChars	szFile;
	int32	iBookmark;

	iBookmark = GetSelectedBookmark();
	if (iBookmark >= 0)
	{
		szFile.Init(masBookmarks.Get((size)iBookmark)->szFile);
		RemoveBookmark((size)iBookmark);
		Changed(szFile.Text(), NULL);
		szFile.Kill();
	}
}


bool CIDEBookmarks::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// The Bookmarks tool window, put at psPlace: a results pane, as the Find
// tool window's.  Remove Bookmark, in its tree's menu and on Delete,
// takes the selected bookmark away, as IntelliJ's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::CreatePane(SDockPlace* psPlace)
{
	Ptr<CCommand>	pCommand;

	mpPane = OMalloc<CIDEResultsPane>(mpcContent, this, "bookmarks", false);
	mpPane->AddViewCommands();
	pCommand = mpPane->GetCommands()->Add("Bookmarks.Remove", "Remove Bookmark", "Delete", Ptr<CImageCel>(), this, &CIDEBookmarks::RemoveSelectedBookmark);
	pCommand->SetEnabledCheck(this, &CIDEBookmarks::CanRemoveSelectedBookmark);
	mpPane->SetDeleteCommand(pCommand);
	mpTool = mpcContent->GetDock()->AddPane(IDE_BOOKMARKS_ID, "Bookmarks", NULL, mpPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::AddBookmark(const char* szFile, int32 iLine, char cMnemonic)
{
	SIDEBookmark*	psBookmark;

	psBookmark = masBookmarks.Add();
	psBookmark->szFile.Init(szFile);
	psBookmark->iLine = iLine;
	psBookmark->iColumn = 0;
	psBookmark->cMnemonic = cMnemonic;
}


void CIDEBookmarks::RemoveBookmark(size uiIndex)
{
	masBookmarks.Get(uiIndex)->szFile.Kill();
	masBookmarks.RemoveAt(uiIndex);
}


void CIDEBookmarks::RemoveAll(void)
{
	size	i;

	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		masBookmarks.Get(i)->szFile.Kill();
	}
	masBookmarks.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The bookmark on a line of a file, or with a mnemonic; -1 for none.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEBookmarks::FindBookmark(const char* szFile, int32 iLine)
{
	SIDEBookmark*	psBookmark;
	size			i;

	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		psBookmark = masBookmarks.Get(i);
		if ((psBookmark->iLine == iLine) && psBookmark->szFile.Equals(szFile))
		{
			return (int32)i;
		}
	}
	return -1;
}


int32 CIDEBookmarks::FindMnemonic(char cMnemonic)
{
	size	i;

	for (i = 0; (cMnemonic != '\0') && (i < masBookmarks.NumElements()); i++)
	{
		if (masBookmarks.Get(i)->cMnemonic == cMnemonic)
		{
			return (int32)i;
		}
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The bookmarks of szFile (and of szOtherFile, NULL for none) have
// changed: their gutters and scroll bars show them as they are, and so
// does the list, and they are written with the workspace's user's file.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::Changed(const char* szFile, const char* szOtherFile)
{
	ShowFile(szFile);
	if (szOtherFile && (StringCompare(szFile, szOtherFile) != 0))
	{
		ShowFile(szOtherFile);
	}
	mbMoved = false;
	ShowList(true);
	mpcContent->GetUserFile()->Write();
}


void CIDEBookmarks::ShowFile(const char* szFile)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->FindTab(szFile);
	if (pTab.IsNotNull())
	{
		ShowBookmarks(pTab);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Toggle Bookmark (Control+F11): the caret's line is bookmarked, or if
// it is already it isn't any more, mnemonic and all.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBookmarks::ToggleBookmark(void)
{
	Ptr<CIDEEditorTab>	pTab;
	int32				iBookmark;
	int					iLine;
	int					iColumn;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	iBookmark = FindBookmark(pTab->GetFileName(), iLine);
	if (iBookmark >= 0)
	{
		RemoveBookmark((size)iBookmark);
	}
	else
	{
		AddBookmark(pTab->GetFileName(), iLine, '\0');
	}
	Changed(pTab->GetFileName(), NULL);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Toggle Bookmark with Mnemonic (Control+Shift+F11): a prompt over the
// editor asks for a digit or a letter, which Enter gives the caret's line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBookmarks::ToggleMnemonicBookmark(void)
{
	Ptr<CIDEEditorTab>	pTab;
	SInt32Vec2			sPosition;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !mpcContent->GetDocuments()->GetCaretPoint(&sPosition))
	{
		return false;
	}
	if (mpPrompt.IsNull())
	{
		mpPrompt = OMalloc<CPromptPopup>(mpcContent->GetWindow(), mpcContent->GetDialogLook());
		mpPrompt->GetAcceptedEvent()->Add(this, &CIDEBookmarks::MnemonicAccepted);
	}
	mpPrompt->Open(sPosition, pTab->GetEditor(), "Mnemonic (0-9, A-Z):", "");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Enter in the mnemonic's prompt: one digit or letter is what it wants.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::MnemonicAccepted(void)
{
	CChars	szMnemonic;
	char	c;

	szMnemonic.Init(mpPrompt->GetText());
	szMnemonic.StripWhitespace();
	c = (szMnemonic.Length() == 1) ? szMnemonic.Text()[0] : '\0';
	szMnemonic.Kill();
	if ((c >= 'a') && (c <= 'z'))
	{
		c = (char)(c - 'a' + 'A');
	}
	if (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'Z')))
	{
		//Closed first: the keys go back to the editor.
		mpPrompt->Close();
		SetMnemonicBookmark(c);
	}
	else
	{
		mpPrompt->SetProblem("Type one digit or one letter.");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The caret's line gets the mnemonic: its bookmark is given it, or one is
// made; the line that had the mnemonic loses its bookmark.  A line that
// has that mnemonic already loses its bookmark instead, as a toggle.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBookmarks::SetMnemonicBookmark(char cMnemonic)
{
	Ptr<CIDEEditorTab>	pTab;
	CChars				szOther;
	int32				iBookmark;
	int32				iHad;
	int					iLine;
	int					iColumn;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	szOther.Init();
	iBookmark = FindBookmark(pTab->GetFileName(), iLine);
	iHad = FindMnemonic(cMnemonic);
	if ((iBookmark >= 0) && (iBookmark == iHad))
	{
		RemoveBookmark((size)iBookmark);
	}
	else
	{
		if (iHad >= 0)
		{
			szOther.Set(masBookmarks.Get((size)iHad)->szFile);
			RemoveBookmark((size)iHad);
			iBookmark = FindBookmark(pTab->GetFileName(), iLine);
		}
		if (iBookmark >= 0)
		{
			masBookmarks.Get((size)iBookmark)->cMnemonic = cMnemonic;
		}
		else
		{
			AddBookmark(pTab->GetFileName(), iLine, cMnemonic);
		}
	}
	Changed(pTab->GetFileName(), szOther.Empty() ? NULL : szOther.Text());
	szOther.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Next Bookmark (F2) and Previous Bookmark (Shift+F2): the caret goes to
// the next bookmarked line of the active file after its own, or the one
// before, round the file's ends.  False, the status line saying so, if
// the file has none (or only the caret's line).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBookmarks::Step(bool bNext)
{
	Ptr<CIDEEditorTab>	pTab;
	CArrayIDEStepPlaces	asPlaces;
	SIDEStepPlace*		psPlace;
	SIDEStepPlace		sPlace;
	SIDELocation		sLocation;
	size				i;
	size				j;
	int32				iFound;
	int					iLine;
	int					iColumn;
	bool				bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}

	//The file's, in line order; the caret anywhere on a line is at its bookmark.
	asPlaces.Init();
	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		if (masBookmarks.Get(i)->szFile.Equals(pTab->GetFileName()))
		{
			sPlace.iLine = masBookmarks.Get(i)->iLine;
			sPlace.iColumn = 0;
			sPlace.iEndColumn = MAX_INT32;
			for (j = 0; (j < asPlaces.NumElements()) && (asPlaces.Get(j)->iLine < sPlace.iLine); j++)
			{
			}
			asPlaces.InsertAt(&sPlace, j);
		}
	}
	iFound = FindIDEStepPlace(&asPlaces, iLine, iColumn, bNext, true);
	psPlace = (iFound >= 0) ? asPlaces.Get((size)iFound) : NULL;
	bResult = false;
	if ((psPlace == NULL) || (psPlace->iLine == iLine))
	{
		mpcContent->ShowStatus((asPlaces.NumElements() == 0) ? "This file has no bookmarks: Toggle Bookmark (Control+F11) makes one." : "This file has no other bookmark.");
	}
	else
	{
		memset(&sLocation, 0, sizeof(SIDELocation));
		sLocation.szFileName.Init(pTab->GetFileName());
		sLocation.iLine = psPlace->iLine;
		sLocation.iContainer = -1;
		bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, false);
		sLocation.szFileName.Kill();
	}
	asPlaces.Kill();
	return bResult;
}


bool CIDEBookmarks::NextBookmark(void) { return Step(true); }
bool CIDEBookmarks::PreviousBookmark(void) { return Step(false); }


//////////////////////////////////////////////////////////////////////////
//
// Control+0 to Control+9: the bookmark with that mnemonic is gone to, in
// whichever file.  False, the status line saying so, if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBookmarks::GoToMnemonic(char cMnemonic)
{
	SIDEBookmark*	psBookmark;
	SIDELocation	sLocation;
	CChars			szStatus;
	int32			iBookmark;
	bool			bResult;

	iBookmark = FindMnemonic(cMnemonic);
	if (iBookmark < 0)
	{
		szStatus.Init("There is no bookmark ");
		szStatus.Append(cMnemonic);
		szStatus.Append(": Toggle Bookmark with Mnemonic (Control+Shift+F11) makes one.");
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		return false;
	}
	psBookmark = masBookmarks.Get((size)iBookmark);
	memset(&sLocation, 0, sizeof(SIDELocation));
	sLocation.szFileName.Init(psBookmark->szFile);
	sLocation.iLine = psBookmark->iLine;
	sLocation.iContainer = -1;
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation, false);
	sLocation.szFileName.Kill();
	return bResult;
}


bool CIDEBookmarks::GoToBookmark0(void) { return GoToMnemonic('0'); }
bool CIDEBookmarks::GoToBookmark1(void) { return GoToMnemonic('1'); }
bool CIDEBookmarks::GoToBookmark2(void) { return GoToMnemonic('2'); }
bool CIDEBookmarks::GoToBookmark3(void) { return GoToMnemonic('3'); }
bool CIDEBookmarks::GoToBookmark4(void) { return GoToMnemonic('4'); }
bool CIDEBookmarks::GoToBookmark5(void) { return GoToMnemonic('5'); }
bool CIDEBookmarks::GoToBookmark6(void) { return GoToMnemonic('6'); }
bool CIDEBookmarks::GoToBookmark7(void) { return GoToMnemonic('7'); }
bool CIDEBookmarks::GoToBookmark8(void) { return GoToMnemonic('8'); }
bool CIDEBookmarks::GoToBookmark9(void) { return GoToMnemonic('9'); }


//////////////////////////////////////////////////////////////////////////
//
// A tab's bookmarks as they are: in its gutter's bookmark column, each
// line's mnemonic on a cell of the accent colour (the cell alone for a
// plain one), and marked on its scroll bar.  The marks need the lines laid
// out: if they aren't yet, Poll places them once they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::ShowBookmarks(Ptr<CIDEEditorTab> pTab)
{
	Ptr<CCodeEditor>			pEditor;
	CArrayTemplate<SInt32Vec2>	asMarks;
	SIDEBookmark*				psBookmark;
	CInt32Rect					cLine;
	ARGB32						sAccent;
	size						i;

	pEditor = pTab->GetCodeEditor();
	sAccent = mpcContent->GetTheme()->GetColour(TC_Accent);
	pEditor->ClearLineIcons(IDE_GUTTER_BOOKMARKS);
	asMarks.Init();
	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		psBookmark = masBookmarks.Get(i);
		if ((psBookmark->iLine >= 0) && psBookmark->szFile.Equals(pTab->GetFileName()))
		{
			pEditor->SetLineIcon(IDE_GUTTER_BOOKMARKS, (size)psBookmark->iLine, (psBookmark->cMnemonic != '\0') ? psBookmark->cMnemonic : ' ', mpcContent->GetTheme()->GetColour(TC_Editor), sAccent);
			if (pTab->GetEditor()->GetLineBounds((size)psBookmark->iLine, &cLine))
			{
				asMarks.Add()->Init(cLine.GetTop(), cLine.GetHeight());
			}
			else
			{
				mbMarksDue = true;
			}
		}
	}
	pTab->GetPane()->GetScrollBar(SBO_Vertical)->SetMarks(IDE_BOOKMARK_MARKS, asMarks.NumElements() ? asMarks.Get(0) : NULL, asMarks.NumElements(), sAccent);
	asMarks.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A press in an editor's gutter, on its bookmark column: a bookmark there
// is taken off.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::IconClicked(CCodeEditor* pcEditor, SInt32Vec2 sIcon)
{
	Ptr<CIDEEditorTab>	pTab;
	size				i;
	int32				iBookmark;

	if (sIcon.x != IDE_GUTTER_BOOKMARKS)
	{
		return;
	}
	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		pTab = mpcContent->GetDocuments()->GetTab(i);
		if (&pTab->GetCodeEditor() == pcEditor)
		{
			iBookmark = FindBookmark(pTab->GetFileName(), sIcon.y);
			if (iBookmark >= 0)
			{
				RemoveBookmark((size)iBookmark);
				Changed(pTab->GetFileName(), NULL);
			}
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of the open file szFile (CIDEDocuments::DocumentChanged): its
// bookmarks move with their lines.  The gutter shows them where they are
// at once; the list and the user's file catch up when the edits stop
// (Poll).
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	SIDEBookmark*	psBookmark;
	size			i;
	bool			bMoved;

	bMoved = false;
	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		psBookmark = masBookmarks.Get(i);
		psBookmark->iColumn = 0;
		if (psBookmark->szFile.Equals(szFile) && AdjustIDEPlace(&psBookmark->iLine, &psBookmark->iColumn, psChange))
		{
			bMoved = true;
		}
	}
	if (bMoved)
	{
		mbMoved = true;
		miMoved = GetDebugTickCount();
		mbMarksDue = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: bookmarks that edits moved are shown where they are
// in the active editor's gutter; once the edits have stopped for
// IDE_BOOKMARKS_REFRESH the list is shown again and the user's file
// written.  Milliseconds until it should be looked at again, -1 if not
// until something changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEBookmarks::Poll(int64 iNow)
{
	Ptr<CIDEEditorTab>	pTab;

	if (mbMarksDue)
	{
		mbMarksDue = false;
		pTab = mpcContent->GetDocuments()->GetActiveTab();
		if (pTab.IsNotNull())
		{
			ShowBookmarks(pTab);
		}
	}
	if (!mbMoved)
	{
		return mbMarksDue ? IDE_BOOKMARKS_REFRESH : -1;
	}
	if (iNow - miMoved < IDE_BOOKMARKS_REFRESH)
	{
		return IDE_BOOKMARKS_REFRESH - (iNow - miMoved);
	}
	mbMoved = false;
	ShowList(true);
	mpcContent->GetUserFile()->Write();
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The Bookmarks tool window's tree: how many there are, then each file
// with a bookmark and under it its bookmarked lines in order, a mnemonic
// after its line.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::ShowList(bool bKeepSelected)
{
	Ptr<CTreeContainer>	pTree;
	CIDEFileLines		cLines;
	SIDEBookmark*		psBookmark;
	SIDEBookmark*		psOther;
	CChars				szText;
	size				i;
	size				j;
	size				uiCount;
	int64				iSelectedData;
	int32				iRoot;
	int32				iFile;
	int					iCompare;

	if (mpPane.IsNull())
	{
		return;
	}

	//By file (its name, then all of its path), then by line: before the first already listed that is after it.
	maiListed.ReInit();
	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		psBookmark = masBookmarks.Get(i);
		for (j = 0; j < maiListed.NumElements(); j++)
		{
			psOther = masBookmarks.Get((size)maiListed.GetValue(j));
			iCompare = StringInsensitiveCompare(GetIDEFileNamePart(psBookmark->szFile.Text()), GetIDEFileNamePart(psOther->szFile.Text()));
			if (iCompare == 0)
			{
				iCompare = StringCompare(psBookmark->szFile.Text(), psOther->szFile.Text());
			}
			if ((iCompare < 0) || ((iCompare == 0) && (psBookmark->iLine < psOther->iLine)))
			{
				break;
			}
		}
		maiListed.InsertAt((int)i, j);
	}

	pTree = mpPane->GetTree();
	iSelectedData = mpPane->BeginShow(bKeepSelected);
	if (masBookmarks.NumElements() == 0)
	{
		return;
	}
	szText.Init("Bookmarks  ");
	szText.Append((uint32)masBookmarks.NumElements());
	iRoot = pTree->AddNode(TREE_NO_NODE, szText.Text(), NULL, 0);
	szText.Kill();
	pTree->SetExpanded(iRoot, true);

	cLines.Init(&mpcContent->GetDocuments());
	for (i = 0; i < maiListed.NumElements(); i += uiCount)
	{
		psBookmark = masBookmarks.Get((size)maiListed.GetValue(i));
		for (uiCount = 1; (i + uiCount < maiListed.NumElements()) && masBookmarks.Get((size)maiListed.GetValue(i + uiCount))->szFile.Equals(psBookmark->szFile); uiCount++)
		{
		}
		szText.Init("  ");
		szText.Append((uint32)uiCount);
		iFile = mpPane->AddGroup(iRoot, GetIDEFileNamePart(psBookmark->szFile.Text()), szText.Text(), psBookmark->szFile.Text());
		szText.Kill();
		pTree->SetExpanded(iFile, true);
		for (j = i; j < i + uiCount; j++)
		{
			psBookmark = masBookmarks.Get((size)maiListed.GetValue(j));
			szText.Init();
			if (psBookmark->cMnemonic != '\0')
			{
				szText.Append("  [");
				szText.Append(psBookmark->cMnemonic);
				szText.Append("]");
			}
			mpPane->AddResult(iFile, &cLines, psBookmark->szFile.Text(), psBookmark->iLine, 0, 0, szText.Empty() ? NULL : szText.Text(), (int64)j + 1);
			szText.Kill();
		}
	}
	cLines.Kill();
	mpPane->EndShow(iSelectedData);
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's source: where the bookmark a row is of is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBookmarks::GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest)
{
	SIDEBookmark*	psBookmark;

	if ((iData <= 0) || ((size)iData > maiListed.NumElements()))
	{
		return false;
	}
	psBookmark = masBookmarks.Get((size)maiListed.GetValue((size)iData - 1));
	memset(psDest, 0, sizeof(SIDELocation));
	psDest->szFileName.Init(psBookmark->szFile);
	psDest->iLine = psBookmark->iLine;
	psDest->iColumn = 0;
	psDest->iContainer = -1;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so have the bookmarks in them.  They aren't
// written: they are the workspace's that is closing, whose file has them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::Clear(void)
{
	RemoveAll();
	mbMoved = false;
	ShowList(false);
}


//////////////////////////////////////////////////////////////////////////
//
// The user's file: <Bookmark File="..." Line="12" Mnemonic="1"/> for each,
// its line from 1.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::WriteUser(CXMLWriter* pcWriter)
{
	SIDEBookmark*	psBookmark;
	char			szMnemonic[2];
	size			i;

	for (i = 0; i < masBookmarks.NumElements(); i++)
	{
		psBookmark = masBookmarks.Get(i);
		pcWriter->Begin(IDE_BOOKMARK_TAG);
		pcWriter->Attribute("File", psBookmark->szFile.Text());
		pcWriter->Attribute("Line", (int32)(psBookmark->iLine + 1));
		if (psBookmark->cMnemonic != '\0')
		{
			szMnemonic[0] = psBookmark->cMnemonic;
			szMnemonic[1] = '\0';
			pcWriter->Attribute("Mnemonic", szMnemonic);
		}
		pcWriter->End();
	}
}


void CIDEBookmarks::ReadUser(CMarkupTag* pcRoot)
{
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szFile;
	char*			szLine;
	char*			szMnemonic;
	size			i;

	RemoveAll();
	for (pcTag = pcRoot ? pcRoot->GetTag(&sIterator) : NULL; pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
	{
		if (StringCompare(pcTag->GetName(), IDE_BOOKMARK_TAG) != 0)
		{
			continue;
		}
		szFile = pcTag->GetAttribute((char*)"File");
		szLine = pcTag->GetAttribute((char*)"Line");
		szMnemonic = pcTag->GetAttribute((char*)"Mnemonic");
		if (szFile && szLine && (atoi(szLine) > 0) && (FindBookmark(szFile, atoi(szLine) - 1) < 0))
		{
			AddBookmark(szFile, atoi(szLine) - 1, (szMnemonic && (FindMnemonic(szMnemonic[0]) < 0)) ? szMnemonic[0] : '\0');
		}
	}
	mbMoved = false;
	ShowList(false);
	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		ShowBookmarks(mpcContent->GetDocuments()->GetTab(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Bookmarks: 2", then each as the list has them,
// "  Shapes.cpp:12 [1]".
//
//////////////////////////////////////////////////////////////////////////
void CIDEBookmarks::Print(CChars* pszDest)
{
	SIDEBookmark*	psBookmark;
	size			i;

	ShowList(true);
	pszDest->Append("Bookmarks: ");
	pszDest->Append((uint32)masBookmarks.NumElements());
	pszDest->Append('\n');
	for (i = 0; i < maiListed.NumElements(); i++)
	{
		psBookmark = masBookmarks.Get((size)maiListed.GetValue(i));
		pszDest->Append("  ");
		pszDest->Append(GetIDEFileNamePart(psBookmark->szFile.Text()));
		pszDest->Append(':');
		pszDest->Append((int32)(psBookmark->iLine + 1));
		if (psBookmark->cMnemonic != '\0')
		{
			pszDest->Append(" [");
			pszDest->Append(psBookmark->cMnemonic);
			pszDest->Append(']');
		}
		pszDest->Append('\n');
	}
}


size CIDEBookmarks::NumBookmarks(void) const { return masBookmarks.NumElements(); }
SIDEBookmark* CIDEBookmarks::GetBookmark(size uiIndex) { return masBookmarks.Get(uiIndex); }
Ptr<CIDEResultsPane> CIDEBookmarks::GetResultsPane(void) const { return mpPane; }
Ptr<CDockPane> CIDEBookmarks::GetTool(void) const { return mpTool; }
Ptr<CPromptPopup> CIDEBookmarks::GetPrompt(void) const { return mpPrompt; }

