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
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEAskClaude.h"


#define IDE_CLAUDE_POLL		50		//Milliseconds between looks at Claude while it works.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mcClaude.Init();
	muiClaudeReloaded = 0;
	muiClaudeTools = 0;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpClaudePane);
	M_Pointer(mpClaudeTool);
	U_Unknown(CClaudeCommand, mcClaude);
	U_Size(muiClaudeReloaded);
	U_Size(muiClaudeTools);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::Free(void)
{
	if (mpClaudePane.IsNotNull() && mpClaudePane->GetSendButton().IsNotNull())
	{
		mpClaudePane->GetSendButton()->GetClickEvent()->Remove(this);
		mpClaudePane->GetStopButton()->GetClickEvent()->Remove(this);
		mpClaudePane->GetNewButton()->GetClickEvent()->Remove(this);
	}
	//A request Claude is still working on is stopped.
	mcClaude.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEAskClaude::Save(CObjectWriter* pcFile) { return false; }
bool CIDEAskClaude::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Send: the request typed in the Claude tab is sent.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::ClaudeSendClicked(void)
{
	CChars	szRequest;

	szRequest.Init(mpClaudePane->GetRequest());
	szRequest.StripWhitespace(true);
	if (!szRequest.Empty() && AskClaude(szRequest.Text()))
	{
		mpClaudePane->SetRequest("");
	}
	szRequest.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Stop: Claude is stopped.  What it changed stays changed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::ClaudeStopClicked(void)
{
	if (mcClaude.IsRunning())
	{
		mcClaude.Terminate();
		mpClaudePane->AppendNote("Stopped.", true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// New Conversation: the next request starts one, with nothing said
// before.  Not while Claude is working.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::ClaudeNewClicked(void)
{
	if (mcClaude.IsRunning())
	{
		mpClaudePane->SetStatus("Claude is still working: stop it first.");
		return;
	}
	mcClaude.Forget();
	mpClaudePane->Clear();
	mpClaudePane->SetStatus("New conversation.");
}


//////////////////////////////////////////////////////////////////////////
//
// Sends szRequest to Claude Code, run in the solution's folder (or the
// open file's, with no solution), carrying the conversation on.  Where the
// caret and the selection are in the open file is said after it.  Files
// with unsaved changes are saved first: Claude reads them from the disk.
// False if Claude is already working or couldn't be started.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEAskClaude::AskClaude(const char* szRequest)
{
	CChars	szDirectory;
	CChars	szFull;
	bool	bResult;

	ShowClaudeTab();
	if (mcClaude.IsRunning())
	{
		mpClaudePane->SetStatus("Claude is still working on the last request.");
		return false;
	}

	szDirectory.Init();
	if (!GetClaudeDirectory(&szDirectory))
	{
		szDirectory.Kill();
		mpClaudePane->SetStatus("Open a solution or a file first: Claude works in its folder.");
		return false;
	}
	if (!mpcContent->GetDocuments()->SaveAllFiles())
	{
		szDirectory.Kill();
		mpClaudePane->SetStatus("Claude wasn't asked: a file couldn't be saved.");
		return false;
	}

	szFull.Init(szRequest);
	AppendClaudeContext(&szFull);
	mpClaudePane->SetDirectory(szDirectory.Text());
	mpClaudePane->AppendRequest(szRequest);
	muiClaudeReloaded = 0;
	muiClaudeTools = 0;
	bResult = mcClaude.Start(szDirectory.Text(), szFull.Text());
	szFull.Kill();
	szDirectory.Kill();

	if (!bResult)
	{
		mpClaudePane->AppendNote("Claude Code couldn't be started: is claude on the path?", true);
		mpClaudePane->SetStatus("");
		return false;
	}
	mpClaudePane->SetStatus("Claude is working.");
	mpcContent->GetWindow()->RequestTick(IDE_CLAUDE_POLL);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The folder Claude works in: the startup solution's, or the open file's
// if no solution is open.  False if neither is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEAskClaude::GetClaudeDirectory(CChars* pszDirectory)
{
	CFileUtil	cFileUtil;

	if (mpcContent->GetSolutions()->IsSolutionOpen())
	{
		pszDirectory->Append(mpcContent->GetSolutions()->GetStartupFile());
	}
	else if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
	{
		pszDirectory->Append(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	}
	else
	{
		return false;
	}
	cFileUtil.FullPath(pszDirectory);
	cFileUtil.RemoveLastFromPath(pszDirectory);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// After the request, where the user is: the open file, the caret's line
// and the lines selected, counted from 1 as Claude counts them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::AppendClaudeContext(CChars* pszRequest)
{
	CFileUtil		cFileUtil;
	CChars			szFile;
	Ptr<CTextArea>	pEditor;
	size			uiStart;
	size			uiEnd;
	size			uiFirst;
	size			uiLast;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return;
	}
	pEditor = mpcContent->GetDocuments()->GetActiveTab()->GetEditor();

	szFile.Init(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	cFileUtil.FullPath(&szFile);
	pszRequest->Append("\n\n(From the HCP65 IDE: the file open in the editor is ");
	pszRequest->Append(szFile);
	szFile.Kill();
	pszRequest->Append(", with the caret on line ");
	pszRequest->Append((uint64)(pEditor->FindLine(pEditor->GetCaretPosition()) + 1));
	if (pEditor->GetSelection(&uiStart, &uiEnd) && (uiEnd > uiStart))
	{
		uiFirst = pEditor->FindLine(uiStart);
		uiLast = pEditor->FindLine(uiEnd - 1);
		if (uiFirst == uiLast)
		{
			pszRequest->Append(" and part of it selected");
		}
		else
		{
			pszRequest->Append(" and lines ");
			pszRequest->Append((uint64)(uiFirst + 1));
			pszRequest->Append(" to ");
			pszRequest->Append((uint64)(uiLast + 1));
			pszRequest->Append(" selected");
		}
	}
	pszRequest->Append(".)");
}


//////////////////////////////////////////////////////////////////////////
//
// What Claude has done since the last look goes in the Claude tab, and
// files it changed that are open are read again.  When it finishes every
// open file is looked at, in case it changed one some other way.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::PollClaude(Ptr<CWindow> pWindow)
{
	CArrayClaudeEvents	aEvents;
	CClaudeEvent*		pcEvent;
	bool				bRunning;
	size				i;
	CChars				szStatus;

	if (!mcClaude.IsRunning())
	{
		return;
	}

	aEvents.Init();
	bRunning = mcClaude.Poll(&aEvents);
	szStatus.Init();
	for (i = 0; i < aEvents.NumElements(); i++)
	{
		pcEvent = aEvents.Get(i);
		mpClaudePane->AppendEvent(pcEvent);
		if (pcEvent->GetType() == CET_ToolUse)
		{
			muiClaudeTools++;
		}
		else if (pcEvent->GetType() == CET_Finished)
		{
			szStatus.Append(pcEvent->IsError() ? "Failed" : "Done");
			if (pcEvent->GetDetail()[0] != '\0')
			{
				szStatus.Append(", $");
				szStatus.Append(pcEvent->GetDetail());
			}
			szStatus.Append('.');
		}
	}
	aEvents.Kill();

	ReloadClaudeFiles(!bRunning);

	if (bRunning)
	{
		//Claude finishes a moment before it exits: the finished status stays until it does.
		if (!szStatus.Empty())
		{
			mpClaudePane->SetStatus(szStatus.Text());
		}
		else if ((muiClaudeTools > 0) && !mcClaude.IsFinished())
		{
			szStatus.Append("Claude is working: ");
			szStatus.Append((uint64)muiClaudeTools);
			szStatus.Append((muiClaudeTools == 1) ? " tool used." : " tools used.");
			mpClaudePane->SetStatus(szStatus.Text());
		}
		pWindow->RequestTick(IDE_CLAUDE_POLL);
	}
	else
	{
		if (szStatus.Empty() && !mcClaude.IsFinished())
		{
			//It exited without saying it had finished: stopped, or it failed before it started.
			if (!mcClaude.GetErrors()->Empty())
			{
				mpClaudePane->AppendNote(mcClaude.GetErrors()->Text(), true);
			}
			szStatus.Append("Claude stopped (exit code ");
			szStatus.Append((int32)mcClaude.GetExitCode());
			szStatus.Append(").");
		}
		if (!szStatus.Empty())
		{
			mpClaudePane->SetStatus(szStatus.Text());
		}
	}
	szStatus.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The open files Claude has changed since the last look are read again.
// With bAll every open file is looked at, and read again if it is no
// longer what its tab shows.  A tab with changes of its own isn't.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::ReloadClaudeFiles(bool bAll)
{
	CArrayChars*		paszChanged;
	Ptr<CIDEEditorTab>	pTab;
	size				i;

	//Every file read again is one edit for undo: undoing any of them undoes them all.
	mpcContent->GetEdit()->BeginUndoGroup();
	paszChanged = mcClaude.GetChangedFiles();
	for (; muiClaudeReloaded < paszChanged->NumElements(); muiClaudeReloaded++)
	{
		pTab = FindClaudeTab(paszChanged->GetText(muiClaudeReloaded));
		if (pTab.IsNotNull())
		{
			ReloadClaudeFile(pTab);
		}
	}

	if (bAll)
	{
		for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
		{
			ReloadClaudeFile(mpcContent->GetDocuments()->GetTab(i));
		}
	}
	mpcContent->GetEdit()->EndUndoGroup();
}


//////////////////////////////////////////////////////////////////////////
//
// pTab shows its file as it is on the disk, if that is different.  False
// if it couldn't be read, or the tab has changes of its own.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEAskClaude::ReloadClaudeFile(Ptr<CIDEEditorTab> pTab)
{
	CTextFile	cFile;
	CFileUtil	cFileUtil;
	CChars		szNote;
	bool		bSame;

	cFile.Init();
	if (!cFile.Read(pTab->GetFileName()))
	{
		cFile.Kill();
		return false;
	}
	bSame = SameIDEText(cFile.Text(), pTab->GetEditor()->GetText());
	if (bSame)
	{
		cFile.Kill();
		return true;
	}

	szNote.Init();
	cFileUtil.GetFileName(&szNote, pTab->GetFileName());
	if (pTab->IsModified())
	{
		szNote.Append(" changed, but it has unsaved changes so it wasn't read again.");
		mpClaudePane->AppendNote(szNote.Text(), true);
		szNote.Kill();
		cFile.Kill();
		return false;
	}

	pTab->Reload(cFile.Text());
	cFile.Kill();
	mpcContent->GetGitStatus()->GitFileSaved(pTab->GetFileName());
	szNote.Append(" read again.");
	mpClaudePane->AppendNote(szNote.Text(), false);
	szNote.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The open file that is szFileName as Claude named it: a full path, in
// any case, with either slash.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEAskClaude::FindClaudeTab(const char* szFileName)
{
	CFileUtil	cFileUtil;
	CChars		szWanted;
	CChars		szTab;
	size		i;
	bool		bFound;

	szWanted.Init(szFileName);
	szWanted.Replace('/', '\\');
	bFound = false;
	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		szTab.Init(mpcContent->GetDocuments()->GetTab(i)->GetFileName());
		cFileUtil.FullPath(&szTab);
		szTab.Replace('/', '\\');
		bFound = StringInsensitiveCompare(szTab.Text(), szWanted.Text()) == 0;
		szTab.Kill();
		if (bFound)
		{
			break;
		}
	}
	szWanted.Kill();
	return bFound ? mpcContent->GetDocuments()->GetTab(i) : Ptr<CIDEEditorTab>();
}


//////////////////////////////////////////////////////////////////////////
//
// True if the texts are the same but for carriage returns.
//
//////////////////////////////////////////////////////////////////////////
bool SameIDEText(const char* szText1, const char* szText2)
{
	for (;;)
	{
		while (*szText1 == '\r')
		{
			szText1++;
		}
		while (*szText2 == '\r')
		{
			szText2++;
		}
		if (*szText1 != *szText2)
		{
			return false;
		}
		if (*szText1 == '\0')
		{
			return true;
		}
		szText1++;
		szText2++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::ShowClaudeTab(void)
{
	mpcContent->GetDock()->Show(mpClaudeTool);
}


CClaudeCommand* CIDEAskClaude::GetClaude(void) { return &mcClaude; }
Ptr<CIDEClaudePane> CIDEAskClaude::GetClaudePane(void) { return mpClaudePane; }
Ptr<CDockPane> CIDEAskClaude::GetTool(void) { return mpClaudeTool; }


//////////////////////////////////////////////////////////////////////////
//
// The Claude tab, with its Send, Stop and New Conversation buttons, put
// at psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::CreatePane(SDockPlace* psPlace)
{
	mpClaudePane = OMalloc<CIDEClaudePane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetFontColours()->GetFont(mpcContent->GetTheme()->GetColour(TC_Icon)), mpcContent->GetCodeFamily());
	mpClaudePane->GetSendButton()->GetClickEvent()->Add(this, &CIDEAskClaude::ClaudeSendClicked);
	mpClaudePane->GetStopButton()->GetClickEvent()->Add(this, &CIDEAskClaude::ClaudeStopClicked);
	mpClaudePane->GetNewButton()->GetClickEvent()->Add(this, &CIDEAskClaude::ClaudeNewClicked);
	mpClaudeTool = mpcContent->GetDock()->AddPane(IDE_CLAUDE_ID, "Claude", NULL, mpClaudePane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// What Claude is writing is kept in view.
//
//////////////////////////////////////////////////////////////////////////
void CIDEAskClaude::ScrollToEnd(void)
{
	if (mpClaudePane.IsNotNull())
	{
		mpClaudePane->ScrollToEnd();
	}
}
bool CIDEAskClaude::IsClaudeRunning(void) { return mcClaude.IsRunning(); }

