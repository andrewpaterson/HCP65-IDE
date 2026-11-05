/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela ClaudeLib

Codaphela ClaudeLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela ClaudeLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela ClaudeLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "ClaudeEvent.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CClaudeEvent::Init(EClaudeEventType eType)
{
	meType = eType;
	mszText.Init();
	mszTool.Init();
	mszDetail.Init();
	mszFile.Init();
	mszDenied.Init();
	mbError = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CClaudeEvent::Kill(void)
{
	mszDenied.Kill();
	mszFile.Kill();
	mszDetail.Kill();
	mszTool.Kill();
	mszText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tools refused are a list, ", " between them; a tool refused twice
// is named once.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeEvent::AddDenied(const char* szTool)
{
	CChars	szList;
	CChars	szName;
	bool	bFound;

	szList.Init(", ");
	szList.Append(mszDenied);
	szList.Append(", ");
	szName.Init(", ");
	szName.Append(szTool);
	szName.Append(", ");
	bFound = szList.Contains(szName.Text());
	szName.Kill();
	szList.Kill();
	if (bFound)
	{
		return;
	}

	if (!mszDenied.Empty())
	{
		mszDenied.Append(", ");
	}
	mszDenied.Append(szTool);
}


void CClaudeEvent::SetText(const char* szText) { mszText.Clear(); mszText.Append(szText); }
void CClaudeEvent::SetTool(const char* szTool) { mszTool.Clear(); mszTool.Append(szTool); }
void CClaudeEvent::SetDetail(const char* szDetail) { mszDetail.Clear(); mszDetail.Append(szDetail); }
void CClaudeEvent::SetFile(const char* szFile) { mszFile.Clear(); mszFile.Append(szFile); }
void CClaudeEvent::SetError(bool bError) { mbError = bError; }
EClaudeEventType CClaudeEvent::GetType(void) { return meType; }
char* CClaudeEvent::GetText(void) { return mszText.Text(); }
char* CClaudeEvent::GetTool(void) { return mszTool.Text(); }
char* CClaudeEvent::GetDetail(void) { return mszDetail.Text(); }
char* CClaudeEvent::GetFile(void) { return mszFile.Text(); }
char* CClaudeEvent::GetDenied(void) { return mszDenied.Text(); }
bool CClaudeEvent::IsError(void) { return mbError; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CArrayClaudeEvents::Init(void)
{
	maEvents.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CArrayClaudeEvents::Kill(void)
{
	Clear();
	maEvents.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// No events.
//
//////////////////////////////////////////////////////////////////////////
void CArrayClaudeEvents::Clear(void)
{
	size	i;

	for (i = 0; i < maEvents.NumElements(); i++)
	{
		maEvents.Get(i)->Kill();
	}
	maEvents.Kill();
	maEvents.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// A new event at the end.  Pointers to events may move as more are added.
//
//////////////////////////////////////////////////////////////////////////
CClaudeEvent* CArrayClaudeEvents::Add(EClaudeEventType eType)
{
	CClaudeEvent*	pcEvent;

	pcEvent = maEvents.Add();
	pcEvent->Init(eType);
	return pcEvent;
}


CClaudeEvent* CArrayClaudeEvents::Get(size uiIndex) { return maEvents.SafeGet(uiIndex); }
size CArrayClaudeEvents::NumElements(void) { return maEvents.NumElements(); }

