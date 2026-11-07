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
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "BaseLib/StringHelper.h"
#include "WindowLib/JewelColours.h"
#include "IDEClaudePane.h"


#define IDE_CLAUDE_REQUEST_HEIGHT	64	//About three lines.

#define IDE_CLAUDE_REQUEST		Set32BitColour((uint8)0x6F, (uint8)0xA8, (uint8)0xFF)  //What was asked.
#define IDE_CLAUDE_TOOL			Set32BitColour((uint8)0x86, (uint8)0x8A, (uint8)0x91)  //A tool used.
#define IDE_CLAUDE_ERROR		Set32BitColour((uint8)0xF7, (uint8)0x54, (uint8)0x64)
#define IDE_CLAUDE_DONE			Set32BitColour((uint8)0x5F, (uint8)0xB8, (uint8)0x65)

//Unicode the IDE can't show, as UTF-8, and the ASCII shown for it.
const char* gaszIDEClaudeASCII[] =
{
	"\xE2\x80\x98", "'",	//Quotes.
	"\xE2\x80\x99", "'",
	"\xE2\x80\x9C", "\"",
	"\xE2\x80\x9D", "\"",
	"\xE2\x80\x93", "-",	//En dash.
	"\xE2\x80\x94", "--",	//Em dash.
	"\xE2\x80\xA6", "...",
	"\xE2\x80\xA2", "*",	//Bullet.
	"\xE2\x86\x92", "->",
	"\xE2\x86\x90", "<-",
	"\xE2\x87\x92", "=>",
	"\xE2\x89\xA4", "<=",
	"\xE2\x89\xA5", ">=",
	"\xE2\x89\xA0", "!=",
	"\xC3\x97", "x",		//Times.
	"\xC2\xA0", " ",		//No-break space.
	"\xE2\x9C\x93", "v",	//Tick.
	NULL
};


//////////////////////////////////////////////////////////////////////////
//
// The conversation and the request are in pFamily's fixed width font, as
// tiles.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pStatusFont, Ptr<CFontFamily> pFamily)
{
	Ptr<CGridContainer>	pRequestRow;

	PreInit();

	mpWindow = pWindow;
	mszDirectory.Init();
	mbScrollToEnd = false;
	mpStatus = OMalloc<CLabel>(pWindow, pStatusFont, "");
	mpStatus->SetAlignment(CSH_Left, CSV_Center);

	mpConversation = OMalloc<CTextArea>(pWindow, pTheme, pFamily, "");
	mpConversation->SetEditable(false);
	mpConversation->SetTiledText(true);
	mpConversation->SetColour(JEWEL_TEXT);
	mpConversation->SetAlignment(CSH_Left, CSV_Top);
	mpConversationPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpConversationPane->SetContent(mpConversation);
	mpConversationPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpRequest = OMalloc<CTextArea>(pWindow, pTheme, pFamily, "");
	mpRequest->SetTiledText(true);
	mpRequest->SetColour(JEWEL_TEXT);
	mpRequest->SetAlignment(CSH_Left, CSV_Top);
	mpRequestPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpRequestPane->SetContent(mpRequest);
	mpRequestPane->SetDesiredSize(-1, IDE_CLAUDE_REQUEST_HEIGHT);
	mpRequestPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpSendButton = OMalloc<CButton>(pWindow, pTheme, "Send");
	mpSendButton->SetAlignment(CSH_Left, CSV_Top);
	mpStopButton = OMalloc<CButton>(pWindow, pTheme, "Stop");
	mpStopButton->SetAlignment(CSH_Left, CSV_Top);
	mpNewButton = OMalloc<CButton>(pWindow, pTheme, "New Conversation");
	mpNewButton->SetAlignment(CSH_Left, CSV_Top);

	pRequestRow = OMalloc<CGridContainer>(pWindow);
	pRequestRow->SetAlignment(CSH_Stretch, CSV_Stretch);
	pRequestRow->AddComponent(mpRequestPane, 0, 0, 1, 1, 1, 1);
	pRequestRow->AddComponent(mpSendButton, 1, 0, 1, 1, 0, 1);
	pRequestRow->AddComponent(mpStopButton, 2, 0, 1, 1, 0, 1);
	pRequestRow->AddComponent(mpNewButton, 3, 0, 1, 1, 0, 1);

	mpPanel = OMalloc<CGridContainer>(pWindow);
	mpPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel->AddComponent(mpStatus, 0, 0, 1, 1, 1, 0);
	mpPanel->AddComponent(mpConversationPane, 0, 1, 1, 1, 1, 1);
	mpPanel->AddComponent(pRequestRow, 0, 2, 1, 1, 1, 0);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::Class(void)
{
	M_Pointer(mpWindow);
	U_String(mszDirectory);
	U_Bool(mbScrollToEnd);
	M_Pointer(mpPanel);
	M_Pointer(mpStatus);
	M_Pointer(mpConversation);
	M_Pointer(mpConversationPane);
	M_Pointer(mpRequest);
	M_Pointer(mpRequestPane);
	M_Pointer(mpSendButton);
	M_Pointer(mpStopButton);
	M_Pointer(mpNewButton);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::Free(void)
{
	mszDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The Claude pane isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEClaudePane::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEClaudePane::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// No conversation, for a new one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::Clear(void)
{
	mpConversation->SetText("");
	mpStatus->SetText("");
}


//////////////////////////////////////////////////////////////////////////
//
// What was asked, after a blank line if the conversation has begun.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::AppendRequest(const char* szRequest)
{
	CChars	sz;

	sz.Init();
	if (mpConversation->GetText()[0] != '\0')
	{
		sz.Append('\n');
	}
	sz.Append("> ");
	sz.Append(szRequest);
	sz.Replace("\n", "\n> ");
	Append(sz.Text(), IDE_CLAUDE_REQUEST);
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Claude did, as a line or lines.  Results that worked aren't shown:
// the tool's line says what was used.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::AppendEvent(CClaudeEvent* pcEvent)
{
	CChars		sz;
	const char*	szEnd;

	sz.Init();
	switch (pcEvent->GetType())
	{
	case CET_Started:
		break;

	case CET_Text:
		Append(pcEvent->GetText(), JEWEL_TEXT);
		break;

	case CET_ToolUse:
		sz.Append("  ");
		sz.Append(pcEvent->GetTool());
		if (pcEvent->GetDetail()[0] != '\0')
		{
			//Only the first line of a command.
			sz.Append(' ');
			AppendDetail(pcEvent->GetDetail(), &sz);
		}
		Append(sz.Text(), IDE_CLAUDE_TOOL);
		break;

	case CET_ToolResult:
		if (pcEvent->IsError())
		{
			sz.Append("    ");
			szEnd = strchr(pcEvent->GetText(), '\n');
			if (szEnd)
			{
				sz.AppendSubString(pcEvent->GetText(), szEnd);
			}
			else
			{
				sz.Append(pcEvent->GetText());
			}
			Append(sz.Text(), IDE_CLAUDE_ERROR);
		}
		break;

	case CET_Finished:
		if (pcEvent->IsError())
		{
			sz.Append("Failed: ");
			sz.Append(pcEvent->GetText());
			Append(sz.Text(), IDE_CLAUDE_ERROR);
		}
		if (pcEvent->GetDenied()[0] != '\0')
		{
			sz.Clear();
			sz.Append("Not allowed without asking: ");
			sz.Append(pcEvent->GetDenied());
			Append(sz.Text(), IDE_CLAUDE_ERROR);
		}
		break;

	case CET_Output:
		Append(pcEvent->GetText(), IDE_CLAUDE_TOOL);
		break;
	}
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Something the IDE says, such as a file read again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::AppendNote(const char* szNote, bool bError)
{
	Append(szNote, bError ? IDE_CLAUDE_ERROR : IDE_CLAUDE_DONE);
}


//////////////////////////////////////////////////////////////////////////
//
// szText as lines after the conversation, scrolled to by ScrollToEnd.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::Append(const char* szText, ARGB32 sColour)
{
	CChars	sz;

	sz.Init();
	if (mpConversation->GetText()[0] != '\0')
	{
		sz.Append('\n');
	}
	ToIDEText(szText, &sz);
	mpConversation->AppendText(sz.Text(), sColour);
	sz.Kill();

	mbScrollToEnd = true;
}


//////////////////////////////////////////////////////////////////////////
//
// If text was added, the conversation is scrolled to its last line's
// start: its left, so a long last line doesn't scroll the rest out of
// sight.  Call it from the window's tick: the window is laid out first,
// so the bottom is the new text's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::ScrollToEnd(void)
{
	if (!mbScrollToEnd)
	{
		return;
	}
	mbScrollToEnd = false;
	mpWindow->Layout();
	mpConversationPane->SetScrollPosition(0, mpConversationPane->GetMaximumScrollPosition().y);
}


//////////////////////////////////////////////////////////////////////////
//
// What a tool was used on: only the first line of a command, and a file
// in Claude's folder named from there.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::AppendDetail(const char* szDetail, CChars* pszDest)
{
	const char*	szEnd;
	size		uiLength;

	uiLength = mszDirectory.Length();
	if ((uiLength > 0) && (strlen(szDetail) > uiLength) && (MemICmp(mszDirectory.Text(), szDetail, uiLength) == 0))
	{
		szDetail += uiLength;
	}

	szEnd = strchr(szDetail, '\n');
	if (szEnd)
	{
		pszDest->AppendSubString(szDetail, szEnd);
		pszDest->Append(" ...");
	}
	else
	{
		pszDest->Append(szDetail);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The folder Claude works in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::SetDirectory(const char* szDirectory)
{
	mszDirectory.Clear();
	mszDirectory.Append(szDirectory);
	mszDirectory.Replace('/', '\\');
	if (!mszDirectory.Empty() && !mszDirectory.EndsWith('\\'))
	{
		mszDirectory.Append('\\');
	}
}


//////////////////////////////////////////////////////////////////////////
//
// How the request is going, such as "Claude is working: 3 tools used".
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::SetStatus(const char* szStatus)
{
	mpStatus->SetText(szStatus);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEClaudePane::SetRequest(const char* szRequest)
{
	mpRequest->SetText(szRequest);
}


//////////////////////////////////////////////////////////////////////////
//
// szUTF8 as the IDE's fonts can show it: ASCII, with tabs and line feeds.
// Carriage returns go.
//
//////////////////////////////////////////////////////////////////////////
void ToIDEText(const char* szUTF8, CChars* pszDest)
{
	const unsigned char*	puc;
	size					i;
	size					uiLength;
	bool					bFound;

	puc = (const unsigned char*)szUTF8;
	while (*puc != '\0')
	{
		if (*puc < 0x80)
		{
			if (*puc != '\r')
			{
				pszDest->Append((char)*puc);
			}
			puc++;
			continue;
		}

		bFound = false;
		for (i = 0; gaszIDEClaudeASCII[i] != NULL; i += 2)
		{
			uiLength = strlen(gaszIDEClaudeASCII[i]);
			if (memcmp(puc, gaszIDEClaudeASCII[i], uiLength) == 0)
			{
				pszDest->Append(gaszIDEClaudeASCII[i + 1]);
				puc += uiLength;
				bFound = true;
				break;
			}
		}
		if (bFound)
		{
			continue;
		}

		//Any other character, its continuation bytes too.
		pszDest->Append('?');
		puc++;
		while ((*puc & 0xC0) == 0x80)
		{
			puc++;
		}
	}
}


Ptr<CComponent> CIDEClaudePane::GetComponent(void) { return mpPanel; }
Ptr<CTextArea> CIDEClaudePane::GetRequestArea(void) { return mpRequest; }
Ptr<CButton> CIDEClaudePane::GetSendButton(void) { return mpSendButton; }
Ptr<CButton> CIDEClaudePane::GetStopButton(void) { return mpStopButton; }
Ptr<CButton> CIDEClaudePane::GetNewButton(void) { return mpNewButton; }
char* CIDEClaudePane::GetRequest(void) { return mpRequest->GetText(); }
char* CIDEClaudePane::GetConversation(void) { return mpConversation->GetText(); }
char* CIDEClaudePane::GetStatus(void) { return mpStatus->GetText(); }

