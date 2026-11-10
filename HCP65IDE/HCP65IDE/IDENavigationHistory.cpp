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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDENavigationHistory.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENavigationPlace::Init(void)
{
	szFile.Init();
	uiProject = SOLUTION_EXPLORER_NO_PROJECT;
	iLine = 0;
	iColumn = 0;
}


void SIDENavigationPlace::Init(SIDENavigationPlace* psPlace)
{
	szFile.Init(psPlace->szFile);
	uiProject = psPlace->uiProject;
	iLine = psPlace->iLine;
	iColumn = psPlace->iColumn;
}


void SIDENavigationPlace::Kill(void)
{
	szFile.Kill();
}


void SIDENavigationPlace::Set(SIDENavigationPlace* psPlace)
{
	szFile.Set(psPlace->szFile);
	uiProject = psPlace->uiProject;
	iLine = psPlace->iLine;
	iColumn = psPlace->iColumn;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	masBack.Init();
	masForward.Init();
	msSettled.Init();
	mbSettled = false;
	msLast.Init();
	mbLast = false;
	miLastChange = 0;
	miJumps = 0;
	mbMoving = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayIDENavigationPlaces, masBack);
	U_Unknown(CArrayIDENavigationPlaces, masForward);
	U_Unknown(SIDENavigationPlace, msSettled);
	U_Bool(mbSettled);
	U_Unknown(SIDENavigationPlace, msLast);
	U_Bool(mbLast);
	U_Int64(miLastChange);
	U_Int32(miJumps);
	U_Bool(mbMoving);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::Free(void)
{
	msLast.Kill();
	msSettled.Kill();
	KillPlaces(&masForward);
	KillPlaces(&masBack);
}


//////////////////////////////////////////////////////////////////////////
//
// The history is for the session only.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::Save(CObjectWriter* pcFile) { return false; }
bool CIDENavigationHistory::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Back and Forward, as IntelliJ's, while there is somewhere to go.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.Back", "Back", "Control+Alt+Cursor Left", Ptr<CImageCel>(), this, &CIDENavigationHistory::NavigateBack)->SetEnabledCheck(this, &CIDENavigationHistory::CanGoBack);
	pCommands->Add("Navigate.Forward", "Forward", "Control+Alt+Cursor Right", Ptr<CImageCel>(), this, &CIDENavigationHistory::NavigateForward)->SetEnabledCheck(this, &CIDENavigationHistory::CanGoForward);
}


//////////////////////////////////////////////////////////////////////////
//
// The Navigate menu's Back and Forward, as IntelliJ's has them first.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::AddNavigateItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.Back"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.Forward"));
	pMenu->AddSeparator();
}


void CIDENavigationHistory::NavigateBack(void) { GoBack(); }
void CIDENavigationHistory::NavigateForward(void) { GoForward(); }
bool CIDENavigationHistory::CanGoBack(void) { return masBack.NumElements() > 0; }
bool CIDENavigationHistory::CanGoForward(void) { return masForward.NumElements() > 0; }
size CIDENavigationHistory::NumBack(void) { return masBack.NumElements(); }
size CIDENavigationHistory::NumForward(void) { return masForward.NumElements(); }


//////////////////////////////////////////////////////////////////////////
//
// A jump is starting (CIDEDocuments::OpenLocation or OpenFile): where the
// caret is now, in psFrom (initialised either way).  False if there is
// nowhere, or the jump is inside another, or Back or Forward is moving:
// nothing is to be recorded.  EndJump must follow.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::BeginJump(SIDENavigationPlace* psFrom)
{
	psFrom->Init();
	miJumps++;
	if ((miJumps > 1) || mbMoving)
	{
		return false;
	}
	return GetCurrentPlace(psFrom);
}


//////////////////////////////////////////////////////////////////////////
//
// The jump BeginJump started has ended: if it went (bJumped, and
// BeginJump said to record it) somewhere far from psFrom, psFrom goes on
// Back and Forward is cleared.  Where it went is where the caret has
// stayed.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::EndJump(SIDENavigationPlace* psFrom, bool bJumped)
{
	SIDENavigationPlace	sTo;

	miJumps--;
	if (bJumped && (miJumps == 0))
	{
		sTo.Init();
		if (GetCurrentPlace(&sTo) && IsFar(psFrom, &sTo))
		{
			Push(psFrom, true);
		}
		sTo.Kill();
	}
	if (miJumps == 0)
	{
		Arrived();
	}
	psFrom->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The caret was put somewhere by the IDE, not the user (a line to go to,
// where the debugger stopped): it isn't a move to record, and where it is
// is where it has stayed.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::CaretPlaced(void)
{
	if (miJumps == 0)
	{
		Arrived();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: a caret moved far from where it last stayed put (a
// click, Control+End, another tab) puts that place on Back.  A place is
// where the caret stays for IDE_NAVIGATION_SETTLE, so paging through a
// file records only where it started.  Milliseconds until it should be
// looked at again, -1 if not until something changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDENavigationHistory::Poll(int64 iNow)
{
	SIDENavigationPlace	sNow;
	int64				iWait;

	sNow.Init();
	iWait = -1;
	if (!GetCurrentPlace(&sNow))
	{
		mbLast = false;
	}
	else if (!mbLast || !sNow.szFile.EqualsIgnoreCase(msLast.szFile) || (sNow.iLine != msLast.iLine) || (sNow.iColumn != msLast.iColumn))
	{
		if (mbSettled && IsFar(&msSettled, &sNow))
		{
			Push(&msSettled, true);
			mbSettled = false;
		}
		msLast.Set(&sNow);
		mbLast = true;
		miLastChange = iNow;
		iWait = IDE_NAVIGATION_SETTLE;
	}
	else if (!mbSettled || !msSettled.szFile.EqualsIgnoreCase(sNow.szFile) || (msSettled.iLine != sNow.iLine) || (msSettled.iColumn != sNow.iColumn))
	{
		if (iNow - miLastChange >= IDE_NAVIGATION_SETTLE)
		{
			msSettled.Set(&sNow);
			mbSettled = true;
		}
		else
		{
			iWait = IDE_NAVIGATION_SETTLE - (iNow - miLastChange);
		}
	}
	sNow.Kill();
	return iWait;
}


//////////////////////////////////////////////////////////////////////////
//
// Back (Control+Alt+Left): the last place on Back, where the caret is now
// put on Forward.  False if Back is empty or its file can't be opened.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::GoBack(void)
{
	SIDENavigationPlace	sPlace;
	SIDENavigationPlace	sNow;
	bool				bResult;

	if (masBack.NumElements() == 0)
	{
		return false;
	}
	sPlace.Init(masBack.Tail());
	masBack.Tail()->Kill();
	masBack.RemoveTail();
	sNow.Init();
	if (GetCurrentPlace(&sNow))
	{
		masForward.Add()->Init(&sNow);
	}
	sNow.Kill();
	bResult = Go(&sPlace);
	sPlace.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Forward (Control+Alt+Right): the place Back last left, where the caret
// is now put on Back again.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::GoForward(void)
{
	SIDENavigationPlace	sPlace;
	SIDENavigationPlace	sNow;
	bool				bResult;

	if (masForward.NumElements() == 0)
	{
		return false;
	}
	sPlace.Init(masForward.Tail());
	masForward.Tail()->Kill();
	masForward.RemoveTail();
	sNow.Init();
	if (GetCurrentPlace(&sNow))
	{
		Push(&sNow, false);
	}
	sNow.Kill();
	bResult = Go(&sPlace);
	sPlace.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The place's file opened (again, if it was closed) with its caret there,
// as near as the file has it now, its editor having the keys.  Nothing is
// recorded.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::Go(SIDENavigationPlace* psPlace)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	int32				iLine;
	bool				bResult;

	mbMoving = true;
	bResult = mpcContent->GetDocuments()->OpenFile(psPlace->szFile.Text(), psPlace->uiProject);
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (bResult && pTab.IsNotNull())
	{
		pEditor = pTab->GetEditor();
		iLine = psPlace->iLine;
		if ((size)iLine >= pEditor->NumLines())
		{
			iLine = (int32)pEditor->NumLines() - 1;
		}
		mpcContent->GetDocuments()->SetCaretPlace(iLine, psPlace->iColumn);
		mpcContent->GetDocuments()->FocusActiveEditor();
	}
	mbMoving = false;
	Arrived();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the caret is now is where it has stayed, for the next move far
// from it to be recorded from.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::Arrived(void)
{
	mbLast = GetCurrentPlace(&msLast);
	mbSettled = mbLast;
	if (mbSettled)
	{
		msSettled.Set(&msLast);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The active editor's caret, false if no file's editor is active.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::GetCurrentPlace(SIDENavigationPlace* psDest)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	size				uiPosition;
	size				uiLine;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	uiPosition = pEditor->GetCaretPosition();
	uiLine = pEditor->FindLine(uiPosition);
	psDest->szFile.Set(pTab->GetFileName());
	psDest->uiProject = pTab->GetProject();
	psDest->iLine = (int32)uiLine;
	psDest->iColumn = (int32)(uiPosition - pEditor->GetLineStart(uiLine));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether psTo is somewhere else: another file, or IDE_NAVIGATION_FAR_LINES
// or more lines away.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigationHistory::IsFar(SIDENavigationPlace* psFrom, SIDENavigationPlace* psTo)
{
	int32	iLines;

	if (!psFrom->szFile.EqualsIgnoreCase(psTo->szFile))
	{
		return true;
	}
	iLines = psTo->iLine - psFrom->iLine;
	return (iLines >= IDE_NAVIGATION_FAR_LINES) || (iLines <= -IDE_NAVIGATION_FAR_LINES);
}


//////////////////////////////////////////////////////////////////////////
//
// psPlace on Back, unless it is the last there already; the oldest goes
// past IDE_NAVIGATION_MAXIMUM.  With bClearForward (a new move, not
// Forward) Forward is cleared, as a browser's is.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::Push(SIDENavigationPlace* psPlace, bool bClearForward)
{
	SIDENavigationPlace*	psTail;

	if (bClearForward)
	{
		KillPlaces(&masForward);
		masForward.Init();
	}
	psTail = masBack.Tail();
	if (psTail && psTail->szFile.EqualsIgnoreCase(psPlace->szFile) && (psTail->iLine == psPlace->iLine))
	{
		return;
	}
	if (masBack.NumElements() >= IDE_NAVIGATION_MAXIMUM)
	{
		masBack.Get(0)->Kill();
		masBack.RemoveAt(0);
	}
	masBack.Add()->Init(psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of the open file szFile (CIDEDocuments::DocumentChanged): the
// places in it move with their text.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	size	i;

	for (i = 0; i < masBack.NumElements(); i++)
	{
		AdjustPlace(masBack.Get(i), szFile, psChange);
	}
	for (i = 0; i < masForward.NumElements(); i++)
	{
		AdjustPlace(masForward.Get(i), szFile, psChange);
	}
	AdjustPlace(&msSettled, szFile, psChange);
	AdjustPlace(&msLast, szFile, psChange);
}


void CIDENavigationHistory::AdjustPlace(SIDENavigationPlace* psPlace, const char* szFile, SIDEPlaceChange* psChange)
{
	if (psPlace->szFile.Equals(szFile))
	{
		AdjustIDEPlace(&psPlace->iLine, &psPlace->iColumn, psChange);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::KillPlaces(CArrayIDENavigationPlaces* pasPlaces)
{
	size	i;

	for (i = 0; i < pasPlaces->NumElements(); i++)
	{
		pasPlaces->Get(i)->Kill();
	}
	pasPlaces->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// "back [Calculator.cpp:367, ...] forward [...]", lines from 1, the
// nearest last, for the script's checks.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigationHistory::Print(CChars* pszDest)
{
	pszDest->Append("back [");
	PrintPlaces(&masBack, pszDest);
	pszDest->Append("] forward [");
	PrintPlaces(&masForward, pszDest);
	pszDest->Append("]");
}


void CIDENavigationHistory::PrintPlaces(CArrayIDENavigationPlaces* pasPlaces, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	size		i;

	for (i = 0; i < pasPlaces->NumElements(); i++)
	{
		if (i > 0)
		{
			pszDest->Append(", ");
		}
		szName.Init();
		cFileUtil.GetFileName(&szName, pasPlaces->Get(i)->szFile.Text());
		pszDest->Append(szName);
		pszDest->Append(':');
		pszDest->Append(pasPlaces->Get(i)->iLine + 1);
		szName.Kill();
	}
}
